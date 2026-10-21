# Mini game host: two objects, one script

A small C++17 project showing an engine-style NeoScript integration. Two native
objects share one compiled program while keeping their own script globals and
`OnUpdate` callbacks. No engine checkout, graphics SDK, or game assets are needed.

The host simulates 120 fixed ticks at 1/60 second per tick, prints object positions,
and optionally records them for an offline 3D wireframe replay in `viewer.html`.
The viewer replays native output; it does not run NeoScript in JavaScript.

## Run a binary package

Extract the ZIP and open a terminal in its folder:

```powershell
# Windows
.\neoscript_minigame.exe --verify --record scene-data.js
```

```sh
# Linux
./neoscript_minigame --verify --record scene-data.js
```

Open `viewer.html` in a browser to replay the recorded motion, pause, and scrub
through frames. It works offline without a web server. Keep `scene-data.js` beside
`viewer.html`. The Windows standalone build statically links the MSVC runtime.

## Build from source

Requirements: CMake 3.21+, a C++17 compiler, and a C/C++ build toolchain.
Windows: install Visual Studio's **Desktop development with C++** workload.
Linux: install GCC or Clang, CMake, and your generator's build tool.

From the NeoScript repository root:

```sh
cmake -S Samples/minigame -B build/minigame
cmake --build build/minigame --config Release --parallel
ctest --test-dir build/minigame -C Release --output-on-failure
```

Run from the repository root:

```powershell
# Windows
.\build\minigame\bin\neoscript_minigame.exe --verify --record build/minigame/bin/scene-data.js
```

```sh
# Linux
./build/minigame/bin/neoscript_minigame --verify --record build/minigame/bin/scene-data.js
```

Open `build/minigame/bin/viewer.html` for the replay. For Visual Studio, add `-A x64`
to the configure command if you need to select the 64-bit architecture explicitly.
Multi-config and single-config generators both place this executable in `bin/`.

Expected final output with the bundled script:

```text
frame 120 | Red (-0.000, -0.140, -0.500) | Blue (1.000, 0.360, 0.500)
PASS: shared program, two independent instances, 120 updates each
```

Rounding may print `0.000` instead of `-0.000`. `--verify` checks native positions,
the two independent exported counters, and callback execution; failures return a
nonzero exit code. It checks this bundled scenario, not arbitrary user scripts.

## Change the behavior

Edit `scripts/mover.ns` beside the executable and rerun it. No C++ rebuild is needed.
For example, change `speed` or the sine wave's amplitude. Use `--script path/to/file.ns`
to load a different file. When working in the source tree, this avoids editing a build copy:

```powershell
.\build\minigame\bin\neoscript_minigame.exe --script Samples/minigame/scripts/mover.ns
```

Rebuilds copy the source script over the executable's script copy, so keep lasting
edits in the source tree. Omit `--verify` after intentionally changing the scenario.

## Follow the integration

1. `main.cpp`: register global names and native method/property dispatchers, then
   `FreezeBindings()` before compiling.
2. Compile `mover.ns` once into a `ProgramHandle`.
3. Create one `InstanceHandle` per native object. `onInstanceBind` connects that
   instance's `GameObject` and `Services` globals before top-level initialization.
4. The script calls `GameObject.AddEvent("OnUpdate", OnUpdate)`. The host retains
   the `FunctionHandle` returned by `CallContext::argFunction()`.
5. Each host tick calls the callback with `dt`; script writes to `GameObject.Position`
   update the actual C++ object. Call `CollectCycles(false)` once per host tick.
6. Release callbacks, destroy instances, destroy the program, then destroy the runtime.

Native object addresses stay stable throughout the run. Production hosts with
dynamically deleted objects need their own object-ID/liveness scheme. A callback
belongs to its original instance; do not invoke it on a different object instance.
This sample expects synchronous callbacks and rejects suspended/unfinished updates.

### This sample's host API

| Binding | Contract |
| --- | --- |
| `GameObject.Name` | Read-only string: `Red` or `Blue` |
| `GameObject.Position` | Read/write `Vector3`; finite components required |
| `GameObject.AddEvent("OnUpdate", function)` | Replace this object's single per-tick callback; receives float `dt` |
| `Services.Log(string)` | Print one line through the host |

These bindings are implemented by this example, not automatically provided by
the language or the standard console. This is a deliberately small host, not a
complete engine API. The script's `export var updates` lets the host inspect state;
callback registration does not require `export fun`.

Missing files, compile errors, invalid native arguments, and unfinished callbacks
produce diagnostics and a nonzero exit status. Compile/debug source names are
preserved. Run trusted scripts: this example is not a security sandbox.

## Make a distributable ZIP

After the standalone build and tests:

```sh
cpack --config build/minigame/CPackConfig.cmake -C Release -B build/packages
```

The ZIP contains the executable, script, offline viewer, this guide, and license
notices. Generated recordings are not required and are not included. Binary builds
are generated artifacts; do not commit them. CI also prepares a Windows ZIP artifact.
