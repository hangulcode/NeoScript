# Embedding NeoScript in a C++ host

For a complete buildable project, start with the
[mini game host](../Samples/minigame/README.md). It registers native objects,
compiles once, creates two independent instances, dispatches script callbacks,
and cleans up handles. The snippets below explain the API in more detail.

The public host API (`NeoScript.h`) and the internal engine surface behind it.
For the language see [../ReadMe.md](../ReadMe.md); for script-callable functions
see [API.md](API.md).

## Windows static linking

Add `Windows/NeoScript.vcxproj` to the host solution and add a project reference
from each executable that uses NeoScript. Keep **Link Library Dependencies** enabled
and **Use Library Dependency Inputs** disabled so the host links the actual `.lib`.
Remove `NeoSource/*.cpp` from the host's compile items to avoid duplicate definitions.
An intermediate static library may reference the project with **Link Library Dependencies**
disabled; the final executable must still reference and link NeoScript directly.

`Windows/NeoScript.sln` builds the library by itself. `Samples/console/console.sln`
uses the same project and output files. From a Visual Studio developer prompt:

```bat
MSBuild Windows\NeoScript.sln /p:Configuration=Release /p:Platform=x64 /m
```

| Configuration | Output under `Build/Windows/lib` | Host CRT |
| --- | --- | --- |
| Release x64 | `NeoScript64.lib` | `/MT` |
| Debug x64 | `NeoScript64-d.lib` | `/MTd` |
| Release x86 | `NeoScript32.lib` | `/MT` |
| Debug x86 | `NeoScript32-d.lib` | `/MTd` |

Solutions use `x86`; direct `.vcxproj` builds use `Win32`. Match the host's platform,
configuration, CRT and MSVC toolset. The project currently uses v145, disables RTTI,
and keeps its sources compatible with C++11. MSVC's minimum selectable mode is
C++14; the project uses that default, while Clang/GCC enforce C++11 compatibility.
Source-based hosts must also compile `NeoSource/NeoRegexVendor.c` as C99 (MSVC: C11), alongside
`NeoSource/*.cpp`. PCRE2 sources and their license are vendored under `ThirdParty/pcre2`.
Add `NeoSource` to the host's include
path and include `NeoScript.h`. The core does not link sockets or TLS; the selected HTTP transport supplies those dependencies.

Outputs and intermediate files are relative to the library project, so multiple
solutions reuse the same build. Build solutions sharing the same configuration
sequentially to avoid concurrent writes to object and PDB files. The static library
contains the C++ API; Unity's C ABI plugin remains a separate build. CMake already
provides the equivalent `neoscript` static-library target.

## Embedding the engine — Host API (`NeoScript.h`)
The public host API is a **3-concept facade: Runtime / Program / Instance**, declared in `NeoScript.h`.
It hides the internal VM (`CNeoVM` / `INeoVMWorker`, the execution-context pool, the bytecode image);
host code only sees opaque handles. Embed through this header — the internal `NeoVMSystem` / `CNeoVM` API described in
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

Compile diagnostics populate `Error.sourceName`, `line` and `column` with the originating
source, including nested imports, independently of `includeDebugInfo`. Set `CompileDesc.sourceName`
for the root source; imported sources use the loader path. `Error.message` also includes the
file name before `Error (line, column): ...`. This applies to both `Compile` and
`CompileToBytecode`; a failed bytecode compilation clears its output. Concatenating source
files does not preserve their original locations automatically.

Script `public const` publishes a compile-time value to importing scripts. It creates no
host-visible global or runtime module lookup; `GetGlobal*` does not expose it, and
`export const` is a compile error. Rebuild cached bytecode for consumers when a public
constant changes. `export var` and `export fun` keep their host visibility and are not
visible to other modules unless also marked `public` (`export public`).

**Host → script calls.** Function indices are fixed per program, so cache the `FunctionHandle` once and
call it every frame with no per-call string lookup. `Call(inst, fn)` returns an `Invocation`: push args,
`invoke()`, then read the return.

Argument buffer capacity is reused per instance, including synchronous nested calls after warm-up.
Only empty storage is cached: argument values are released after execution has accepted them,
and a separately prepared Invocation keeps its own arguments. Reset releases the spare nested-call buffers.

