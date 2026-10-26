#include "NeoRegex.h"
#define PCRE2_STATIC 1
#define PCRE2_CODE_UNIT_WIDTH 8
#include "../ThirdParty/pcre2/pcre2.h"
#include <algorithm>
#include <cstdint>
#include <list>
#include <stdexcept>

namespace NeoScript {
namespace {
const size_t TextLimit = 8 * 1024 * 1024;
const size_t CacheLimit = 2 * 1024 * 1024;
// 백트래킹 한도이자 작업 예산의 기본값. 예산은 입력 길이에 비례해 늘어난다.
const uint32_t WorkLimit = 1000000;
const uint64_t WorkPerByte = 32;
struct CodeFree { void operator()(pcre2_code* p) const { pcre2_code_free(p); } };
struct CompileFree { void operator()(pcre2_compile_context* p) const { pcre2_compile_context_free(p); } };
struct MatchFree { void operator()(pcre2_match_context* p) const { pcre2_match_context_free(p); } };
struct DataFree { void operator()(pcre2_match_data* p) const { pcre2_match_data_free(p); } };
typedef std::unique_ptr<pcre2_code, CodeFree> Code;

// 매칭 예산. 콜아웃마다 1단위, 직전 콜아웃 위치에서 전진한 입력 바이트마다 1단위를 쓴다.
// 소유·탐욕 반복은 콜아웃 없이 입력을 삼키므로 전진량을 다음 콜아웃에서 과금한다.
// 뒤로 돌아간 뒤 같은 구간을 다시 읽어도 전진이므로 다시 과금되고(전방 탐색 반복 등),
// 시작 위치가 바뀌면 그 위치부터 센다. 시작 위치마다 끝까지 훑는 제곱 시간도 이렇게 걸린다.
struct WorkBudget
{
    uint64_t remaining;
    PCRE2_SIZE startMatch;
    PCRE2_SIZE lastPosition;
};

int CountWork(pcre2_callout_block* block, void* data)
{
    WorkBudget& budget = *static_cast<WorkBudget*>(data);
    if (block->start_match != budget.startMatch)
    {
        budget.startMatch = block->start_match;
        budget.lastPosition = block->start_match;
    }
    uint64_t cost = 1;
    if (block->current_position > budget.lastPosition)
        cost += block->current_position - budget.lastPosition;
    budget.lastPosition = block->current_position;
    if (budget.remaining < cost)
    {
        budget.remaining = 0;
        return PCRE2_ERROR_CALLOUT;
    }
    budget.remaining -= cost;
    return 0;
}

// PCRE2 한도 오류를 종류별로 알린다. (*LIMIT_DEPTH=...) 같은 패턴 내 지시로도 깊이 한도에 걸릴 수 있다.
const char* LimitMessage(int status)
{
    if (status == PCRE2_ERROR_CALLOUT) return "regular expression work limit exceeded";
    if (status == PCRE2_ERROR_MATCHLIMIT) return "regular expression backtracking limit exceeded";
    if (status == PCRE2_ERROR_DEPTHLIMIT) return "regular expression depth limit exceeded";
    if (status == PCRE2_ERROR_HEAPLIMIT) return "regular expression memory limit exceeded";
    return "regular expression matching failed";
}

void Append(std::string& output, const std::string& input, size_t offset, size_t size)
{
    if (size > TextLimit - output.size())
        throw std::runtime_error("regular expression output exceeds limit");
    output.append(input, offset, size);
}

void Replace(std::string& output, const std::string& format, const std::string& input,
    PCRE2_SIZE* offsets, size_t groups, uint64_t& remaining)
{
    for (size_t i = 0; i < format.size(); ++i)
    {
        if (remaining == 0) throw std::runtime_error("regular expression work limit exceeded");
        --remaining;
        if (format[i] != '$' || i + 1 == format.size()) { Append(output, format, i, 1); continue; }
        const char c = format[i + 1];
        if (c == '$') { Append(output, format, ++i, 1); continue; }
        if (c == '`') { Append(output, input, 0, offsets[0]); ++i; continue; }
        if (c == '\'') { Append(output, input, offsets[1], input.size() - offsets[1]); ++i; continue; }
        size_t group = 0;
        if (c != '&')
        {
            if (c < '0' || c > '9') { Append(output, format, i, 1); continue; }
            group = static_cast<size_t>(c - '0');
            if (i + 2 < format.size() && format[i + 2] >= '0' && format[i + 2] <= '9')
            {
                group = group * 10 + static_cast<size_t>(format[i + 2] - '0');
                ++i;
            }
        }
        ++i;
        if (group < groups && offsets[group * 2] != PCRE2_UNSET)
            Append(output, input, offsets[group * 2], offsets[group * 2 + 1] - offsets[group * 2]);
    }
}
}

struct RegexEngine::Impl {
    struct Entry {
        std::string pattern;
        bool ignoreCase;
        Code code;
        size_t bytes;
        Entry(const std::string& p, bool i, Code c, size_t b)
            : pattern(p), ignoreCase(i), code(std::move(c)), bytes(b) {}
    };
    std::list<Entry> cache;
    Code uncached;
    size_t bytes = 0;

