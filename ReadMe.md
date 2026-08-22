<p align="center">
  <img src="/docs/img/Neo_Icon.png" alt="My Image">
</p>

# Neo Script Documentation
	- The grammar uses a C-like syntax, but it is somewhat similar to Lua script.
	- It was developed in Visual Studio Pro 2026 C++.
	- After some more features are added, port to C#

> **Looking for a specific function?** [`docs/API.md`](docs/API.md) is the complete
> script-side API reference — every `math` / `system` / `coroutine` function and every
> `string` / `list` / `map` / `async` method, with exact signatures and argument types.
> This file covers the language itself and the C++ host API.

### License
	MIT license
	NeoScript is free software.
	The only requirement is that if you do use NeoScript, then you should give us credit by including the copyright notice somewhere in your product or its documentation.

### VS Code extension
	- Extension source: Tools/vscode-neo-script
	- Supports .ns syntax highlighting, IntelliSense completion and signature help, and Debug Adapter Protocol debugging.
	- The debugger runs the currently selected .ns file as a top-level script.
	- Put the code you want to debug in the script body. If you want to debug an exported function, call it from top-level script code.
	- Build Samples/console in x64 Release, then use the "Debug Neo Script" launch configuration.
	- If the adapter executable is not in the default sample path, set neoScript.debugAdapterPath or add adapterPath to launch.json.
	- Set libPath in launch.json when the Neo Script Lib directory is outside the workspace folder.
	- IntelliSense is served by console.exe --lsp. Set neoScript.languageServerPath to that executable when it is not found automatically; it may use the same console.exe as the debugger.
	  Example: "neoScript.languageServerPath": "${workspaceFolder}\\..\\Samples\\console\\x64\\Release\\console.exe"

### Console runner
`Samples/console` is the main sample executable. It can run built-in samples, benchmarks, the VS Code debug adapter, or an arbitrary script file.

Build:
```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Samples\console\console.sln /p:Configuration=Release /p:Platform=x64 /m
```

Run a script file:
```powershell
Samples\console\x64\Release\console.exe --file TestScript\module.ns
```

Other useful commands:
```powershell
Samples\console\x64\Release\console.exe --list
Samples\console\x64\Release\console.exe --run performance
Samples\console\x64\Release\console.exe --bench
Samples\console\x64\Release\console.exe --dap
```

The old standalone `Samples/Neo` runner has been removed. Use `console.exe --file <script.ns>` for the same simple compile-and-run workflow.

#### VS Code debugger setup
1. Build the debug adapter:
```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Samples\console\console.sln /p:Configuration=Release /p:Platform=x64 /m
```

2. Open a folder that contains .ns files in VS Code.

