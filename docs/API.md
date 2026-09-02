# NeoScript Script API Reference

Everything a `.ns` script can call: keyword intrinsics, the three built-in modules
(`math` / `system` / `coroutine`), and the method sets of `string` / `list` / `map` / `async`.

- The **host** side (`IRuntime`, `CallContext`, `FunctionHandle`, binding native objects) lives in
  [`ReadMe.md`](../ReadMe.md). This file never describes C++ API.
- Language syntax (`for`, `foreach`, `switch`, operators, `const`, compile-time defines) also lives
  in `ReadMe.md`. This file only describes *callables*.

**Source of truth.** Regenerate this file from these when they change:

| What | Where |
| :--- | :--- |
| module functions, type methods | `NeoSource/NeoLib.cpp` - `AddGlobalLibFun()` and `RegObjLibrary()` |
| keyword intrinsics | `NeoSource/NeoParser.cpp` - `InitDefaultTokenString()` |
| `type()` result strings | `NeoSource/NeoVM.cpp` - the `NDF_*` switch |
| editor keyword / builtin lists | `Tools/vscode-neo-script/syntaxes/ns.tmLanguage` - hand-maintained, update it with this file |

Every signature and behavioural note below was verified by running it through
`Samples/console/console.exe --file`.

## Conventions

- `name(arg: type, ...) -> ret`. `void` means the call evaluates to `null`.
- `float` is `NS_FLOAT` (single precision). An `int` passed to a `float` parameter is converted
  silently; the result is still `float`.
- **Arity is exact.** A wrong argument count or type raises the runtime error
  `invalid function call`. There are no optional parameters except where an entry says so.
- Module functions need `import` first (`import math;`). Keyword intrinsics and `print` do not.
- `import name;` lowercases `name`, looks for `<libPath>/name.ns`, then falls back to the built-in
  module table. `import name as alias;` renames it locally. Import is a compile-time include, so
  each importer gets its **own copy** of that module's globals.

---

## 1. Keyword intrinsics - no import, one opcode each

These are reserved words, not functions. They cannot be stored in a variable or passed as a
callback.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `tostring(x)` | `string` | Same conversion `print` uses. |
| `toint(x)` | `int` | Truncates a float toward zero; parses a numeric string. |
| `tofloat(x)` | `float` | Parses a numeric string. |
| `tosize(x)` | `int` | string: UTF-8 character count. list/map/set/array: element count. Vector2/3/4/Quaternion: component count. Anything else: `0`. |
| `type(x)` | `string` | Exact strings in section 11. |
| `sleep(ms: int)` | `void` | Suspends the instance. Rejected inside a nested native -> script call. |
| `yield` | - | Statement, not a call. Suspends the current coroutine. |
| `__LINE__` | `int` | Current source line, substituted at compile time. |

`totype` does **not** exist. Use `type`.

## 2. Global functions

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `print(x)` | `void` | Writes `tostring(x)`. With the default `std::cout` sink it appends a newline. |
| `print(x, y)` | `void` | Writes both concatenated, **without** a trailing newline (default sink). |

`print` is the only global native. Three or more arguments raise `invalid function call`. When the
host installs a print sink (`NeoVMSystem::m_pFunPrint`), both forms go through it as one string and no
newline is added.

---

## 3. `math`

### 3.1 Scalar

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.abs(x: float)` | `float` | |
| `math.acos(x: float)` | `float` | radians |
| `math.asin(x: float)` | `float` | radians |
| `math.atan(x: float)` | `float` | radians; one-argument form only, there is no `atan2` |
| `math.ceil(x: float)` | `float` | returns a float, not an int |
| `math.floor(x: float)` | `float` | returns a float, not an int |
| `math.round(x: float)` | `float` | returns a float, not an int |
| `math.sin(radian: float)` | `float` | |
| `math.cos(radian: float)` | `float` | |
| `math.tan(radian: float)` | `float` | |
| `math.log(x: float)` | `float` | natural log |
| `math.log10(x: float)` | `float` | |
| `math.exp(x: float)` | `float` | |
| `math.pow(base: float, exp: float)` | `float` | |
| `math.sqrt(x: float)` | `float` | |
| `math.deg(radian: float)` | `float` | radians to degrees |
| `math.rad(degree: float)` | `float` | degrees to radians |

### 3.2 Interpolation and clamping

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Clamp01(x: float)` | `float` | |
| `math.Clamp(x: float, min: float, max: float)` | `float` | |
| `math.SmoothStep01(t: float)` | `float` | |
| `math.Lerp(a: float, b: float, t: float)` | `float` | `a + (b - a) * t`, **not** clamped |
| `math.Lerp3(a: Vector3, b: Vector3, t: float)` | `Vector3` | not clamped |

