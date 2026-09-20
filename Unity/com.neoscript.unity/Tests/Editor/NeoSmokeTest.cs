// NeoSmokeTest.cs — 유니티 안에서 바인딩을 끝까지 굴려 보는 검증.
//
// 에디터 메뉴(Tools > NeoScript > Run smoke test)로도 돌고, 배치모드로도 돈다:
//
//   Unity.exe -batchmode -nographics -projectPath <프로젝트> \
//             -executeMethod NeoScript.Tests.NeoSmokeTest.RunAll -logFile -
//
// 배치모드에서는 실패가 있으면 종료 코드 1 이다. CI 에 그대로 걸 수 있다.
//
// 덮는 범위: 네이티브 플러그인 임포트 설정, C#→스크립트 호출, 스크립트→C# 디스패치(메서드와
// 프로퍼티), 관리 로더를 통한 import, 유니티 벡터 왕복, 컬렉션 빌드/읽기, 전역 변수,
// 실패 전파, 시간분할 실행, 바이트코드 왕복.

using System;
using System.Collections.Generic;
using System.Text;
using UnityEditor;
using UnityEngine;

namespace NeoScript.Tests
{
    /// <summary>스크립트에 노출할 C# 객체. 스크립트에서는 Host.Xxx() 로 보인다.</summary>
    internal sealed class HostObject : INeoObject, INeoProperties
    {
        internal readonly List<string> Received = new List<string>();
        internal int Version = 7;

        public bool Invoke(NeoCallContext ctx, NeoName method)
        {
            if (method.Is("Double"))
            {
                ctx.Return(ctx.ArgInt(0) * 2);
                return true;
            }

            if (method.Is("Echo"))
            {
                string text = ctx.ArgString(0);
                Received.Add(text);
                ctx.Return("echo:" + text);
                return true;
            }

            if (method.Is("Move"))
            {
                // 스크립트의 Vector3 가 유니티 Vector3 로 그대로 넘어온다.
                ctx.Return(ctx.ArgVector3(0) + new Vector3(0f, 1f, 0f));
                return true;
            }

            if (method.Is("Stats"))
            {
                NeoMapBuilder map = ctx.ReturnMap();
                map.Set("hp", 42);
                map.Set("name", "orc");
                map.Set("pos", new Vector3(1f, 2f, 3f));
                return true;
            }

            if (method.Is("SumList"))
            {
                NeoListReader list = ctx.ArgList(0);
                int total = 0;
                for (int i = 0; i < list.Count; ++i)
                    total += list.GetInt(i);
                ctx.Return(total);
                return true;
            }

            // 실패는 사유를 남기고 false. 그래야 스크립트 스택트레이스가 같이 붙는다.
            ctx.Fail(4242, "unknown Host method: " + method.ToString());
            return false;
        }

        public bool GetProperty(NeoCallContext ctx, NeoName name)
        {
            if (name.Is("version")) { ctx.Return(Version); return true; }
            if (name.Is("time")) { ctx.Return(1.5f); return true; }
            return false;
        }

        public bool SetProperty(NeoCallContext ctx, NeoName name)
        {
            if (name.Is("version")) { Version = ctx.ArgInt(0); return true; }
            return false;
        }
    }

    public static class NeoSmokeTest
    {
        private const string Source = @"
import math;
import util;

export var counter = 10;

print(""script global init ran"");

export fun Add(var a, var b) { return a + b; }
export fun Greet(var who) { return ""hi "" + who; }
export fun Bump() { counter = counter + 1; return counter; }
export fun HostDouble(var v) { return Host.Double(v); }
export fun HostEcho(var s) { return Host.Echo(s); }
export fun HostMove() { return Host.Move(math.Vector3(1.0, 2.0, 3.0)); }
export fun HostStatsHp() { var r = Host.Stats(); return r[""hp""]; }
export fun HostSum() { return Host.SumList([1, 2, 3, 4]); }
export fun HostVersion() { return Host.version; }
export fun Loot() { var m = {}; m[""gold""] = 77; m[""name""] = ""chest""; return m; }
export fun Where() { return math.Vector3(4.0, 5.0, 6.0); }
export fun ViaImport(var v) { return util.triple(v); }
export fun Boom() { return Host.Nope(); }
export fun Busy() { var s = 0; for (var i in 0, 400000) { s = s + 1; } return s; }
";