The **return value lives in the instance's shared, borrowed context**, so reading it must not outlive that
context. Prefer the **safe return terminals** — they detach the result from the Invocation's lifetime:

```cpp
FunctionHandle getScore = rt->FindFunction(cr.program, "GetScore");

// Scalar: invokeR() snapshots the return into an owned CallResult (int/float/bool/string/vec).
CallResult r = rt->Call(inst, getScore).argInt(playerId).invokeR();
if (r.ok()) int score = r.asInt();
// r stays valid even after other Calls on the same instance — safe to hold several at once.

// Collection: invokeReadMap / invokeReadList / invokeReadArray read inside a callback, where the context is guaranteed
// alive. Copy out what you need; do not keep the reader past the callback.
rt->Call(inst, "GetInventory").invokeReadMap([&](MapReader inv){
    inv.getInt("gold", gold);
});

// ArrayView exposes the primitive storage without a copy. Its view and raw pointer are
// valid only inside this callback. Bool arrays use packed bits; int/float arrays expose elements.
rt->Call(inst, "GetWeights").invokeReadArray([&](ArrayView weights){
    if (weights.elementType() == ArrayElementType::Float)
        ConsumeWeights(weights.floats(), weights.count());
});
```

The low-level `invoke()` + `retInt()/retMap()/...` still exist, but those read the **live** shared context
and are invalidated by the next `Call` on the same instance:

```cpp
Invocation a = rt->Call(inst, "GetScore"); a.invoke();
Invocation b = rt->Call(inst, "GetHp");    b.invoke();  // ← flushes a's return context
int score = a.retInt();  // Invalidated return: 0. Use invokeR() to keep the original value.
```

Reading an Invocation before execution, while suspended, or after its result has been invalidated
returns an empty/default value (`None`, zero, false, or an empty view). It never exposes the
result of a newer call. Invocation arguments and execution are one-shot: a consumed handle
cannot modify or execute a later prepared call.

String arguments, global reads, borrowed returns, owned `CallResult` strings, and collection
readers preserve the full byte length of string values, including embedded NUL.
`CallContext::argString()` also preserves a string argument's full length; non-string coercion
retains its existing behavior. Hosts validating text must use `StringView::size()`.
`MapReader::stringKeys(out, &unsupportedKeyType)` enumerates borrowed string-key views;
it returns false and clears `out` when a non-string key is present. Do not mutate the map
or retain these views outside the reader's callback. `sameStorage()` on map/list readers
compares container identity without exposing VM pointers, allowing hosts to reject cycles
while independently processing shared references. `ListBuilder::pushNull()` appends a
null element without changing its position. `MapReader::isNativeObject(key)`,
`ListReader::isNativeObject(index)` and `CallContext::argIsNativeObject(i)` inspect the
existing native-object VM type, independently of its userData pointer. Reader `isNull()`
checks for an actual null; both predicates return false for a missing key/index.
`ValueType::None` alone does not distinguish null from unsupported opaque VM values.
Serializers must check `isNull()` before omitting a map value or emitting a list null,
and reject remaining unsupported values. The ValueType enum, C ABI/C# type values,
script `type()` intrinsic and bytecode format are unchanged.

`ArrayView` follows exactly the same borrowed lifetime. `CallContext::argAsArray`, nested
`MapReader::getArray`/`ListReader::getArray`, `Invocation::retArray`, and the `invokeReadArray`
callback expose the VM's storage directly. The raw pointer is valid only while its source callback
or return view is valid, must not be retained or used from another thread, and is `nullptr` for the
wrong element accessor. Script-side `append` or size-changing `resize` can reallocate the storage,
so do not re-enter script and retain the pointer across a structural array change. `boolBits()` is
packed least-significant-bit first: bit `i` is
`(bits[i >> 3] >> (i & 7)) & 1`; `ints()` and `floats()` return contiguous element storage.
For Vector2/3/4 arrays, `vectorDimension()` returns 2/3/4 and `count()` counts vectors.
`getVec(index, out4)` copies one value into four floats, zeroing unused components.
`setVec(index, components, dimension)` writes one value and requires the exact array dimension;
shared vectors detach before modification. Both return false for invalid indices, pointers or
non-vector arrays. There is no writable float buffer for vector arrays: `vectorData()` was removed.
Scalar arrays return dimension zero; `floats()` remains exclusive to scalar float arrays.
Quaternion arrays retain script w,x,y,z component order. The view is still borrowed, but the
components copied into the host's own buffer can be retained.