    pcre2_code* Compile(const std::string& pattern, bool ignoreCase)
    {
        for (auto i = cache.begin(); i != cache.end(); ++i)
            if (i->ignoreCase == ignoreCase && i->pattern == pattern)
            {
                cache.splice(cache.begin(), cache, i);
                return cache.front().code.get();
            }
        std::unique_ptr<pcre2_compile_context, CompileFree> context(pcre2_compile_context_create(nullptr));
        if (!context) throw std::bad_alloc();
        pcre2_set_parens_nest_limit(context.get(), 128);
        pcre2_set_max_pattern_compiled_length(context.get(), 1024 * 1024);
        int error = 0;
        PCRE2_SIZE offset = 0;
        // (*COMMIT)·(*SKIP) 같은 제어 구문은 시작 최적화가 시도를 건너뛰면 의미가 달라진다.
        // 그런 패턴만 최적화를 끄고, 일반 패턴은 첫 문자·필수 문자·최소 길이 최적화를 그대로 쓴다.
        const bool controlVerbs = pattern.find("(*") != std::string::npos;
        const uint32_t options = PCRE2_ALT_BSUX | PCRE2_MATCH_UNSET_BACKREF | PCRE2_DOLLAR_ENDONLY |
            PCRE2_NEVER_UTF | PCRE2_NEVER_UCP | PCRE2_AUTO_CALLOUT |
            (controlVerbs ? PCRE2_NO_START_OPTIMIZE : 0) | (ignoreCase ? PCRE2_CASELESS : 0);
        Code code(pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(), options,
            &error, &offset, context.get()));
        if (!code)
        {
            PCRE2_UCHAR message[256] = {};
            pcre2_get_error_message(error, message, sizeof(message));
            throw std::runtime_error(std::string("invalid regular expression: ") +
                reinterpret_cast<const char*>(message));
        }
        PCRE2_SIZE compiledBytes = 0;
        pcre2_pattern_info(code.get(), PCRE2_INFO_SIZE, &compiledBytes);
        const size_t retained = compiledBytes + pattern.size();
        if (retained > CacheLimit) { uncached = std::move(code); return uncached.get(); }
        while (!cache.empty() && (cache.size() >= 16 || bytes + retained > CacheLimit))
        {
            bytes -= cache.back().bytes;
            cache.pop_back();
        }
        cache.emplace_front(pattern, ignoreCase, std::move(code), retained);
        bytes += retained;
        return cache.front().code.get();
    }
};

RegexEngine::RegexEngine() {}
RegexEngine::~RegexEngine() {}

RegexResult RegexEngine::Run(const std::string& subject, const std::string& pattern,
    const std::string& replacement, int operation, bool ignoreCase)
{
    if (subject.size() > TextLimit || replacement.size() > TextLimit)
        throw std::runtime_error("regular expression input exceeds limit");
    if (!_impl) _impl.reset(new Impl());
    pcre2_code* code = _impl->Compile(pattern, ignoreCase);
    std::unique_ptr<pcre2_match_context, MatchFree> context(pcre2_match_context_create(nullptr));
    std::unique_ptr<pcre2_match_data, DataFree> data(pcre2_match_data_create_from_pattern(code, nullptr));
    if (!context || !data) throw std::bad_alloc();
    // 콜아웃은 예산 계산에만 쓴다. 시작 최적화 여부는 Compile 에서 패턴별로 정한다.
    WorkBudget budget = { WorkLimit + WorkPerByte * static_cast<uint64_t>(subject.size()), static_cast<PCRE2_SIZE>(-1), 0 };
    pcre2_set_callout(context.get(), CountWork, &budget);
    pcre2_set_match_limit(context.get(), WorkLimit);
    // 매칭 프레임은 힙에 쌓이므로 깊이는 힙 한도가 묶는다. 고정 깊이 한도는 반복 그룹의
    // 반복 횟수까지 제한해 평범한 패턴을 깨뜨리므로 두지 않는다.
    pcre2_set_heap_limit(context.get(), 8192);
    RegexResult result;
    size_t search = 0, copied = 0;
    bool retryNonempty = false;
    for (;;)
    {
        if (budget.remaining == 0) throw std::runtime_error("regular expression work limit exceeded");
        --budget.remaining;
        uint32_t options = operation == 0 ? PCRE2_ANCHORED | PCRE2_ENDANCHORED : 0;
        if (retryNonempty) options |= PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
        const int status = pcre2_match(code, reinterpret_cast<PCRE2_SPTR>(subject.data()),
            subject.size(), search, options, data.get(), context.get());
        if (status == PCRE2_ERROR_NOMATCH)
        {
            if (retryNonempty && search < subject.size()) { ++search; retryNonempty = false; continue; }
            break;
        }
        if (status < 0) throw std::runtime_error(LimitMessage(status));
        result.matched = true;
        PCRE2_SIZE* offsets = pcre2_get_ovector_pointer(data.get());
        const size_t groups = pcre2_get_ovector_count(data.get());
        if (operation == 0) return result;
        if (operation == 1)
        {
            size_t captured = 0;
            for (size_t i = 0; i < groups; ++i)
            {
                const size_t length = offsets[i * 2] == PCRE2_UNSET ? 0 : offsets[i * 2 + 1] - offsets[i * 2];
                if (length > TextLimit - captured) throw std::runtime_error("regular expression capture output exceeds limit");
                captured += length;
                result.captures.push_back(length ? subject.substr(offsets[i * 2], length) : std::string());
            }
            return result;
        }
        Append(result.text, subject, copied, offsets[0] - copied);
        Replace(result.text, replacement, subject, offsets, groups, budget.remaining);
        copied = offsets[1];
        search = copied;
        retryNonempty = offsets[0] == offsets[1];
        if (search == subject.size()) break;
    }
    if (operation == 2) Append(result.text, subject, copied, subject.size() - copied);
    return result;
}
}
