// NeoDomainReloadGuard.cs — 도메인 리로드 경계에서 네이티브 상태를 정리한다.
//
// 유니티는 스크립트를 다시 컴파일할 때마다 관리 도메인을 갈아엎는다. 그 순간 관리 쪽
// NeoRuntime 객체는 사라지지만 네이티브 런타임은 그대로 남는다. 주인이 없어진 채로 남으면
// 디스패치 슬롯과 VM 메모리가 새고, 리로드가 반복되는 에디터에서는 금방 쌓인다.
// 그래서 리로드 직전에 전부 파괴한다.
//
// [왜 플러그인을 언로드하지 않는가]
// 에디터는 DllImport 로 한 번 로드한 네이티브 플러그인을 놓지 않으므로, NeoScript.dll 을
// 다시 빌드하려면 에디터를 닫아야 한다. 흔히 쓰는 우회가 FreeLibrary 로 참조를 떨구는
// 것인데, **여기서는 쓰지 않는다. 실제로 해 보면 플러그인이 그 세션 내내 죽는다.**
//   · 리로드 없이 해제하면 Mono 가 캐싱해 둔 P/Invoke 함수 포인터가 해제된 주소를 가리키고,
//     다음 호출이 에디터를 통째로 죽인다.
//   · 도메인 리로드 안에서 해제해도 살아 돌아오지 못한다. 유니티는 플러그인 이름을 이미
//     로드한 모듈 핸들로 해소하는데, 그 핸들이 죽은 뒤에는 다시 로드하지 않는다 —
//     이후 모든 호출이 EntryPointNotFoundException 이 된다.
// 둘 다 실측으로 확인했다. 네이티브를 고쳤으면 에디터를 닫고 빌드할 것.
//
// 에디터를 닫지 않고 반복 수정하고 싶다면 DllImport 대신 호스트가 직접 LoadLibrary/
// GetProcAddress 로 함수 포인터 테이블을 잡는 구조로 바꿔야 한다. 그건 바인딩 계층의
// 설계 변경이라 여기서 우회할 수 있는 문제가 아니다.

#if UNITY_EDITOR

using UnityEditor;

namespace NeoScript.Editor
{
    [InitializeOnLoad]
    internal static class NeoDomainReloadGuard
    {
        static NeoDomainReloadGuard()
        {
            AssemblyReloadEvents.beforeAssemblyReload += OnBeforeAssemblyReload;
            EditorApplication.playModeStateChanged += OnPlayModeChanged;
        }

        private static void OnBeforeAssemblyReload()
        {
            // 순서가 중요하다. 둘 다 네이티브를 호출하므로 모듈이 살아 있는 지금 해야 한다.
            NeoRuntime.DisposeAll();

            // 네이티브가 들고 있는 로그 콜백은 관리 델리게이트의 썽크 주소다. 리로드와 함께
            // 그 주소가 죽는데 모듈은 계속 살아 있으므로, 떼어 내지 않으면 다음 print 가
            // 죽은 주소로 점프한다.
            NeoRuntime.ClearLogSink();
        }

        private static void OnPlayModeChanged(PlayModeStateChange change)
        {
            // 플레이가 **완전히 끝난 뒤**에 정리한다. ExitingPlayMode 에서 하면 아직 Update 가
            // 도는 컴포넌트 밑에서 런타임을 빼 버려 ObjectDisposedException 이 난다.
            // 여기까지 오면 OnDestroy 는 이미 지났으므로, 주인이 정리를 잊었을 때의 안전망이다.
            if (change == PlayModeStateChange.EnteredEditMode)
                NeoRuntime.DisposeAll();
        }
    }
}

#endif
