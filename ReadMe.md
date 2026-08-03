<p align="center">
  <img src="/docs/img/Neo_Icon.png" alt="My Image">
</p>

# Neo Script Documentation
	- The grammar uses a C-like syntax, but it is somewhat similar to Lua script.
	- It was developed in Visual Studio Pro 2026 C++.
	- After some more features are added, port to C#

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
The scalar float type `NS_FLOAT` is **`float` (32-bit) by default**, matching the game
engine's native `float3`/`float4` layout. This lets vector value types (Vec2/Vec3/Vec4/
Quaternion) be stored inline and marshalled to the engine without per-component conversion.

Define `NS_DOUBLE_PRECISION` at build time to use `double` instead. Note this changes the
numeric results of scripts and the memory layout of numeric values, so pick one precision
per build. (Previously the default was `double`, opted into `float` via `NS_SINGLE_PRECISION`;
the default is now reversed.)

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
- `tosize(vector)` returns the component count. List-only operations such as `append`,
  `insert`, `len`, and `foreach` are not supported for vector values.
- Vector math APIs (`Lerp3`, `Normalize3`, `Cross3`, `DistanceSquared3`,
  `RotateVectorByQuat`, and quaternion helpers) require vector value types, not `[x, y, z]`
  List literals.
- Quaternion component order is `w, x, y, z`: `math.Quaternion(w, x, y, z)`.

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

**Cooperative / time-sliced execution** for long or infinite functions (replaces the old
`BindWorkerFunction` + per-frame `Run` loop):

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

> Some `IRuntime` members are intentionally not implemented yet and are marked `[미구현]` in the header
> (`RegisterFunction`, `ResetInstance`, `Cancel`, the `async` family, `CallContext::fail`,
> `Invocation::error`). They return `false` / a no-op until supported; don't rely on them.

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
- `INeoVMWorker::GetExecutionState()` returns `Idle`, `Running`, `SuspendedSleep`, or
  `SuspendedDebugger`. It is derived from the live worker context, so it cannot drift from the actual
  execution state.
- `INeoVM::CompileAndLoadRunVM` / `CompileAndLoadVM` + `INeoVM::PCall` — run the script body (top level).
- `INeoVMWorker::ExecuteTop(fid, args)` — run a function as a fresh top-level execution.
  Returns `NeoExecStatus`: `NEOEXEC_COMPLETED`, `NEOEXEC_SUSPENDED`, or `NEOEXEC_ERROR`.
- `INeoVMWorker::ResumeTop()` — continue a suspended top-level execution.
- `INeoVMWorker::IsSuspended()` — a retained (suspended) execution is pending. A per-frame host loop should
  do `if (w->IsSuspended()) w->ResumeTop(); else w->ExecuteN(fid, args...);` so a breakpoint/sleep resumes
  instead of restarting.
- `Call` / `CallN` / `iCall` / `iCallN` (host → script function) auto-acquire a context when the VM is idle
  and return it when done; when called from inside a running script (native callback) they reuse the current
  context (nested call).

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

