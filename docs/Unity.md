# NeoScript × Unity

Running the NeoScript VM from Unity. For the language itself see
[../ReadMe.md](../ReadMe.md), for the functions a script can call see
[API.md](API.md), and for the C++ host API see [Embedding.md](Embedding.md).

## Layers

C# cannot take C++ name mangling, virtual functions or `std::` types across
P/Invoke, so there are three layers.

| Layer | Location | Role |
| :--- | :--- | :--- |
| C++ facade | `NeoSource/NeoScript.h` | The VM's public API. The engine (a C++ host) uses this directly |
| C ABI | `Unity/Native/NeoScriptC.h` `.cpp` | The facade flattened to plain C — the only shape P/Invoke can call |
| C# binding | `Unity/com.neoscript.unity/Runtime/` | A UPM package wrapping the C ABI |

The C ABI lives outside `NeoSource/` for one reason: the engine repository
compiles `NeoSource/*.cpp` wholesale, and the engine uses the C++ facade
directly, so it has no use for this shim. Keeping it there would drag a file
into every engine build that nothing in that build calls.

## Setup

### 1. Build the native plugin

```powershell
pwsh Unity\build_windows.ps1                 # editor + Win64 player
pwsh Unity\build_android.ps1                 # arm64-v8a
```

```bash
./Unity/build_ios.sh                         # macOS with Xcode only
```

Each script copies its output straight into
`Unity/com.neoscript.unity/Runtime/Plugins/`.

**The binaries are not in the repository.** They are build output and are
gitignored, so a fresh checkout has to run the scripts above before opening
Unity.

Importer settings are not committed either — `NeoPluginImportSettings.cs`
applies them in code. Unity deletes a `.meta` whose asset is missing, so
without the binaries there is no way to commit the settings alone.

### 2. Reference the package from a Unity project

Add a local path to the project's `Packages/manifest.json`. Relative paths
resolve against the `Packages` folder.

```json
{
  "dependencies": {
    "com.neoscript.unity": "file:../../path/to/NeoScript/Unity/com.neoscript.unity",
    ...
  }
}
```

Edits made in the repository show up in the editor immediately. Not keeping a
copy is the whole point.

## Minimal use

```csharp
using NeoScript;
using UnityEngine;

// print/error are process-global hooks. Install them once at startup.
NeoRuntime.SetLogSink(Debug.Log, Debug.LogError);

var runtime = new NeoRuntime();
runtime.RegisterObject("Unity", new UnityBridge());   // register before Freeze
runtime.Freeze();

NeoProgram program = runtime.Compile(sourceText, "enemy.ns");
NeoInstance instance = program.CreateInstance();

int hp = instance.Call("GetHp").InvokeInt();
```

Teardown has an order. These are native resources; the GC will not collect them.

```csharp
function?.Dispose();
instance?.Dispose();
program?.Dispose();
runtime?.Dispose();
```

## Exposing a C# object to script

`INeoObject` receives every method through one dispatcher. Implement
`INeoProperties` as well if the object needs fields.

```csharp
sealed class UnityBridge : INeoObject, INeoProperties
{
    public bool Invoke(NeoCallContext ctx, NeoName method)
    {
        if (method.Is("Log"))         { Debug.Log(ctx.ArgString(0)); return true; }
        if (method.Is("SetPosition")) { target.position = ctx.ArgVector3(0); return true; }

        ctx.Fail(1, "unknown method: " + method.ToString());
        return false;
    }

    public bool GetProperty(NeoCallContext ctx, NeoName name)
    {
        if (name.Is("time")) { ctx.Return(Time.time); return true; }
        return false;
    }

    public bool SetProperty(NeoCallContext ctx, NeoName name) => false;
}
```

Script side:

```
Unity.Log("hello");
Unity.SetPosition(math.Vector3(1.0, 2.0, 3.0));
var t = Unity.time;
```

`NeoName` is a UTF-8 view, not a string. The dispatcher runs on every call, so
comparing with `method.ToString()` allocates once per call and piles up garbage
every frame. Compare with `Is()`, which does not allocate.

`Vector2/3/4` and `Quaternion` are VM value types, so they cross to the Unity
types as-is. There is no conversion cost.

## Constraints worth knowing up front

**Only `export`ed declarations are visible to the host.** Functions need
`export fun` and globals need `export var` before `Call` / `TryGetInt` can reach
them. A plain `var` global cannot be found.

**Every call must be finished.** `instance.Call(...)` borrows the instance's
execution context. The `Invoke*` terminals return it automatically, but if you
push arguments and then abandon the call, call `Abandon()`. One instance has at
most one live call at a time (a nested native-to-script call is the exception).

**Readers and builders are valid only inside the scope that issued them.**
`NeoMapReader`, `NeoListReader` and `NeoArrayView` are views over VM storage, so
nothing is copied. Carry one outside its dispatcher call or its `InvokeReadMap`
callback and it points at dead memory. Copy out what you need while inside.

```csharp
int gold = 0;
instance.Call("GetInventory").InvokeReadMap(map => map.TryGet("gold", out gold));
```

**At most 64 native object types can be registered.** The VM's dispatcher is a
bare function pointer with nowhere to carry context, so each type needs its own
fixed thunk, which is what caps the count. Raise `NS_MAX_OBJECT_SLOTS` in
`NeoScriptC.h` and rebuild the native plugin if you need more.

**A VM instance is not thread-safe.** Use one instance from one thread, which in
Unity means the main thread.

**Never let an exception escape a callback.** The binding wraps every
`INeoObject` implementation in try/catch and turns a leak into a script runtime
error, but leaving a reason with `ctx.Fail(code, reason)` and returning `false`
is far better than being swallowed — that is what attaches the script IP, line
and stack trace.

## Keeping the frame