### 3.3 Random

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.srand(seed: int)` | `void` | per-worker generator |
| `math.rand()` | `int` | `0 .. 32767` |
| `math.Rand01()` | `float` | `rand() / 32767` - 15-bit resolution, not a full-precision float |
| `math.RandRange(min: float, max: float)` | `float` | `min + (max - min) * Rand01()` |

### 3.4 Vector and quaternion constructors

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Vector2(x: float, y: float)` | `Vector2` | compiled to the `NOP_VEC_MAKE` intrinsic, not a native call |
| `math.Vector3(x: float, y: float, z: float)` | `Vector3` | same |
| `math.Vector4(x: float, y: float, z: float, w: float)` | `Vector4` | same |
| `math.Quaternion(w: float, x: float, y: float, z: float)` | `Quaternion` | **`w` comes first** |

Section 10 describes what the resulting values support.

### 3.5 Vector math

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Cross3(a: Vector3, b: Vector3)` | `Vector3` | |
| `math.DistanceSquared3(a: Vector3, b: Vector3)` | `float` | squared, no `sqrt` |
| `math.Normalize3(v: Vector3, fallback: Vector3)` | `Vector3` | returns `fallback` when `lengthSq < 1e-8`; `fallback` is required |
| `math.RotateVectorByQuat(quat: Quaternion, v: Vector3)` | `Vector3` | quaternion first |
| `math.quat_from_basis(right: Vector3, up: Vector3, forward: Vector3)` | `Quaternion` | |
| `math.quat_slerp(a: Quaternion, b: Quaternion, t: float)` | `Quaternion` | |

These accept vector **value types** only. A `list` of three numbers is rejected.

### 3.6 Bit, colour, formatting

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Hash32(value: int)` | `int` | 32-bit integer mix; the result is the signed reinterpretation, so it is often negative |
| `math.ColorRGB(r: float, g: float, b: float)` | `int` | components are **0.0-1.0**, clamped. Packs `0xFFBBGGRR` (alpha forced to 255) |
| `math.ColorARGB(a: float, r: float, g: float, b: float)` | `int` | components are **0.0-1.0**, clamped. Packs `0xAABBGGRR` |
| `math.tostr(value: float)` | `string` | `%.9g`, round-trippable float text. `"" .. 0.1` gives `0.1`; `math.tostr(0.1)` gives `0.100000001` |

The engine's metadata table declares the `ColorRGB` / `ColorARGB` parameters as `int`. The
implementation reads them as floats and clamps to `0..1` - the metadata string is stale, the
behaviour documented here is what runs.

---

## 4. `system`

Use `import system;` before calling these functions.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `system.time()` | `int` | Unix seconds |
| `system.date(format: string, time: int)` | `string` | `strftime` + `localtime`, output capped at 79 chars |
| `system.clock()` | `float` | `clock() / CLOCKS_PER_SEC` in seconds. Use for deltas, not wall time |
| `system.array(initial: bool\|int\|float, size: int)` | `array` | Fixed-size primitive array. `initial` chooses the element type and fills every element; `size` must be non-negative |
| `system.load(source: string, name: string)` | `module` | Compiles `source` at run time. `name` is type-checked but unused. A compile failure raises `invalid function call`; it does not return `null` |
| `system.pcall(m: module)` | `void` | Runs the module's top-level code |
| `system.set(l: list)` | `set` | Builds a set from a list; duplicates collapse |
| `system.aysnc_create()` | `async` | Creates an HTTP request object. **The typo is the real name** |

`system.pcall` is **not** a protected call. A runtime error inside the module propagates to the
caller and aborts it, and there is no success/failure return value. The name is historical.

A module value returned by `system.load` has no callable members; `system.pcall(m)` running the
chunk's top level is the only thing you can do with one. To call named functions across files use
`import file as alias;` and `alias.Fn()`, which the parser resolves at compile time.

---

## 5. `coroutine`

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `coroutine.create(f: function)` | `coroutine` | `f` may be a named function or a capturing lambda |
| `coroutine.resume(co: coroutine, ...)` | `void` | Extra arguments become `f`'s parameters on the **first** resume. Requires status `suspended`, otherwise `invalid function call` |
| `coroutine.status(co: coroutine)` | `string` | `"suspended"` / `"running"` / `"dead"` / `"normal"` |
| `coroutine.close()` | `coroutine` | Closes the *current* coroutine |
| `coroutine.close(co: coroutine)` | `coroutine` | Closing an already-dead coroutine succeeds as a no-op |

