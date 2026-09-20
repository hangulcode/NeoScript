// NeoScriptDemo.cs — GameObject 에 붙이고 Play 하면 도는 최소 예제.
// 씬에 꽂는 건 Tools > NeoScript > Set up demo scene 이 해 준다(큐브를 만들어 붙인다).
//
// 보여주는 것:
//   · 런타임 하나를 만들고 C# 객체를 스크립트 전역으로 노출한다
//   · 스크립트를 컴파일해 인스턴스를 만든다
//   · 매 프레임 스크립트 Update(dt) 를 부르고, 스크립트는 Unity.* 로 되돌아온다
//   · 수명 정리 — 네이티브 자원은 GC 가 치워 주지 않는다

using System;
using NeoScript;
using UnityEngine;

public sealed class NeoScriptDemo : MonoBehaviour
{
    /// <summary>인스펙터 기본값. 스모크 테스트가 이 문자열이 실제로 컴파일되는지 확인한다.</summary>
    public const string DefaultScript = @"
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
        Unity.Log(""neo script is driving this transform"");
}
";

    [TextArea(10, 30)]
    public string Script = DefaultScript;

    private NeoRuntime _runtime;
    private NeoProgram _program;
    private NeoInstance _instance;
    private NeoFunction _update;
    private UnityBridge _bridge;

    /// <summary>스크립트에서 Unity.* 로 보이는 객체.</summary>
    private sealed class UnityBridge : INeoObject, INeoProperties
    {
        internal Transform Target;

        public bool Invoke(NeoCallContext ctx, NeoName method)
        {
            if (method.Is("Log"))
            {
                Debug.Log("[neo] " + ctx.ArgString(0));
                return true;
            }

            if (method.Is("SetPosition"))
            {
                if (Target != null)
                    Target.localPosition = ctx.ArgVector3(0);
                return true;
            }

            ctx.Fail(1, "unknown Unity method: " + method.ToString());
            return false;
        }

        public bool GetProperty(NeoCallContext ctx, NeoName name)
        {
            if (name.Is("time")) { ctx.Return(Time.time); return true; }
            if (name.Is("deltaTime")) { ctx.Return(Time.deltaTime); return true; }
            return false;
        }

        public bool SetProperty(NeoCallContext ctx, NeoName name) => false;
    }

    private void Awake()
    {
        Boot();
    }

    private void Boot()
    {
        Teardown();

        try
        {
            // print/error 는 프로세스 전역 훅이다. 한 번만 걸면 된다.
            NeoRuntime.SetLogSink(Debug.Log, Debug.LogError);

            _bridge = new UnityBridge { Target = transform };

            _runtime = new NeoRuntime(new NeoRuntimeOptions
            {
                // import 는 여기서 해석한다. 실제 프로젝트라면 Resources/에셋번들을 읽는다.
                Loader = path =>
                {
                    var asset = Resources.Load<TextAsset>("NeoScripts/" + System.IO.Path.GetFileNameWithoutExtension(path));
                    return asset != null ? asset.bytes : null;
                },
            });

            _runtime.RegisterObject("Unity", _bridge);
            _runtime.Freeze();

            _program = _runtime.Compile(Script, "demo.ns");
            _instance = _program.CreateInstance();
            // 매 프레임 부를 함수는 핸들을 캐싱한다. 프레임마다 이름으로 찾을 이유가 없다.
            _update = _program.FindFunction("Update");

            // 여기까지 찍혔으면 네이티브 플러그인 로드와 컴파일이 끝난 것이다.
            // 뭔가 안 보일 때 어디까지 갔는지 가장 먼저 알려 주는 신호다.
            Debug.Log($"[neo] runtime up, '{name}' is now driven by demo.ns");
        }
        catch (Exception e)
        {
            // 실패하면 반드시 꺼야 한다. 안 그러면 Update 가 매 프레임 Boot 을 재시도한다.
            Debug.LogError("[neo] " + e.Message);
            Teardown();
            enabled = false;
        }
    }

    private void Update()
    {
        // 플레이 중 스크립트를 고치면 유니티가 도메인을 리로드한다. 네이티브 런타임은 그때
        // 파괴되고, 이 컴포넌트의 비직렬화 필드도 null 로 리셋되지만 Awake 는 다시 불리지 않는다.
        // 둘 중 어느 쪽이든 결론은 같다 — 다시 세운다.
        if (_instance == null || _update == null || !_instance.IsAlive)
        {
            Boot();
            return;
        }

        // Timeout 은 스크립트가 무한루프에 빠져도 프레임을 지키는 안전망이다.
        NeoResult result = _instance.Call(_update).Arg(Time.deltaTime).Timeout(8).Invoke();
        if (!result.Ok)
        {
            Debug.LogError("[neo] Update failed: " + result.Error);
            enabled = false;
        }
    }

    private void OnDestroy() => Teardown();

    private void Teardown()
    {
        // 네이티브 자원이라 순서가 있다: 함수 → 인스턴스 → 프로그램 → 런타임.
        _update?.Dispose();
        _instance?.Dispose();
        _program?.Dispose();
        _runtime?.Dispose();
        _update = null;
        _instance = null;
        _program = null;
        _runtime = null;
    }
}