Only one Invocation may be prepared on an instance at a time. A second `Call` while the first
is armed or suspended returns a falsy Invocation. Borrowed results and their nested readers remain
valid until that Invocation ends or a subsequent call invalidates its completed return context;
repeated `retMap()` or `retList()` reads within the same call do not invalidate earlier readers.
Use `invokeR` / `invokeReadMap` when the result does not need a longer borrowed lifetime.
Synchronous native-to-script calls during execution are still allowed. `FunctionHandle` includes
its owning runtime/program identity; using it with another runtime or program is rejected.

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

`Invocation::budget(n)`, `StartSliced(..., budget)`, and `ResumeDesc::instructionBudget`
limit actual VM dispatch work; zero means no instruction limit. A fused stored-vector reference
and its write count as one dispatch unit. `Resume` applies the supplied budget to that slice
(including zero); `UpdateSliced` retains the current sliced-execution settings. A fresh ordinary
Invocation starts with no time or instruction limit, independently of previous calls.
Native callbacks cannot be preempted while their C++ code is running. `ResumeDesc::deltaTime`
is reserved and currently ignored; script sleep uses the existing clock.

If an Invocation suspends, keep that original Invocation alive while calling `Resume` or
`UpdateSliced`. When execution completes, its `retX` methods can read the final return value.
Destroying the Invocation while it is suspended lets execution continue but relinquishes the
right to read its eventual return. Starting another call or rerunning global initialization
while the instance is suspended is rejected.

`ResetInstance`, `Cancel`, `Resume`, `UpdateSliced`, and debugger execution reject unsafe
reentry from a currently executing callback. Destruction is also refused while an instance
is executing or still has live Invocation owners. Use `CanDestroyInstance` / `CanDestroy`
to check this before releasing host resources, and release Invocations before their instance
and runtime. Reset preserves the instance's original `runGlobalInit` setting.


**Native method dispatcher** — one function exposes many script methods; `ctx.userData()` is the
per-instance object bound above:

```cpp
bool GameObjectMethod(CallContext& ctx, StringView method) {
    auto* self = static_cast<GameObject*>(ctx.userData());
    if (method.str() == "AddPos") { self->AddPos(ctx.argInt(0), ctx.argInt(1)); return true; }
    return false;   // unknown method
}
```

**Sharing native objects.** `CallContext::retInstanceGlobal` can return any stored script value,
including native objects nested in maps or lists. Shared native values retain their binding
metadata. They do not own the host `userData` object: the host must keep it alive while its
source instance remains valid. Resetting or destroying that source invalidates the old binding;
subsequent access from another instance reports a runtime error. Native callbacks retain the
source object instance identity, while function arguments belong to the actual executing
instance's program, and custom failure codes are reported on that executing instance. Object types are runtime-specific and remain stable when more objects are
registered; duplicate object type names are rejected.

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

### Memory: cycles and pool pages are collected separately

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

## Compile-time defines
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


## Execution context pool
> **Internal / advanced.** This section describes the low-level `NeoVMSystem` / `CNeoVM` engine and its pool. The public
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

NeoScript::CNeoVM* vm = NeoScript::NeoVMSystem::CompileAndLoadRunVM(param, &vparam);
// ... use vm ...
NeoScript::NeoVMSystem::ReleaseVM(vm);

