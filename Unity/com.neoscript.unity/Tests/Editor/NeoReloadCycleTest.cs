// NeoReloadCycleTest.cs — 도메인 리로드를 건너서도 플러그인이 멀쩡한지 검증한다.
//
// 이게 왜 따로 있느냐: 스크립트를 고칠 때마다 유니티는 도메인을 갈아엎고, 그 경계에서
// NeoDomainReloadGuard 가 네이티브 런타임을 파괴하고 로그 콜백을 떼어 낸다. 이 정리가
// 조금만 어긋나도 리로드 건너편 첫 호출에서 에디터가 통째로 죽는다 — 실제로 그런 적이 있다.
// 에디터 스모크도 PlayMode 테스트도 한 도메인 안에서만 돌기 때문에 이 경계를 못 본다.
//
// 도메인이 리로드되면 이 메서드의 스택도 사라지므로 SessionState 에 표식을 남겨 이어 받는다.
//
//   Unity.exe -batchmode -nographics -projectPath <프로젝트> \
//             -executeMethod NeoScript.Tests.NeoReloadCycleTest.RunAll -logFile -

using System;
using UnityEditor;

namespace NeoScript.Tests
{
    [InitializeOnLoad]
    public static class NeoReloadCycleTest
    {
        private const string StateKey = "NeoScript.ReloadCycleTest";

        static NeoReloadCycleTest()
        {
            // 도메인 로드마다 불린다. 표식이 있을 때만 건너편 절반을 실행한다.
            if (SessionState.GetInt(StateKey, 0) != 1)
                return;
            SessionState.EraseInt(StateKey);

            try
            {
                // 리로드 건너편. 정리가 어긋나 있으면 여기서 죽거나 예외가 난다.
                Probe("after reload");
                Console.WriteLine("NEO_RELOAD_CYCLE_OK");
                EditorApplication.Exit(0);
            }
            catch (Exception e)
            {
                Console.WriteLine("NEO_RELOAD_CYCLE_FAILED " + e);
                EditorApplication.Exit(1);
            }
        }

        public static void RunAll()
        {
            try
            {
                // 먼저 플러그인을 건드려서 Mono 가 진입점을 해소·캐싱하고, 네이티브에 로그
                // 콜백이 걸리게 만든다. 그 상태가 아니면 애초에 재현되지 않는다.
                Probe("before reload");

                SessionState.SetInt(StateKey, 1);
                EditorUtility.RequestScriptReload();
                // 여기서 반환한다. 도메인이 리로드되고 static 생성자가 이어받는다.
            }
            catch (Exception e)
            {
                SessionState.EraseInt(StateKey);
                Console.WriteLine("NEO_RELOAD_CYCLE_FAILED " + e);
                EditorApplication.Exit(1);
            }
        }

        private static void Probe(string what)
        {
            NeoRuntime.SetLogSink(_ => { }, _ => { });

            using (var runtime = new NeoRuntime())
            {
                runtime.Freeze();
                using (NeoProgram program = runtime.Compile("export fun F() { return 7; }", "probe.ns"))
                using (NeoInstance instance = program.CreateInstance())
                {
                    int value = instance.Call("F").InvokeInt();
                    if (value != 7)
                        throw new Exception($"{what}: probe returned {value}, expected 7");
                }
            }

            Console.WriteLine("NEO_RELOAD_CYCLE_PROBE " + what);
        }
    }
}
