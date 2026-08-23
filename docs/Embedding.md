# Embedding NeoScript in a C++ host

The public host API (`NeoScript.h`) and the internal engine surface behind it.
For the language see [../ReadMe.md](../ReadMe.md); for script-callable functions
see [API.md](API.md).

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
  Script B. A worker configured for time-limited execution cannot start Script B at all. These restrictions prevent
  Script B from being suspended after its native caller has already returned to Script A.
- A synchronous callback may call ordinary functions and return values normally. For delayed work, have the native
  API schedule a new top-level `ExecuteTop`/`ResumeTop` execution instead of invoking Script B synchronously.

> Note: `NeoExecContextPool_Create` returns an opaque handle; its full type lives in the internal headers.
> Only the pointer, the two factory functions, and `NeoLoadVMParam::execPool` are part of the public API.
