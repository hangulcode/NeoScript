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
- A declaration without an initializer (`var value;`) starts as `null` whenever it executes; see the [declaration rules](ScriptAuthoring.md#basics).
- `float` is `NS_FLOAT` (single precision). An `int` passed to a `float` parameter is converted
  silently; the result is still `float`.
- Floating-point constants and stored values preserve the sign of zero. `0.0 == -0.0`
  is true and both zeros identify the same map key; their floating-point reciprocals
  are positive and negative infinity, respectively.
- `const` floating-point expressions calculate in double precision, then become float32 values when used at run time. Their rounding can differ from a sequence of runtime float32 operations. Non-finite constant-expression results are compile errors.
- `%` requires integer operands in runtime, `const`, and `case` expressions. Use `math.fmod` for floating-point remainders at run time.
- **Arity is exact.** A wrong argument count or type raises the runtime error
  `invalid function call`. There are no optional parameters except where an entry says so.
- Built-in module functions such as `math.sqrt` require a call; they cannot be stored as function values. Use a script wrapper for callbacks.
- Module functions need `import` first (`import math;`). Keyword intrinsics and `print` do not.
- `import name;` lowercases `name`, looks for `<libPath>/name.ns`, then falls back to the built-in
  module table. `import name as alias;` renames it locally. Import is a compile-time include;
  repeated imports within one compilation reuse the module. Separate runtime instances do not
  share its globals.
- Top-level names are private to their file. `public var`, `public const` and `public fun`
  are visible to importing modules through `alias.name`; `export var` and `export fun` are
  visible to the C++ host by name. The keywords are independent (`export public` for both);
  `export const` and a repeated keyword are compile errors. Different modules may use the
  same private name. Host-declared globals remain visible in every module; export names
  must be unique per program.
- `alias.variable` reads and writes a public variable's global slot directly, so selectors,
  compound assignment and increment work as on a local global and there is no call cost.
  A function without `public` cannot be called or taken as a value through an alias.
- `alias.CONSTANT` is resolved during compilation and also works in `const` initializers
  and `switch case` expressions. Import first; assignment and increment are errors. Names
  are not imported unqualified. Recompile consumers after a public constant changes.

---

## 1. Keyword intrinsics - no import, one opcode each

These are reserved words, not functions. They cannot be stored in a variable or passed as a
callback. Selectors may follow their results: `tostring(5).len()` is valid.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `tostring(x)` | `string` | Same conversion `print` uses. Function values, including captured lambdas, produce `"function"`; null produces `"null"`. |
| `toint(x)` | `int` | Truncates a float toward zero, clamps to `-9223372036854775808..9223372036854775807`, and maps NaN to `0`; also parses a numeric string. |
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

Width is limited to **4,096 characters**, `%f` precision to **1,024 fractional digits**,
and the complete result of one call to **1 MiB (1,048,576 UTF-8 bytes)**. Expansion limits
are checked before padding or numeric conversion; string fields are checked before copying,
and literal text and all fields share the result limit. Exceeding a limit is a runtime error,
never silent truncation. `%s` precision only truncates existing text and remains an int-range
value; its actual output still counts toward the byte limit. These are formatting limits,
independent of compiler image capacity and other runtime string operations.

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
| `math.Repeat(x: float, length: float)` | `float` | positive modulo, `0 <= result < length` for finite inputs, including large magnitudes; zero results are positive zero; `length <= 0` returns `0` |
| `math.PingPong(x: float, length: float)` | `float` | bounces between `0` and positive `length`; preserves small positions and handles large finite lengths |

### 3.3 Angles (radians)

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.WrapAngle(radian: float)` | `float` | wraps finite inputs into `(-pi, pi]`, including large magnitudes |
| `math.DeltaAngle(from: float, to: float)` | `float` | shortest signed difference, in `(-pi, pi]`, including large finite endpoints |
| `math.LerpAngle(from: float, to: float, t: float)` | `float` | interpolates along the shortest arc; `t=0/1` returns the wrapped endpoint, including large finite angles |
| `math.MoveTowardAngle(current: float, target: float, maxDelta: float)` | `float` | turns at most `maxDelta` along the shortest arc; result wrapped; reaches large finite targets when the step covers the remaining arc |

### 3.4 Distances

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Distance2(x0: float, y0: float, x1: float, y1: float)` | `float` | scalar 2-D distance, handy for x/z on a height map |
| `math.Dot3(a: Vector3, b: Vector3)` | `float` | |
| `math.Length3(v: Vector3)` | `float` | |
| `math.Distance3(a: Vector3, b: Vector3)` | `float` | `sqrt(DistanceSquared3)` |

### 3.5 Random

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.srand(seed: int)` | `void` | per-worker generator |
| `math.rand()` | `int` | `0 .. 32767` |
| `math.Rand01()` | `float` | `rand() / 32767` - 15-bit resolution, not a full-precision float |
| `math.RandRange(min: float, max: float)` | `float` | `min + (max - min) * Rand01()` |

### 3.6 Vector and quaternion constructors

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Vector2(x: float, y: float)` | `Vector2` | compiled to the `NOP_VEC_MAKE` intrinsic, not a native call |
| `math.Vector3(x: float, y: float, z: float)` | `Vector3` | same |
| `math.Vector4(x: float, y: float, z: float, w: float)` | `Vector4` | same |
| `math.Quaternion(w: float, x: float, y: float, z: float)` | `Quaternion` | **`w` comes first** |

Direct constructor assignments to local or global variables write the completed result without
an intermediate value copy. Arguments are evaluated and validated before replacing the destination;
existing copies keep their values. An invalid component releases the unfinished vector and leaves
the destination unchanged by the constructor.

Section 10 describes what the resulting values support.

### 3.7 Vector math

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Cross3(a: Vector3, b: Vector3)` | `Vector3` | |
| `math.DistanceSquared3(a: Vector3, b: Vector3)` | `float` | squared, no `sqrt` |
| `math.Normalize3(v: Vector3, fallback: Vector3)` | `Vector3` | normalizes finite components without squared-length overflow; returns required `fallback` when `lengthSq < 1e-8` |
| `math.RotateVectorByQuat(quat: Quaternion, v: Vector3)` | `Vector3` | quaternion first |
| `math.quat_from_basis(right: Vector3, up: Vector3, forward: Vector3)` | `Quaternion` | |
| `math.quat_slerp(a: Quaternion, b: Quaternion, t: float)` | `Quaternion` | |

These accept vector **value types** only. A `list` of three numbers is rejected.

### 3.8 Bit, colour, formatting

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `math.Hash32(value: int)` | `int` | 32-bit integer mix; the result is the signed reinterpretation, so it is often negative |
| `math.ColorRGB(r: float, g: float, b: float)` | `int` | components are **0.0-1.0**, clamped. Packs `0xFFBBGGRR` (alpha forced to 255) |
| `math.ColorARGB(a: float, r: float, g: float, b: float)` | `int` | components are **0.0-1.0**, clamped. Packs `0xAABBGGRR` |
| `math.tostr(value: float)` | `string` | `%.9g`, round-trippable float text. `"" .. 0.1` gives `0.1`; `math.tostr(0.1)` gives `0.100000001` |

Color conversion treats NaN components as `0`, clamps negative infinity to `0` and positive
infinity to `1`, then truncates each scaled component to a byte. `ColorRGB` always keeps alpha
at `255`, including when an RGB component is NaN.

The engine's metadata table declares the `ColorRGB` / `ColorARGB` parameters as `int`. The
implementation reads them as floats and clamps to `0..1` - the metadata string is stale, the
behaviour documented here is what runs.

---

## 4. `system`

Use `import system;` before calling these functions.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `system.jsonEncode(value, sortKeys = false)` | `string\|null` | UTF-8 JSON; optional recursive key sorting. Failure returns null; see `jsonError()` and [JSON rules](JSON.md) |
| `system.jsonDecode(text: string)` | `var` | Strict int64/float32 JSON conversion; any supported root. Failure returns null |
| `system.jsonError()` | `string` | Last JSON conversion error in this execution instance, including data path; empty on success |
| `system.time()` | `int` | Unix seconds |
| `system.date(format: string, time: int)` | `string` | Local-time formatting; UTF-8 output limited to 79 bytes; invalid formats or oversized output raise an error |
| `system.clock()` | `float` | `clock() / CLOCKS_PER_SEC` in seconds. Use for deltas, not wall time |
| `system.array(initial: bool\|int\|float\|Vector2\|Vector3\|Vector4, size: int)` | `array` | Dense typed array. `initial` chooses the element type and fills the initial elements; `size` must be non-negative |
| `system.load(source: string, name: string)` | `module` | Compiles `source` at run time. `name` is type-checked but unused. A compile failure raises `invalid function call`; it does not return `null` |
| `system.pcall(m: module)` | `void` | Runs the module's top-level code |
| `system.set(l: list)` | `set` | Builds a set from a list; duplicates collapse |
| `system.aysnc_create()` | `async` | Creates an HTTP request object. **The typo is the real name** |

`system.pcall` is **not** a protected call. A runtime error inside the module propagates to the
caller and aborts it, and there is no success/failure return value. The name is historical.
The call is synchronous: `yield`, `sleep`, or another suspension from that module raises an error.

`system.date` preserves UTF-8 literals and converts localized names to UTF-8. Empty format returns `""`.
Positive timestamps accept signed 64-bit Unix seconds, up to `253402300799` (the end of year 9999),
subject to platform `time_t` limits. For compatibility, negative values in the former signed 32-bit
range retain their unsigned 32-bit interpretation, rather than representing dates before 1970.
Other negative values and unsupported timestamps are rejected. Supported `strftime` directives and modifiers depend on the platform;
unsupported or incomplete directives are rejected before calling the C library.

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
| `coroutine.close(co: coroutine)` | `coroutine` | Returns `co`; closing an already-dead coroutine succeeds as a no-op |

`yield` (the keyword) suspends. After the native `resume` callback, the VM switches to the
coroutine before executing the caller's next statement. The coroutine runs until it yields or
finishes, then the caller continues. Sleeping or host execution budgets can suspend the instance.
Closing the main execution context is an error. Calls from native code back into script, including
sort comparators, are limited to 64 nested calls per worker; exceeding this reports a runtime error.

---

## 6. `string` methods

Call these on a variable, function result, string literal, string constant, or parenthesized expression.
Selectors can continue after a call: `GetWords()[0].len()` and `GetText().split(",")[0].len()` work too.
`"literal".len()`, `TEXT.len()` for a string const, and `("a" .. "b").len()` also work.
Indexes and lengths are UTF-8 **character** counts, not bytes.
A runtime string may contain at most `2147483647` UTF-8 bytes. Concatenation and replacement
check growth before expanding; exceeding the limit raises a runtime error. Formatting has the
smaller limit described in section 2.1. Compiled string literals have separate image limits.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `s.len()` | `int` | character count |
| `s.codepoint(index: int)` | `int` | Unicode scalar at a zero-based character index; `-1` for negative/out-of-range indexes or invalid UTF-8 before or at that index; exactly one integer argument required |
| `s.sub(start: int, count: int)` | `string` | clamps `start` to `[0, s.len()]`; clips the count at the end; `count <= 0` returns `""` |
| `s.find(needle: string)` | `int` | character index, `-1` when not found |
| `s.upper()` | `string` | ASCII only - per-byte `::toupper` |
| `s.lower()` | `string` | ASCII only |
| `s.trim()` | `string` | strips **spaces only**, not tabs or newlines |
| `s.ltrim()` | `string` | leading spaces |
| `s.rtrim()` | `string` | trailing spaces |
| `s.replace(find: string, to: string)` | `string` | **first occurrence only**; no match returns the original text |
| `s.replaceAll(find: string, to: string)` | `string` | all non-overlapping literal matches, left to right; no match or empty `find` returns the original text |
| `s.regexMatch(pattern: string [, ignoreCase: bool])` | `bool` | whole-string ECMAScript match; case-sensitive by default |
| `s.regexSearch(pattern: string [, ignoreCase: bool])` | `list` or `null` | first match: element 0 is the full match, subsequent elements are capture groups; unmatched optional groups are empty strings |
| `s.regexReplace(pattern: string, replacement: string [, ignoreCase: bool])` | `string` | replaces every non-overlapping match; `$&` is the full match, `$1` etc. are captures, `$$` is a literal dollar |
| `s.split(sep: string)` | `list` | multi-character separators work; always returns at least one element; pass a non-empty separator |
| `s.format(...)` | `string` | uses `s` as the pattern; see section 2.1 |

Regex methods use C++11 `std::regex` with ECMAScript grammar and the classic locale. Patterns
are compiled per call. They operate on UTF-8 **bytes**, not Unicode code points: literal UTF-8 text
matches, but `.`, character classes, and case folding are not Unicode-aware. Use doubled backslashes
in script strings (`"\\d+"`). Empty matches are supported and replacement always advances.
Invalid patterns, wrong argument types, and regex complexity errors are runtime errors.
The native regex engine cannot yield or obey a VM execution budget: regex calls are rejected during
time-limited execution, like collection sorting. Without a limit, complex patterns can take a long time.

These methods return a new value and do not modify `s`. `replaceAll` never searches text
inserted by the replacement. For compatibility, `replace("", x)` still prepends `x` once;
`replaceAll("", x)` leaves the text unchanged. `find("")` returns `0`.
Empty input and `sub(s.len(), count)` return an empty substring. Wrong argument counts
or types are still errors.

Strings are **not** indexable. `s[0]` raises `cannot read by index from string`.
An empty `split` separator raises `split separator must not be empty` before a result list is allocated.

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
| `l.pop()` | `var` | removes and returns the last element; empty lists are runtime errors |
| `l.removeSwap(index: int)` | `var` | removes and returns the indexed element, moves the last element into its place; `0 <= index < l.len()` |
| `l.sort(cmp: function)` | `void` | stable in-place sort; `cmp(a,b)` returns bool: true when a belongs before b |
| `l.find(value)` | `int` | first matching index, or -1; uses script `==` |
| `l.contains(value)` | `bool` | whether `find(value)` succeeds |
| `l.reserve(n: int)` | `void` | reserves capacity without changing length; negative n is an error |
| `l.clear()` | `void` | removes all elements while retaining capacity; aliases observe the change |
| `l.clone()` | `list` | new list with shallow-copied elements |

Search does not coerce values to the container's element type. Numeric int/float equality,
NaN, vector value equality, and reference identity follow script `==`.
`clone()` creates independent outer storage; nested collections stay shared and vectors retain
copy-on-write value semantics. A self-reference in the source still points to the source in its clone.
These shallow-copy rules also apply to arrays, maps and sets. `clear()` on any collection retains
its allocated capacity and invalidates an active foreach iterator if the collection was nonempty.
Calling `clear()` on an empty collection does nothing.
Index with `l[i]`; `l[i] = v` only works for an index that already exists, so grow with
`resize`/`append` first.
Assigning `l[i] = null` keeps the list length and all other indices unchanged; it replaces only
that element's value. This differs from assigning `null` to a map entry, which removes its key.

Invalid insert/remove indices and non-integer indices are runtime errors. Aliases observe the
same list changes. Removal preserves references in the returned value, including nested containers.
Insertion, removal (including `pop` and `removeSwap`) and sorting during foreach invalidate the active iterator.
`removeSwap` does not preserve order and moves at most one element, unlike `remove`, which shifts
the remaining tail. Removing the last element requires no replacement. Both `pop` and `removeSwap`
retain capacity and return the removed value with its normal reference/vector value semantics.
A null element can be returned normally; an empty `pop` is a distinct runtime error.

Sort accepts named functions and capturing lambdas with two arguments. Equal elements keep
their original order. The comparator must return bool and must not modify the list or its elements.
It runs synchronously: yield, sleep, async waits, coroutine.resume and coroutine.close are errors,
as in other synchronous native-to-script callbacks.
When an execution time limit is enabled (`timeoutMs >= 0`, including `timeoutMs == 0`),
`list.sort` fails with `list.sort is not allowed during time-limited execution` before calling
the comparator or modifying the list. The same instance can sort in a later unlimited call.
With no time limit, sort remains synchronous; a nonterminating comparator will not return.
Sorting works on retained snapshots, then writes back only on success. Structural changes
(insert, append, remove, resize to a different size, or another sort) during comparisons abort with
`list was modified during sort`; callback errors and non-bool results also abort. Callback side
effects are not rolled back. List slots are shallow snapshots; contained maps/lists remain references.

### 7.1 Dense arrays

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
- All element types support `=`. Numeric elements support `+=`, `-=`, `*=`, `/=`, `%=`, `++`, and `--`.
- Vector elements require exactly the array's component count. `a[i]` and `foreach` return vector
  values: modifying a copied local vector does not modify the array. `a[i][j]` component assignment,
  `+=`, `-=`, `*=`, `/=`, `++`, and `--` write back to the array. Whole-vector compound assignment
  is unsupported; use `a[i] = a[i] + delta`. Array length counts vectors, not float components.
- Vector elements retain ordinary `VAR_VEC` values, sharing storage until a write requires a copy
  (the same copy-on-write semantics as vectors in lists). Shrinking releases removed values.
  `system.array(math.Quaternion(w,x,y,z), n)` creates a Vector4 array with the same w,x,y,z order; there is no automatic
  conversion to a host quaternion layout. Resizing uses the vector initial value captured at creation.
- int and float arrays convert between those two numeric types on assignment. Float to int truncates
  toward zero, clamps to the int64 range, and maps NaN to zero, matching `toint`. Bool arrays accept only bool values.
- `a.append(value)` appends exactly one value. `a.resize(size)` changes the length; newly grown
  elements receive the initial value passed to `system.array`, while shrinking drops the tail.
  A negative size becomes zero, matching `list.resize`.
- There is no middle insertion, remove, or slice operation. Changing the length with `append` or
  `resize` during `foreach` raises `collection was modified during foreach`; assigning an existing
  element is allowed. Bool storage is bit-packed, an implementation detail that does not change
  script indexing.

| Additional array method | Returns | Notes |
| :--- | :--- | :--- |
| `a.find(value)` | `int` | first matching index, or -1; script `==`, without assignment conversion |
| `a.contains(value)` | `bool` | whether `find(value)` succeeds |
| `a.reserve(n: int)` | `void` | reserves capacity without changing length; negative n is an error |
| `a.clear()` | `void` | removes all elements, retaining capacity and the creation initializer |
| `a.clone()` | `array` | independent storage, same element type, values and creation initializer |
| `a.fill(value)` | `void` | overwrites existing elements using the usual array assignment conversion |

`fill` validates the value even for an empty array; a type error leaves the array unchanged.
It does not change the length or the creation initializer used by subsequent `resize` growth.
List/array `reserve` and array `fill` do not invalidate foreach iterators. A clone need not retain
unused reserved capacity. For example, an int array containing `1` matches `1.0`, but not `1.5`,
when searched; `fill(1.5)` stores `1` under normal numeric assignment rules.

## 8. `map` methods

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `m.len()` | `int` | |
| `m.contains(key)` | `bool` | checks key presence using the existing map key equality rules |
| `m.remove(key)` | `bool` | true if an entry was removed, false if absent |
| `m.clear()` | `void` | removes all entries while retaining capacity |
| `m.clone()` | `map` | new map with shallow-copied keys and values; iteration order is unspecified |
| `m.reserve(n: int)` | `void` | pre-sizes the bucket array |
| `m.keys()` | `list` | **order is unspecified** (bucket order), not insertion order |
| `m.values()` | `list` | same order as `keys()` |
| `m.sort(cmp: function)` | `void` | sorts the map's **values** in place |

`m.sort` details:

- Sorted values are assigned in the map's current iteration order. Keys stay in place;
  integer keys do not imply numeric order. Read `m.values()` to obtain the sorted sequence.
- `cmp(a, b)` receives two values and must return `bool`; anything else counts as `false`.
- The comparator may be a named function or a capturing lambda.
- If the callback inserts or erases keys the sort aborts with the runtime error
  `map was modified during sort`. Failed sorts never write the sorted snapshot back; callback side effects remain.
- Comparator errors abort immediately. Sorting is synchronous and rejects yield, sleep, async waits,
  coroutine switching, and execution with a host time limit, as list sorting does.
- A **capturing** comparator writes back into its own capture storage, not into the enclosing
  function, so counting hits inside a comparator does not work. See *Captured anonymous functions*
  in `ReadMe.md`.

Access with `m["key"]` or `m.key`. Assigning to a missing key inserts it.
`remove(key)` is the explicit equivalent of deleting via `m[key] = null`, with a boolean result.
Removing an existing key invalidates an active foreach iterator; removing a missing key does not.
`contains` does not modify the map or coerce numeric key types.

Reference keys (list, map, set, array, function, closure, coroutine, runtime module, async,
and host object) use the same identity rule as `==`/`!=`. A container's contents do not
determine its identity. Host objects compare by registered type and host handle, so returning
the same object again preserves lookup identity even when the return wrapper is new.
Floating-point keys `+0.0` and `-0.0` are the same key in both maps and sets.
Numeric keys remain type-sensitive: key `1` and key `1.0` are distinct despite `1 == 1.0`.
Vector keys use component count and exact component values, just like vector `==`.
Separately constructed equal vectors find/overwrite/remove the same key; `+0.0` and `-0.0`
are equivalent. Changing the original vector or a vector returned by `keys()` does not change
the stored key. NaN retains floating-point comparison rules (`NaN != NaN`); do not use
vectors containing NaN as lookup keys. No approximate comparison is performed.

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

Build a set with `system.set(list)`; duplicates collapse.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `s.add(value)` | `bool` | true if inserted, false if already present |
| `s.remove(value)` | `bool` | true if removed, false if absent |
| `s.contains(value)` | `bool` | membership under set key equality |
| `s.clear()` | `void` | removes all elements while retaining capacity |
| `s.clone()` | `set` | new set with shallow-copied elements; iteration order is unspecified |
| `s.values()` | `list` | new list of all elements, in unspecified order; shallow copies |

`values()` does not modify the set and may be called during foreach. Changing the returned list
structure does not change the set. Nested containers remain shared; returned vectors use copy-on-write,
so changing one does not change a stored set key.

Adding a new element, removing an existing element, or clearing a nonempty set invalidates
an active foreach iterator. Duplicate adds and missing removes leave it valid.
Set membership preserves type-sensitive key equality: `1` and `1.0` are distinct, unlike
list/array search. Null is a valid set element.
Reference elements use the identity rules described for map keys above; two references to
one host object or container collapse, while separate containers with equal contents remain distinct.
Vector elements follow the value-key rules above, so equal vectors collapse even with separate storage.

## 9. `async` methods

Create the object with `system.aysnc_create()`.

| Signature | Returns | Notes |
| :--- | :--- | :--- |
| `a.add_header(name: string, value: string)` | `void` | call **before** `get`/`post`; accumulates |
| `a.get(timeoutMs: int, url: string, callback: function)` | `void` | `timeoutMs == -1` means no timeout |
| `a.post(timeoutMs: int, url: string, body: string, callback: function)` | `void` | same timeout rule |
| `a.wait()` | `bool` | blocks while pending; `false` before a request starts or on timeout, `true` once completed. Completion does not imply HTTP success; inspect the callback result |
| `a.close()` | `void` | |

- The callback signature is `fun(ok: bool, result: string)`. On failure `result` carries the error
  text.
- `get` / `post` require the object to be in the `READY` state - freshly created. `close()` is currently a no-op; create a new object for each request.
- `get`, `post`, and `wait` are all rejected inside a nested native -> script call
  (`RTE_NESTED_NOT_ALLOWED`), together with `sleep` and `yield`. See *Synchronous native-to-script
  callbacks* in `ReadMe.md`.
- The core delegates transport to the host's `IHttpTransport`; it contains no HTTP/TLS implementation.
  Without a transport, the callback receives `false, "HTTP transport is not configured"`.
- The console connects libcurl and accepts `http://` and `https://`, with certificate and hostname
  verification enabled. It returns the response body for all HTTP status codes (including 4xx/5xx);
  `ok` describes transport success, not HTTP status. Redirects are not followed automatically.
- Host callbacks execute on the VM thread after the background transport completes. Runtime destruction
  waits for an in-flight transport call; host implementations must honor the request timeout.

---

## 10. Vector and quaternion value types

`Vector2` / `Vector3` / `Vector4` / `Quaternion` are **value types**, not lists. Assignment preserves
value semantics: storage may be shared internally, but modifying a component detaches a shared value.

| Operation | Supported | Notes |
| :--- | :--- | :--- |
| `v[0]`, `v[1]`, `v[2]`, `v[3]` | yes | read and write; out of range raises `vector index out of range` |
| `v[i] += n`, `-=`, `*=`, `/=` | yes | integer index and int/float operand; preserves value-copy semantics; `%=` is unsupported for float components |
| `list[i][j] = n`, `obj.Position[j] += n` | yes | a component write through a container element or a native property is written back to that element or property; through a native property it costs one getter and one setter per statement, so batch several components in a local |
| `v[i]++`, `v[i]--`, `++v[i]`, `--v[i]` | yes | postfix returns the old value, prefix the updated value, including when the result is placed in a list/map literal |
| `v.x`, `v.y`, `v.z`, `v.w` | **no** | named component access does not exist - index instead |
| `a + b`, `a - b` | yes | component-wise; both sides need the same component count |
| `a * scalar`, `a / scalar` | yes | component-wise |
| `a == b`, `a != b` | yes | same component count and exact component values; also used by map keys and set elements |
| `type(v)` | yes | `"Vector2"` / `"Vector3"` / `"Vector4"` - a quaternion also reports `"Vector4"` |
| `"" .. v` | yes | formats as `(3, 4)` / `(1, 2, 3)` / `(1, 0, 0, 0)` |
| `tosize(v)` | yes | component count: `2` / `3` / `4` / `4` |
| `foreach` | **no** | raises `foreach does not support vector` |

Operands and call arguments are evaluated from left to right. A scalar or vector argument keeps
the value observed at that point; a container argument retains a shared reference, not a deep copy.
Function-value calls capture the callee before evaluating arguments. Dot calls retain the receiver
but resolve its member after argument evaluation: changing `m.f` in an argument can change the
function called by `m.f(...)`; use `m["f"](...)` to capture the stored function beforehand.
An indexed assignment resolves its containing path after its right-hand side has run, subject
to the retained results described below.
If that side changes an index variable or replaces the element, the write uses the current value,
including the other vector components. Parentheses around the target do not change evaluation order.
Computed index expressions and calls that produce the target container run once before the
right-hand side, and their results are retained. Plain root or index variables are read for the write.
For example, `Get()[0] += Change()` retains the container returned by `Get()`.
List/map/vector-array component writes can modify the stored vector directly; shared vectors still
detach on modification. Internal references do not escape the write or become script values.
Native vector properties still call their getter and setter once per successful component write.
WREF precedes an ordinary collection write. Stored vectors consume both atomically; other values
use normal write dispatch. Native vector properties retain a resumable getter/setter boundary.
There is no separate vector write-back opcode. Regenerate images older than bytecode version 0134 for 64-bit integers, local initialization, ordered operand evaluation, deferred assignment paths, and the earlier signed-zero and foreach fixes.
A selector immediately following an indexed assignment keeps the assigned slot writable:
`(a[0][0] = math.Vector3(1,2,3))[1] = 9` updates `a[0][0]` to `(1,9,3)`.
Reading that result into a local still makes an independent vector value via copy-on-write.

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
| dense array | `"array"` |
| set | `"set"` |
| script function or lambda | `"function"` |
| coroutine | `"coroutine"` |
| module (`system.load`) | `"module"` |
| async object | `"asynchronous"` |
| Vector2 / Vector3 / Vector4 | `"Vector2"` / `"Vector3"` / `"Vector4"` |
| Quaternion | `"Vector4"` - there is no distinct quaternion type at run time |
| bound native object (`RegisterObject` / `BindObject`) | `"null"` - **not distinguishable from a real null** |

`type(FunctionName)` and `type(fun() { })` both return `"function"`, just like a function
stored in a variable. Named functions (including imported module functions) can be used
directly as map keys, comparison operands, and string-concatenation operands. These uses
have the same behavior as first assigning the function to a variable. `==`/`!=` and map
keys compare function identity; they do not call the function. Capturing lambdas create
separate closure identities when evaluated. Function declarations are not writable:
`FunctionName = ...`, `++FunctionName`, and `FunctionName++` are compile errors.

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
- **`import`, `const`, `public` and `export` are file-scope only.** Inside a block or a function
  the compiler reports `'<keyword>' is only allowed at file scope`.
- **Imported compile errors identify the source file.** Their line and column belong to that
  module, even through nested imports. Concatenated sources still need a host-provided mapping
  back to original files; concatenation alone loses those boundaries.
- **Native arity is not checked at compile time.** A wrong count surfaces at run time as
  `invalid function call 'Method'`. Native property failures identify `read` or `write` and
  the property name. Names remain available without debug info; explicit host error messages
  and error codes are preserved. Calling a non-callable value reports its type.
- Script `int` values are signed 64-bit. Addition, subtraction, multiplication, negation, and increment/decrement wrap to the low 64 bits.
- **`/` on two ints truncates toward zero.** Use `tofloat` when you want a real quotient.
  Integer `/`, `%`, `/=`, and `%=` report a runtime error for a zero divisor or `INT64_MIN` with
  divisor `-1`, without modifying the destination. The same invalid operations in `const` or
  `case` expressions are compile errors. Ordinary literal expressions retain runtime errors.
  Floating-point division retains IEEE infinity/NaN behavior. Ordered comparisons (`<`, `<=`, `>`, `>=`)
  involving NaN are false in expressions and branches; negation and short-circuit operators preserve that result.
- `<<`, `>>`, `<<=`, and `>>=` require integer operands and a shift count in `0..63`.
  An invalid count is a runtime error before changing the destination; `const` and `case` expressions reject it at compile time.
  Left shifts retain the low 64 bits, and right shifts extend the sign bit, including for negative operands.
- **`..` concatenates, `+` adds.** `1 .. 2` is `"12"`.
- **`elif` is removed**; use `else if`.
- **Lambdas capture by value.** Read *Captured anonymous functions* in `ReadMe.md` before relying on
  shared state, recursion, or writes reaching the enclosing frame.
- **Reference cycles need the host.** Nothing in script reclaims a cycle; the host must call
  `CollectCycles`. A map holding a lambda that captured that map is a cycle.
