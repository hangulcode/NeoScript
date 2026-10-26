#include "NeoScript.h"
#include "NeoRegex.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
using namespace NeoScript;

static int checks = 0, failures = 0;
static void Check(bool ok, const char* name)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", name); }
}

static void Script(IRuntime* runtime, const std::string& source, bool expected = true, int timeout = -1, const char* expectedError = nullptr)
{
    for (bool debug : {false, true})
    {
        CompileDesc desc;
        desc.source = source;
        desc.sourceName = "http_regex.ns";
        desc.includeDebugInfo = debug;
        auto program = runtime->Compile(desc);
        Check(bool(program.program), "compile");
        if (!program.program) { std::printf("%s\n", program.error.message.c_str()); continue; }
        auto instance = runtime->CreateInstance(program.program);
        Check(runtime->RunGlobalInit(instance) == RunStatus::Completed, "global init");
        {
            auto call = runtime->Call(instance, "Test");
            call.timeout(timeout);
            const auto status = call.invoke();
            Check(expected ? status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool()
                           : status != RunStatus::Completed, source.c_str());
            if (expectedError && call.error().message.find(expectedError) == std::string::npos)
            {
                Check(false, "specific runtime error");
                std::printf("expected %s; got %s\n", expectedError, call.error().message.c_str());
            }
            else if (expectedError) Check(true, "specific runtime error");
            if (expected && status != RunStatus::Completed) std::printf("%s\n", call.error().message.c_str());
        }
        runtime->DestroyInstance(instance);
        runtime->DestroyProgram(program.program);
    }
}

struct MockTransport : IHttpTransport
{
    std::thread::id owner = std::this_thread::get_id();
    std::atomic<int> requests{0};
    HttpResponse Send(const HttpRequest& request) override
    {
        ++requests;
        if (request.url == "https://throw") throw std::runtime_error("mock failure");
        HttpResponse result;
        if (request.url == "https://unlimited") std::this_thread::sleep_for(std::chrono::milliseconds(20));
        result.success = std::this_thread::get_id() != owner &&
            request.timeoutMs == (request.url == "https://unlimited" ? -1 : 1000);
        if (request.method == "POST")
            result.success = result.success && request.body == "payload" && request.headers.size() == 1
                && request.headers[0].first == "X-Test" && request.headers[0].second == "value";
        result.body = request.method;
        return result;
    }
};

struct CancellableTransport : IHttpTransport
{
    std::atomic<int> entered{0}, cancelled{0}, expired{0};
    std::atomic<bool> unlimited{true};
    HttpResponse Send(const HttpRequest& request) override
    {
        if (request.timeoutMs != -1) unlimited = false;
        ++entered;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!request.IsCancelled() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (request.IsCancelled()) ++cancelled;
        else ++expired;
        HttpResponse result; result.error = "cancelled"; return result;
    }
};