3. Create .vscode/launch.json:
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "neo-script",
      "request": "launch",
      "name": "Debug Neo Script",
      "program": "${file}",
      "cwd": "${workspaceFolder}",
      "libPath": "${workspaceFolder}\\Lib"
    }
  ]
}
```

4. Open a .ns file, set breakpoints, and start "Debug Neo Script".

The VS Code debugger currently supports:
	- Breakpoints
	- Continue, pause
	- Step into, step over, step out
	- Call stack
	- Local variables
	- Watch expressions for visible local variables
	- print output in the Debug Console
	- Runtime exception stop and exceptionInfo

### Numeric precision
The scalar float type `NS_FLOAT` is **`float` (32-bit)**, matching the game engine's native
`float3`/`float4` layout. This lets vector value types (Vec2/Vec3/Vec4/Quaternion) be stored
inline and marshalled to the engine without per-component conversion.

Precision is **not configurable** — there is no double-precision build. Scripts that need more
than 24 bits of mantissa must keep the value in an `int`, or split it. (Earlier versions had a
build switch for `double`; it was removed rather than carried as an untested second layout,
since `sizeof(VarInfo)` and therefore the var stack, every list, and both halves of every map
entry (`MapData`) all depend on it.)

### Integer arithmetic follows C, not the scripting-language convention
`int / int` yields an `int`, so `1 / 100` is `0`. Division promotes to float only when at
least one side already is one.

```
var a = 1 / 100;        // 0
var b = 1.0 / 100;      // 0.01
var c = (n * 1.0) / m;  // float, whatever n and m are
```

This is what C, C++, Java and Go do, but it differs from Lua 5.3+, Python 3 and JavaScript,
where `/` always produces a float. Nothing reports the difference: the result is a valid
number, so there is no error to catch and no warning to read — a ratio meant to be
fractional simply comes out as `0` and whatever it drives stays at its starting value.

Multiply one side by `1.0` when a fraction is intended, particularly when the operands come
from `len()` or a `const`, which are integers and easy to miss.

### Vector value types
`math.Vector2`, `math.Vector3`, `math.Vector4`, and `math.Quaternion` create dedicated
value types. They are not Lists and are passed directly to native engine APIs that read
vectors through `VarInfo::GetVec*`.

```cpp
var position = math.Vector3(1.0, 2.0, 3.0);
var direction = math.Normalize3(position, math.Vector3(0.0, 1.0, 0.0));
var moved = position + direction * 2.0;
```

- Components can be read and written with indexing (`position[0]`, `position[1] = 5.0`).
- Vector assignment is a value copy; assigning `b = a` does not alias `a`.
- Vector arithmetic supports component-wise `+`, `-`, `*`, `/` and scalar multiplication/division.
- `tosize(vector)` returns the component count. List operations (`append`, `insert`, `len`,
  `foreach`) are not supported for vector values.
- The `math` vector and quaternion functions require vector value types, not `[x, y, z]` List
  literals. Full list: [`docs/API.md`](docs/API.md#35-vector-math).
- Quaternion component order is `w, x, y, z`: `math.Quaternion(w, x, y, z)`.
- **There is one vector type, not four.** Internally all of these are `VAR_VEC` plus a component
  count of 1-4; the count lives in unused padding in `VarInfo`, so it costs no memory. A
  quaternion is a 4-component vector — the VM does not track "this is a rotation", and `w,x,y,z`
  is a convention between the script and the engine, not a type. Two consequences: `type()`
  reports `"Vector4"` for a quaternion, and `q1 + q2` is a component-wise add rather than an error
  (quaternion multiplication is `math.quat_*`, which is what you want for composing rotations).
  Arithmetic between different component counts is still rejected.

### Embedding the engine — Host API (`NeoScript.h`)
The public host API is a **3-concept facade: Runtime / Program / Instance**, declared in `NeoScript.h`.
It hides the internal VM (`INeoVM` / `INeoVMWorker`, the execution-context pool, the bytecode image);
host code only sees opaque handles. Embed through this header — the internal `INeoVM` API described in
*Execution context pool* below is not the public surface.

- **Runtime** (`IRuntime`, `CreateRuntime` / `DestroyRuntime`) — owns native bindings and produces programs
  and instances. Register native objects/functions, then call `FreezeBindings()` before compiling.
- **Program** (`ProgramHandle`) — an immutable, shared, refcounted compiled image. One program backs many
  instances. Compilation is split into a build step and a load step:
  - `Compile(source) → program` — the common "compile and run now" path (does both steps internally).
  - `CompileToBytecode(source, out) → Error` — produces only the **bytecode artifact** (no program), for
    when you want to *save* the compile result (offline cache, or a server storing user scripts in a DB).
  - `LoadProgram(bytecode) → program` — turns a saved/streamed bytecode artifact into a runnable program.
- **Instance** (`InstanceHandle`) — one program's per-instance execution state and global variables.

```cpp
#include "NeoScript.h"
using namespace NeoScript;

// 1) Runtime: register native objects, then freeze.
RuntimeDesc rd;
rd.onInstanceBind = [](IRuntime* rt, InstanceHandle inst, void* userData) {
    // Bind a script global (declared by the script) to a native dispatcher for this instance.
    rt->BindGlobalObject(inst, "Game", rt->GetObjectType("GameObjectRef"), userData);
};
IRuntime* rt = CreateRuntime(rd);

NativeObjectDesc od;
od.name = "GameObjectRef";          // dispatcher type (used by BindGlobalObject / setObject)
od.method = &GameObjectMethod;      // bool(CallContext&, StringView method)
od.declareGlobal = false;
rt->RegisterObject(od);
rt->FreezeBindings();

// 2) Program: compile source (or LoadProgram(bytecode) from a cache).
CompileDesc cd; cd.source = scriptText;
CompileResult cr = rt->Compile(cd);
if (!cr.program) { /* inspect cr.error */ }

// 3) Instance: per-instance globals + state. userData is passed to bound object dispatchers.
InstanceDesc idesc; idesc.userData = self; idesc.runGlobalInit = true;
InstanceHandle inst = rt->CreateInstance(cr.program, idesc);
```

To **save a compile** instead of running it now, produce the bytecode artifact and store it; load it later:

```cpp
std::vector<uint8_t> bytecode;
if (rt->CompileToBytecode(cd, bytecode).ok())
    saveToCacheOrDb(bytecode);                     // no program created

// ...later / on another run / on the game server...
ProgramHandle prog = rt->LoadProgram(loadFromCacheOrDb());
InstanceHandle inst = rt->CreateInstance(prog, idesc);
```

**Host → script calls.** Function indices are fixed per program, so cache the `FunctionHandle` once and
call it every frame with no per-call string lookup. `Call(inst, fn)` returns an `Invocation`: push args,
`invoke()`, then read the return.

The **return value lives in the instance's shared, borrowed context**, so reading it must not outlive that
context. Prefer the **safe return terminals** — they detach the result from the Invocation's lifetime:

```cpp
FunctionHandle getScore = rt->FindFunction(cr.program, "GetScore");