`yield` (the keyword) suspends. The context switch happens when the interpreter next runs, not
inside the native call, so statements after `coroutine.resume(...)` in the caller still run first.

---

## 6. `string` methods

Call these on a **variable**: `"literal".len()` is a syntax error, assign to a variable first.
Indexes and lengths are UTF-8 **character** counts, not bytes.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `s.len()` | `int` | character count |
| `s.sub(start: int, count: int)` | `string` | `start` out of range raises `invalid function call` |
| `s.find(needle: string)` | `int` | character index, `-1` when not found |
| `s.upper()` | `string` | ASCII only - per-byte `::toupper` |
| `s.lower()` | `string` | ASCII only |
| `s.trim()` | `string` | strips **spaces only**, not tabs or newlines |
| `s.ltrim()` | `string` | leading spaces |
| `s.rtrim()` | `string` | trailing spaces |
| `s.replace(find: string, to: string)` | `string` | **first occurrence only** |
| `s.split(sep: string)` | `list` | multi-character separators work; always returns at least one element |

`s.replace(x, y)` where `x` is absent throws, surfacing as the runtime error `exception`. Guard it:
`if (s.find(x) >= 0) s = s.replace(x, y);`

Strings are **not** indexable. `s[0]` raises `cannot read by index from string`.

## 7. `list` methods

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `l.len()` | `int` | |
| `l.resize(n: int)` | `void` | grows with `null`, shrinks by dropping the tail |
| `l.append(value)` | `void` | appends |
| `l.append(value, index: int)` | `void` | inserts **at** `index` - note the value comes first |

There is no `remove`, `insert`, `sort`, `find`, or `clear` on lists. Use `l.resize(0)` to clear.
Index with `l[i]`; `l[i] = v` only works for an index that already exists, so grow with
`resize`/`append` first.

### 7.1 Fixed primitive arrays

```cpp
import system;

var flags = system.array(false, 1024);
var ids = system.array(0, 1024);
var weights = system.array(0.0, 1024);
```

`system.array(initial, size)` creates an `array` whose fixed element type is selected by the first
argument. Arrays are reference values like List: assignment shares the same storage. `a.len()` and
`tosize(a)` return the size. `foreach(var value in a)` is supported and yields a copy of each value;
`foreach(var index, value in a)` is rejected.

