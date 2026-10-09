# NeoScript Script API Reference

Everything a `.ns` script can call: keyword intrinsics, the three built-in modules
(`math` / `system` / `coroutine`), and the method sets of `string` / `list` / `array` / `map` / `set` / `async`.

Start with [ScriptAuthoring.md](ScriptAuthoring.md) for runnable examples, declaration and
module rules, collection semantics, and practical checks before writing script.

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
  module table. `import name as alias;` renames it locally. Import is a compile-time include;
  repeated imports within one compilation reuse the module. Separate runtime instances do not
  share its globals.
- Aliases expose module functions and `export const` values. Use a getter function to pass
  a module variable; `alias.variable` is unsupported. Plain `const` names remain file-local.
- `alias.CONSTANT` is resolved during compilation and also works in `const` initializers
  and `switch case` expressions. Import first; assignment and increment are errors. Names
  are not imported unqualified. Recompile consumers after an exported constant changes.

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
| `format(pattern: string, ...)` | `string` | Typed formatting; same rules as `pattern.format(...)` below |

`print` accepts at most two arguments; three or more raise `invalid function call`. When the
host installs a print sink (`NeoVMSystem::m_pFunPrint`), both forms go through it as one string and no
newline is added.

### 2.1 String formatting

Both `format("%04d: %.2f %s", 7, 1.25, "kg")` and
`"%04d: %.2f %s".format(7, 1.25, "kg")` return `"0007: 1.25 kg"`.
No import is needed. The pattern is parsed by the VM, not passed unchecked to C varargs.

| Form | Contract |
| --- | --- |
| `%d` | int only; signed decimal |
| `%f` | int or float; fixed-point, default six fractional digits |
| `%s` | string only; use `tostring` explicitly for other values |
| `%%` | literal percent, consumes no argument |
| `%8d`, `%8s` | minimum width, left-padded with spaces |
| `%-8s` | left alignment, right-padded with spaces |
| `%04d`, `%08.2f` | numeric zero padding, after a minus sign |
| `%.2f` | two fractional digits; rounding follows stream formatting |
| `%.2s` | at most two UTF-8 characters; never cuts a character's bytes |

Width and string precision count characters, not display columns. Numeric formatting uses the
classic locale (`.` decimal point). Width does not truncate. `-` takes precedence over `0`.
Precision is supported only for `%f` and `%s`; zero padding is numeric only. Unsupported forms
(including `*`, length modifiers and `%n`), repeated flags, incomplete patterns, out-of-range
width/precision, wrong types, and missing or extra arguments are runtime errors. Literal
backslash-u text written in a pattern is not decoded again at runtime.

---

## 3. `math`

### 3.1 Scalar

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.abs(x: float)` | `float` | |
| `math.acos(x: float)` | `float` | radians |
| `math.asin(x: float)` | `float` | radians |
| `math.atan(x: float)` | `float` | radians |
| `math.atan2(y: float, x: float)` | `float` | radians, full quadrant; argument order is `(y, x)` like C |
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
| `math.hypot(x: float, y: float)` | `float` | `sqrt(x*x + y*y)` |
| `math.fmod(x: float, y: float)` | `float` | C `fmod`; sign follows `x`. `y == 0` returns `0`, never NaN |
| `math.trunc(x: float)` | `float` | toward zero |
| `math.fract(x: float)` | `float` | `x - floor(x)`, always `0..1` |
| `math.sinh(x: float)` `math.cosh(x: float)` `math.tanh(x: float)` | `float` | |
| `math.Sign(x: float)` | `float` | `-1`, `0`, or `1` |
| `math.Min(a: float, b: float)` `math.Max(a: float, b: float)` | `float` | |
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
| `math.InverseLerp(a: float, b: float, value: float)` | `float` | `(value - a) / (b - a)`, not clamped; `0` when `a == b` |
| `math.Remap(value, inMin, inMax, outMin, outMax)` | `float` | `Lerp(outMin, outMax, InverseLerp(inMin, inMax, value))`, not clamped |
| `math.SmoothStep(edge0: float, edge1: float, x: float)` | `float` | GLSL `smoothstep`: clamped Hermite between the two edges |
| `math.MoveToward(current: float, target: float, maxDelta: float)` | `float` | steps at most `maxDelta` toward `target`; frame-rate independent when `maxDelta = rate * dt` |
| `math.Repeat(x: float, length: float)` | `float` | positive modulo, `0 <= result < length`; `length <= 0` returns `0` |
| `math.PingPong(x: float, length: float)` | `float` | bounces between `0` and `length` |

### 3.3 Angles (radians)

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.WrapAngle(radian: float)` | `float` | wraps into `(-pi, pi]` |
| `math.DeltaAngle(from: float, to: float)` | `float` | shortest signed difference, in `(-pi, pi]` |
| `math.LerpAngle(from: float, to: float, t: float)` | `float` | interpolates along the shortest arc |
| `math.MoveTowardAngle(current: float, target: float, maxDelta: float)` | `float` | turns at most `maxDelta` along the shortest arc; result wrapped |