// Scalar: invokeR() snapshots the return into an owned CallResult (int/float/bool/string/vec).
CallResult r = rt->Call(inst, getScore).argInt(playerId).invokeR();
if (r.ok()) int score = r.asInt();
// r stays valid even after other Calls on the same instance — safe to hold several at once.

// Collection: invokeReadMap / invokeReadList read inside a callback, where the context is guaranteed
// alive. Copy out what you need; do not keep the reader past the callback.
rt->Call(inst, "GetInventory").invokeReadMap([&](MapReader inv){
    inv.getInt("gold", gold);
});
```

The low-level `invoke()` + `retInt()/retMap()/...` still exist, but those read the **live** shared context
and are invalidated by the next `Call` on the same instance:

```cpp
Invocation a = rt->Call(inst, "GetScore"); a.invoke();
Invocation b = rt->Call(inst, "GetHp");    b.invoke();  // ← flushes a's return context
int score = a.retInt();  // ⚠️ no longer 100 — use invokeR() for this pattern
```

So: **one live `Invocation` per instance** at a time — a second `Call` while one is still armed (built but
not yet invoked) returns a falsy `Invocation`; and a value read with `retX` is only valid until the next
`Call`. Use `invokeR` / `invokeReadMap` to avoid both footguns. (Nested native→script calls *during*
`invoke()` are still allowed.) `FunctionHandle` also carries its owning program, so passing a handle to an
instance of a different program is rejected.

**Captured anonymous functions.** An anonymous function captures only the surrounding function's local
values that it reads; globals retain their normal global lookup. A captured `FunctionHandle` owns its
capture storage, so keep and call the handle itself rather than saving only `FunctionHandle::index`.
On each call NeoScript copies that storage into the lambda's normal local stack slots, then copies the
capture slots back when the call returns (or is unwound/cancelled). This keeps bytecode and ordinary local
access on the VM stack.

Nesting works to any depth: a lambda inside a lambda still sees the outermost local, because every
intermediate lambda captures it as well. Each frame in that chain must itself be an anonymous function —
a *named* function declared between them does not forward outer locals, and the reference is rejected at
compile time with `unknown identifier`.

This is deliberately **value capture with call-boundary write-back**, not Lua/JavaScript shared upvalues.
Two lambdas created from the same outer local do not share later writes, while repeated non-overlapping
calls through the *same* lambda retain its updated captured values. Re-entering the same captured lambda
recursively or through a callback is also defined by that copy-back order: the later return writes its
snapshot last. Three consequences are worth spelling out, because they differ from what Lua or JavaScript
would do with the same source:

```cpp
// 1. Two lambdas over the same local do not share it.
fun MakePair()
{
    var n = 0;
    return [ fun() { n = n + 1; return n; }, fun() { return n; } ];
}
var p = MakePair();
var inc = p[0];   var get = p[1];
inc();   inc();          // 1, 2 — the first lambda's own copy
print(get());            // 0    — the second lambda still holds its creation-time snapshot

// 2. A lambda's writes never reach the frame that created it.
fun Run() { var n = 0; var f = fun() { n = 5; }; f(); print(n); }   // prints 0, not 5

// 3. A lambda cannot name itself: the capture is taken before the assignment completes.
fun MakeBroken()
{
    var self = 0;
    self = fun(var k) { if (k <= 1) return 1; return self(k - 1); };   // `self` captured as 0
    return self;                                                      // "invalid function call"
}
```

Case 1 usually shows up in callbacks that count. `m.sort(fun(var a, var b) { hits = hits + 1; ... })`
accumulates into the comparator's own capture storage; `hits` in the enclosing function never moves.

For shared mutable state — and for recursion — capture a container instead. The capture copies the
*reference*, so every holder sees the same map or list:

```cpp
fun MakePair()
{
    var state = { "n" : 0 };
    return [ fun() { state["n"] = state["n"] + 1; return state["n"]; },
             fun() { return state["n"]; } ];          // reads 2 after two increments
}