        // Samples~/Basic/NeoScriptDemo.cs 가 쓰는 관용구를 그대로 따라간다. 예제가 조용히
        // 썩는 걸 막으려는 것이다 — Samples~ 는 유니티가 컴파일하지 않으므로 여기서 문법을 지킨다.
        private const string DemoStyleSource = @"
import math;

export var ticks = 0;

fun Wobble(var t)
{
    return math.Vector3(math.sin(t) * 2.0, 1.0, math.cos(t) * 2.0);
}

export fun Update(var dt)
{
    ticks = ticks + 1;
    Host.Move(Wobble(Host.time));
}
";

        // import 로 끌어올 가상 모듈. 실제 프로젝트에서는 Resources 나 에셋번들에서 읽는다.
        private const string UtilSource = "export fun triple(var v) { return v * 3; }\n";

        private static int s_checks;
        private static int s_failures;
        private static readonly StringBuilder s_log = new StringBuilder();

        [MenuItem("Tools/NeoScript/Run smoke test")]
        public static void RunFromMenu()
        {
            Run();
            Debug.Log(s_log.ToString());
        }

        /// <summary>배치모드 진입점. 실패가 있으면 종료 코드 1.</summary>
        public static void RunAll()
        {
            int failures = Run();
            Console.WriteLine(s_log.ToString());
            EditorApplication.Exit(failures == 0 ? 0 : 1);
        }

        private static int Run()
        {
            s_checks = 0;
            s_failures = 0;
            s_log.Clear();
            s_log.AppendLine("=== NeoScript Unity smoke ===");

            var printed = new List<string>();
            NeoRuntime.SetLogSink(
                message => { printed.Add(message); s_log.AppendLine("  [script] " + message); },
                message => s_log.AppendLine("  [script error] " + message));

            NeoRuntime runtime = null;
            NeoProgram program = null;
            NeoInstance instance = null;
            var host = new HostObject();

            try
            {
                // 에디터에서 도는 것만으로는 부족하다. 네이티브 플러그인이 PluginImporter 로
                // 잡혀 있지 않으면 플레이어 빌드에 안 실리는데, 빌드해 보기 전까지는 모른다.
                CheckPluginImport();

                runtime = new NeoRuntime(new NeoRuntimeOptions
                {
                    Loader = path =>
                    {
                        s_log.AppendLine("  [loader] " + path);
                        return path.EndsWith("util.ns", StringComparison.OrdinalIgnoreCase)
                            ? Encoding.UTF8.GetBytes(UtilSource)
                            : null;
                    },
                });
                Check(true, "runtime created");

                runtime.RegisterObject("Host", host);
                runtime.Freeze();
                Check(true, "Host registered and bindings frozen");

                program = runtime.Compile(Source, "smoke.ns");
                Check(true, "script compiled");

                instance = program.CreateInstance();
                Check(instance.IsAlive, "instance alive");
                Check(printed.Contains("script global init ran"), "print reached the managed log sink");

                // --- 스칼라 ---
                Check(instance.Call("Add").Arg(20).Arg(22).InvokeInt() == 42, "Add(20, 22) == 42");
                Check(instance.Call("Greet").Arg("neo").InvokeString() == "hi neo", "Greet(\"neo\")");

                // --- 캐싱한 함수 핸들 ---
                using (NeoFunction add = program.FindFunction("Add"))
                {
                    Check(add != null && add.IsValid, "Add resolved to a cached handle");
                    Check(instance.Call(add).Arg(1).Arg(2).InvokeInt() == 3, "cached handle call == 3");
                }

                // --- import 가 관리 로더를 거쳐 해소된다 ---
                Check(instance.Call("ViaImport").Arg(5).InvokeInt() == 15, "imported util.triple(5) == 15");

                // --- 스크립트 → C# 객체 ---
                Check(instance.Call("HostDouble").Arg(21).InvokeInt() == 42, "Host.Double(21) == 42");
                Check(instance.Call("HostEcho").Arg("hey").InvokeString() == "echo:hey", "Host.Echo round-trip");
                Check(host.Received.Contains("hey"), "C# side observed the script's argument");
                Check(instance.Call("HostStatsHp").InvokeInt() == 42, "C#-built return map reaches script");
                Check(instance.Call("HostSum").InvokeInt() == 10, "script list read from C# == 10");
                Check(instance.Call("HostVersion").InvokeInt() == 7, "C# property get == 7");

                // --- 벡터가 유니티 타입으로 오간다 ---
                Vector3 moved = instance.Call("HostMove").InvokeVector3();
                Check(moved == new Vector3(1f, 3f, 3f), "Vector3 round-trips through C# ({0})", moved);
                Vector3 where = instance.Call("Where").InvokeVector3();
                Check(where == new Vector3(4f, 5f, 6f), "script Vector3 return ({0})", where);

                // --- 컬렉션 반환 ---
                int gold = 0;
                string chest = null;
                instance.Call("Loot").InvokeReadMap(map =>
                {
                    map.TryGet("gold", out gold);
                    map.TryGet("name", out chest);
                });
                Check(gold == 77, "map return: gold == 77");
                Check(chest == "chest", "map return: name == chest");

                // --- 전역 변수 (export var 만 보인다) ---
                Check(instance.TryGetInt("counter", out int counter) && counter == 10, "global read == 10");
                instance.Call("Bump").InvokeInt();
                Check(instance.TryGetInt("counter", out counter) && counter == 11, "global sees script write");
                Check(instance.SetInt("counter", 200), "global write accepted");
                Check(instance.Call("Bump").InvokeInt() == 201, "script sees host write");

                // --- 실패가 사유를 달고 올라온다 ---
                NeoResult failed = instance.Call("Boom").Invoke();
                Check(!failed.Ok, "unknown native method fails the call");
                Check(failed.Error != null && failed.Error.Contains("unknown Host method"),
                      "ctx.Fail message surfaces");

                // --- 시간 분할 실행 ---
                Check(instance.StartSliced("Busy", timeoutMs: 1), "sliced run started");
                int slices = 0;
                NeoRunStatus status;
                do
                {
                    status = instance.UpdateSliced();
                    ++slices;
                } while (status == NeoRunStatus.Suspended && slices < 100000);
                Check(status == NeoRunStatus.Completed, "sliced run completed in {0} slice(s)", slices);
                Check(!instance.IsRunning, "instance idle after sliced run");

                // --- 샘플이 쓰는 관용구가 여전히 컴파일되는지 ---
                using (NeoProgram demo = runtime.Compile(DemoStyleSource, "demo.ns"))
                    Check(demo != null, "sample-style script compiles");

                // --- 바이트코드 왕복 ---
                byte[] bytecode = runtime.CompileToBytecode(Source, "smoke.ns");
                Check(bytecode.Length > 0, "CompileToBytecode produced {0} bytes", bytecode.Length);
                using (NeoProgram loaded = runtime.LoadProgram(bytecode))
                using (NeoInstance loadedInstance = loaded.CreateInstance())
                {
                    Check(loadedInstance.Call("Add").Arg(2).Arg(3).InvokeInt() == 5,
                          "instance from loaded bytecode runs");
                }
            }
            catch (Exception e)
            {
                ++s_failures;
                s_log.AppendLine("  EXCEPTION  " + e);
            }
            finally
            {
                instance?.Dispose();
                program?.Dispose();
                runtime?.Dispose();
            }

            s_log.AppendLine($"=== {s_checks} checks, {s_failures} failures ===");
            return s_failures;
        }