Two handles keep a runaway or infinite script from eating a whole frame.

```csharp
// 1. a per-call ceiling
instance.Call(update).Arg(Time.deltaTime).Timeout(8).Invoke();

// 2. slice a long job across frames
instance.StartSliced("BuildWorld", timeoutMs: 4);
while (instance.UpdateSliced() == NeoRunStatus.Suspended)
    yield return null;
```

## Precompiling to bytecode

`Compile` runs the parser. Baking once at build time and shipping only the image
is the normal arrangement: it loads faster and the source is not packaged
verbatim.

```csharp
// build time (editor)
byte[] image = runtime.CompileToBytecode(source, "enemy.ns");
File.WriteAllBytes(outputPath, image);

// runtime
NeoProgram program = runtime.LoadProgram(bytes);
```

## Resolving imports

`import` is a compile-time include. Unity has no filesystem paths, so supply the
loader yourself.

```csharp
var runtime = new NeoRuntime(new NeoRuntimeOptions
{
    Loader = path =>
    {
        var asset = Resources.Load<TextAsset>("NeoScripts/" + Path.GetFileNameWithoutExtension(path));
        return asset != null ? asset.bytes : null;   // null when not found
    },
});
```

Built-in modules such as `math` go through the loader first as well. Returning
`null` makes the engine fall back to its own implementation, so there is nothing
to special-case.

## Platforms

| Platform | Output | Resolution |
| :--- | :--- | :--- |
| Windows (editor / player, x64) | `NeoScript.dll` | `DllImport("NeoScript")` |
| Android arm64-v8a | `libNeoScript.so` | same |
| iOS arm64 | `libNeoScriptUnity.a` | `DllImport("__Internal")` — dynamic loading is forbidden, so it links statically |

The Android `.so` is stripped in release. Unstripped it carries about 19MB of
debug symbols into every APK.

Using async (`system.async` / http) makes the VM spin up a background thread. If
a script never uses it, no thread is created.

## Rebuilding the native plugin

**Close the editor first.** Unity never releases a native plugin it has loaded
through DllImport, so `NeoScript.dll` cannot be overwritten while the editor is
running.

The usual workaround is to drop the reference with `FreeLibrary`. **This package
does not do that. It was tried and it failed twice, both measured.**

- Releasing the module without a domain reload leaves Mono holding cached
  P/Invoke function pointers into freed memory. The next call jumps there and
  takes the whole editor down.
- Releasing it inside a domain reload does not crash, but the plugin never comes
  back. Unity resolves the plugin name to an already-loaded module handle and
  does not reload it once that handle is dead, so every later call raises
  `EntryPointNotFoundException`.

Iterating on the native side without closing the editor would require the
binding to resolve entry points itself through `LoadLibrary`/`GetProcAddress`
instead of DllImport. That is a design change in the binding layer, not
something a workaround can reach.

## Domain reloads

The package cleans up at the reload boundary (`NeoDomainReloadGuard`). Managed
`NeoRuntime` objects vanish with the domain while the native runtimes do not, so
they are all destroyed just before the reload, and the log callback the native
side holds is detached as well — that callback is the address of a managed
delegate thunk, which dies with the reload while the module keeps pointing at it.

Which means **a component that calls into script every frame has to survive a
reload.** With "Recompile And Continue Playing" a reload can happen mid-play:
the component survives, but the instance it held is dead and its non-serialized
fields are reset to null, and `Awake` is not called again. Check
`instance.IsAlive` and rebuild when it is dead —
`Samples~/Basic/NeoScriptDemo.cs` is shaped that way.

## Verification

The C ABI itself is covered by a smoke test written in plain C. Compiling it as
C is half the test: if C++ ever creeps into the ABI header, it fails here rather
than inside a P/Invoke host.

```powershell
cmake -S . -B build\unity -A x64
cmake --build build\unity --config Release --target neoscript_c_abi_smoke
build\unity\Release\neoscript_c_abi_smoke.exe
```

On the Unity side there are three suites, because each is blind to what the
others catch. The edit-mode smoke test sweeps the whole binding surface.

```powershell
Unity.exe -batchmode -nographics -projectPath <project> `
          -executeMethod NeoScript.Tests.NeoSmokeTest.RunAll -logFile -
```

It returns the failure count as its exit code, so it drops straight into CI. In
the editor it is **Tools > NeoScript > Run smoke test**.

The PlayMode tests see what that one cannot. The edit-mode smoke test finishes
inside a single frame, so it **cannot observe a lifetime problem that only
appears across frame boundaries**. The bug where entering play mode destroyed a
native runtime underneath a live component, raising `ObjectDisposedException`,
was caught here and nowhere else.

```powershell
Unity.exe -batchmode -projectPath <project> `
          -runTests -testPlatform PlayMode -testResults results.xml
```

These live inside the package, so the project's `Packages/manifest.json` needs
the following for Unity to find them.

```json
"testables": [ "com.neoscript.unity" ]
```

The third suite covers the domain reload boundary. Both of the others run
**inside a single domain**, so neither can see cleanup that goes wrong across a
reload. This one uses the plugin before and after one.

```powershell
Unity.exe -batchmode -nographics -projectPath <project> `
          -executeMethod NeoScript.Tests.NeoReloadCycleTest.RunAll -logFile -
```

## When the ABI drifts

`NeoScriptC.h` and `NeoNative.cs` mirror each other. Rather than corrupting
memory quietly, a mismatch fails loudly at startup: struct sizes are compared
(`NeoNativeLayout.Verify`) and the ABI version is checked. When you change the C
ABI, raise `NS_ABI_VERSION` and raise `NeoNative.AbiVersion` on the C# side to
match. The C++ enums and the C constants are pinned by `static_assert` in
`NeoScriptC.cpp`.