fun MakeFact()
{
    var box = {};
    box["f"] = fun(var k) { if (k <= 1) return 1; return k * box.f(k - 1); };
    return box["f"];                                  // f(5) == 120
}
```

The recursion idiom stores the lambda in the very container it captured, which is a reference cycle —
see *Memory: cycles and pool pages are collected separately* below.

`FunctionHandle` is no longer a POD type because captured handles retain an internal reference. Copy, move,
and destroy it normally; do not put it in a `union`, `memcpy` it, or serialize its object bytes. Runtime and
its handles remain thread-confined like the execution context pool; transfer work between threads through
your own synchronization and recreate/call handles on the owning runtime thread.

**Cooperative / time-sliced execution** for long or infinite functions (replaces the old
bind-a-worker-function + per-frame `Run` loop):

```cpp
rt->StartSliced(inst, "run", /*timeoutMs*/0, /*budget*/10);   // 10 instructions per slice
while (rt->IsRunning(inst)) rt->UpdateSliced(inst);           // advance one slice per frame
```

**Native method dispatcher** — one function exposes many script methods; `ctx.userData()` is the
per-instance object bound above:

```cpp
bool GameObjectMethod(CallContext& ctx, StringView method) {
    auto* self = static_cast<GameObject*>(ctx.userData());
    if (method.str() == "AddPos") { self->AddPos(ctx.argInt(0), ctx.argInt(1)); return true; }
    return false;   // unknown method
}
```

**Debugger** is a separate optional interface: `IDebugger* dbg = rt->GetDebugger();` (null in non-debug
builds) — used only by the editor / DAP adapter.

**Logging.** `SetLogHandler(print, error)` installs a **process-global** sink for all script `print`/`error`
and compile diagnostics. `RuntimeDesc.printFn` / `errorFn` share that same global hook (the first created
Runtime's values apply; `SetLogHandler` overrides — last writer wins), so log handlers are not per-Runtime.

```cpp
rt->DestroyInstance(inst);
rt->DestroyProgram(cr.program);
DestroyRuntime(rt);
```

#### Memory: cycles and pool pages are collected separately

Containers are reference counted, and reference counting cannot reclaim a **cycle** — `m["self"] = m`,
or a parent/child pair that point at each other, keeps a non-zero count forever. Whenever a
container's reference count drops without reaching zero it is recorded as a cycle candidate. The host
chooses when to examine those candidates with `CollectCycles`.

**This one is not optional.** Nothing else drains the candidate queue — a host that never calls
`CollectCycles` leaks every cycle its scripts create, silently and forever. Put it in the frame loop
next to your other per-frame housekeeping. A host with no frame (a server, a tool) still needs a
periodic call: once per server tick, not once per script instance.

Captured lambdas are part of the same graph: a lambda's capture storage holds container children
just like a map value does. `m["cb"] = fun() { return m["n"]; }` is a cycle in exactly the way
`m["self"] = m` is, and so is the `box["f"] = fun(...) { ... box.f(...) ... }` recursion idiom.
Passing a capturing lambda to `coroutine.create` extends such a cycle to the coroutine, and a leaked
coroutine pins an entire execution context (a 50K-entry var stack by default) rather than one small
pool slot — a missed `CollectCycles` costs orders of magnitude more there than with plain containers.

`TrimMemory` is a separate, optional concern: returning already-empty pool pages to the OS. It never
examines or collects cycles.

```cpp
// Per-frame or another host-selected safe point: one incremental pass.
rt->CollectCycles(false);  // max(16, ceil(candidateCount * 2%)) candidates

// Loading screen or explicit cleanup point: drain all pending candidates,
// then hand the freed pool slots back to the OS. This order matters.
rt->CollectCycles(true);
rt->TrimMemory(true);

// Optional pool-page reclamation on its own.
rt->TrimMemory(false);   // pages past the hold time, up to SetTrimPagesPerCall() of them
rt->TrimMemory(true);    // every empty page, ignoring the hold time and page budget
```

- `CollectCycles(false)` is the normal incremental call. `CollectCycles(true)` drains the whole
  candidate queue and is not a per-frame call.
- `TrimMemory(false)` returns empty pages whose hold time (default 5 s) has elapsed, at most
  `SetTrimPagesPerCall()` pages (4 by default).
- `TrimMemory(true)` ignores hold time and page budget. It still does **not** touch cycle candidates.
- At an explicit cleanup point, call `CollectCycles(true)` **before** `TrimMemory(true)`. Cycle
  garbage occupies pool slots until it is collected, so trimming first leaves those pages partly
  occupied and returns less than you expect.

**Cost.** One call is a single pass. Every candidate in the budget is traversed together, so an
object reachable from several candidates is visited once for the whole pass, not once per candidate.
Two things are never walked into:

- A container that has never held a container child cannot be part of a cycle, so it is skipped
  outright. A list of coordinate lists, a mesh cache, a table of plain values — none of it enlarges
  the walk, however large it is.
- Objects the VM owns outside the reference graph — modules, running or scheduled coroutines,
  in-flight async requests — are treated as roots and are not expanded. A reference held by one of
  them keeps its target alive without ever being traversed.

What is left is proportional to the number of *interlinked* containers the candidates can reach, and
that still has no fixed ceiling: a candidate embedded in a large graph of containers pays for that
graph. Call it at a host safe point where a variable-cost pass is acceptable, and measure worst-case
frame time rather than the average if your title keeps many containers cross-referenced.

> There is deliberately **no API for registering a standalone global function**. Native bindings go
> through `RegisterObject` only; if you need a global entry point, expose it as an object method
> (`Services.Foo()`). Native async (`beginAsync` / `CompleteAsync`) is likewise absent — the VM
> execution model does not support it.

### Compile-time defines
Host applications can provide C-style compile-time defines through `CompileDesc::defines`
(or a prebuilt `CreateDefineSet` for reuse). This is useful for engine constants such as keyboard codes.

The define table is token based, so the host can prepare values before compilation.
During script compilation, identifiers such as `KEY_LEFT` are replaced before normal parsing.
This does not create a script global variable and has no runtime lookup cost.

```cpp
using namespace NeoScript;