### 3.4 Distances

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Distance2(x0: float, y0: float, x1: float, y1: float)` | `float` | scalar 2-D distance, handy for x/z on a height map |
| `math.Dot3(a: Vector3, b: Vector3)` | `float` | |
| `math.Length3(v: Vector3)` | `float` | |
| `math.Distance3(a: Vector3, b: Vector3)` | `float` | `sqrt(DistanceSquared3)` |

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
| `system.array(initial: bool\|int\|float, size: int)` | `array` | Dense primitive array. `initial` chooses the element type and fills the initial elements; `size` must be non-negative |
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

`yield` (the keyword) suspends. After the native `resume` callback, the VM switches to the
coroutine before executing the caller's next statement. The coroutine runs until it yields or
finishes, then the caller continues. Sleeping or host execution budgets can suspend the instance.

---

## 6. `string` methods

Call these on a variable, function result, string literal, string constant, or parenthesized expression.
Selectors can continue after a call: `GetWords()[0].len()` and `GetText().split(",")[0].len()` work too.
`"literal".len()`, `TEXT.len()` for a string const, and `("a" .. "b").len()` also work.
Indexes and lengths are UTF-8 **character** counts, not bytes.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `s.len()` | `int` | character count |
| `s.sub(start: int, count: int)` | `string` | clamps `start` to `[0, s.len()]`; clips the count at the end; `count <= 0` returns `""` |
| `s.find(needle: string)` | `int` | character index, `-1` when not found |
| `s.upper()` | `string` | ASCII only - per-byte `::toupper` |
| `s.lower()` | `string` | ASCII only |
| `s.trim()` | `string` | strips **spaces only**, not tabs or newlines |
| `s.ltrim()` | `string` | leading spaces |
| `s.rtrim()` | `string` | trailing spaces |
| `s.replace(find: string, to: string)` | `string` | **first occurrence only**; no match returns the original text |
| `s.replaceAll(find: string, to: string)` | `string` | all non-overlapping literal matches, left to right; no match or empty `find` returns the original text |
| `s.split(sep: string)` | `list` | multi-character separators work; always returns at least one element; pass a non-empty separator |
| `s.format(...)` | `string` | uses `s` as the pattern; see section 2.1 |

These methods return a new value and do not modify `s`. `replaceAll` never searches text
inserted by the replacement. For compatibility, `replace("", x)` still prepends `x` once;
`replaceAll("", x)` leaves the text unchanged. `find("")` returns `0`.
Empty input and `sub(s.len(), count)` return an empty substring. Wrong argument counts
or types are still errors.

Strings are **not** indexable. `s[0]` raises `cannot read by index from string`.
An empty `split` separator currently prevents the search loop from advancing; reject it before calling.

String literals support `\uXXXX` with exactly four hex digits. A high-surrogate escape must be
followed by a low-surrogate escape, e.g. `"\uD83D\uDE00"` is one emoji. Both quote styles work.
Malformed hex, lone/mismatched surrogates, and `\u0000` are compile errors: current runtime
string APIs cannot preserve an embedded NUL. `"\\u0041"` keeps the backslash-u text literally.

## 7. `list` methods

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `l.len()` | `int` | |
| `l.resize(n: int)` | `void` | grows with `null`, shrinks by dropping the tail |
| `l.append(value)` | `void` | appends |
| `l.append(value, index: int)` | `void` | inserts **at** `index` - note the value comes first |
| `l.insert(index: int, value)` | `void` | inserts before index; `0 <= index <= l.len()` |
| `l.remove(index: int)` | `var` | returns the removed value, shifts the tail left; `0 <= index < l.len()` |
| `l.sort(cmp: function)` | `void` | stable in-place sort; `cmp(a,b)` returns bool: true when a belongs before b |

There is no `find` or `clear` on lists. Use `l.resize(0)` to clear.
Index with `l[i]`; `l[i] = v` only works for an index that already exists, so grow with
`resize`/`append` first.
Assigning `l[i] = null` keeps the list length and all other indices unchanged; it replaces only
that element's value. This differs from assigning `null` to a map entry, which removes its key.

Invalid insert/remove indices and non-integer indices are runtime errors. Aliases observe the
same list changes. Removal preserves references in the returned value, including nested containers.
Insertion, removal and sorting during foreach invalidate the active iterator.

Sort accepts named functions and capturing lambdas with two arguments. Equal elements keep
their original order. The comparator must return bool and must not modify the list or its elements.
It runs synchronously: yield, sleep, async waits, coroutine.resume and coroutine.close are errors,
as in other synchronous native-to-script callbacks.
Execution time slices resume only after the synchronous sort has finished.
Sorting works on retained snapshots, then writes back only on success. Structural changes
(insert, append, remove, resize to a different size, or another sort) during comparisons abort with
`list was modified during sort`; callback errors and non-bool results also abort. Callback side
effects are not rolled back. List slots are shallow snapshots; contained maps/lists remain references.

### 7.1 Primitive arrays

```cpp
import system;