NeoScript::NeoExecContextPool_Destroy(pool);     // after all VMs that used it are released
```

Host entry points:
- `INeoVMWorker::GetExecutionState()` returns `Idle`, `Running`, `SuspendedSleep`,
  `SuspendedDebugger`, or `SuspendedSlice`. It is derived from the live worker context, so it cannot
  drift from the actual execution state.
- `NeoVMSystem::CompileAndLoadRunVM` / `CompileAndLoadVM` — load a program and run its script body.
- `INeoVMWorker::ExecuteTop(fid, args)` — run a function as a fresh top-level execution.
  Returns `NeoExecStatus`: `NEOEXEC_COMPLETED`, `NEOEXEC_SUSPENDED`, or `NEOEXEC_ERROR`.
- `INeoVMWorker::ResumeTop()` — continue a suspended top-level execution.
- `INeoVMWorker::IsSuspended()` — a retained (suspended) execution is pending. A per-frame host loop should
  do `if (w->IsSuspended()) w->ResumeTop(); else w->ExecuteTop(fid, args);` so a breakpoint/sleep resumes
  instead of restarting.

This is the low-level `NeoVMSystem` / `CNeoVM` surface. Host code should prefer the `IRuntime` API above
(`Call` / `Invocation` / `StartSliced`), which manages context acquisition and nesting for you.

### Synchronous native-to-script callbacks

`Script A -> native API -> Script B` is a **synchronous nested call**. The native API must receive Script B's
result before it can return to Script A, so Script B does not own a resumable execution context.

- Breakpoints, stepping, and a requested debugger pause are suppressed while Script B is running. Debug Script A
  before the native call, or debug Script B through a top-level entry point instead.
- `sleep(...)`, `yield`, `async.get(...)`, `async.post(...)`, and `async.wait()` are rejected with a runtime error in
  Script B. A worker with a host time limit or a nonzero restrictive instruction budget cannot start Script B at all. These restrictions prevent
  Script B from being suspended after its native caller has already returned to Script A. Debugger
  cooperative slices are different: synchronous callbacks finish before the next debugger slice.
- Native `CallContext` arguments and borrowed argument strings remain valid across synchronous reentry,
  including while the child `Invocation` exists and after its destruction. The child uses a separate stack frame.
- Native property getters and setters also use a separate temporary frame, preserving pending script call arguments.
  That frame is released after each callback, so repeated property access does not accumulate stack usage.
- A synchronous callback may call ordinary functions and return values normally. For delayed work, have the native
  API schedule a new top-level `ExecuteTop`/`ResumeTop` execution instead of invoking Script B synchronously.

> Note: `NeoExecContextPool_Create` returns an opaque handle; its full type lives in the internal headers.
> Only the pointer, the two factory functions, and `NeoLoadVMParam::execPool` are part of the public API.

### Compiled image capacity

The current image format is **0142**, adding the fused compound-assignment read opcodes (`READ_ADD/SUB/MUL/DIV`), storing scalar floating-point constants as double and retaining signed 64-bit integer values.
Recompile older cached images; they are rejected rather than interpreted using the new layout.
`MOVI` retains a signed 32-bit immediate, sign-extended to 64 bits. Larger integer literals
use the constant pool, with `LOADK` when needed; instructions remain eight bytes.
`READ_ADD`, `READ_SUB`, `READ_MUL`, and `READ_DIV` implement `n3 op= n1[n2]` in one 8-byte
instruction: `n1` is the collection, `n2` the key, and `n3` the variable that is read and
written. They use the ordinary operand flags; the upper flag bits must be zero. Numeric map and
list elements run inline in the dispatch loop, numeric array elements and vector components
through a local copy, and every other element type, missing key, host getter, or error through a
slow path that reproduces the separate READ and arithmetic instructions, including their error
messages. A host getter's pause or coroutine-transfer request takes effect after the
arithmetic. Three-operand and store shapes (`x = a[i] op b[j]`, `e[i] = a[k] op y`) are not
fused: measured on this VM, either as an out-of-line call or inlined into the dispatch loop they
ran slower than the separate instructions. Recompile images older than format 0142.

Public scalar integer arguments, results, globals, map/list values, and integer array
elements use `int64_t`. C# exposes them as `long` through C ABI 7. Rebuild hosts and plugins
together. Counts, indices, function IDs, statuses, and timeout parameters retain their
existing widths. C++ map/list/global integer readers also accept an `int32_t&` output:
these checked compatibility overloads return false without changing the output if the value
is outside the 32-bit range. Prefer `int64_t` for complete script integer values.

`system.array32` exposes distinct `ArrayElementType::Int32` and `Float32` values.
`ArrayView::ints32()` returns `int32_t*`; `floats32()` returns `float*`. The ordinary
`ints()`/`floats()` accessors return null for these narrow arrays, and narrow accessors
return null for other element types. C hosts use `NS_ELEM_INT32`/`NS_ELEM_FLOAT32`
and `NsArrayInts32`/`NsArrayFloats32`. Pointer lifetime rules are unchanged.

Scalar float arguments, results, globals, and collection values use `double`; float array
views expose `double*`. Vector component buffers remain `float*`. C++ map/list/global
readers also accept `float&` for explicit narrowing compatibility; use `double&` to retain
all script precision. C ABI 7 adds `NsCallResult.d` for scalars while `f[4]` retains vectors.
Recompile C++ consumers and rebuild native/managed plugins together.
Double literals that round-trip exactly through float32, including the sign of zero,
may use the 32-bit `MOVF` immediate. Execution widens it to double. Other literals stay
in the double constant pool; instruction width is unchanged. Rebuild cached images: older
loaders reject these immediates in double builds.

Indexed null assignment uses the ordinary assignment instruction; lists keep their positions
and maps remove the key at runtime. Instructions remain eight bytes. `LOADK` sits beside the
`MOV` instructions; its destination flag handles both local and global storage.

- Constant indices 0–14,999 use the existing direct 16-bit operand path. Larger indices use `LOADK`
  with a 32-bit constant index and a 16-bit destination. A direct assignment needs one instruction;
  an arithmetic operand, table key, or method name may need an extra load into a temporary slot.
- Global variables use signed operands -2 through -32,768; -1 remains reserved. Each VM stores
  `[reversed globals | reserved slot | constants]`, with its operand pointer at constant zero.
  Ordinary global/constant access is pointer addition without a sign-dependent branch. Increasing
  the constant pool does not consume global indices. Strings remain owned by each VM.
- Code buffers and the complete image grow beyond the former 1 MiB ceiling. The current signed
  32-bit image offsets impose an `INT_MAX` byte ceiling; memory availability can impose a lower one.
  Checked arithmetic and write failures stop compilation, including failures in intermediate buffers.
- Function IDs are separate from constants and remain signed 16-bit: IDs 0–32,767, including the
  global initializer. Defining many functions with distinct return literals can also grow the constant pool.

Remaining encoding limits are checked before narrowing or serialization:

| Item | Limit |
| --- | --- |
| Globals | 32,767, independent of constant count |
| Arguments per definition/call | 2,766 |
| Named local/capture slots | Arguments + locals + captures ≤ 9,999 |
| Temporary slots | Arguments + temporaries ≤ 4,999 |
| Relative branch | -32,768 through 32,767 instructions |
| Serialized string or symbol name | 32,767 UTF-8 bytes per string |
| Switch tables | 65,536 |
| Debug source index / line | 0–65,535 / 1–65,535 |

Constants also have checked 32-bit compiler indices; image size and available memory usually become
the practical limit first. Without debug information, source line numbers use `int` and can exceed
65,535. Optional optimizations fall back to ordinary code when their own spare slots are exhausted.

On failure, `CompileToBytecode` clears its output and returns an error containing the limit and,
where available, the source location. The low-level compiler likewise resets its output cursor.
`neoscript_compiler_limits_regression` covers large pools/images, independent global indices,
save/load/execution, and encoding boundaries.

#### Function return slots

`SCallStack::_iReturnValueIndex` keeps global destinations in the same signed-offset form as opcode
operands. Calling and returning use these meanings:

| Stored value | Destination |
| --- | --- |
| 0 or greater | Absolute index in the caller's execution stack |
| -1 | No explicit destination; use the ordinary return temporary |
| -2 through -32,768 | Global offset from the pointer to constant zero |

For a global destination, the call stores `OP.n3` directly and the return resolves it as
`m_pVarGlobal_Pointer + slot`. Do not add `ConstantBase()` or encode/decode a physical array index.
The negative offset must be applied to the constant-zero pointer, not the allocation's first element.
Local destinations still include the caller's stack base, and calls without an explicit destination
keep their existing temporary-return behavior.

This removes the extra global-destination conversion and keeps opcode and call-stack numbering
consistent, without growing instructions or call frames. It does not change the constant/global
limits; any performance difference needs measurement. Producers and consumers must use the same
representation. Call frames are runtime state and do not add a separate image-format requirement.


Loading an image validates section lengths and counts before allocation, along with opcode,
operand, function, branch, and switch-table bounds. Truncated sections, invalid boolean bytes,
and unsupported internal opcodes are rejected. Per-function writable stack capacity includes
outgoing call slots and is checked before argument preparation, including coroutine entry.
This validation does not change the serialized image format.

## HTTP and HTTPS transport

`NeoHttp.h` defines `HttpRequest`, `HttpResponse`, and `IHttpTransport`. The core neither links
libcurl nor owns sockets/TLS. Configure each runtime before creating instances:

```cpp
NeoScript::RuntimeDesc desc;
desc.httpTransport = std::make_shared<MyHttpTransport>();
auto runtime = NeoScript::CreateRuntime(desc);
```

Implement `HttpResponse Send(const HttpRequest&)`: return `success=true` and `body` after a
completed transfer, or `success=false` and `error`. A thrown exception becomes a failed script
callback. Requests contain the method, URL, body, headers, and timeout in milliseconds.
The legacy API accepts the same shared pointer in `NeoVMSystem::CreateVM(transport)`.
A missing transport completes requests with an explicit failure; no network implementation is
selected implicitly. Existing hosts must configure a transport to retain networking.

The runtime keeps shared ownership until its worker thread has stopped. `Send` runs synchronously
on that background thread, never on the script thread. It must not call the VM; return owned response
strings instead. If one transport is shared across runtimes, it must tolerate concurrent `Send` calls.
Each runtime serializes its own requests. Honor `timeoutMs`: `-1` means no time limit, while `0`
means an immediate timeout. Each request carries its own cancellation signal. Check
`request.IsCancelled()` before starting and periodically during blocking operations; cancellation
must stop the transfer and return promptly, even with an unlimited timeout. Runtime shutdown signals
cancellation, discards queued work, and joins the worker before freeing its state. A transport that
ignores cancellation can still delay shutdown. Cancellation never targets another runtime's requests,
even when the transport object is shared.
C hosts can use `NsRuntimeCreateWithHttpTransport(desc, send, user)`. The synchronous callback reads
method, URL, body, timeout and headers through `NsHttp*` accessors, polls `NsHttpIsCancelled`, and
calls `NsHttpRespond` to copy its body/error before returning. Request/response pointers are valid
only during that callback. Keep `user` and the callback alive until runtime destruction returns;
do not reenter the VM from the transport thread. These are additive ABI 6 entry points.

Regex patterns default to a maximum of 16,384 UTF-8 bytes. Set `RuntimeDesc::maxRegexPatternBytes`
before `CreateRuntime` to change this limit; zero permits only empty patterns. The legacy equivalent
is the second argument to `NeoVMSystem::CreateVM(transport, maxRegexPatternBytes)`. The limit is
checked before compiling each pattern, including patterns constructed at runtime. It limits pattern
size. PCRE2 additionally enforces work, backtracking, heap and text limits, and caches compiled patterns;
see the [regex limits and syntax](API.md#6-string-methods). These resource limits do not promise a
wall-clock deadline; regex remains unavailable inside time-limited VM invocations.

`Samples/console/HttpTransport.cpp` is the portable libcurl adapter. The Windows console project
builds static libcurl through `BuildConfig/ConsoleCurl.targets`; its cached source and libraries live
under `Build/curl-<Platform>`. A CMake release supporting Visual Studio 2026 (tested with 4.3.2) and network access are needed
on the first build.
Both architectures and Debug/Release use matching static CRT settings. HTTPS uses Schannel's native
certificate store; no extra TLS DLL or CA bundle is shipped. Peer and hostname verification remain on.
HTTP error status codes still count as completed transfers; redirects are not followed.
The adapter can also be compiled by another host against libcurl 7.85+ built with HTTPS support.
Non-Windows builds use that libcurl installation's TLS backend and CA configuration.

Core-only CMake and Visual Studio builds do not download or link libcurl. Redistribute the curl
copyright notice in `THIRD_PARTY_NOTICES.txt` with console binaries.
