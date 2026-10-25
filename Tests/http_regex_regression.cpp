#include "NeoScript.h"
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
            if (expectedError) Check(call.error().message.find(expectedError) != std::string::npos, "specific runtime error");
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
    Script(runtime, "export fun Test(){return \"a\".regexMatch(\"" + pattern + "\");}");
    for (const char* method : {"regexMatch", "regexSearch", "regexReplace"})
    {
        std::string source = "export fun Test(){var p=\"" + pattern + "\"; p=p..\"a\"; return \"a\"." + method + "(p";
        if (std::string(method) == "regexReplace") source += ",\"b\"";
        Script(runtime, source + ");}", false, -1, error);
    }
    DestroyRuntime(runtime);
    RuntimeDesc desc; desc.maxRegexPatternBytes = 5;
    runtime = CreateRuntime(desc);
    Script(runtime, u8R"ns(export fun Test(){return "가".regexMatch("(가)");})ns");
    Script(runtime, u8R"ns(export fun Test(){return "가".regexMatch("(가a)");})ns", false, -1, error);
    DestroyRuntime(runtime);
    desc.maxRegexPatternBytes = 16385;
    runtime = CreateRuntime(desc);
    Script(runtime, "export fun Test(){return \"a\".regexMatch(\"[" + std::string(16383, 'a') + "]\");}");
    DestroyRuntime(runtime);
    desc.maxRegexPatternBytes = 0;
    runtime = CreateRuntime(desc);
    Script(runtime, R"ns(export fun Test(){return "".regexMatch("");})ns");
    Script(runtime, R"ns(export fun Test(){return "a".regexMatch("a");})ns", false, -1, error);
    DestroyRuntime(runtime);
}

int main()
{
    Cancellation();
    PatternLimits();
    auto runtime = CreateRuntime();
    for (const char* expression : {
        R"ns("\uD55C\uAE00".regexMatch("\uD55C\uAE00"))ns",
        R"ns("a12".regexSearch("\\d+")[0]=="12")ns",
        R"ns("abc123".regexMatch("[a-z]+[0-9]+"))ns",
        R"ns(!"xabc".regexMatch("abc"))ns",
        R"ns("ABC".regexMatch("abc",true))ns",
        R"ns(!"ABC".regexMatch("abc"))ns",
        R"ns("ab12cd".regexSearch("([a-z]+)([0-9]+)")[2]=="12")ns",
        R"ns("no match".regexSearch("[0-9]+") == null)ns",
        R"ns("b".regexSearch("(a)?b")[1]=="")ns",
        R"ns("".regexMatch(""))ns",
        R"ns("ab".regexReplace("", "-")=="-a-b-")ns",
        R"ns("a1 b2".regexReplace("([a-z])([0-9])", "$2$1")=="1a 2b")ns",
        R"ns("Aa".regexReplace("a", "b",true)=="bb")ns",
        R"ns("abc".regexReplace("z", "x")=="abc")ns"
    }) Script(runtime, std::string("export fun Test(){return ") + expression + ";}");
    Script(runtime, R"ns(export fun Test(){var s="abc123"; s=s.regexReplace("[0-9]+","x"); return s=="abcx";})ns");
    Script(runtime, R"ns(export fun Test(){return "a".regexMatch("[");})ns", false);
    Script(runtime, R"ns(export fun Test(){return "a".regexMatch(1);})ns", false);
    Script(runtime, R"ns(export fun Test(){return "a".regexMatch("a",1);})ns", false);
    Script(runtime, R"ns(export fun Test(){return "a".regexMatch("a");})ns", false, 1000);
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
