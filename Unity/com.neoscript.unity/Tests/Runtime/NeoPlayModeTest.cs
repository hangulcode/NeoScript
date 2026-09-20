// NeoPlayModeTest.cs — Play 모드에서 도는지 확인한다.
//
// 에디터 모드 스모크(Tests/Editor/NeoSmokeTest.cs)가 통과해도 Play 모드는 별개다:
// 플레이 진입 시 도메인이 리로드되고, 에디터 플러그인 언로더가 그 직전에 네이티브 런타임을
// 파괴한다. 실제로 이 경로에서 ObjectDisposedException 이 났던 적이 있어서 테스트로 못을 박는다.
//
//   Unity.exe -batchmode -projectPath <프로젝트> -runTests -testPlatform PlayMode \
//             -testResults results.xml
//
// 패키지 테스트라 프로젝트의 manifest.json 에 "testables": ["com.neoscript.unity"] 가 필요하다.

using System.Collections;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;

namespace NeoScript.Tests
{
    /// <summary>스크립트가 Unity.* 로 되돌아오는 경로. 데모와 같은 모양이다.</summary>
    internal sealed class TransformBridge : INeoObject, INeoProperties
    {
        internal Transform Target;
        internal int Logs;

        public bool Invoke(NeoCallContext ctx, NeoName method)
        {
            if (method.Is("SetPosition"))
            {
                if (Target != null)
                    Target.localPosition = ctx.ArgVector3(0);
                return true;
            }
            if (method.Is("Log"))
            {
                ++Logs;
                return true;
            }
            ctx.Fail(1, "unknown Unity method: " + method.ToString());
            return false;
        }

        public bool GetProperty(NeoCallContext ctx, NeoName name)
        {
            if (name.Is("time")) { ctx.Return(Time.time); return true; }
            return false;
        }

        public bool SetProperty(NeoCallContext ctx, NeoName name) => false;
    }

    public sealed class NeoPlayModeTest
    {
        private const string Source = @"
import math;

export var ticks = 0;

fun Wobble(var t)
{
    return math.Vector3(math.sin(t) * 2.0, 1.0, math.cos(t) * 2.0);
}

export fun Update(var dt)
{
    ticks = ticks + 1;
    Unity.SetPosition(Wobble(Unity.time));
    if (ticks == 1)
        Unity.Log(""first tick"");
}
";

        [UnityTest]
        public IEnumerator ScriptDrivesATransformAcrossFrames()
        {
            const int frames = 10;

            var probe = new GameObject("neo playmode probe");
            var bridge = new TransformBridge { Target = probe.transform };
            NeoRuntime.SetLogSink(_ => { }, Debug.LogError);

            NeoRuntime runtime = null;
            NeoProgram program = null;
            NeoInstance instance = null;
            NeoFunction update = null;

            try
            {
                runtime = new NeoRuntime();
                runtime.RegisterObject("Unity", bridge);
                runtime.Freeze();

                program = runtime.Compile(Source, "playmode.ns");
                instance = program.CreateInstance();
                update = program.FindFunction("Update");
                Assert.IsNotNull(update, "Update should resolve to a function handle");

                Vector3 start = probe.transform.localPosition;

                for (int i = 0; i < frames; ++i)
                {
                    Assert.IsTrue(instance.IsAlive, $"instance died before frame {i}");

                    NeoResult result = instance.Call(update).Arg(Time.deltaTime).Timeout(8).Invoke();
                    Assert.IsTrue(result.Ok, $"frame {i} failed: {result.Error}");

                    // yield 는 프레임을 넘긴다. 여기가 핵심이다 — 에디터 모드 테스트는 한 프레임
                    // 안에서 끝나므로 프레임 경계를 넘는 수명 문제를 못 잡는다.
                    yield return null;
                }

                Assert.AreNotEqual(start, probe.transform.localPosition,
                    "the script should have moved the transform");
                Assert.AreEqual(1, bridge.Logs, "Unity.Log should have been called once");

                Assert.IsTrue(instance.TryGetInt("ticks", out int ticks), "ticks should be readable");
                Assert.AreEqual(frames, ticks, "the script should have ticked once per frame");
            }
            finally
            {
                update?.Dispose();
                instance?.Dispose();
                program?.Dispose();
                runtime?.Dispose();
                if (probe != null)
                    Object.Destroy(probe);
            }
        }

        /// <summary>
        /// 런타임을 파괴한 뒤 그 인스턴스를 부르면 예외가 나야 한다 — 죽은 네이티브 핸들로
        /// 넘어가 프로세스를 죽이는 것보다 낫다. 플레이 중 도메인 리로드가 정확히 이 상태를 만든다.
        /// </summary>
        [Test]
        public void CallingIntoADestroyedRuntimeThrowsInsteadOfCrashing()
        {
            var runtime = new NeoRuntime();
            runtime.Freeze();
            NeoProgram program = runtime.Compile("export fun F() { return 1; }", "dead.ns");
            NeoInstance instance = program.CreateInstance();

            Assert.AreEqual(1, instance.Call("F").InvokeInt());

            runtime.Dispose();

            Assert.IsFalse(instance.IsAlive, "the instance must report dead once its runtime is gone");
            Assert.Throws<System.ObjectDisposedException>(() => instance.Call("F").InvokeInt());
        }
    }
}
