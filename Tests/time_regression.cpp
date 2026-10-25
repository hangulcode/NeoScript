#include "NeoScript.h"
#include "NeoTime.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>

using namespace NeoScript;
static int checks = 0, failures = 0;

static void Check(bool valid, const char* label)
{
    ++checks;
    if (!valid) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}

static ProgramHandle Compile(IRuntime* runtime, const std::string& source, bool debug, bool image)
{
    CompileDesc desc;
    desc.source = source; desc.sourceName = "time.ns"; desc.includeDebugInfo = debug;
    ProgramHandle program;
    Error error;
    if (image)
    {
        std::vector<uint8_t> bytes;
        error = runtime->CompileToBytecode(desc, bytes);
        if (error.ok()) program = runtime->LoadProgram(bytes, &error);
    }
    else
    {
        auto result = runtime->Compile(desc);
        program = result.program; error = result.error;
    }
    Check(bool(program), error.message.c_str());
    return program;
}

static void Script(IRuntime* runtime, const std::string& body, bool debug, bool image, const char* error = nullptr)
{
    auto program = Compile(runtime, "import system; export fun Test(){" + body + "}", debug, image);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(bool(instance), "create instance");
    if (instance)
    {
        {
            auto call = runtime->Call(instance, "Test");
            const RunStatus status = call.invoke();
            if (error) Check(status == RunStatus::Failed && call.error().message.find(error) != std::string::npos, body.c_str());
            else Check(status == RunStatus::Completed && call.retType() == ValueType::Bool && call.retBool(), body.c_str());
            if (!error && status != RunStatus::Completed) std::fprintf(stderr, "%s\n", call.error().message.c_str());
        }
        runtime->DestroyInstance(instance);
    }
    runtime->DestroyProgram(program);
}