var flags = system.array(false, 1024);
var ids = system.array(0, 1024);
var weights = system.array(0.0, 1024);
```

`system.array(initial, size)` creates an `array` whose element type is selected by the first
argument. Arrays are reference values like List: assignment shares the same storage. `a.len()` and
`tosize(a)` return the current size. `foreach(var value in a)` is supported and yields a copy of
each value; `foreach(var index, value in a)` is rejected.

- Only integer indexes in `[0, a.len())` are valid. Out-of-range reads and writes are runtime errors.
- `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `++`, and `--` work on an element.
- int and float arrays convert between those two numeric types on assignment. Float to int truncates
  toward zero. Bool arrays accept only bool values.
- `a.append(value)` appends exactly one value. `a.resize(size)` changes the length; newly grown
  elements receive the initial value passed to `system.array`, while shrinking drops the tail.
  A negative size becomes zero, matching `list.resize`.
- There is no middle insertion, remove, or slice operation. Changing the length with `append` or
  `resize` during `foreach` raises `collection was modified during foreach`; assigning an existing
  element is allowed. Bool storage is bit-packed, an implementation detail that does not change
  script indexing.

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

### 8.1 `set`

`s.len()` and `tosize(s)` both return the number of unique elements. Unknown methods and
wrong argument counts raise a runtime error; they never reuse an earlier call's return value.

```cpp
import system;
var s = system.set([1, 2, 3]);
print(tosize(s));               // 3
print(s.len());                 // 3
foreach (var v in s) print(v);  // iteration works
```

Build a set with `system.set(list)`; duplicates collapse. There are no `add`, `remove`, or
`contains` methods; `len()` is its only registered method.

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
| primitive array | `"array"` |
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
- **Declare variables and const before use.** This includes global variables referenced inside
  function bodies. Only named function signatures are collected ahead of use, per module;
  calls to later definitions and mutual recursion work. `fun F(...);` prototypes are rejected.
- **Imported compile errors identify the source file.** Their line and column belong to that
  module, even through nested imports. Concatenated sources still need a host-provided mapping
  back to original files; concatenation alone loses those boundaries.
- **Native arity is not checked at compile time.** A wrong count surfaces at run time as
  `invalid function call`.
- **`/` on two ints is integer division**, C semantics. Use `tofloat` when you want a real quotient.
- **`..` concatenates, `+` adds.** `1 .. 2` is `"12"`.
- **`elif` is removed**; use `else if`.
- **Lambdas capture by value.** Read *Captured anonymous functions* in `ReadMe.md` before relying on
  shared state, recursion, or writes reaching the enclosing frame.
- **Reference cycles need the host.** Nothing in script reclaims a cycle; the host must call
  `CollectCycles`. A map holding a lambda that captured that map is a cycle.
