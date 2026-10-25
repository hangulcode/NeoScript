using System;
using System.Threading;

namespace NeoScript.Tests
{
    // Unity 에디터와 독립 관리 바인딩 검사에서 같은 HTTP 수명 회귀를 실행한다.
    public static class NeoHttpRegression
    {
        private static void Check(bool value, string name)
        {
            if (!value) throw new Exception("HTTP regression: " + name);
        }
        public static void Run()
        {
            int owner = Thread.CurrentThread.ManagedThreadId;
            NeoHttpRequest saved = null;
            using (var runtime = new NeoRuntime(new NeoRuntimeOptions { HttpTransport = request =>
            {
                saved = request;
                if (request.Url == "https://throw") throw new Exception("mock failure");
                bool ok = Thread.CurrentThread.ManagedThreadId != owner && request.Method == "POST" &&
                    request.Url == "https://test" && request.Body == "payload" && request.TimeoutMs == -1 &&
                    request.Headers.Count == 1 && request.Headers[0].Key == "X-Test" && request.Headers[0].Value == "v";
                return new NeoHttpResponse(ok, "copied response");
            }}))
            using (var program = runtime.Compile(@"import system;
var ok=false; fun Done(var success,var text){ok=success && text==""copied response"";}
fun Failed(var success,var text){ok=!success && text==""mock failure"";}
export fun Probe(){var a=system.aysnc_create();a.add_header(""X-Test"",""v"");
a.post(-1,""https://test"",""payload"",Done);a.wait();return ok;}
export fun Error(){var a=system.aysnc_create();a.get(1000,""https://throw"",Failed);a.wait();return ok;}"))
            using (var instance = program.CreateInstance())
            {
                Check(instance.Call("Probe").InvokeBool(), "managed request and copied response");
                bool closed = false;
                try { bool unused = saved.IsCancellationRequested; }
                catch (ObjectDisposedException) { closed = true; }
                Check(closed, "request lifetime ends with Send");
                Check(instance.Call("Error").InvokeBool(), "managed exceptions become failed callbacks");
            }
            using (var entered = new ManualResetEventSlim())
            using (var cancelled = new ManualResetEventSlim())
            using (var runtime = new NeoRuntime(new NeoRuntimeOptions { HttpTransport = request =>
            {
                entered.Set();
                var until = DateTime.UtcNow.AddSeconds(3);
                while (!request.IsCancellationRequested && DateTime.UtcNow < until) Thread.Sleep(1);
                if (request.IsCancellationRequested) cancelled.Set();
                return new NeoHttpResponse(false, "cancelled");
            }}))
            using (var program = runtime.Compile(@"import system;fun Done(var ok,var text){}
var a=system.aysnc_create();a.get(-1,""https://stall"",Done);"))
            using (var instance = program.CreateInstance())
            {
                Check(entered.Wait(2000), "managed transfer started");
                runtime.Dispose();
                Check(cancelled.IsSet, "runtime shutdown cancels managed transfer");
            }
        }
    }
}