static void Semantics(IRuntime* runtime, bool debug, bool image)
{
    Script(runtime, R"ns(
        var s=system.time_span(1,2,3,4,5); var alias=s;
        if(s.total_milliseconds()!=93784005 || s.format()!="1.02:03:04.005")return false;
        var p=s.parts(); if(p.sign!=1 || p.days!=1 || p.hours!=2 || p.minutes!=3 || p.seconds!=4 || p.milliseconds!=5)return false;
        p.days=99; if(s.parts().days!=1)return false;
        var t=s.add(system.time_span_ms(995));
        if(t.total_milliseconds()!=93785000 || alias.total_milliseconds()!=93784005)return false;
        if(t.compare(s)!=1 || s.compare(t)!=-1 || t.subtract(s).compare(system.time_span_ms(995))!=0)return false;
        if(system.time_span(0,25,0,0,0).format()!="1.01:00:00.000")return false;
        if(system.time_span(1,-24,0,0,-1).format()!="-0.00:00:00.001")return false;
        var negative=system.time_span_ms(-93784005); var n=negative.parts();
        if(negative.format()!="-1.02:03:04.005" || n.sign!=-1 || n.days!=1 || n.hours!=2 || n.minutes!=3 || n.seconds!=4 || n.milliseconds!=5)return false;
        if(system.time_span_ms(1500).total_seconds()!=1.5 || system.time_span_ms(-1500).total_seconds()!=-1.5)return false;
        if(system.time_span_ms(0).format()!="0.00:00:00.000" || system.time_span_ms(0).parts().sign!=0)return false;
        var storage=[s,{"span":negative}]; s=null; negative=null;
        if(storage[0].format()!="1.02:03:04.005" || storage[1].span.parts().sign!=-1)return false;
        var captured=fun(){return alias.total_milliseconds();}; alias=null;
        return captured()==93784005;
    )ns", debug, image);
    Script(runtime, R"ns(
        var min=system.time_span_ms(-9223372036854775808);var max=system.time_span_ms(9223372036854775807);
        if(min.format()!="-106751991167.07:12:55.808" || max.format()!="106751991167.07:12:55.807")return false;
        if(min.parts().milliseconds!=808 || min.parts().days!=106751991167)return false;
        if(min.add(max).total_milliseconds()!=-1 || min.subtract(min).total_milliseconds()!=0)return false;
        if(max.subtract(max).total_milliseconds()!=0 || max.compare(min)!=1)return false;
        if(system.time_span(-106751991167,-7,-12,-55,-808).total_milliseconds()!=-9223372036854775808)return false;
        return system.time_span(106751991167,7,12,55,807).total_milliseconds()==9223372036854775807;
    )ns", debug, image);
    Script(runtime, R"ns(
        if(system.utc_date("%Y-%m-%d %H:%M:%S %z %Z",0)!="1970-01-01 00:00:00 +0000 UTC")return false;
        if(system.utc_date("%Y-%m-%d %H:%M:%S",-1)!="1969-12-31 23:59:59")return false;
        if(system.utc_date("%Y-%m-%d %w %j",-2208988800)!="1900-01-01 1 001")return false;
        if(system.utc_date("%Y-%m-%d",951782400)!="2000-02-29")return false;
        if(system.utc_date("%Y-%m-%d",4107542400)!="2100-03-01")return false;
        if(system.utc_date("%Y-%m-%d %H:%M:%S",2147483648)!="2038-01-19 03:14:08")return false;
        if(system.utc_date("%Y-%m-%d %H:%M:%S",253402300799)!="9999-12-31 23:59:59")return false;
        if(system.utc_date("",0)!="" || system.utc_date("%%",0)!="%")return false;
        return system.utc_date("%z/%Z/%z/%Z",-1)=="+0000/UTC/+0000/UTC";
    )ns", debug, image);
    Script(runtime, u8R"ns(return system.utc_date("%Y년 %m월 %d일",0)=="1970년 01월 01일";)ns", debug, image);
    Script(runtime, "return system.utc_date(\"" + std::string(79,'a') + "\",0).len()==79;", debug, image);
    Script(runtime, "return system.utc_date(\"" + std::string(74,'a') + "%z\",0).len()==79;", debug, image);
    Script(runtime, "return system.utc_date(\"" + std::string(80,'a') + "\",0);", debug, image, "79 bytes");
    Script(runtime, "return system.utc_date(\"" + std::string(75,'a') + "%z\",0);", debug, image, "79 bytes");
    for (const char* format : {"%", "%Q", "%E", "%E?"})
        Script(runtime, std::string("return system.utc_date(\"") + format + "\",0);", debug, image, "invalid system.utc_date");
    for (const char* value : {"-2208988801", "253402300800", "9223372036854775807", "-9223372036854775808"})
        Script(runtime, std::string("return system.utc_date(\"%Y\",") + value + ");", debug, image, "timestamp");
    for (const char* expression : {
        "system.time_span_ms(9223372036854775807).add(system.time_span_ms(1))",
        "system.time_span_ms(-9223372036854775808).add(system.time_span_ms(-1))",
        "system.time_span_ms(9223372036854775807).subtract(system.time_span_ms(-1))",
        "system.time_span_ms(-9223372036854775808).subtract(system.time_span_ms(1))",
        "system.time_span(106751991168,0,0,0,0)", "system.time_span(0,0,0,9223372036854775807,0)",
        "system.time_span(106751991167,7,12,55,808)", "system.time_span(-106751991167,-7,-12,-55,-809)"})
        Script(runtime, std::string("return ") + expression + ";", debug, image, "overflow int64");
    for (const char* expression : {"system.time_span_ms(0).add(1)", "system.time_span_ms(0).compare(system.stopwatch())"})
        Script(runtime, std::string("return ") + expression + ";", debug, image, "expects a time_span");
    for (const char* expression : {"system.time_span_ms(1.0)", "system.time_span_ms(true)", "system.time_span(0,0,0,1.0,0)",
        "system.utc_date(1,0)", "system.utc_date(\"%Y\",0.0)", "system.stopwatch().elapsed_ms(1)", "system.time_span_ms(1).format(1)"})
        Script(runtime, std::string("return ") + expression + ";", debug, image, "");
    Script(runtime, "var s=system.time_span_ms(1);s.milliseconds=2;return true;", debug, image, "");
    Script(runtime, R"ns(
        for(var i in 0,2000){var a=system.time_span_ms(i);var w=system.stopwatch();a=a.add(a);w=w.elapsed();
            if(a.total_milliseconds()!=i*2 || w.total_milliseconds()<0)return false;}
        return true;
    )ns", debug, image);
}

static int64_t ReadInt(IRuntime* runtime, InstanceHandle instance, const char* name)
{
    auto call = runtime->Call(instance, name);
    Check(call.invoke()==RunStatus::Completed && call.retType()==ValueType::Int, name);
    return call.retInt();
}