static void Cancellation()
{
    auto transport = std::make_shared<CancellableTransport>();
    RuntimeDesc desc; desc.httpTransport = transport;
    IRuntime* runtimes[2] = {CreateRuntime(desc), CreateRuntime(desc)};
    for (auto runtime : runtimes)
    {
        CompileDesc compile;
        compile.source = "import system; fun Done(var ok,var text){} "
            "var a=system.aysnc_create(); a.get(-1,\"https://unused\",Done); "
            "var b=system.aysnc_create(); b.get(-1,\"https://queued\",Done);";
        auto program = runtime->Compile(compile);
        Check(bool(program.program), "cancellation compile");
        if (program.program)
        {
            auto instance = runtime->CreateInstance(program.program);
            Check(runtime->RunGlobalInit(instance) == RunStatus::Completed, "launch without waiting");
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (transport->entered < 2 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Check(transport->entered == 2, "both runtimes started a transfer");
    DestroyRuntime(runtimes[0]);
    Check(transport->cancelled == 1 && transport->expired == 0, "shutdown cancels only its own transfer");
    DestroyRuntime(runtimes[1]);
    Check(transport->cancelled == 2 && transport->expired == 0, "both shutdowns cancel promptly");
    Check(transport->entered == 2, "queued requests discarded during shutdown");
    Check(transport->unlimited, "unlimited timeout delivered unchanged");
}

static void PatternLimits()
{
    const char* error = "regular expression pattern exceeds the host byte limit";
    auto runtime = CreateRuntime();
    // 문자 클래스 하나로 컴파일 비용과 무관하게 기본 상한의 포함 경계를 검사한다.
    const std::string pattern = "[" + std::string(16382, 'a') + "]";
    Script(runtime, "export fun Test(){return \"a\".regex_match(\"" + pattern + "\");}");
    for (const char* method : {"regex_match", "regex_search", "regex_replace"})
    {
        std::string source = "export fun Test(){var p=\"" + pattern + "\"; p=p..\"a\"; return \"a\"." + method + "(p";
        if (std::string(method) == "regex_replace") source += ",\"b\"";
        Script(runtime, source + ");}", false, -1, error);
    }
    DestroyRuntime(runtime);
    RuntimeDesc desc; desc.maxRegexPatternBytes = 5;
    runtime = CreateRuntime(desc);
    Script(runtime, u8R"ns(export fun Test(){return "가".regex_match("(가)");})ns");
    Script(runtime, u8R"ns(export fun Test(){return "가".regex_match("(가a)");})ns", false, -1, error);
    DestroyRuntime(runtime);
    desc.maxRegexPatternBytes = 16385;
    runtime = CreateRuntime(desc);
    Script(runtime, "export fun Test(){return \"a\".regex_match(\"[" + std::string(16383, 'a') + "]\");}");
    DestroyRuntime(runtime);
    desc.maxRegexPatternBytes = 0;
    runtime = CreateRuntime(desc);
    Script(runtime, R"ns(export fun Test(){return "".regex_match("");})ns");
    Script(runtime, R"ns(export fun Test(){return "a".regex_match("a");})ns", false, -1, error);
    DestroyRuntime(runtime);
}

static void RegexBounds()
{
    RegexEngine engine;
    const auto limited = [&engine](const std::string& text, const std::string& pattern, const std::string& replacement, int operation)
    {
        bool failed = false;
        try { engine.Run(text, pattern, replacement, operation, false); }
        catch (const std::runtime_error&) { failed = true; }
        Check(failed, "regex resource bound");
    };
    limited(std::string(8 * 1024 * 1024 + 1, 'a'), "a", "", 1);
    limited("a", std::string(129, '(') + "a" + std::string(129, ')'), "", 0);
    // 치환 문자열은 예산(1,000,000 + 입력 바이트당 32)으로 묶이며 입력이 1바이트라 2백만 자는 초과한다.
    limited("a", "a", std::string(2000001, 'x'), 2);
    const std::string binary("a\0b", 3);
    Check(engine.Run(binary, "b", "c", 2, false).text == std::string("a\0c", 3), "regex embedded NUL output");
    Check(engine.Run("abc", "b", "$$:$&:$`:$'", 2, false).text == "a$:b:a:cc", "replacement special tokens");
    Check(engine.Run("ab", "(a)(b)", "$2$1:$12:$01", 2, false).text == "ba::a", "replacement capture numbers");
    Check(engine.Run("a\nb", "a.*b", "", 0, false).matched == false, "dot excludes newline");
    Check(engine.Run("a\n", "a$", "", 1, false).matched == false, "dollar matches absolute end");
    Check(engine.Run("abc", "a(?=b)", "", 1, false).captures[0] == "a", "positive lookahead");
    Check(engine.Run("ac", "a(?!b)", "", 1, false).captures[0] == "a", "negative lookahead");
}

int main()
{
    RegexBounds();
    Cancellation();
    PatternLimits();
    auto runtime = CreateRuntime();
    for (const char* expression : {
        R"ns("\uD55C\uAE00".regex_match("\uD55C\uAE00"))ns",
        R"ns("a12".regex_search("\\d+")[0]=="12")ns",
        R"ns("abc123".regex_match("[a-z]+[0-9]+"))ns",
        R"ns(!"xabc".regex_match("abc"))ns",
        R"ns("ABC".regex_match("abc",true))ns",
        R"ns(!"ABC".regex_match("abc"))ns",
        R"ns("ab12cd".regex_search("([a-z]+)([0-9]+)")[2]=="12")ns",
        R"ns("no match".regex_search("[0-9]+") == null)ns",
        R"ns("b".regex_search("(a)?b")[1]=="")ns",
        R"ns("".regex_match(""))ns",
        R"ns("ab".regex_replace("", "-")=="-a-b-")ns",
        R"ns("a1 b2".regex_replace("([a-z])([0-9])", "$2$1")=="1a 2b")ns",
        R"ns("Aa".regex_replace("a", "b",true)=="bb")ns",
        R"ns("abc".regex_replace("z", "x")=="abc")ns"
    }) Script(runtime, std::string("export fun Test(){return ") + expression + ";}");
    // 플랫폼별 재귀 스택 대신 제한된 PCRE2 인터프리터를 사용하는지 검사한다.
    // 필수 문자 'c' 가 없으면 시작 최적화가 시도 자체를 건너뛰어 즉시 불일치로 끝난다.
    Script(runtime, R"ns(export fun Test(){var s="";for(var i in 0,20000)s=s.."a"; return s.regex_search("(a|b)*c")==null;})ns");
    // 시작 위치마다 끝까지 훑는 제곱 시간은 전진 바이트 과금으로 예산에 걸린다.
    Script(runtime, R"ns(export fun Test(){var s="";for(var i in 0,20000)s=s.."a"; s=s.."cy"; return s.regex_search("[ab]*c(?=x)")==null;})ns", false, -1, "limit");
    // 반복 그룹은 고정 깊이 한도가 아니라 힙 한도에만 묶인다.
    Script(runtime, R"ns(export fun Test(){var s="";for(var i in 0,2000)s=s.."ab"; return s.regex_match("(ab)*") && s.regex_match("(?:ab)*") && s.regex_search("(ab)+$")!=null;})ns");
    Script(runtime, R"ns(export fun Test(){var s="";for(var i in 0,2000)s=s.."a,"; return s.regex_match("([^,]*,)+");})ns");
    // 제어 구문은 시작 최적화 없이 매칭해 첫 위치의 (*COMMIT) 이 이후 시도를 막는 의미를 지킨다.
    Script(runtime, R"ns(export fun Test(){return "DEFABC".regex_search("(*COMMIT)ABC")==null && "DEFABC".regex_replace("(*COMMIT)ABC","X")=="DEFABC" && "ABC".regex_search("(*COMMIT)ABC")!=null;})ns");
    // 같은 시작 위치에서 입력을 다시 읽는 전방 탐색 반복도 전진량으로 과금된다. 10회는 예산 안, 100회는 초과.
    Script(runtime, R"ns(export fun Test(){var s="a";for(var i in 0,15)s=s..s; return s.regex_match("^(?:(?=a*)){10}a*$");})ns");
    Script(runtime, R"ns(export fun Test(){var s="a";for(var i in 0,15)s=s..s; return s.regex_match("^(?:(?=a*)){100}a*$");})ns", false, -1, "work limit");
    // 실패하는 긴 역참조 비교는 위치를 전진시키지 않으므로 캡처 길이를 미리 과금해 예산에 걸리게 한다.
    // 수량자로 반복한 단언은 PCRE2 가 한 번으로 접으므로 단언을 직접 나열해 비교를 실제로 반복시킨다.
    Script(runtime, R"ns(export fun Test(){var s="a";for(var i in 0,16)s=s..s; s=s.sub(0,s.len()-1).."b"; var p="^(a{32768})";for(var i in 0,1000)p=p.."(?!\1)"; return s.regex_match(p..".*$");})ns", false, -1, "work limit");
    Script(runtime, R"ns(export fun Test(){var t="";for(var i in 0,2000)t=t.."word word, "; return t.regex_search("(\w+)\s+\1")!=null && t.regex_replace("(\w+)\s+\1","$1").len()<t.len();})ns");
    // 패턴 안의 깊이 지시는 깊이 한도 메시지로 알린다.
    Script(runtime, R"ns(export fun Test(){return "ab".regex_match("(*LIMIT_DEPTH=1)(ab)*");})ns", false, -1, "depth limit");
    // 입력 길이에 비례하는 예산이라 큰 입력의 선형 검색·치환은 통과한다.
    Script(runtime, R"ns(export fun Test(){var s="a";for(var i in 0,20)s=s..s; return s.regex_search("c")==null && s.regex_replace("a","b").len()==s.len();})ns");
    // 필수 문자 'b' 가 있어야 시작 최적화를 지나 지수 백트래킹에 들어가고, 거기서 한도에 걸린다.
    Script(runtime, R"ns(export fun Test(){return "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa!b".regex_match("(a*)*b");})ns", false, -1, "limit");
    Script(runtime, R"ns(export fun Test(){return !"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa!".regex_match("(a*)*b");})ns");
    Script(runtime, R"ns(export fun Test(){return "aaa".regex_replace("a*","-")=="-";})ns");
    Script(runtime, R"ns(export fun Test(){return "ab".regex_match("a|ab") && "aa".regex_match("(a)\\1");})ns");
    Script(runtime, R"ns(export fun Test(){for(var i in 0,40){var p="a"..i; if(!p.regex_match(p))return false;} return "A".regex_match("a",true) && !"A".regex_match("a");})ns");
    Script(runtime, R"ns(export fun Test(){var s="abc123"; s=s.regex_replace("[0-9]+","x"); return s=="abcx";})ns");
    Script(runtime, R"ns(export fun Test(){return "a".regex_match("[");})ns", false);
    Script(runtime, R"ns(export fun Test(){return "a".regex_match(1);})ns", false);
    Script(runtime, R"ns(export fun Test(){return "a".regex_match("a",1);})ns", false);
    Script(runtime, R"ns(export fun Test(){return "a".regex_match("a");})ns", false, 1000);
    Script(runtime, R"ns(import system; var ok=false; fun Done(var success,var text){ok=!success && text=="HTTP transport is not configured";}
        export fun Test(){var a=system.aysnc_create(); a.get(1000,"https://unused",Done); return a.wait() && ok;})ns");
    DestroyRuntime(runtime);

    auto transport = std::make_shared<MockTransport>();
    RuntimeDesc desc;
    desc.httpTransport = transport;
    runtime = CreateRuntime(desc);
    desc.httpTransport.reset();
    Script(runtime, R"ns(import system; var done=0; fun Done(var success,var text){if(success && text=="GET") done++;}
        export fun Test(){var i=0; while(i<200){var a=system.aysnc_create(); a.get(1000,"https://test",Done); if(!a.wait()) return false; i++;} return done==200;})ns");
    Script(runtime, R"ns(import system; var ok=false; fun Done(var success,var text){ok=success && text=="POST";}
        export fun Test(){var a=system.aysnc_create(); a.add_header("X-Test","value"); a.post(1000,"https://test","payload",Done); return a.wait() && ok;})ns");
    Script(runtime, R"ns(import system; var ok=false; fun Done(var success,var text){ok=!success && text=="mock failure";}
        export fun Test(){var a=system.aysnc_create(); a.get(1000,"https://throw",Done); return a.wait() && ok;})ns");
    Script(runtime, R"ns(import system; var ok=false; fun Done(var success,var text){ok=success;}
        export fun Test(){var a=system.aysnc_create(); a.get(-1,"https://unlimited",Done); return a.wait() && ok;})ns");
    Check(transport->requests == 406, "transport request count");
    std::weak_ptr<IHttpTransport> weak = transport;
    transport.reset();
    Check(!weak.expired(), "runtime owns transport");
    DestroyRuntime(runtime);
    Check(weak.expired(), "runtime releases transport");
    std::printf("http/regex: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
