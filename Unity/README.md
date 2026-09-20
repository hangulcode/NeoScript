# Unity

Everything needed to run NeoScript from Unity. The full write-up is in
**[../docs/Unity.md](../docs/Unity.md)**.

| Path | Contents |
| :--- | :--- |
| `Native/` | The C ABI (`NeoScriptC.h` / `.cpp`) — the C++ facade flattened to plain C |
| `com.neoscript.unity/` | UPM package: C# binding, editor support, tests, sample |
| `build_windows.ps1` | `NeoScript.dll` for the editor and the Win64 player |
| `build_android.ps1` | `libNeoScript.so` (arm64-v8a by default) |
| `build_ios.sh` | `libNeoScriptUnity.a` — **runs on macOS with Xcode only** |

## Quick start

```powershell
pwsh Unity\build_windows.ps1
```

Add the package to the Unity project's `Packages/manifest.json` (paths resolve
against the `Packages` folder):

```json
"com.neoscript.unity": "file:../../path/to/NeoScript/Unity/com.neoscript.unity"
```

## Verification

Run all three. Each sees what the others cannot: the smoke test covers the
binding surface, the PlayMode tests cover lifetimes across frame boundaries, and
the reload cycle covers cleanup at the domain boundary.

```powershell
Unity.exe -batchmode -nographics -projectPath <project> `
          -executeMethod NeoScript.Tests.NeoSmokeTest.RunAll -logFile -

Unity.exe -batchmode -projectPath <project> `
          -runTests -testPlatform PlayMode -testResults results.xml

Unity.exe -batchmode -nographics -projectPath <project> `
          -executeMethod NeoScript.Tests.NeoReloadCycleTest.RunAll -logFile -
```

In the editor it is **Tools > NeoScript > Run smoke test**.

Rebuilding the native plugin means **closing the editor**. Releasing the plugin
at runtime was tried and removed; see "Rebuilding the native plugin" in
`docs/Unity.md` for why.

## Worth knowing

The plugin binaries are not in the repository. They are build output and are
gitignored, so run the build scripts above before opening the project. Importer
settings are applied by `NeoPluginImportSettings.cs` rather than committed
`.meta` files, because Unity deletes a `.meta` whose asset is missing and there
is no way to commit the settings alone.