        private static void CheckPluginImport()
        {
            const string root = "Packages/com.neoscript.unity/Runtime/Plugins/";

            var windows = AssetImporter.GetAtPath(root + "x86_64/NeoScript.dll") as PluginImporter;
            Check(windows != null, "Windows plugin imported as a PluginImporter");
            if (windows != null)
            {
                Check(windows.GetCompatibleWithEditor(), "Windows plugin enabled for the editor");
                Check(windows.GetCompatibleWithPlatform(BuildTarget.StandaloneWindows64),
                      "Windows plugin enabled for the Win64 player");
                Check(!windows.GetCompatibleWithPlatform(BuildTarget.Android),
                      "Windows plugin is not offered to Android");
            }

            // 안드로이드 바이너리는 아직 안 빌드했을 수 있다. 있으면 설정을 확인한다.
            var android = AssetImporter.GetAtPath(root + "Android/arm64-v8a/libNeoScript.so") as PluginImporter;
            if (android != null)
            {
                Check(android.GetCompatibleWithPlatform(BuildTarget.Android),
                      "Android plugin enabled for Android");
                Check(!android.GetCompatibleWithEditor(), "Android plugin is not loaded by the editor");
            }
            else
            {
                s_log.AppendLine("  skip  Android plugin (not built yet)");
            }
        }

        private static void Check(bool condition, string format, params object[] args)
        {
            ++s_checks;
            string what = args.Length > 0 ? string.Format(format, args) : format;
            if (condition)
            {
                s_log.AppendLine("  ok    " + what);
            }
            else
            {
                ++s_failures;
                s_log.AppendLine("  FAIL  " + what);
            }
        }
    }
}
