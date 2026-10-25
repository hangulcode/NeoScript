#include "NeoScript.h"
#include <atomic>
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

static void Script(IRuntime* runtime, const std::string& source, bool expected = true, int timeout = -1)
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
        result.success = std::this_thread::get_id() != owner && request.timeoutMs == 1000;
        if (request.method == "POST")
            result.success = result.success && request.body == "payload" && request.headers.size() == 1
                && request.headers[0].first == "X-Test" && request.headers[0].second == "value";
        result.body = request.method;
        return result;
    }
};

int main()
{
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
    Check(transport->requests == 404, "transport request count");
    std::weak_ptr<IHttpTransport> weak = transport;
    transport.reset();
    Check(!weak.expired(), "runtime owns transport");
    DestroyRuntime(runtime);
    Check(weak.expired(), "runtime releases transport");
    std::printf("http/regex: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