CompileDefine defs[] = {
    { "KEY_LEFT",  DefineKind::Int, "37" },
    { "KEY_RIGHT", DefineKind::Int, "39" },
};

CompileDesc cd;
cd.source  = scriptText;
cd.defines = defs;                     // inline; or use CreateDefineSet + cd.defineSet for reuse

CompileResult cr = rt->Compile(cd);    // rt = an IRuntime from CreateRuntime
```

Script code:
```cpp
if (key == KEY_LEFT)
{
    print("left");
}
```

Supported define kinds (`DefineKind`):
	- `Identifier`
	- `Int`
	- `Float`
	- `String`
	- `Bool`
	- `Null`

### Script `const` (compile-time constants)
Scripts can declare their own compile-time constants with `const`.
Like host defines, a `const` is resolved during tokenization: it creates no runtime
variable, costs nothing at runtime, and participates in constant folding.
Because the name is replaced at compile time, a `const` can never be modified.

```cpp
const BTN_START_GAME = 10;
const GREETING = "Hello";
const HALF = 1.5;
const MY_KEY = KEY_SPACE;                       // alias of a host define
const ATTACK_MASK = GAMEPAD_A | GAMEPAD_B;      // constant expressions are evaluated at compile time

GameObject.AddEvent("OnButtonClick", BTN_START_GAME, StartNewGame);
```

Rules:
	- Allowed only at the global scope (top level of a script).
	- Value must be a compile-time constant expression: literals (`int`, `float`,
	  `string`, `true`, `false`, `null`), other defines/consts, unary `-` `~`,
	  binary `+ - * / % << >> & ^ |`, and parentheses.
	- A `const` is local to the script file that declares it; it is not visible
	  in imported modules (host defines are visible everywhere).
	- Redeclaring a name that is already a const, host define, global variable,
	  or function is a compile error.

### Execution context pool
> **Internal / advanced.** This section describes the low-level `INeoVM` engine and its pool. The public
> host API (see *Embedding the engine* above) hides all of it — the Runtime owns and injects the pool for
> you. Read on only if you work on the engine internals rather than embedding it.

An **execution context** is one runtime stack set: the operand/local var stack + the call stack + the
instruction/stack-pointer registers (internally `NeoExecContext`, formerly a per-worker inline `CoroutineInfo`).

Previously every loaded VM/worker owned its own stacks for its whole lifetime, so N objects cost N large
stacks even while idle. Now a worker does **not** own stacks. It borrows an execution context from a
caller-owned `NeoExecContextPool` only while it is actually running, and returns it when the run finishes.
The default (main) execution and coroutines draw from the **same** pool.

Lifecycle — *borrow on execute / return on complete / retain on suspend*:
- On a top-level run the worker acquires a context from the pool.
- On normal completion (or error) the context is returned to the pool for reuse.
- If the run is **suspended** (breakpoint pause; `sleep`/`yield` in time-limited mode) the context is kept
  (not returned) until the run resumes and completes. A suspended VM therefore holds one context; an idle
  VM holds none. So pool size tracks *concurrently live executions*, not object count.

The pool is **required** — there is no hidden internal fallback. The host owns it and injects it through
`NeoLoadVMParam::execPool`. A pool is thread-confined and has no internal synchronization, so create and use
one pool per thread (e.g. `thread_local`). Reusing an already allocated context takes the no-lock fast path;
growing the pool may allocate memory. A VM inherits the pool of the VM that created it (e.g. modules loaded by
`system.load`/`pcall`), so nested module workers need no separate setup.

```cpp
// One pool per thread. varStackSize = entries in each context's var stack.
NeoScript::NeoExecContextPool* pool = NeoScript::NeoExecContextPool_Create(50 * 1024);