- Only integer indexes in `[0, a.len())` are valid. Out-of-range reads and writes are runtime errors.
- `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `++`, and `--` work on an element.
- int and float arrays convert between those two numeric types on assignment. Float to int truncates
  toward zero. Bool arrays accept only bool values.
- Arrays have no `append`, `remove`, `resize`, or slice operations. Bool storage is bit-packed;
  that is an implementation detail and does not change script indexing.

## 8. `map` methods

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `m.len()` | `int` | |
| `m.reserve(n: int)` | `void` | pre-sizes the bucket array |
| `m.keys()` | `list` | **order is unspecified** (bucket order), not insertion order |
| `m.values()` | `list` | same order as `keys()` |
| `m.sort(cmp: function)` | `void` | sorts the map's **values** in place |

`m.sort` details:

- `cmp(a, b)` receives two values and must return `bool`; anything else counts as `false`.
- The comparator may be a named function or a capturing lambda.
- If the callback inserts or erases keys the sort aborts with the runtime error
  `map was modified during sort`, and the map keeps its pre-sort values.
- A **capturing** comparator writes back into its own capture storage, not into the enclosing
  function, so counting hits inside a comparator does not work. See *Captured anonymous functions*
  in `ReadMe.md`.

Access with `m["key"]` or `m.key`. Assigning to a missing key inserts it.

### 8.1 `set` has no methods

No method table is registered for `set`. A method call on one does **not** raise - it silently
evaluates to `null`, so `s.len()` is a bug that looks like it works:

```cpp
var s = system.set([1, 2, 3]);
print(tosize(s));               // 3   <- use this
foreach (var v in s) print(v);  // iteration works
var n = s.len();                // null, no error
```

Build a set with `system.set(list)`; duplicates collapse. There is no add, remove, or membership
test from script - a set is a build-once, iterate-many value.

## 9. `async` methods

Create the object with `system.aysnc_create()`.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `a.add_header(name: string, value: string)` | `void` | call **before** `get`/`post`; accumulates |
| `a.get(timeoutMs: int, url: string, callback: function)` | `void` | `timeoutMs == -1` means no timeout |
| `a.post(timeoutMs: int, url: string, body: string, callback: function)` | `void` | same timeout rule |
| `a.wait()` | `bool` | blocks; `false` on timeout. The callback runs when the wait completes |
| `a.close()` | `void` | |

- The callback signature is `fun(ok: bool, result: string)`. On failure `result` carries the error
  text.
- `get` / `post` require the object to be in the `READY` state - freshly created, or after `close`.
- `get`, `post`, and `wait` are all rejected inside a nested native -> script call
  (`RTE_NESTED_NOT_ALLOWED`), together with `sleep` and `yield`. See *Synchronous native-to-script
  callbacks* in `ReadMe.md`.
- HTTP transport is Windows-only in the current worker-thread implementation.

---

## 10. Vector and quaternion value types

`Vector2` / `Vector3` / `Vector4` / `Quaternion` are **value types**, not lists. They live inline in
a `VarInfo`, so assignment copies: there is no reference sharing and no allocation.

| Operation | Supported | Notes |
| :--- | :--- | :--- |
| `v[0]`, `v[1]`, `v[2]`, `v[3]` | yes | read and write; out of range raises `vector index out of range` |
| `v.x`, `v.y`, `v.z`, `v.w` | **no** | named component access does not exist - index instead |
| `a + b`, `a - b` | yes | component-wise; both sides need the same component count |
| `a * scalar`, `a / scalar` | yes | component-wise |
| `type(v)` | yes | `"Vector2"` / `"Vector3"` / `"Vector4"` - a quaternion also reports `"Vector4"` |
| `"" .. v` | yes | formats as `(3, 4)` / `(1, 2, 3)` / `(1, 0, 0, 0)` |
| `tosize(v)` | yes | component count: `2` / `3` / `4` / `4` |
| `foreach` | **no** | raises `foreach does not support vector` |

`Quaternion` stores `w, x, y, z` in that order, so `q[0]` is `w`.

There is **one** vector type at run time, not four: all of these are `VAR_VEC` plus a 1-4 component
count. A quaternion is just a 4-component vector, so `type(q)` is `"Vector4"` and `q1 + q2` is a
component-wise add rather than an error. Compose rotations with `math.quat_slerp` /
`math.quat_from_basis` / `math.RotateVectorByQuat`, never with `*`.

## 11. `type()` return strings

| Value | `type(x)` |
| :--- | :--- |
| `null` / uninitialized | `"null"` |
| integer | `"int"` |
| float | `"float"` |
| boolean | `"bool"` |
| string | `"string"` |
| map | `"map"` |
| list | `"list"` |
| fixed primitive array | `"array"` |
| set | `"set"` |
| script function or lambda | `"function"` |
| coroutine | `"coroutine"` |
| module (`system.load`) | `"module"` |
| async object | `"asynchronous"` |
| Vector2 / Vector3 / Vector4 | `"Vector2"` / `"Vector3"` / `"Vector4"` |
| Quaternion | `"Vector4"` - there is no distinct quaternion type at run time |
| bound native object (`RegisterObject` / `BindObject`) | `"null"` - **not distinguishable from a real null** |

`type()` on an inline lambda literal (`type(fun() { })`) reports `"null"`; assign it to a variable
first if you need the type.

A host-bound native object also reports `"null"`, so `type()` cannot identify one. Comparison
still works, though: `obj == null` is false for a live object and true for one the host never
produced, so `!= null` is the presence test to use.

## 12. Gotchas worth knowing before writing script

- **Only `bool` is truthy.** `if (x)` and `while (x)` take the branch **only** when `x` is a
  boolean `true`. `if (1)`, `if ("a")`, `if (someMap)` are all false - there is no truthiness
  conversion. Write the comparison out: `if (count > 0)`, `if (name != "")`.
- **Declare before use.** The compiler makes one top-to-bottom pass. A function must be defined
  above its first call site or the file - and every file importing it - fails with
  `unknown identifier`.
- **Native arity is not checked at compile time.** A wrong count surfaces at run time as
  `invalid function call`.
- **`/` on two ints is integer division**, C semantics. Use `tofloat` when you want a real quotient.
- **`..` concatenates, `+` adds.** `1 .. 2` is `"12"`.
- **`elif` is removed**; use `else if`.
- **Lambdas capture by value.** Read *Captured anonymous functions* in `ReadMe.md` before relying on
  shared state, recursion, or writes reaching the enclosing frame.
- **Reference cycles need the host.** Nothing in script reclaims a cycle; the host must call
  `CollectCycles`. A map holding a lambda that captured that map is a cycle.
