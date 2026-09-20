// NeoPluginImportSettings.cs — 네이티브 플러그인 임포트 설정을 코드로 잡는다.
//
// 플러그인 바이너리는 빌드 산출물이라 저장소에 넣지 않는다(빌드할 때마다 바뀌는 수 MB 를
// 히스토리에 쌓을 이유가 없다). 그런데 유니티는 대상 파일이 없는 .meta 를 지우므로, 설정을
// .meta 로 미리 커밋해 둘 수도 없다. 그래서 바이너리가 나타나는 순간 여기서 설정을 넣는다.
//
// 기본값(Any Platform)으로 들어오면 조용히 엉뚱하게 터진다 — 안드로이드 빌드에 Windows
// DLL 을 밀어 넣거나, iOS 정적 라이브러리를 에디터가 로드하려 들거나 하는 식으로.

using System;
using UnityEditor;

namespace NeoScript.Editor
{
    internal sealed class NeoPluginImportSettings : AssetPostprocessor
    {
        private const string PluginRoot = "com.neoscript.unity/Runtime/Plugins/";
        private const string PackagePlugins = "Packages/com.neoscript.unity/Runtime/Plugins/";

        private static readonly string[] KnownPlugins =
        {
            PackagePlugins + "x86_64/NeoScript.dll",
            PackagePlugins + "Android/arm64-v8a/libNeoScript.so",
            PackagePlugins + "Android/armeabi-v7a/libNeoScript.so",
            PackagePlugins + "iOS/libNeoScriptUnity.a",
        };

        private void OnPreprocessAsset()
        {
            if (!(assetImporter is PluginImporter importer))
                return;
            if (assetPath.IndexOf(PluginRoot, StringComparison.Ordinal) < 0)
                return;
            Configure(assetPath, importer);
        }

        // OnPreprocessAsset 만으로는 부족하다. 유니티는 내용이 그대로면 임포트를 건너뛰고
        // 캐시된 결과를 쓰므로, 설정이 비어 있는 채로 굳어 버릴 수 있다(플러그인 바이너리를
        // 저장소 밖에서 만들어 넣는 우리 구조에서 실제로 그렇게 된다).
        // 그래서 도메인 리로드마다 한 번 확인하고, 어긋났을 때만 재임포트한다.
        // delayCall 이 아니라 동기로 돈다. 배치모드에서 -executeMethod 가 먼저 실행돼
        // 설정이 아직 안 잡힌 상태를 보는 일이 없어야 한다.
        // 재임포트가 OnPreprocessAsset 을 부르지만 같은 설정을 다시 쓸 뿐이고 도메인
        // 리로드를 일으키지 않으므로 루프가 생기지 않는다.
        [InitializeOnLoadMethod]
        private static void VerifyOnLoad()
        {
            foreach (string path in KnownPlugins)
            {
                if (!(AssetImporter.GetAtPath(path) is PluginImporter importer))
                    continue;   // 그 플랫폼 바이너리를 아직 안 만든 상태
                if (IsConfigured(path, importer))
                    continue;
                Configure(path, importer);
                importer.SaveAndReimport();
            }
        }

        private static bool IsConfigured(string path, PluginImporter importer)
        {
            if (path.EndsWith("/x86_64/NeoScript.dll", StringComparison.Ordinal))
                return importer.GetCompatibleWithEditor()
                    && importer.GetCompatibleWithPlatform(BuildTarget.StandaloneWindows64)
                    && !importer.GetCompatibleWithAnyPlatform();
            if (path.EndsWith("libNeoScript.so", StringComparison.Ordinal))
                return importer.GetCompatibleWithPlatform(BuildTarget.Android)
                    && !importer.GetCompatibleWithEditor()
                    && !importer.GetCompatibleWithAnyPlatform();
            if (path.EndsWith("libNeoScriptUnity.a", StringComparison.Ordinal))
                return importer.GetCompatibleWithPlatform(BuildTarget.iOS)
                    && !importer.GetCompatibleWithEditor()
                    && !importer.GetCompatibleWithAnyPlatform();
            return true;
        }

        private static void Configure(string path, PluginImporter importer)
        {
            if (path.EndsWith("/x86_64/NeoScript.dll", StringComparison.Ordinal))
                ConfigureWindows(importer);
            else if (path.EndsWith("/arm64-v8a/libNeoScript.so", StringComparison.Ordinal))
                ConfigureAndroid(importer, "ARM64");
            else if (path.EndsWith("/armeabi-v7a/libNeoScript.so", StringComparison.Ordinal))
                ConfigureAndroid(importer, "ARMv7");
            else if (path.EndsWith("libNeoScriptUnity.a", StringComparison.Ordinal))
                ConfigureIos(importer);
        }

        private static void ConfigureWindows(PluginImporter importer)
        {
            Reset(importer);
            importer.SetCompatibleWithEditor(true);
            importer.SetEditorData("OS", "Windows");
            importer.SetEditorData("CPU", "x86_64");
            importer.SetCompatibleWithPlatform(BuildTarget.StandaloneWindows64, true);
            importer.SetPlatformData(BuildTarget.StandaloneWindows64, "CPU", "x86_64");
        }

        private static void ConfigureAndroid(PluginImporter importer, string cpu)
        {
            Reset(importer);
            importer.SetCompatibleWithPlatform(BuildTarget.Android, true);
            importer.SetPlatformData(BuildTarget.Android, "CPU", cpu);
        }

        private static void ConfigureIos(PluginImporter importer)
        {
            Reset(importer);
            importer.SetCompatibleWithPlatform(BuildTarget.iOS, true);
        }

        /// <summary>
        /// 모든 플랫폼을 끄고 시작한다. 켜는 것만 나열하면 이전 설정이 남아 섞일 수 있다.
        /// </summary>
        private static void Reset(PluginImporter importer)
        {
            importer.SetCompatibleWithAnyPlatform(false);
            importer.SetCompatibleWithEditor(false);
            importer.SetCompatibleWithPlatform(BuildTarget.StandaloneWindows64, false);
            importer.SetCompatibleWithPlatform(BuildTarget.StandaloneLinux64, false);
            importer.SetCompatibleWithPlatform(BuildTarget.StandaloneOSX, false);
            importer.SetCompatibleWithPlatform(BuildTarget.Android, false);
            importer.SetCompatibleWithPlatform(BuildTarget.iOS, false);
        }
    }
}