Lower is better. **ms**, best of 5 runs after a warm-up. `x Neo` = how many times faster than Neo
(so `1.22x` means Lua finished in 82% of Neo's time; `0.88x` means it was slower than Neo).

| Benchmark | What it stresses | Neo (ms) | Lua (ms) | C++ (ms) | Lua vs Neo | C++ vs Neo |
| :-------- | :--------------- | -------: | -------: | -------: | ---------: | ---------: |
| `loop_sum`      | integer loop, VM dispatch floor | **226** | 192 |  12.5 | 1.18x | 18.1x |
| `float_math`    | float mul/add/sub chain         | **203** | 312 |  57.8 | 0.65x |  3.5x |
| `func_call`     | script function call overhead   | **133** | 156 |   4.2 | 0.85x | 31.7x |
| `fib_recursive` | recursion, fib(32)              |  **86** |  88 |   6.7 | 0.98x | 12.8x |
| `array_rw`      | sequential array write + read   |  **50** |  46 |   2.4 | 1.09x | 20.8x |
| `map_str`       | string-key hash lookup          |  **48** |  41 |  70.3 | 1.17x |  0.7x |
| `string_ops`    | string build + length           |  **76** | 151 |  13.3 | 0.50x |  5.7x |
| `particles`     | game-style float + array sim    |  **59** |  49 |   3.3 | 1.20x | 17.9x |
| **total**       |                                 | **881** | 1035 | 170.5 | 0.85x |  5.2x |

**Reading the numbers.**
- **Neo is ~15% faster than Lua overall**, but the totals matter far less than the spread.
  Neo leads on `string_ops` (2.00x), `float_math` (1.54x) and `func_call` (1.17x); `fib_recursive`
  is a tie. Lua still leads on `particles` (1.20x), `loop_sum` (1.18x), `map_str` (1.17x) and
  `array_rw` (1.09x). Container indexing used to be the widest gap and is now the narrowest
  (`map_str` 1.35x → 1.17x, `particles` 1.29x → 1.20x, `array_rw` 1.13x → 1.09x) after the two
  container changes below. What is left on those four is the **dispatch floor**, not the handlers —
  see the note on interpreter dispatch below.
  All 8 checksums match across the three languages, which is what proves they did the same work.
- C++ is a **reference ceiling**, not a peer: it is 3-33x faster on compute-bound loops.
  The exception is `map_str`, where `std::unordered_map<std::string,…>` is actually *slower*
  than both script VMs — both interpreters intern their strings and cache the hash, while the
  C++ map rehashes the key and chases a pointer on every lookup.
- **Compare within one run, not across runs.** Re-measuring on a differently loaded machine moved
  *Lua's* numbers by 20-30% with its source untouched, so absolute milliseconds from separate
  sessions are not comparable. Every figure in the table above comes from a single session.

#### What these benchmarks found (and fixed)
Building the suite paid for itself several times over. It exposed two **code-generation defects**
(1-2), where the evidence is the emitted bytecode rather than a stopwatch — instruction counts
don't drift with machine load. It then exposed structural properties of the **interpreter loop**
(3-4, 6-7) and of the **value representation** (5), where the evidence *is* the stopwatch, so each
of those claims is an A/B measurement of two full rebuilds run interleaved in one session:

1. **Boolean materialization in conditions.** `&&` / `||` emitted correct short-circuit jumps and
   *then* built a `true`/`false` value that the enclosing `if` immediately re-tested — so
   `if (a || b)` compiled worse than the equivalent `else if` chain. Conditions are now compiled
   in branch context: the operator hands its false-jump list to the `if`, which patches it
   directly. Comparisons inside `&&`/`||` are also fused into single compare-and-jump ops
   (`JLS`, `JLE`), an optimization single conditions already had.
   **The loop body of a two-clause `if` went from ten instructions to five.**
2. **A missing entry in a peephole table.** Assigning a temporary into a variable is normally
   elided by retargeting the producing instruction's destination, but the bitwise ops
   (`NOP_AND` / `NOP_OR`) were absent from that table, so every `var x = a & b;` left a dead
   `MOV`. **One instruction removed per bitwise assignment.**

3. **Keeping the hot path small matters more than saving an instruction.** Teaching `for` to honor
   a negative `step` (see *for loop* below) added a sign test and an error branch to `For()`, which
   is `__forceinline`d into the dispatch loop. That alone slowed *every* benchmark that loops by
   8-19% — far more than the added work explains. Moving the rare cases (negative step, step 0)
   into a `__declspec(noinline)` helper so the inlined body stays tiny recovered all of it and
   then some: `float_math` −11%, `map_str` −10%, `particles` −7% **versus the original code**.
   In a bytecode interpreter the size of the inlined dispatch body is itself a performance
   parameter.
4. **The same split, applied to arithmetic, was worth 8% overall.** Each of the eight arithmetic
   handlers (`Add3`/`Sub3`/`Mul3`/`Div3` and the compound-assign `Add2`…`Div2`) inlined 42-72 lines
   into the dispatch loop: the int/float cases that virtually all code takes, *followed by* string
   concatenation, vector math, metatable dispatch, list merging and error formatting. Splitting each
   one so only the four int/float combinations stay inlined and everything else moves to a
   `noinline` `…Rare` helper — **a pure code move, no semantic change** — cut the total from
   970 ms to **891 ms**: `float_math` −14%, `fib_recursive` −9%, `loop_sum` −8%,
   `array_rw` −7%. A scalar-arithmetic microbenchmark dropped 25%. Note that `loop_sum` and
   `fib_recursive` sped up too, which no single handler explains — shrinking the dispatch switch
   helps every opcode through the instruction cache, not just the ones that were edited.
5. **`sizeof(VarInfo)` is multiplied by everything.** Vector value types (`Vector2`…`Quaternion`)
   used to store their four floats *inline* in the `VarInfo` union. Sixteen bytes of payload forced
   the whole struct from 16 to **24 bytes** — and `VarInfo` is the element type of the var stack,
   of every list, and of both halves of every map node (a map node was 60 bytes; Lua's is 24).
   Moving the components behind a pooled `VecInfo*` brought `VarInfo` back to 16 bytes and cut the
   suite by **7.5%**: `particles` −23%, `func_call` −12%, `float_math` −11%, `map_str` −11%.
   The cost is paid where vectors are *created*: a vector microbenchmark got 18% slower overall,
   concentrated in construction (`math.Vector3(...)` +23%) and copy-then-write (+50%). Two things
   keep that bill down — assignment shares the storage and only copies on mutation
   (copy-on-write, so `var b = a` stays free and value semantics are preserved), and a store that
   is already singly-owned is reused in place, which makes the common `acc = acc + delta`
   accumulation allocate nothing at all (measured at ±0%). Reading components even got *faster*
   (−9%), because the smaller `VarInfo` outweighs the extra indirection.
6. **Indexing a list went through two function boundaries.** `list[i]` compiled to a single
   opcode, but the handler called `ListInfo::GetValue()` in another translation unit, which in turn
   reached the value copy through the VM pointer (`_pVM->Move`). Inlining the integer-index case
   directly into the read/write handlers — bucket access with a single unsigned range check, using
   the worker's already-inlined `Move` — is again **a pure code move**: `particles` −28%,
   `float_math` −12%, `array_rw` −9%, with the non-container benchmarks unchanged, exactly as the
   change predicts. This is the same lesson as items 3 and 4 seen from the other side: what the
   dispatch loop *cannot* inline costs as much as what it inlines too much of.
7. **Item 4's split, applied to container indexing.** `CltRead` / `CltInsert` still inlined every
   collection kind into the dispatch loop — property-backed maps, vector component access, the
   string-indexer path, and two `SetErrorFormat(…, GetDataType(…).c_str())` calls whose `std::string`
   temporary is constructed and destroyed inline. Only `list[int]` and `map[string]` stay inlined
   now; the rest moved to `noinline` `CltReadRare` / `CltInsertRare`. Every container benchmark
   improved (`particles` −2%, `array_rw` −2%, `map_str` −2%) while the three benchmarks that touch
   no container regressed by the same order (`loop_sum` +3%, `func_call` +2%, `fib_recursive` +2%),
   which is the I-cache budget being *moved* rather than saved. It was adopted anyway: the engine
   workload this VM exists for is container-heavy, and that is the side of the trade worth buying.
   It is the one change here justified by target workload rather than by the suite total.

Ideas that did *not* survive measurement:

- **A dedicated opcode for the common `step == 1` loop.** The premise was that skipping the
  step-slot read would pay off, but `[begin][end][step]` are adjacent and already in cache, and
  the sign test predicts perfectly — the specialized op measured **4-5% slower** than the general
  one (reproduced over three runs), so it was removed. Fewer opcodes also keeps stored bytecode
  compatible.
- **A separate loop-entry opcode** to pre-validate `step` once per loop. Making `For()` itself
  sign-aware removed the infinite-loop cases for free, leaving only `step == 0` to handle, so the
  extra opcode and its jump patching were dropped.
- **Branchless operand fetch.** Each opcode tests an `argFlag` bit per operand to pick the local or
  the global base, up to three times — about 35 branches per particle-loop iteration, where Lua's
  `R[A] = base + A` has none. Rewriting the fetch to select the base pointer with a ternary so it
  compiles to `cmov` measured **exactly at baseline**: MSVC was already emitting `cmov` for the
  two-return form. Reverted.
- **Reordering the `switch` cases** so the hottest opcodes sit next to each other in source. Case
  order does not affect the jump table (it is indexed by opcode value) but it does decide code
  layout, so moving `CLT_READ` / `CLT_MOV` up beside the arithmetic handlers — away from the bulky
  `CALL` / `STR_ADD` / `TO*` bodies they currently sit behind — looked free. It was a clear
  **regression**: `particles` +8%, `map_str` +22%. MSVC's own layout was better than the guess.
  Worth recording that layout sensitivity of this size exists at all: a two-line reordering with no
  semantic change moved one benchmark by a fifth.

#### The remaining gap is dispatch, not handlers
Three consecutive attempts to close the last ~20% on `particles` came back at noise or worse (the
last two entries above, plus the container split of item 7 which only moved the cost around). That
is a result in itself, and disassembling the benchmark's Lua build explains it.

Counting the emitted bytecode for the identical inner loop, **Neo issues 15 opcodes to Lua's 19** —
Neo fuses each comparison into a single compare-and-jump (`JLS` / `JLE`) where Lua spends an `LTI`
plus a `JMP`. Neo executes *fewer* instructions and still finishes slower, so the cost is per
opcode: roughly **2.0 ns for Neo against 1.3 ns for Lua**.

The reason is the dispatch structure. `lua55.dll` in this comparison is a **MinGW-w64 / GCC** build
(GNU ld 2.28, imports `msvcrt.dll`, carries `GCC:` / `libgcc` / `__mingw` strings), and under
`__GNUC__` Lua's `lvm.c` enables `LUA_USE_JUMPTABLE` — computed-goto **direct threading**. The
disassembly confirms it: 54 register-indirect jumps clustered in `luaV_execute`, where a
switch-based build would have one. Every opcode gets its own indirect branch and therefore its own
branch-predictor history, instead of all of them sharing a single one at the top of the loop.

MSVC has no labels-as-values, so NeoScript cannot express that dispatch at all. On this toolchain
the interpreter is at its structural ceiling, and further micro-optimization of individual handlers
is not where the remaining difference lives. The one path that would change the shape of the curve
is building the VM translation unit with `clang-cl` and enabling a threaded dispatch under
`#if defined(__clang__)`; that is a toolchain decision, not a code one, and has not been attempted.

All compiler changes are covered by the 2,783-case regression suite plus dedicated correctness
tests: 43 cases for logical operators ([`TestScript/logic_ops.ns`](TestScript/logic_ops.ns)) and
26 for loops ([`TestScript/for_loop.ns`](TestScript/for_loop.ns)).

#### Methodology
Micro-benchmarks are easy to get wrong, so the harness is explicit about it:

1. **Identical algorithm.** The three files perform the same operations, in the same order,
   on the same data structures. No language-specific shortcuts.
2. **Self-timed.** Each language times only the measured region with its own clock
   (`system.clock` / `os.clock` / `steady_clock`), so process start-up and compilation are excluded.
3. **Warm-up + best-of-5, interleaved.** The best run is reported, which rejects scheduler noise.
   When two builds are compared they are run round-robin in one session (`old, new, old, new, …`)
   so any drift in machine load lands on both. Comparing builds also means **full rebuilds** —
   an incremental LTCG link after a header change can produce a binary that is both slower and,
   in one case, outright corrupt.
4. **Checksum-verified.** Every benchmark returns a checksum and all three languages must print
   the *same* value — this is what proves they really did the same work. All 8 checksums match.
5. **Binary-exact floats.** Constants are powers of two (`0.5`, `0.25`, `1/64`) and accumulators are
   kept under 2^24, so NeoScript's default **float32** scalar and Lua/C++'s **double** produce
   bit-identical checksums. (Without this, a large float accumulation alone drifts them apart.)
6. **Optimizer-proofed C++.** A naive port lets MSVC delete the work outright — `fib` folded to a
   constant and `float_math` reported 0 ms. The C++ file therefore reads its loop bounds through
   `volatile`, and each benchmark's inner value depends on the previous result, so the loops cannot
   be hoisted, closed-form-solved, or CSE'd away. The numbers above are of code that actually ran.

To reproduce: build `Samples/console` in Release x64, run `build_cpp.bat`, then

```powershell
.\Samples\bench\bench_cpp.exe
lua55.exe Samples\bench\bench.lua
.\Samples\console\x64\Release\console.exe --file Samples\bench\bench.ns
```

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
	- double: stores a 4-byte float by default (8-byte when `NS_DOUBLE_PRECISION` is defined)
	- string: stores text
	- list: an array-like container
	- map: a key/value container
	- set: a key-only container


### Neo Script reserved words
	- var: declares a variable
	- fun: declares a function
	- import: imports a module from the Lib directory
	- export: makes a variable or function available to C++
	- tostring (x): converts x to a string
	- toint (x): converts x to an integer
	- tofloat (x): converts x to a floating-point value
	- tosize (x): 
		x is a string: returns its length
		x is a list, map, or set: returns its element count
		otherwise: returns 0
	- totype (x): returns the container type of x as a string
	- sleep (x): pauses execution for x milliseconds; 1000 is one second
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

### Built-in system function use system.
	## Basic
	- print (x): prints x as a string

	## system
	- clock (): returns the current time
	- load (): compiles and loads a script, then returns it as a module
	- pcall (x): executes module x
	- meta(x, y): binds meta function y to variable x
	- set(x): converts list x to a set
	
	## coroutine
	- create (): creates and returns a coroutine in suspended mode
	- resume (x): activates coroutine x
	- status (x): returns the coroutine status as a string
	- close ([x]): closes a coroutine

	## math
	- abs (x): returns the absolute value of x
	- acos (x), asin (x), atan (x), ceil (x), floor (x), round (x)
	- sin (x), cos (x), tan (x), log (x), log10 (x), exp (x)
	- pow (x, y), sqrt (x), srand (x), rand (): equivalent to their C library counterparts
	- deg (x): converts radians to degrees
	- radian (x): converts degrees to radians

	## string
	- len (): returns the string length
	- find (x): returns the index of string x
	- sub (x, y): returns y characters beginning at index x
	- upper (): converts lowercase English letters to uppercase
	- lower (): converts uppercase English letters to lowercase
	- trim (): removes whitespace from both ends of a string
	- ltrim (): removes leading whitespace
	- rtrim (): removes trailing whitespace
	- replace (x, y): replaces x with y
	- split (x): splits on x and returns a list

	## list
	- len (): returns the number of list items
	- resize (x): changes the list item count
	- append (x, [y]): appends x; y optionally specifies the position
	- broadcast (x): returns the element-wise sum of two matrices
	- multiply (x): returns the matrix product of two matrices
	- dot (x): returns per-row dot products as a list
	- sum (): returns the sum of all matrix elements

	## map
	- len (): returns the number of map items
	- reserve (x): reserves capacity for x items without changing the item count
	- sort (): sorts map values
	- keys(): returns map keys in a list
	- values(): returns map values in a list

	## set
	- len (): returns the number of set items

### Comment
	- //: single-line comment
	- /* */: multi-line comment

#### Neo Script Debugger Image
![](/docs/img/code_debugger.png)

### Benchmark code

All three files below run the **same eight benchmarks with the same algorithm**; only the syntax
differs. Each returns a checksum that must match across languages (see *Methodology* above).
The measuring harness (warm-up, best-of-5, reporting) is omitted here for brevity — see the full
files in [`Samples/bench/`](Samples/bench/).

#### NeoScript — [`Samples/bench/bench.ns`](Samples/bench/bench.ns)
```cpp
// ---------------------------------------------------------------- 1. 정수 루프
// VM 디스패치의 순수 비용. inner 합 = 12,497,500 (float32 정확 범위)
fun LoopSum(var outer, var inner)
{
    var last = 0;
    for (var o in 0, outer, 1)
    {
        var bias = last & 1;           // 직전 결과에 의존(진짜 데이터 의존성) → 축약 불가
        var sum = 0;
        for (var i in 0, inner, 1)
            sum += i + bias;
        last = sum;
    }
    return last;
}

// ------------------------------------------------------------- 2. 부동소수 산술
// mul/add/sub 혼합. 상수·피연산자를 모두 **이진 정확값**(1/2, 1/4 배수)으로 잡아
// float32(Neo)와 double(Lua/C++)이 완전히 같은 체크섬을 내도록 한다.
fun FloatMath(var outer, var inner)
{
    var last = 0.0;
    for (var o in 0, outer, 1)
    {
        var bias = toint(last) & 1;    // 직전 결과 의존
        var acc = 0.0;
        for (var i in 0, inner, 1)
        {
            var x = ((i + bias + toint(acc)) & 15) * 0.5;   // acc 의존 → 예측 불가
            acc += x * 1.5 - x * 0.25;
        }
        last = acc;
    }
    return toint(last);
}

// ---------------------------------------------------------------- 3. 함수 호출
fun Inc(var x)
{
    return x + 1;
}
fun FuncCall(var outer, var inner)
{
    var last = 0;
    for (var o in 0, outer, 1)
    {
        var bias = last & 1;           // 직전 결과 의존
        var sum = 0;
        for (var i in 0, inner, 1)
            sum += Inc(i + bias);
        last = sum;
    }
    return last;
}

// ------------------------------------------------------------------- 4. 재귀
fun Fib(var n)
{
    if (n < 2)
        return n;
    return Fib(n - 1) + Fib(n - 2);
}

// -------------------------------------------------------- 5. 배열 순차 쓰기/읽기
fun ArrayRW(var size, var reps)
{
    var a = [];
    a.resize(size);
    var last = 0;
    for (var r in 0, reps, 1)
    {
        var bias = last & 1;           // 직전 결과 의존
        for (var i in 0, size, 1)
            a[i] = (i + bias) & 255;
        var sum = 0;
        for (var i in 0, size, 1)
            sum += a[i];
        last = sum;
    }
    return last;
}

// ------------------------------------------------------- 6. 해시맵(문자열 키) 조회
fun MapStr(var outer, var inner)
{
    var m = {};
    m["alpha"] = 1;
    m["bravo"] = 2;
    m["charlie"] = 3;
    m["delta"] = 4;
    m["echo"] = 5;
    m["foxtrot"] = 6;
    m["golf"] = 7;
    m["hotel"] = 8;
    var last = 0;
    for (var o in 0, outer, 1)
    {
        var sum = 0;
        for (var i in 0, inner, 1)
        {
            sum += m["alpha"];
            sum += m["hotel"];
            sum += m["charlie"];
            sum += m["foxtrot"];
        }
        last = sum;
    }
    return last;
}

// ----------------------------------------------------------- 7. 문자열 생성/길이
fun StringOps(var outer, var inner)
{
    var last = 0;
    for (var o in 0, outer, 1)
    {
        var total = 0;
        for (var i in 0, inner, 1)
        {
            var s = "item" .. i;
            total += s.len();
        }
        last = total;
    }
    return last;
}

// ------------------------------------------- 8. 파티클 시뮬(게임형 부동소수+배열)
// pos += vel*dt, 경계 반사. 게임 스크립트에서 가장 흔한 형태의 워크로드.
fun Particles(var count, var steps)
{
    var px = []; px.resize(count);
    var py = []; py.resize(count);
    var vx = []; vx.resize(count);
    var vy = []; vy.resize(count);

    for (var i in 0, count, 1)
    {
        px[i] = (i & 63) * 1.0;
        py[i] = (i & 31) * 1.0;
        vx[i] = ((i & 7) - 4) * 0.25;
        vy[i] = ((i & 15) - 8) * 0.125;
    }

    var dt = 0.015625;   // 1/64 — 이진 정확값(0.016 은 부정확해 언어별 체크섬이 갈린다)
    for (var s in 0, steps, 1)
    {
        for (var i in 0, count, 1)
        {
            var nx = px[i] + vx[i] * dt;
            var ny = py[i] + vy[i] * dt;
            if (nx < 0.0 || nx > 64.0)
                vx[i] = -vx[i];
            if (ny < 0.0 || ny > 32.0)
                vy[i] = -vy[i];
            px[i] = nx;
            py[i] = ny;
        }
    }

    // +1000 오프셋: 위치가 음수가 될 수 있어(반사 직전) truncate/floor 차이가 생기는 것을 막는다.
    var chk = 0;
    for (var i in 0, count, 1)
        chk += toint(px[i] + py[i] + 1000.0);
    return chk;
}
```

#### Lua 5.5 — [`Samples/bench/bench.lua`](Samples/bench/bench.lua)
```lua
-- ---------------------------------------------------------------- 1. 정수 루프
local function LoopSum(outer, inner)
    local last = 0
    for o = 1, outer do
        local bias = last & 1          -- 직전 결과에 의존(진짜 데이터 의존성) → 축약 불가
        local sum = 0
        for i = 0, inner - 1 do
            sum = sum + i + bias
        end
        last = sum
    end
    return last
end

-- ------------------------------------------------------------- 2. 부동소수 산술
local function FloatMath(outer, inner)
    local last = 0.0
    for o = 1, outer do
        local bias = math.floor(last) & 1  -- 직전 결과 의존
        local acc = 0.0
        for i = 0, inner - 1 do
            local x = ((i + bias + math.floor(acc)) & 15) * 0.5   -- acc 의존 → 예측 불가
            acc = acc + (x * 1.5 - x * 0.25)
        end
        last = acc
    end
    return math.floor(last)
end

-- ---------------------------------------------------------------- 3. 함수 호출
local function Inc(x)
    return x + 1
end
local function FuncCall(outer, inner)
    local last = 0
    for o = 1, outer do
        local bias = last & 1          -- 직전 결과 의존
        local sum = 0
        for i = 0, inner - 1 do
            sum = sum + Inc(i + bias)
        end
        last = sum
    end
    return last
end

-- ------------------------------------------------------------------- 4. 재귀
local function Fib(n)
    if n < 2 then
        return n
    end
    return Fib(n - 1) + Fib(n - 2)
end

-- -------------------------------------------------------- 5. 배열 순차 쓰기/읽기
local function ArrayRW(size, reps)
    local a = {}
    for i = 1, size do a[i] = 0 end          -- resize 상당(배열부 미리 확보)
    local last = 0
    for r = 1, reps do
        local bias = last & 1          -- 직전 결과 의존
        for i = 0, size - 1 do
            a[i + 1] = (i + bias) & 255
        end
        local sum = 0
        for i = 0, size - 1 do
            sum = sum + a[i + 1]
        end
        last = sum
    end
    return last
end

-- ------------------------------------------------------- 6. 해시맵(문자열 키) 조회
local function MapStr(outer, inner)
    local m = {}
    m["alpha"] = 1
    m["bravo"] = 2
    m["charlie"] = 3
    m["delta"] = 4
    m["echo"] = 5
    m["foxtrot"] = 6
    m["golf"] = 7
    m["hotel"] = 8
    local last = 0
    for o = 1, outer do
        local sum = 0
        for i = 0, inner - 1 do
            sum = sum + m["alpha"]
            sum = sum + m["hotel"]
            sum = sum + m["charlie"]
            sum = sum + m["foxtrot"]
        end
        last = sum
    end
    return last
end

-- ----------------------------------------------------------- 7. 문자열 생성/길이
local function StringOps(outer, inner)
    local last = 0
    for o = 1, outer do
        local total = 0
        for i = 0, inner - 1 do
            local s = "item" .. i
            total = total + #s
        end
        last = total
    end
    return last
end

-- ------------------------------------------- 8. 파티클 시뮬(게임형 부동소수+배열)
local function Particles(count, steps)
    local px, py, vx, vy = {}, {}, {}, {}
    for i = 0, count - 1 do
        px[i + 1] = (i & 63) * 1.0
        py[i + 1] = (i & 31) * 1.0
        vx[i + 1] = ((i & 7) - 4) * 0.25
        vy[i + 1] = ((i & 15) - 8) * 0.125
    end

    local dt = 0.015625                       -- 1/64 (이진 정확값)
    for s = 1, steps do
        for i = 1, count do
            local nx = px[i] + vx[i] * dt
            local ny = py[i] + vy[i] * dt
            if nx < 0.0 or nx > 64.0 then
                vx[i] = -vx[i]
            end
            if ny < 0.0 or ny > 32.0 then
                vy[i] = -vy[i]
            end
            px[i] = nx
            py[i] = ny
        end
    end

    local chk = 0
    for i = 1, count do
        chk = chk + math.floor(px[i] + py[i] + 1000.0)
    end
    return chk
end
```

#### C++ (MSVC /O2) — [`Samples/bench/bench.cpp`](Samples/bench/bench.cpp)
```cpp
// ---------------------------------------------------------------- 1. 정수 루프
static int64_t LoopSum(int outer, int inner)
{
    int64_t last = 0;
    for (int o = 0; o < outer; ++o)
    {
        const int bias = (int)(last & 1);  // 직전 결과에 의존(진짜 데이터 의존성) → 축약 불가
        int64_t sum = 0;
        for (int i = 0; i < inner; ++i)
            sum += i + bias;
        last = sum;
    }
    return last;
}

// ------------------------------------------------------------- 2. 부동소수 산술
static int64_t FloatMath(int outer, int inner)
{
    double last = 0.0;
    for (int o = 0; o < outer; ++o)
    {
        const int bias = (int)((int64_t)last & 1);  // 직전 결과 의존
        double acc = 0.0;
        for (int i = 0; i < inner; ++i)
        {
            double x = ((i + bias + (int64_t)acc) & 15) * 0.5;   // acc 의존 → 예측 불가
            acc += (x * 1.5 - x * 0.25);
        }
        last = acc;
    }
    return (int64_t)last;
}

// ---------------------------------------------------------------- 3. 함수 호출
// noinline: 스크립트의 "함수 호출" 비용과 대응시키기 위해 인라인 제거.
// (인라인 허용 시 호출 자체가 사라져 '함수 호출' 측정이 성립하지 않는다.)
__declspec(noinline) static int64_t Inc(int64_t x)
{
    return x + 1;
}
static int64_t FuncCall(int outer, int inner)
{
    int64_t last = 0;
    for (int o = 0; o < outer; ++o)
    {
        const int bias = (int)(last & 1);  // 직전 결과 의존
        int64_t sum = 0;
        for (int i = 0; i < inner; ++i)
            sum += Inc(i + bias);
        last = sum;
    }
    return last;
}

// ------------------------------------------------------------------- 4. 재귀
static int64_t Fib(int n)
{
    if (n < 2)
        return n;
    return Fib(n - 1) + Fib(n - 2);
}

// -------------------------------------------------------- 5. 배열 순차 쓰기/읽기
static int64_t ArrayRW(int size, int reps)
{
    std::vector<int64_t> a(size, 0);
    int64_t last = 0;
    for (int r = 0; r < reps; ++r)
    {
        const int bias = (int)(last & 1);  // 직전 결과 의존
        for (int i = 0; i < size; ++i)
            a[i] = (i + bias) & 255;
        int64_t sum = 0;
        for (int i = 0; i < size; ++i)
            sum += a[i];
        last = sum;
    }
    return last;
}

// ------------------------------------------------------- 6. 해시맵(문자열 키) 조회
static int64_t MapStr(int outer, int inner)
{
    std::unordered_map<std::string, int64_t> m;
    m["alpha"] = 1;
    m["bravo"] = 2;
    m["charlie"] = 3;
    m["delta"] = 4;
    m["echo"] = 5;
    m["foxtrot"] = 6;
    m["golf"] = 7;
    m["hotel"] = 8;
    // 스크립트의 상수 문자열(미리 만들어진 VM 문자열)에 대응 — 조회마다 임시 string 생성 방지.
    static const std::string kAlpha = "alpha";
    static const std::string kHotel = "hotel";
    static const std::string kCharlie = "charlie";
    static const std::string kFoxtrot = "foxtrot";

    int64_t last = 0;
    for (int o = 0; o < outer; ++o)
    {
        int64_t sum = 0;
        for (int i = 0; i < inner; ++i)
        {
            sum += m.find(kAlpha)->second;
            sum += m.find(kHotel)->second;
            sum += m.find(kCharlie)->second;
            sum += m.find(kFoxtrot)->second;
        }
        last = sum;
    }
    return last;
}

// ----------------------------------------------------------- 7. 문자열 생성/길이
static int64_t StringOps(int outer, int inner)
{
    int64_t last = 0;
    for (int o = 0; o < outer; ++o)
    {
        int64_t total = 0;
        for (int i = 0; i < inner; ++i)
        {
            std::string s = "item" + std::to_string(i);
            total += (int64_t)s.size();
        }
        last = total;
    }
    return last;
}

// ------------------------------------------- 8. 파티클 시뮬(게임형 부동소수+배열)
static int64_t Particles(int count, int steps)
{
    std::vector<double> px(count), py(count), vx(count), vy(count);
    for (int i = 0; i < count; ++i)
    {
        px[i] = (i & 63) * 1.0;
        py[i] = (i & 31) * 1.0;
        vx[i] = ((i & 7) - 4) * 0.25;
        vy[i] = ((i & 15) - 8) * 0.125;
    }

    const double dt = 0.015625;   // 1/64 (이진 정확값)
    for (int s = 0; s < steps; ++s)
    {
        for (int i = 0; i < count; ++i)
        {
            double nx = px[i] + vx[i] * dt;
            double ny = py[i] + vy[i] * dt;
            if (nx < 0.0 || nx > 64.0)
                vx[i] = -vx[i];
            if (ny < 0.0 || ny > 32.0)
                vy[i] = -vy[i];
            px[i] = nx;
            py[i] = ny;
        }
    }

    int64_t chk = 0;
    for (int i = 0; i < count; ++i)
        chk += (int64_t)(px[i] + py[i] + 1000.0);
    return chk;
}
```


### YoutTube
[![YoutTube](https://i9.ytimg.com/vi_webp/iEpmlLcxBVg/mq2.webp?sqp=CLTL6L8G-oaymwEmCMACELQB8quKqQMa8AEB-AH-CYAC0AWKAgwIABABGEkgZSg2MA8=&rs=AOn4CLAwiLyh_R2B056F-Eej9t5PiuJhmw)](https://www.youtube.com/watch?v=iEpmlLcxBVg)
