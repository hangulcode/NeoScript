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
const uint32_t WorkLimit = 1000000;
struct CodeFree { void operator()(pcre2_code* p) const { pcre2_code_free(p); } };
struct CompileFree { void operator()(pcre2_compile_context* p) const { pcre2_compile_context_free(p); } };
struct MatchFree { void operator()(pcre2_match_context* p) const { pcre2_match_context_free(p); } };
struct DataFree { void operator()(pcre2_match_data* p) const { pcre2_match_data_free(p); } };
typedef std::unique_ptr<pcre2_code, CodeFree> Code;

int CountWork(pcre2_callout_block*, void* data)
{
    auto& remaining = *static_cast<uint32_t*>(data);
    if (remaining == 0) return PCRE2_ERROR_CALLOUT;
    --remaining;
    return 0;
}

void Append(std::string& output, const std::string& input, size_t offset, size_t size)
{
    if (size > TextLimit - output.size())
        throw std::runtime_error("regular expression output exceeds limit");
    output.append(input, offset, size);
}

void Replace(std::string& output, const std::string& format, const std::string& input,
    PCRE2_SIZE* offsets, size_t groups, uint32_t& remaining)
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
        const uint32_t options = PCRE2_ALT_BSUX | PCRE2_MATCH_UNSET_BACKREF | PCRE2_DOLLAR_ENDONLY |
            PCRE2_NEVER_UTF | PCRE2_NEVER_UCP | PCRE2_AUTO_CALLOUT | PCRE2_NO_START_OPTIMIZE |
            (ignoreCase ? PCRE2_CASELESS : 0);
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
    uint32_t remaining = WorkLimit;
    pcre2_set_callout(context.get(), CountWork, &remaining);
    pcre2_set_match_limit(context.get(), WorkLimit);
    pcre2_set_depth_limit(context.get(), 1000);
    pcre2_set_heap_limit(context.get(), 8192);
    RegexResult result;
    size_t search = 0, copied = 0;
    bool retryNonempty = false;
    for (;;)
    {
        if (remaining == 0) throw std::runtime_error("regular expression work limit exceeded");
        --remaining;
        uint32_t options = operation == 0 ? PCRE2_ANCHORED | PCRE2_ENDANCHORED : 0;
        if (retryNonempty) options |= PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
        const int status = pcre2_match(code, reinterpret_cast<PCRE2_SPTR>(subject.data()),
            subject.size(), search, options, data.get(), context.get());
        if (status == PCRE2_ERROR_NOMATCH)
        {
            if (retryNonempty && search < subject.size()) { ++search; retryNonempty = false; continue; }
            break;
        }
        if (status < 0) throw std::runtime_error("regular expression work, depth or memory limit exceeded");
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
        Replace(result.text, replacement, subject, offsets, groups, remaining);
        copied = offsets[1];
        search = copied;
        retryNonempty = offsets[0] == offsets[1];
        if (search == subject.size()) break;
    }
    if (operation == 2) Append(result.text, subject, copied, subject.size() - copied);
    return result;
}
}