NeoScript::NeoLoadVMParam vparam;
vparam.execPool          = pool;                 // required
vparam.NeoGlobalInterface = myGlobalBind;        // optional

NeoScript::INeoVM* vm = NeoScript::INeoVM::CompileAndLoadRunVM(param, &vparam);
// ... use vm ...
NeoScript::INeoVM::ReleaseVM(vm);

NeoScript::NeoExecContextPool_Destroy(pool);     // after all VMs that used it are released
```

Host entry points:
- `INeoVMWorker::GetExecutionState()` returns `Idle`, `Running`, `SuspendedSleep`,
  `SuspendedDebugger`, or `SuspendedSlice`. It is derived from the live worker context, so it cannot
  drift from the actual execution state.
- `INeoVM::CompileAndLoadRunVM` / `CompileAndLoadVM` — load a program and run its script body.
- `INeoVMWorker::ExecuteTop(fid, args)` — run a function as a fresh top-level execution.
  Returns `NeoExecStatus`: `NEOEXEC_COMPLETED`, `NEOEXEC_SUSPENDED`, or `NEOEXEC_ERROR`.
- `INeoVMWorker::ResumeTop()` — continue a suspended top-level execution.
- `INeoVMWorker::IsSuspended()` — a retained (suspended) execution is pending. A per-frame host loop should
  do `if (w->IsSuspended()) w->ResumeTop(); else w->ExecuteTop(fid, args);` so a breakpoint/sleep resumes
  instead of restarting.

This is the low-level `INeoVM` surface. Host code should prefer the `IRuntime` API above
(`Call` / `Invocation` / `StartSliced`), which manages context acquisition and nesting for you.

#### Synchronous native-to-script callbacks

`Script A -> native API -> Script B` is a **synchronous nested call**. The native API must receive Script B's
result before it can return to Script A, so Script B does not own a resumable execution context.

- Breakpoints, stepping, and a requested debugger pause are suppressed while Script B is running. Debug Script A
  before the native call, or debug Script B through a top-level entry point instead.
- `sleep(...)`, `yield`, `async.get(...)`, `async.post(...)`, and `async.wait()` are rejected with a runtime error in
  Script B. A worker configured for time-limited execution cannot start Script B at all. These restrictions prevent
  Script B from being suspended after its native caller has already returned to Script A.
- A synchronous callback may call ordinary functions and return values normally. For delayed work, have the native
  API schedule a new top-level `ExecuteTop`/`ResumeTop` execution instead of invoking Script B synchronously.

> Note: `NeoExecContextPool_Create` returns an opaque handle; its full type lives in the internal headers.
> Only the pointer, the two factory functions, and `NeoLoadVMParam::execPool` are part of the public API.

### Performance test results

Sources: [`Samples/bench/bench.ns`](Samples/bench/bench.ns) · [`bench.lua`](Samples/bench/bench.lua) ·
[`bench.cpp`](Samples/bench/bench.cpp) — all three implement the **same algorithm**, statement for statement.

```
CPU   : Intel Core i7-12700F (12C/20T)      RAM : 64 GB
OS    : Windows 11 Pro 64-bit (build 26200)
Neo   : NeoScript (console.exe, Release x64)
Lua   : Lua 5.5.0 (official Win64 binary)
C++   : MSVC 19.51 /O2 /std:c++17 (x64)
```

Lower is better. **ms**, best of 5 in-process repetitions, minimum across 7 interleaved runs of the
three languages. `x Neo` = how many times faster than Neo (so `1.22x` means Lua finished in 82% of
Neo's time; `0.88x` means it was slower than Neo).

| Benchmark | What it stresses | Neo (ms) | Lua (ms) | C++ (ms) | Lua vs Neo | C++ vs Neo |
| :-------- | :--------------- | -------: | -------: | -------: | ---------: | ---------: |
| `loop_sum`      | integer loop, VM dispatch floor | **167** | 192 |  12.8 | 0.87x | 13.1x |
| `float_math`    | float mul/add/sub chain         | **183** | 307 |  58.3 | 0.60x |  3.1x |
| `func_call`     | script function call overhead   | **115** | 157 |   4.2 | 0.73x | 27.4x |
| `fib_recursive` | recursion, fib(32)              |  **78** |  88 |   6.7 | 0.89x | 11.6x |
| `array_rw`      | sequential array write + read   |  **43** |  46 |   2.4 | 0.93x | 17.8x |
| `map_str`       | string-key hash lookup          |  46 |  **37** |  71.5 | 1.24x |  0.6x |
| `string_ops`    | string build + length           |  **82** | 148 |  13.5 | 0.55x |  6.1x |
| `particles`     | game-style float + array sim    |  53 |  **49** |   3.4 | 1.08x | 15.8x |
| **total**       |                                 | **767** | 1024 | 172.7 | 0.75x |  4.4x |

**Reading the numbers.**
- **Neo is ~33% faster than Lua overall** and leads on 6 of 8 benchmarks. All 8 checksums match
  across the three languages, which is what proves they did the same work.
- `map_str` is the row Lua wins clearly: 1.24x on the minimum, **1.15x on the median** (Neo 46,
  Lua 40). Lua interns *every* short string, so a table lookup is a pointer compare. Neo interns
  only map/set keys and program constants, which keeps temporary string creation cheap at the cost
  of this one case.
- `map_str` has only 8 keys, so which slot each key lands in — and therefore the score — shifts with
  any change to the hash. Lua is also the noisier side here (37-41 ms across runs, against Neo's
  46-48). Treat differences under ~10% on this row as noise.
- `particles` reads as a 1.08x Lua win here, but that is inside the layout band described under
  *Methodology* below — the same source has measured anywhere from 44 to 59 ms across builds. Treat
  this row as a tie, not a loss.
- C++ is a **reference ceiling**, not a peer: 3-27x faster on compute-bound loops. The exception is
  `map_str`, where `std::unordered_map<std::string,…>` is *slower* than both VMs — the interpreters
  cache the string hash; the C++ map rehashes on every lookup.

#### Methodology

1. **Identical algorithm.** The three files perform the same operations, in the same order, on the
   same data structures. Checksums must match across languages — that is what proves they did the
   same work.
2. **Self-timed.** Each language times only the measured region with its own clock
   (`system.clock` / `os.clock` / `steady_clock`), so process start-up and compilation are excluded.
3. **Best of 5** in-process repetitions after a warm-up run, then the minimum across 7 runs of the
   whole suite, with the three languages interleaved inside each run so machine drift cannot land
   on one language only.
4. **Compare within one run, not across runs.** Re-measuring on a differently loaded machine moved
   *Lua's* numbers by 20-30% with its source untouched. Every figure above comes from one session.
5. **A/B any change.** Build-to-build variation on this suite is large and it is *not* measurement
   noise — it is code layout. Deleting cold code from the interpreter, or anything else that moves
   the hot blocks, reshuffles branch-target alignment and uop-cache packing. Measured across five
   builds whose float-arithmetic source was byte-identical, an isolated float loop ranged 0.030 s to
   0.042 s (40%), while an integer loop stayed inside 0.013-0.014 s. `float_math` and `particles` are
   the two rows that feel this most; `loop_sum` and `array_rw` are the stable ones.

   So: accept a change only when two builds measured alternately in one session agree, and treat a
   move of under ~10% on `float_math`, `particles` or `map_str` as layout, not as a real regression.
   The durable fix is PGO (`/LTCG:PGINSTRUMENT` → run the bench → `/LTCG:PGOPTIMIZE`), which makes
   block placement deliberate instead of accidental.

All compiler and VM changes are covered by the 2,785-case regression suite (`console.exe --smoke`).

### Sample
	- console / hello: prints "hello"
	- console / callback: calls functions between C++ and Neo Script
	- console / map_callback: invokes C++ functions and reads variables through a map
	- console / 9_times: prints multiplication tables from 1 through 9
	- console / string: string feature examples
	- console / list: list examples, including matrix operations
	- console / map: map feature examples
	- console / contailer: list, set, and map examples
	- console / slice_run: executes a script for a fixed slice and resumes it later
	- console / time_limit: executes a script for a fixed time budget and resumes it later
	- console / divide_by_zero: divide-by-zero exception handling
	- console / delegate: function pointer examples
	- console / meta: meta function examples
	- console / coroutine: coroutine examples
	- console / module: module import and usage

### var data structure
	- null: represents no value; uninitialized variables are null
	- bool: stores true or false
	- int: stores a 4-byte integer
	- double: stores a 4-byte float (`NS_FLOAT`)
	- string: stores text
	- list: an array-like container
	- map: a key/value container
	- set: a key-only container


### Neo Script reserved words
	- var: declares a variable
	- fun: declares a function. `fun(...) { ... }` without a name is an anonymous function; it
	  captures the enclosing function's locals **by value** — see "Captured anonymous functions"
	  in the Host API section for what that does and does not share
	- import: imports a module from the Lib directory
	- export: makes a variable or function available to C++
	- tostring (x) / toint (x) / tofloat (x) / tosize (x) / type (x) / sleep (x):
	  built-in intrinsics. Return types and exact semantics: docs/API.md section 1
	- return [x]: returns from the current function, optionally with x
	- break: exits the current loop
	- continue: starts the next loop iteration
	- if (x) / else / else if: C-style conditional chain; the legacy `elif` syntax is removed
	- switch / case / default: see "switch statement" below
	- for: `for (var a in start, end [, step])` — see "for loop" below. `end` is **exclusive**
	  and `step` is optional (defaults to 1)
	- foreach: iterates a map, list, or set
		- map: `foreach (var key, value in map)` or `foreach (var key in map)`
		- list / set: single variable only, `foreach (var value in list)`
		  (two-variable form is not supported and reports a runtime error)
	- true / false: boolean values
	- null: no value
	- ++ / --: increments or decrements a variable by one
	- && / ||: logical operators, equivalent to C semantics (short-circuit evaluation:
	  the right operand is not evaluated when the result is already decided)
	- & / | / ^ / << / >>: bitwise operators, equivalent to C semantics and precedence
	  (`&` binds tighter than `^`, which binds tighter than `|`)
	- > / < / >= / <=: comparison operators, equivalent to C semantics
	- x..y: converts x and y to strings and concatenates them

### for loop
```cpp
for (var i in 0, n)        // step omitted -> 1
    total += i;