static void Clocks(IRuntime* runtime, bool debug, bool image)
{
    auto program = Compile(runtime, R"ns(
        import system; var watch=system.stopwatch();var alias=watch;
        export fun Milliseconds(){return system.time_ms();}
        export fun Seconds(){return system.time();}
        export fun Elapsed(){return watch.elapsed_ms();}
        export fun Snapshot(){return watch.elapsed().total_milliseconds();}
        export fun Restart(){alias.restart();return watch.elapsed_ms();}
    )ns", debug, image);
    if (!program) return;
    auto instance = runtime->CreateInstance(program);
    Check(runtime->RunGlobalInit(instance)==RunStatus::Completed,"clock globals");
    typedef std::chrono::system_clock WallClock;
    const auto before = WallClock::now();
    const int64_t ms = ReadInt(runtime,instance,"Milliseconds");
    const int64_t seconds = ReadInt(runtime,instance,"Seconds");
    const auto after = WallClock::now();
    Check(ms >= std::chrono::duration_cast<std::chrono::milliseconds>(before.time_since_epoch()).count() &&
          ms <= std::chrono::duration_cast<std::chrono::milliseconds>(after.time_since_epoch()).count(),"Unix milliseconds match host clock");
    Check(seconds >= static_cast<int64_t>(WallClock::to_time_t(before)) &&
          seconds <= static_cast<int64_t>(WallClock::to_time_t(after)),"Unix seconds match host clock");
    const int64_t initial = ReadInt(runtime,instance,"Elapsed");
    // 성능을 측정하지 않고 호스트가 스크립트를 실행하지 않는 동안의 경과를 검사한다.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const int64_t later = ReadInt(runtime,instance,"Snapshot");
    Check(initial >= 0 && later >= initial + 15,"stopwatch includes host idle time");
    const auto restartBefore = std::chrono::steady_clock::now();
    const int64_t restarted = ReadInt(runtime,instance,"Restart");
    const auto restartAfter = std::chrono::steady_clock::now();
    Check(restarted >= 0 && restarted <= std::chrono::duration_cast<std::chrono::milliseconds>(restartAfter-restartBefore).count(),
        "restart through alias resets stopwatch");
    Check(ReadInt(runtime,instance,"Elapsed") >= restarted,"elapsed monotonic after restart");
    runtime->DestroyInstance(instance); runtime->DestroyProgram(program);
}

static void CalendarReference()
{
    // 1970~2999를 C 라이브러리와 대조한다. Windows CRT의 범위 밖은 스크립트 경계 시험이 담당한다.
    for (long long seconds=0; seconds<32503680000LL; seconds+=123456789)
    {
        const std::time_t stamp=static_cast<std::time_t>(seconds);
        if (static_cast<long long>(stamp)!=seconds) break;
        std::tm reference;
#ifdef _WIN32
        if (gmtime_s(&reference,&stamp)!=0) break;
#else
        if (!gmtime_r(&stamp,&reference)) break;
#endif
        std::tm actual;
        Check(TimeLibrary::UtcCalendar(seconds,actual) && actual.tm_year==reference.tm_year &&
            actual.tm_mon==reference.tm_mon && actual.tm_mday==reference.tm_mday &&
            actual.tm_hour==reference.tm_hour && actual.tm_min==reference.tm_min && actual.tm_sec==reference.tm_sec &&
            actual.tm_wday==reference.tm_wday && actual.tm_yday==reference.tm_yday,"UTC calendar agrees with CRT");
    }
}

static void Builtins(IRuntime* runtime)
{
    std::vector<BuiltinInfo> builtins;
    runtime->GetBuiltins(builtins);
    for (const char* entry : {"system.time_ms", "system.utc_date", "system.time_span", "system.time_span_ms", "system.stopwatch",
        "time_span.total_milliseconds", "time_span.total_seconds", "time_span.parts", "time_span.format", "time_span.add",
        "time_span.subtract", "time_span.compare", "stopwatch.elapsed", "stopwatch.elapsed_ms", "stopwatch.restart"})
    {
        bool found = false;
        for (const auto& info : builtins)
            if (info.module + "." + info.name == entry && !info.ret.empty() && info.argCount >= 0) found = true;
        Check(found,entry);
    }
}

int main()
{
    // 로컬 시간대가 UTC와 달라도 UTC 포맷과 오프셋이 변하지 않아야 한다.
#ifdef _WIN32
    _putenv_s("TZ","PST8PDT"); _tzset();
#else
    setenv("TZ","PST8PDT",1); tzset();
#endif
    auto runtime=CreateRuntime();
    Builtins(runtime);
    for (bool debug : {false,true}) for (bool image : {false,true})
    {
        Semantics(runtime,debug,image);
        Clocks(runtime,debug,image);
    }
    CalendarReference();
    Check(runtime->CanDestroy(),"all invocation leases released");
    DestroyRuntime(runtime);
    std::printf("time_regression: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