for (var i in 0, n, 2)     // every other element
    total += i;

for (var i in n, 0, -1)    // counts down: n-1 … 1
    total += i;
```

- `end` is **exclusive**: `for (var i in 0, 5)` runs with `i` = 0,1,2,3,4.
- `step` is optional and defaults to `1`, matching Lua/Python. Both `for (var i in 0, n)` and
  `for (var i in 0, n, 1)` are accepted and compile identically.
- `step` may be **negative** to count down. Direction is decided at run time from the sign of
  `step`, so `begin`/`end`/`step` can all be runtime values (function arguments, fields, …),
  which the compiler cannot inspect.
- If the direction contradicts the range the loop simply **runs zero times** — `for (var i in 0, 5, -1)`
  does not spin forever.
- `step == 0` raises the runtime error `'for' step is zero` instead of looping forever. This is
  checked for variables too, not just literals.

### switch statement
`switch` dispatches on a value through a compile-time table, so matching cost does not
grow with the number of cases.

```cpp
switch (command)
{
case CMD_FIRE:          // const / compile-time constant expressions are allowed
    Fire();
case 2, 3:              // one body can match several values
    MoveTo(command);
case "reload":
    Reload();
default:
    Idle();
}
```

Rules:
	- `case` values must be compile-time constants of type `bool`, `int` or `string`.
	  Constant expressions and `const` / host defines are allowed (`case 1 + 2:`, `case CMD_FIRE:`).
	- **`float` is not allowed as a case value** and is a compile error, because matching would
	  depend on exact floating-point equality. Use `int` or `string` keys instead.
	- Matching is a **strict type match**: `true != 1`, `"1" != 1`.
	- Strings compare as exact UTF-8 bytes.
	- Duplicate case values and a duplicate `default` are compile errors.
	- `default` is optional and may appear anywhere. With no match and no `default`,
	  execution continues after the switch.
	- **There is no fallthrough.** Each case body jumps to the end of the switch when it finishes.
	- `break` exits the innermost switch only; inside a loop it does not exit the loop.
	- `continue` still applies to the innermost loop, and `return` behaves as usual.
	- If the switch value is a type that cannot be a case key (float, map, list, vector, null, ...),
	  the `default` branch is taken.

### Built-in functions
Every function a script can call - `print`, the `math` / `system` / `coroutine` modules, and the
`string` / `list` / `map` / `async` method sets - is listed with exact signatures and argument
types in [`docs/API.md`](docs/API.md). That file is generated from `NeoSource/NeoLib.cpp`, so it
stays complete; this one deliberately does not repeat it.

### Comment
	- //: single-line comment
	- /* */: multi-line comment

#### Neo Script Debugger Image
![](/docs/img/code_debugger.png)

### Benchmark code

All three files run the **same eight benchmarks with the same algorithm**; only the syntax differs.
Each returns a checksum that must match across languages.

- [`Samples/bench/bench.ns`](Samples/bench/bench.ns) — NeoScript
- [`Samples/bench/bench.lua`](Samples/bench/bench.lua) — Lua
- [`Samples/bench/bench.cpp`](Samples/bench/bench.cpp) — C++ (build with `build_cpp.bat`)

Run: `console.exe --file Samples/bench/bench.ns`

### YoutTube
[![YoutTube](https://i9.ytimg.com/vi_webp/iEpmlLcxBVg/mq2.webp?sqp=CLTL6L8G-oaymwEmCMACELQB8quKqQMa8AEB-AH-CYAC0AWKAgwIABABGEkgZSg2MA8=&rs=AOn4CLAwiLyh_R2B056F-Eej9t5PiuJhmw)](https://www.youtube.com/watch?v=iEpmlLcxBVg)
