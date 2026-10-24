# NeoScript Script Authoring Guide

> **Line-limit exception: only this document (`docs/ScriptAuthoring.md`) is exempt from line limits.**
> The limits on other documents, including `AGENTS.md`, still apply.

A practical starting point for people and AI writing `.ns` scripts.
This guide describes the compiler and standard library in the current repository.
Do not infer supported features or behavior from syntax that resembles another language.
When looking for a feature, consult both the examples here and the [full API reference](API.md).

## 1. Rules to check before writing a script

| Topic | Required rule |
| --- | --- |
| Function declaration order | Named functions in the same module can be called before their definitions. A forward declaration such as `fun F();` is an error. |
| Variable declaration order | Declare global and local variables and `const` before use, including uses inside function bodies. |
| Module access | Only `public` functions, constants, and variables are accessible as `alias.F()`, `alias.NAME`, and `alias.value`. `export` exposes declarations to the C++ host, not to other modules. |
| Conditions | Conditions in `if` and `while` must be `bool`. Numbers and strings are not automatically treated as true. |
| Numbers | `int` is a 32-bit integer; `float` is a 32-bit floating-point number. `/` between integers performs integer division. |
| Indices | Lists, dense arrays, and vectors use zero-based indices. Strings do not support `s[i]`. |
| Strings | Concatenate with `..`, extract with `sub(start, count)`, and check for `-1` when `find` fails. |
| Assigning `null` | Keeps the slot in a list; removes the key from a map. |
| Copying | Assigning a list, map, or dense array shares its storage. Assigning a vector copies its value. |
| Reference comparison | `==` on containers, functions, and engine objects checks identity. Separate containers with equal contents are not equal. |
| Anonymous functions | Capture local values by value. Updating a captured scalar does not update the outer local variable. |
| API names | Case and argument count matter. Search `API.md` before concluding that a feature is unavailable. |
| Validation environment | Distinguish host APIs from the NeoScript standard API. The console does not provide game engine bindings. |

Quick navigation:

- [Basic syntax and numbers](#basics), [Functions and declaration order](#functions), [Modules and public constants](#modules)
- [Branches, loops, and switch](#control), [Containers](#containers), [Strings](#strings)
- [Function values and closures](#closures), [Math, vectors, and random numbers](#math), [Coroutines](#coroutines)
- [Other features](#other), [Running scripts and diagnosing errors](#verification), [Limits and checklist](#checklist)

Unless stated otherwise, each example can run as a separate file.
The `cpp` code-fence label enables syntax highlighting; the examples are NeoScript, not C++.
The `argument: type` notation in API tables is descriptive. Actual function parameters use `var name`.

<a id="basics"></a>
## 2. Basic syntax and numbers

End statements with `;` and enclose blocks in `{ ... }`. Use `//` and `/* ... */` for comments. Comments separate tokens; block comments must end with `*/`.
Identifiers and function names are case-sensitive. Use lowercase file and resource names.
Save source files containing Korean text as UTF-8 with a BOM, as required by the repository rules.

```cpp
var count = 3;
var label = "apple";
var enabled = true;
var optional;                     // Initially null

fun Describe(var name, var amount)
{
    return name .. ": " .. amount;
}

print(Describe(label, count));     // apple: 3
print(optional == null);          // true
```

A declaration without an initializer starts as `null` each time execution reaches it, including
inside a loop. This initialization is retained in both debug and non-debug builds, even when
later statements assign a value before it is read.

Common operators include arithmetic `+ - * / %`, comparisons `== != < <= > >=`, logical `&& ||`,
and bitwise `& | ^ ~ << >>`. Assignment, `+= -= *= /= %=`, and `++ --` are also available.
Use explicit comparisons in logical expressions and parentheses to make mixed operations clear.
`&&` and `||` short-circuit: the right operand is not evaluated once the result is known.
Other operands and call arguments are evaluated left to right. Each scalar or vector value is
fixed before the next operand runs. Container arguments retain shared storage, so later mutation
through an alias remains visible. A later reassignment of the original variable does not replace the argument.
Function-value calls capture the callee before arguments. Dot calls retain their receiver but look
up the member after arguments: if an argument changes `m.f`, `m.f(...)` calls the new member.
Use `m["f"](...)` when the stored function must be captured before argument evaluation.

```cpp
var count = 2;
var capacity = 5;
print(count / capacity);             // 0
print(tofloat(count) / capacity);    // 0.4
print("count=" .. count);            // count=2
print((count > 0) && (count < capacity)); // true
```

- `%` and `%=` require integer operands. Floating-point `%` is also rejected in `const` and `case` expressions; use `math.fmod(x, y)` at run time.
- Integer `/`, `%`, `/=`, and `%=` fail with a runtime error when the divisor is zero or
  the operands are `-2147483648` and `-1`. The destination is unchanged on failure.
  In `const` and `case` expressions, these are compile errors. Floating-point division
  continues to produce infinity/NaN for zero divisors. Ordered comparisons with NaN are false in both expressions and branches.
  Unary `!` accepts boolean literals directly (`!true`, `!false`).
- Bit shifts require two integer operands and a count in `0..31`; invalid counts are runtime errors
  (`const` and `case` expressions report compile errors). Failed compound shifts leave the destination unchanged.
  Left shifts retain the low 32 bits; right shifts extend the sign bit.
- For floating-point division, convert one operand with `tofloat` **before dividing**.
  `tofloat(count / capacity)` only converts the already-truncated result.
- `float` is single precision. Converting a large integer ID to float and back may change its value.
  Float-to-int conversion truncates toward zero, clamps overflow/infinity to the int32 endpoints,
  and converts NaN to zero. Integer literals outside int32 are compile errors.
  `const` floating-point expressions deliberately calculate in double precision before runtime
  float32 conversion: `(16777216.0 + 1.0) - 16777216.0` is `1` as a const expression but `0`
  when evaluated step by step at run time. Non-finite const results are compile errors.
  Constants, assignments, and container values preserve signed zero. `0.0 == -0.0` is true,
  but `1.0 / 0.0` is positive infinity and `1.0 / -0.0` is negative infinity.
  The two zeros still identify the same map key.
- Use `..`, not `+`, for string concatenation. It converts values to strings and joins them.
- Function values, including captured lambdas, become `"function"` through `tostring`, string concatenation, and `print`. Null remains `"null"`.
- `toint`, `tofloat`, `tostring`, `tosize`, and `type` require no import.
  They are keywords and cannot be stored in variables or passed as function callbacks.
- In the default console, `print(value)` prints a line. To print several values,
  concatenate them first, as in `print("x=" .. x .. ", y=" .. y)`.
  `print(a, b)` concatenates two values but does not append a newline in the default console.
  Three or more arguments are unsupported. A host-provided print function may use a different newline policy.

<a id="functions"></a>
## 3. Functions are collected first; variables follow declaration order

### Named functions and mutual recursion

The compiler collects function names and argument information for each module first.
Calls and function-value references may precede definitions. Mutual recursion needs no forward declarations.

```cpp
print(IsEven(10));                  // true

fun IsEven(var n)
{
    if (n == 0) return true;
    return IsOdd(n - 1);
}

fun IsOdd(var n)
{
    if (n == 0) return false;
    return IsEven(n - 1);
}
```

A declaration without a body, such as `fun IsEven(var n);`, is a **compile error**.
Provide a single function definition. Both direct and function-value calls must use the exact argument count.
A variable and a named function cannot share a name in the same module. Local variables and parameters
shadow module aliases. Named nested functions cannot read or write an enclosing function's locals.
Collecting declarations does not reorder imports or global initialization.

### Declare globals before function bodies that use them

```cpp
var score = 10;

fun AddScore(var amount)
{
    score += amount;
    return score;
}

print(AddScore(5));                 // 15
```

The following example is **intentionally invalid**. It fails because of declaration order, even if called later.

```cpp
fun ReadScore() { return score; }   // Error: score has not been declared here
var score = 10;
```

The same rule applies to local variables and `const`.
Distinguish where a call is allowed from when the required values have been initialized.
Globals initialize in source order; avoid initialization-time calls that depend on state initialized later.

Named nested functions also belong to the module's function namespace and do not capture outer locals.
Use [anonymous functions](#closures) to capture local values.

### Chain selectors after calls

```cpp
fun MakeItems() { return [10, 20, 30]; }
fun MakeText() { return "red,blue"; }
fun MakeRecord() { return {"items": [7, 8]}; }

print(MakeItems().len());            // 3
print(MakeItems()[1]);               // 20
print(MakeText().split(",")[1].len()); // 4
print(MakeRecord().items[0]);         // 7
```

`f().member`, `f()[i]`, and indexing after method calls are supported.
There is no need to introduce a temporary variable merely to work around these forms.
The same selectors work on string literals (`"abc".len()`), string constants (`TEXT.len()`),
and parenthesized expressions (`("a" .. "b").len()`). Strings themselves do not support indexing; use `sub(i, 1)`.

<a id="modules"></a>
## 4. Modules, global names, and public constants

### Imports are resolved at compile time

`import settings as cfg;` finds `settings.ns` in the loader's library path
and exposes the module through the alias `cfg`. Do not supply a quoted path or file extension.
Without an alias, `import settings;` allows calls such as `settings.Function()`.
Place imports before code that uses their aliases.

If no file is found, the loader looks for a built-in module with the same name (`math`, `system`, or `coroutine`).
Avoid naming user files after these modules.
Nested imports use the same loader library path.
Do not assume they are resolved relative to the current file's directory. Circular imports are errors.

- Repeated imports of the same module within one compilation reuse the loaded module.
  Two aliases do not create two copies of its state.
- Separately created execution instances do not automatically share module globals.
- A file's top-level `var` is visible only within that file. Different modules can use the same variable name.
  Reading or writing another file's variable by its unqualified name is a compile error.
- Add `public` to declarations exposed to other modules. Read and write a `public var` as `alias.name`.
  Selectors, assignments, and increments such as `cfg.items[0]` and `cfg.count += 1` work just like local module globals.
  Access resolves to the same global slot at compile time, with no function-call cost. Only `public fun` is callable
  through an alias; use `public const` for shared constants.
- `export var` and `export fun` expose declarations to the C++ host, not to other modules. Use `export public`
  when both are needed. Repeated modifiers and `export const` are errors. Exported names must be unique
  across the program. Host-registered global objects are visible to all modules.
- A list, map, or array returned by a function shares the original storage. Make a separate copy when needed.
  When repeatedly reading the same array in a loop, keep the returned reference in a local variable instead of calling a getter each time.
  If the original variable is reassigned to another array, the saved reference still points to the previous array.
- A direct call to a zero-argument function whose only statement returns a variable is optimized to a value copy,
  including builds with debug information. Calls through function values and getters with side effects remain actual calls.

### const and public const

`const` declares a **compile-time constant** at file scope.
Declare it before use, outside blocks and functions; otherwise compilation fails with `'const' is only allowed at file scope`.
Initializers may use integers, floats, strings, bools, null, and expressions involving known constants.
Runtime variables, function calls, lists, and maps cannot be constant initializers.

This example uses two files. Place `settings.ns` in the library directory searched by the loader.

**settings.ns**

```cpp
public const TEXT_SPEED = 22;
public const MODE_PLAY = 2;
const INTERNAL_LIMIT = 100;         // Used only within this file

var settings_volume = 5;
var settings_points = [10, 20];

public fun GetVolume() { return settings_volume; }
public fun SetVolume(var value) { settings_volume = value; }
public fun GetPoint(var index) { return settings_points[index]; }
fun Reset() { settings_volume = 5; }  // Without public, other files cannot call it
```

**main.ns**

```cpp
import settings as cfg;
const DOUBLE_SPEED = cfg.TEXT_SPEED * 2;

print(DOUBLE_SPEED);                // 44
cfg.SetVolume(8);
print(cfg.GetVolume());             // 8
print(cfg.GetPoint(1));              // 20

switch (2)
{
case cfg.MODE_PLAY:
    print("play");                  // play
default:
    print("other");
}
```

| Declaration | Script visibility | Runtime exposure to the C++ host |
| --- | --- | --- |
| `const NAME = ...;` | This file only | None |
| `public const NAME = ...;` | `alias.NAME` | None (`export const` is an error) |
| `var value = ...;` | This file only | None |
| `public var value = ...;` | Read/write through `alias.value` | None |
| `export var value = ...;` | This file only | Global value under a unique name |
| `fun F() { ... }` | This file only | None |
| `public fun F() { ... }` | `alias.F()` | None |
| `export fun F() { ... }` | This file only | Callable function |
| `export public var/fun` | Visible to other modules | Exposed to the host (use when both are needed) |

`import`, `const`, `public`, and `export` are allowed only at file scope. Using them inside a block or function
produces the compile error `'<keyword>' is only allowed at file scope`.

Public constants are not introduced as unqualified names in the importing file. Use `cfg.TEXT_SPEED`.
`cfg.INTERNAL_LIMIT`, `cfg.settings_volume`, and `cfg.Reset()` are errors.
Public constants cannot be assigned, compound-assigned, or incremented/decremented.
To re-export a constant to other modules, explicitly write `public const COPY = cfg.TEXT_SPEED;`.
Changing a constant requires recompiling its consumers' images as well.

When a function returns an entire list or map, the caller receives **the same storage**.
For a read-only interface, return individual elements, as in `GetPoint(index)`, or return a copy.

<a id="control"></a>
## 5. Branches, loops, and switch

### Boolean conditions and short-circuit evaluation

`if (1)`, `if ("text")`, and `if (list)` are not treated as true.
Use comparisons such as `if (count > 0)`, `if (text != "")`, and `if (value != null)`.
Use `else if`; the legacy `elif` syntax has been removed.

```cpp
var item = null;
if ((item != null) && (item.enabled == true))
{
    print("enabled");
}
else
{
    print("unavailable");           // Only this is printed; item.enabled is not evaluated
}
```

### Range for and foreach

```cpp
var values = [10, 20, 30];
var sum = 0;
for (var i in 0, values.len())       // 0, 1, 2
{
    sum += values[i];
}
print(sum);                        // 60

var backward = "";
for (var i in 3, 0, -1)             // 3, 2, 1: inclusive start, exclusive end
{
    backward = backward .. i;
}
print(backward);                   // 321

var sumAgain = 0;
foreach (var value in values) sumAgain += value;
print(sumAgain);                   // 60
```

- The syntax is `for (var i in start, end[, step])`; the default step is 1.
  Do not use `for (var i = 0; i < n; i++)` in the current default build.
- The start is inclusive and the end is exclusive. To traverse a list backward, use `len()-1` to `-1`.
- The step can be a runtime value. A direction inconsistent with the range runs zero iterations; a zero step is a runtime error. An increment beyond the integer boundary ends the range without wrapping.
- Numeric range bounds and steps are converted to int32 before iteration. Fractional parts are
  truncated toward zero, so a step of `0.5` becomes zero and raises an error. Prefer integer inputs.
- A single `foreach` variable over a list, dense array, or set holds the **value**, not the index.
  Use a range `for` when you need an index.
- `foreach (var v in [1,2,3])`, containers returned by functions, and `m.keys()` are supported.
  The source expression is evaluated once on entry, and its value stays alive until the loop ends.
  Reassigning the source variable in the body does not change the collection being traversed.
  Nested loops keep independent sources; they do not deep-copy the containers. Changes through an alias
  still affect the original storage and remain subject to the iteration mutation rules.
- Maps support `foreach (var key in map)` and `foreach (var key, value in map)`.
  Strings yield one character per iteration and accept only one iteration variable.
- Avoid changing the element count during iteration. Changing a dense array's length is an error.
  To remove several map keys, iterate over the list returned by `keys()` and modify the original map.
- `while (condition)` is also available. `break` exits the innermost loop or switch,
  and `continue` advances to the next loop iteration. There is no `do ... while`.

### Constant expressions in case labels

```cpp
const CMD_START = 1;
const CMD_PAUSE = CMD_START + 1;
var command = 2;
var result = "";

switch (command)
{
case CMD_START:
    result = "start";
case CMD_PAUSE, 3:
    result = "pause";
default:
    result = "unknown";
}
print(result);                     // pause
```

Each case body exits the switch when it finishes. **There is no C/C++-style fallthrough.**
The example therefore needs no `break`. Separate multiple values for one body with commas.

Cases accept `bool`, `int`, and `string` literals, constant expressions, `const`, host defines, and public module constants.
A `var` is a runtime variable, even if its value never changes, so it cannot be a case constant.
Float cases, duplicate case values, and duplicate defaults are errors.
Matching is type-strict: `true`, `1`, and `"1"` are different cases.
Follow repository conventions: order cases by value and place default last.

<a id="containers"></a>
## 6. Containers and copy semantics

| Type | Creation | Size | Key differences |
| --- | --- | --- | --- |
| `list` | `[1, "a", null]`, `[]` | `.len()` / `tosize` | Mixed element types; preserves order |
| `map` | `{"hp": 10}`, `{}` | `.len()` / `tosize` | Key-based access; do not rely on iteration order |
| Dense `array` | `system.array(0, n)` | `.len()` / `tosize` | One element type: bool, int, float, or Vector2/3/4 |
| `set` | `system.set(list)` | `.len()` / `tosize` | Removes duplicates at creation; supports iteration |
| Vector | `math.Vector3(x, y, z)` | **`tosize`** | Fixed component count, value copying, dedicated math functions |

### Lists: append, insert, and resize

```cpp
var items = [10, 20, 30];
items[1] = null;
print(items.len());                 // 3
print(items[1] == null);             // true
print(items[2]);                    // 30

items.append(40);                   // Append at the end
items.append(5, 0);                 // Value, position: insert at index 0
print(items[0]);                    // 5
items.resize(6);                    // New slots contain null
print(items[5] == null);             // true
items.resize(0);                    // Empty the list
print(items.len());                 // 0
```

Index assignment requires an existing slot. Grow the list with `append` or `resize` first.
Insert with `insert(position, value)` or the existing `append(value, position)`, remove with `remove(position)`,
and empty with `resize(0)`. There are no `find` or `clear` methods.
`remove` returns the removed value, shifts later elements, and reduces the length. Assigning `null` keeps the slot.
Insertion positions range from 0 to length; removal positions range from 0 to length-1. Invalid types or ranges are errors.

```cpp
var items = [3, 1, 2];
items.insert(1, 9);                 // [3, 9, 1, 2]
print(items.remove(1));             // 9: back to [3, 1, 2]
items.sort(fun(var a, var b) { return a < b; });
print(items[0]);                    // 1
print(items[2]);                    // 3
```

`sort(comparator)` sorts the original list; other references to it see the result.
The comparator takes two arguments and returns `true` if the first should precede the second, or `false` otherwise.
Sorting preserves the original order of equivalent elements. Use `a > b` for descending order.
Comparators run synchronously. `yield`, `sleep`, async waits, and `coroutine.resume` or `close` are errors.
List sorting is a runtime error when the host sets an execution time limit (`timeoutMs >= 0`, including zero).
It is rejected before invoking the comparator or changing the list; the limit is never silently disabled.
Sorting is available only in calls without a time limit, and the comparator must terminate.
Do not modify the list being sorted or its elements inside the comparator. Adding, removing, resizing, or sorting again
is detected and aborts with an error without overwriting the list with the sort result. Callback side effects are not rolled back.
Insertion, removal, and sorting during `foreach` are also errors; perform them after iteration finishes.

### Maps: key lookup and deletion

```cpp
var indexes = [7, 9];
var record = {"name": "oak", "id": indexes[1]};
var byId = {indexes[0]: record};
print(record.id);                   // 9: the element value, not the entire list
print(byId[7].name);                // oak

record.hp = 100;                    // Assignment adds a missing key
record["hp"] = null;               // Remove the key
print(record.hp == null);           // true
print(record.len());                // 2

var t = {};
t.fun = 3;                         // After a dot, fun is a member name
print(t.fun);                       // 3
```

- `m.name` and `m["name"]` access the same string key. Use `m[key]` for a dynamic key.
- Reading a missing key returns `null`. Assigning `null` removes the key instead of storing a value,
  so use a separate flag or wrapper to distinguish a present-but-empty value from a missing key.
- The order returned by `m.keys()` and `m.values()` is not insertion order.
  Do not make game logic or saved output depend on it.
- `m.reserve(n)` reserves capacity in advance.
- Vector keys compare by **component count and exact component values**, not by storage address.
  After `m[math.Vector3(1,2,3)] = "a";`, a newly created Vector3 with the same values can look up, overwrite, or remove the entry.
  Vector2 and Vector3 differ; `+0.0` and `-0.0` are equal. Sets also merge equal vectors.
  Modifying the original vector or a vector returned by `m.keys()` does not change the stored key.
  This is not approximate comparison: normalize coordinates affected by rounding errors before using them as keys. Avoid NaN components, which are unequal even to themselves.
- Floating-point `+0.0` and `-0.0` identify the same map or set key.
- `m.sort(cmp)` sorts **map values**. It is neither the list-sorting API nor a key-sorting API.
  Comparator errors stop sorting immediately; suspension and host time limits are rejected, as for list sorting.
  See the [API reference](API.md#8-map-methods) for the bool comparator result and restrictions on adding or removing keys during sorting.

### Shared references and shallow copies

```cpp
fun CopyList(var source)
{
    var result = [];
    foreach (var value in source) result.append(value);
    return result;
}

var original = [10, 20];
var shared = original;
shared[0] = 99;
print(original[0]);                 // 99

var copied = CopyList(original);
copied[0] = 7;
print(original[0]);                 // Still 99
```

This creates only a new outer list. Nested maps, lists, and arrays still share their storage.
Copy a map similarly by iterating its keys and values. Implement deep copying according to your data structure.
Passing or returning a container does not automatically copy it.

### Reference comparison and reference keys

`==` on lists, maps, sets, and dense arrays checks **whether storage is shared**. Separately created containers
are unequal even when their contents match; their contents are not compared recursively. `!=` is the inverse.
Functions compare by function identity; closures, coroutines, runtime modules, and async objects compare by instance identity.
Host objects with the same registered type and object handle remain equal across repeated lookups or returns.

```cpp
var a = [1, 2];
var alias = a;
var other = [1, 2];
print(a == alias);                 // true
print(a == other);                 // false
var keyed = {};
keyed[a] = "found";
a.append(3);
print(keyed[alias]);               // found: changing contents does not change the reference key
print(keyed[other]);               // null: a different list
```

Map keys and set deduplication use the same identity rules for reference values.
Rebinding a host global object changes only the global slot. Existing copies of its reference and map keys
still refer to the previous object. The host manages object lifetime; `== null` is not an object-liveness check.
Value comparison for numbers, strings, bools, null, and vectors is unchanged. Numeric map/set keys distinguish types,
so keys `1` and `1.0` are separate even though `1 == 1.0` is true.

### Dense arrays: numbers, flags, and vectors

```cpp
import system;

var ids = system.array(0, 3);
var weights = system.array(0.5, 2);
var flags = system.array(false, 2);
ids[1] = 7;
ids[1] += 2;
weights.resize(3);
flags[0] = true;

print(ids[1]);                      // 9
print(weights[2]);                  // 0.5: the initial value specified at creation
print(flags[0]);                    // true
```

The type of `initial` determines the element type. Use `0` for an int array and `0.0` for a float array.
Only integer indices are allowed; out-of-range reads and writes are errors.
Assignments convert between int and float; float-to-int truncates toward zero. Bool arrays accept only bools.
`append(value)` and `resize(n)` are supported, but insertion at an arbitrary position, as with lists, is not.

Vector arrays keep the same reference semantics for the array and value semantics for its elements:

```cpp
import system;
import math;

var initial = math.Vector3(1, 2, 3);
var positions = system.array(initial, 2);
var alias = positions;
alias[0][1] += 5;                    // positions[0] becomes (1, 7, 3)
var p = positions[0];
p[0] = 99;                          // positions[0] is unchanged
positions[1] = p;                    // explicitly store the changed value
initial[0] = 50;
positions.resize(3);                // positions[2] is (1, 2, 3)
```

Vector2, Vector3, and Vector4 arrays require values of the exact component count, including in
`append`. `len()` and `tosize` count vectors. `foreach` yields vector values, so modifying the loop
variable does not update the array. Component writes (`positions[i][j]`) do update it, using the
same evaluation order as other container component writes. Whole-vector compound assignment is
unsupported; use `positions[i] = positions[i] + delta`. Internally, each element is an ordinary
`VAR_VEC` value. Reads share the existing vector; modifying a shared value copies it first, just
like vectors stored in lists. Shrinking the array releases the removed elements. The creation
value is retained separately so later growth restores that value even after element writes.

### Sets: remove duplicates and iterate

```cpp
import system;
var unique = system.set([1, 1, 2, 3]);
print(tosize(unique));              // 3
var sum = 0;
foreach (var value in unique) sum += value;
print(sum);                        // 6
```

Read a set's size with `unique.len()` or `tosize(unique)`.
There are no `add`, `remove`, or `contains` methods; calling a missing method is a runtime error.
For frequent membership checks, additions, or removals, consider a map with `true` values.

<a id="strings"></a>
## 7. Strings: use the built-in search, substring, and replacement methods

| Operation | Usage | Result and boundary behavior |
| --- | --- | --- |
| Length | `s.len()` / `tosize(s)` | Number of UTF-8 characters |
| Character code | `s.codepoint(index)` | Unicode code point at a zero-based index; returns -1 for negative/out-of-range indices or invalid UTF-8 through that position. Requires one integer argument |
| Substring | `s.sub(start, count)` | The second argument is a length, not an end position |
| Search | `s.find(text)` | First position; `-1` if absent; `0` for an empty search string |
| Replace first | `s.replace(find, to)` | Replaces the first match; returns the original if absent |
| Replace all | `s.replaceAll(find, to)` | Replaces all non-overlapping matches; returns the original if absent |
| Split | `s.split(separator)` | Supports multi-character separators. **Do not pass an empty separator** |
| Case conversion | `s.upper()` / `s.lower()` | ASCII only |
| Trim | `s.trim()` / `ltrim()` / `rtrim()` | Removes only spaces (` `), not tabs or newlines |
| Formatting | `format(pattern, ...)` / `pattern.format(...)` | Integers, floats, and strings with width and precision |

```cpp
var text = "A한😀글Z";
print(text.len());                  // 5
print(text.codepoint(2));           // 128512 (😀)
print("각".codepoint(0));            // 44033; Hangul final-consonant index = (code - 44032) % 28
print(text.sub(1, 3));              // 한😀글
print(text.find("😀"));             // 2
print(text.find("missing"));        // -1
print(text.sub(99, 10) == "");      // true
print(text.sub(-2, 2));             // A한
print(text.sub(1, 0) == "");        // true
```

`sub` clamps the start to `[0, length]` and limits the requested count to the remaining characters.
A nonpositive count or a start at the end returns an empty string.
Indices do not count bytes. They also do not combine combining characters or multi-code-point emoji
into a single visible character.
Read one character with `s.sub(i, 1)`; `s[i]` is unsupported.

```cpp
var text = "red/red/red";
print(text.replace("red", "blue"));    // blue/red/red
print(text.replaceAll("red", "blue")); // blue/blue/blue
print(text.replace("none", "x"));      // red/red/red
print(text);                          // The original is still red/red/red

var grow = "aaa";
print(grow.replaceAll("a", "aa"));     // aaaaaa: replacement text is not searched again
print(grow.replaceAll("", "x"));       // aaa
print(grow.replace("", "x"));          // xaaa: existing replace behavior

var fields = "left::right";
print(fields.split("::")[1]);          // right
```

String methods return results without changing the original variable.
Assign the result, as in `text = text.replaceAll(...)`, to retain the change.
Search and replacement use **exact string matching**, not regular expressions or Lua patterns.
The current implementation cannot make progress on `split("")`; reject empty separators before calling it.
Incorrect argument counts or types are not automatically corrected.

Strings support the escapes `\n`, `\r`, `\t`, `\"`, `\'`, and `\\`.
`\uXXXX` specifies a character using four hexadecimal digits. `"\uAE00\uC790"` is `"글자"`,
and `"\uD83D\uDE00"` is one emoji. Literal UTF-8 characters are also allowed.
Invalid hexadecimal digits and unpaired surrogates are compile errors. The NUL character `\u0000`
is also a compile error because the current string API cannot preserve it. To include a literal backslash-u, use `"\\u0041"`.
There is no `match` or regular-expression API. Consider `math.tostr` for float strings intended for storage and restoration.

### Width and decimal precision

```cpp
print(format("id=%04d, weight=%.2f %s", 7, 1.25, "kg")); // id=0007, weight=1.25 kg
print("[%4.2s]".format("한😀글"));                       // [  한😀]
print("done=%d%%".format(100));                          // done=100%
```

Use global `format` or the string `format` method without an import.
`%d` accepts int, `%f` accepts int or float, `%s` accepts string, and `%%` inserts a literal percent sign.
`%f` defaults to six decimal places; change this with forms such as `%.2f`. `%8d` sets a minimum width of 8,
`%04d` pads a number with leading zeroes, and `%-8s` left-aligns text.
`%.2s` reads at most two UTF-8 characters; width also counts characters, not display columns.
The decimal separator is always `.`, regardless of system locale. Invalid formats and extra arguments are errors.
Width is limited to **4,096 characters**, `%f` precision to **1,024 decimal places**,
and the entire result of one call to **1 MiB of UTF-8 (1,048,576 bytes)**. Exceeding a limit is a runtime error.
The total includes all conversions and literal text; checks occur before large conversions, copies, or padding operations.
`%s` precision may span the int range because it truncates an existing string, but actual output remains subject to the byte limit.
These are `format` expansion limits, separate from compiled-image limits and other string-operation limits.
See the [format API](API.md#21-string-formatting) for supported formats.

<a id="closures"></a>
## 8. Function values and anonymous-function captures

Functions can be stored in variables, lists, and maps, or passed to other functions.
Using a function name directly in `type(One)`, `callbacks[One]`, `f == One`, `One == f`, or `"callback: " .. One`
is equivalent to first storing it in a variable. Module functions (`module.Function`) follow the same rules.
Comparison and map keys use function identity without calling the function. Function names themselves cannot be assigned
or incremented/decremented: `One = ...`, `++One`, and `One++` are compile errors.
Create an anonymous function with `fun(var x) { ... }`; arrow-function syntax is not supported.

```cpp
fun Apply(var callback, var value) { return callback(value); }
fun Twice(var value) { return value * 2; }

print(Apply(Twice, 3));              // 6
var addOne = fun(var value) { return value + 1; };
print(Apply(addOne, 3));             // 4
```

### Each anonymous function has independent local captures

```cpp
fun MakeCounter(var start)
{
    var value = start;
    return fun()
    {
        value += 1;
        return value;
    };
}

var first = MakeCounter(10);
var second = MakeCounter(20);
print(first());                     // 11
print(first());                     // 12: the same anonymous function retains its captured state
print(second());                    // 21
```

Surrounding local values are captured by value when the anonymous function is created.
Updating a captured scalar does not update the outer local variable or another anonymous function's capture of it.
Globals access the original global storage rather than a captured copy.

### Share a container to share state between callbacks

```cpp
fun MakeSharedCounter()
{
    var state = {"value": 0};
    var increment = fun() { state.value += 1; };
    var read = fun() { return state.value; };
    return [increment, read];
}

var callbacks = MakeSharedCounter();
callbacks[0]();
callbacks[0]();
print(callbacks[1]());              // 2
```

The captured map references point to the same storage, so the callbacks share state.
Storing a function inside the map that it captures can create a reference cycle.
Cycle collection depends on the host's support for `CollectCycles`.
Prefer named functions for recursion. For reentry or nested captures,
see the [capture discussion in the embedding guide](Embedding.md).

<a id="math"></a>
## 9. Use the existing math and vector functions

Start with `import math;`. Preserve the capitalization in `math.Clamp` and `math.Lerp`.
Trigonometric functions use lowercase names, such as `math.sin`, `math.cos`, and `math.atan2`.

| Purpose | Available functions |
| --- | --- |
| Absolute value, square root, powers | `abs`, `sqrt`, `pow` |
| Rounding, floor, ceiling | `round`, `floor`, `ceil` — return float; use `toint` if an integer is needed |
| Minimum, maximum, clamping | `Min`, `Max`, `Clamp`, `Clamp01` |
| Linear and inverse interpolation | `Lerp`, `Lerp3`, `InverseLerp`, `Remap` |
| Smooth transitions | `SmoothStep01`, `SmoothStep` |
| Movement with a maximum step | `MoveToward(current, target, maxDelta)` |
| Repeating and ping-pong values | `Repeat`, `PingPong` |
| Angle interpolation | `WrapAngle`, `DeltaAngle`, `LerpAngle`, `MoveTowardAngle` |
| Radian/degree conversion | `rad`, `deg` |
| Distance, dot product, cross product | `Distance2`, `Distance3`, `DistanceSquared3`, `Length3`, `Dot3`, `Cross3` |
| Normalization | `Normalize3(vector, fallback)` — fallback is required |
| Rotation | `Quaternion`, `quat_from_basis`, `quat_slerp`, `RotateVectorByQuat` |
| Colors, hashes, float strings | `ColorRGB`, `ColorARGB`, `Hash32`, `tostr` |

See the [math API](API.md#3-math) for complete argument order and types.
`Lerp`, `InverseLerp`, and `Remap` do not automatically clamp to `[0, 1]`.
Combine them with `Clamp01` when needed. Angle functions use radians.
`DeltaAngle`, `LerpAngle`, and `MoveTowardAngle` handle large finite angles without losing the wrapped
target; `LerpAngle(a,b,0)` and `LerpAngle(a,b,1)` return the wrapped endpoints. `PingPong` preserves
small positions even when its positive length is large. `Normalize3` handles large finite components
without overflowing the intermediate squared length; its small-vector fallback rule is unchanged.

```cpp
import math;

var position = math.Vector3(1.0, 2.0, 3.0);
var copy = position;
copy[0] = 9.0;
print(position[0]);                 // 1: vectors are copied by value
print(tosize(position));            // 3

var zero = math.Vector3(0.0, 0.0, 0.0);
var forward = math.Vector3(0.0, 0.0, 1.0);
var direction = math.Normalize3(zero, forward);
print(direction[2]);                // 1: uses fallback when the length is too small
print(math.Lerp(10.0, 20.0, 0.5));   // 15
print(math.MoveToward(0.0, 10.0, 2.0)); // 2
```

- Direct assignments such as `v = math.Vector3(x, y, z)` avoid an intermediate value copy.
  Existing copies stay independent. Arguments are evaluated left to right before the constructor
  writes `v`; invalid component types release the unfinished vector without overwriting it.
  Argument side effects still occur.
- Vectors and lists such as `[x, y, z]` are different types. Do not pass a list to a vector API.
- Access components with `v[0]` and `v[1]`; `v.x`, `v.y`, and `v.z` are unsupported.
- Component `+=`, `-=`, `*=`, and `/=` accept int/float operands and preserve value-copy semantics.
  Indices must be integers in range. `%=` is unsupported because vector components are floats.
- A component write through a container element or a native property (`list[0][1] = n`, `obj.Position[0] += n`)
  reaches that element or property. Through a native property each statement costs one getter and one setter,
  so when changing several components copy into a variable, modify it, then store it back once.
- Indexed assignments resolve the containing path after evaluating the right-hand side, except
  for computed indices and function-produced containers retained as described below.
  If the right-hand side replaces an intermediate list or map, the write follows the replacement.
  For example, if `F()` changes `i`, `list[i][0] += F()` reads and writes the element at the new `i`,
  just like a single-level indexed assignment. Parentheses around the target do not change this rule.
  Computed index expressions and calls that produce the target container run once before the right-hand
  side; their results are retained. For example, `Get()[0] += Change()` retains the container returned
  by `Get()`. Plain root or index variables are read for the write.
  List/map/vector-array writes may use a temporary internal reference; normal reads and assignment results
  remain values. Copies still stay independent, and native properties retain their getter/setter behavior.
  WREF pairs with an ordinary write and handles vector write-back; rebuild older compiled scripts and caches for bytecode version 0133 (also required for signed-zero constants, foreach source retention, checked shift folding, and corrected NaN branch behavior).
- A selector immediately after an indexed assignment can still modify the assigned vector:
  `(a[0][0] = math.Vector3(1,2,3))[1] = 9` stores `(1,9,3)` in `a[0][0]`.
  Assigning the result to a separate local instead keeps ordinary vector copy semantics.
- Postfix `++` and `--` return the previous component value, including inside list/map literals;
  prefix forms return the updated value. For `var a = [v[0]++];`, `a[0]` receives the old value.
- Addition and subtraction work on vectors with the same component count; multiplication and division support a vector and a scalar.
- Vectors do not support `.len()`, list methods, or `foreach`. Use `tosize` for the component count.
- `math.Quaternion(w, x, y, z)` takes **w first**. Its runtime type name is `"Vector4"`.
  Do not interpret ordinary vector multiplication `*` as rotation composition.
- `ColorRGB` and `ColorARGB` components range from 0.0 to 1.0, not integer values from 0 to 255.
  Color conversion maps NaN to 0, clamps negative/positive infinity to 0/1, and truncates the scaled
  components to bytes. `ColorRGB` always sets alpha to 255.

### Random numbers and reproducibility

Available functions include `math.srand(seed)`, `math.rand()`, `math.Rand01()`, and `math.RandRange(min, max)`.
`rand()` ranges from 0 to 32767, and `Rand01()` **includes 1.0**.
Using `toint(math.Rand01() * list.len())` directly as an index can therefore go out of bounds.
Random state belongs to the worker. A fixed seed alone does not make host-call order, worker reuse, time inputs,
or map iteration deterministic. Control these inputs and execution order in reproducibility tests as well.

<a id="coroutines"></a>
## 10. Coroutines and yielding execution

Coroutines suspend and resume execution; they are not separate OS threads.
After `import coroutine;`, use `create`, `resume`, `status`, and `close`.

```cpp
import coroutine;

var progress = 0;
fun Work(var amount)
{
    progress += amount;
    yield;                         // A statement, not a function call
    progress += amount;
}

var co = coroutine.create(Work);
print(coroutine.status(co));        // suspended
coroutine.resume(co, 2);            // Extra arguments on the first resume are passed to Work
print(progress);                   // 2
print(coroutine.status(co));        // suspended
coroutine.resume(co);
print(progress);                   // 4
print(coroutine.status(co));        // dead
```

In this normal flow, the resumed coroutine runs until it yields or finishes,
then execution returns to the statement after `resume` in the caller. Use shared state or another explicit mechanism to transfer results.
Call `resume` only when the coroutine is `suspended`. Resuming a `dead` coroutine is an error.
Calling `close(co)` on a finished coroutine is allowed and returns that same coroutine.

`sleep(ms)` pauses the execution instance for a duration; it serves a different purpose from `yield;`.
Suspension operations such as `sleep` and `yield` are restricted inside synchronous callbacks that reenter script from the host.
Also consult the host contract for per-frame resumption, execution budgets, cancellation, and lifetime.

<a id="other"></a>
## 11. Other useful features

| Need | Feature and caveats |
| --- | --- |
| Type checking | `type(x)` returns strings such as `"int"`, `"float"`, `"list"`, `"map"`, `"array"`, and `"function"` |
| Size across types | `tosize(x)` supports strings, lists, maps, arrays, sets, and vectors |
| Source line number | `__LINE__` is the current line number in the source |
| Time | `system.time()` returns Unix seconds; `system.date(format, time)` formats local time |
| Timing | `system.clock()` returns seconds based on C `clock()`; do not assume it measures wall-clock time |
| Dynamic code | `system.load(source, name)` creates a module; `system.pcall(module)` runs its top-level code |
| HTTP | `system.aysnc_create()` creates a request object. **The actual API spelling is aysnc** |
| Editor support | The VS Code extension provides completion, signature help, breakpoints, stepping, call stacks, and local-variable inspection |

### JSON conversion (also available in the console)

After `import system;`, use `system.jsonEncode(value, sortKeys = false)` and `system.jsonDecode(text)`.
They convert between values and UTF-8 JSON strings without file I/O. For stable output suitable for comparisons,
pass `true` as the second encoding argument to sort string keys in every nested map by UTF-8 byte order. List order is unchanged.

```javascript
import system;
var state = {"x": 100.0, "ships": [1, null, 3]};
var text = system.jsonEncode(state, true);
if (text == null) print(system.jsonError());
var copy = system.jsonDecode(text);
if (system.jsonError() != "") print(system.jsonError());
```

- Integers and floats are written distinctly, as `1` and `1.0`. Even `0.1` round-trips to the same float32 value.
- Numbers without a decimal point or exponent decode as int32; those with either decode as float32. Out-of-range numbers fail. A UTF-8 BOM is accepted on input.
- Supported values are string-keyed maps, lists, strings, int, float, bool, and null. The root need not be a map.
- Null map values omit the key; null list values keep their positions. Shared references decode as independent values.
- `system.array`, sets, vectors, functions, host objects, cycles, NaN/infinity, invalid UTF-8, and NUL are rejected.
- Nesting is limited to 64 levels starting at the root container. Duplicate keys keep the last value; a final null removes the key.
- Failure returns null, and `system.jsonError()` reports a reason and a path such as `$.ships[2]`.
  The next successful conversion clears the error. A successful `jsonDecode("null")` is distinguished by an empty error string.
- Successful JSON serialization does not guarantee a successful file save. Check the file API's result too.
- JSON conversion does not dictate runtime data layout. Keep dense arrays for per-tick processing,
  and convert to maps and lists at the persistence boundary. See [JSON.md](JSON.md) for detailed rules.

`system.pcall` is not Lua's protected call. Internal errors propagate to the caller and stop execution.
The result of `system.load` also does not support calling named functions as `module.Function()`.
Use static imports for calls between files. The current implementation does not use the `name` argument to `load`,
so do not rely on it to attach a filename to dynamic source.

See the [async API](API.md#9-async-methods) for the HTTP object's `add_header`, `get`, `post`, `wait`, `close`,
and callback arguments. The current HTTP transport implementation targets Windows.
`wait()` blocks; do not treat it as a per-frame polling operation. It returns `false` before a request starts or on timeout,
and `true` after completion; the callback reports whether the request succeeded.

`type()` alone cannot distinguish a host-bound native object from ordinary `null`.
Object validity follows the host's binding contract.
Passing a function name or anonymous-function expression directly to `type()` returns `"function"`.

## 12. Distinguish host APIs from standard features

Engine APIs such as `Services.*` are registered by the host.
They are not automatically available in this repository's default console.
Verify availability, argument order, return types, and nullability against the host's binding definitions and actual call sites.
Do not invent engine APIs by analogy with similar names elsewhere.

For headless tests, isolate pure logic in functions and pass external inputs as arguments.
Replace only the required interfaces for host-dependent rendering, audio, time, and file access.
Passing a console test does not establish that the real engine bindings and object lifetimes have been validated.

Expose host-callable functions with `export fun`.
The console's `--file` runs top-level code, so merely defining a test function does not execute it.
Call it explicitly at the end of the test file.

<a id="verification"></a>
## 13. Run scripts in the console and locate errors

Build the console from the repository root. Use the path of your installed Visual Studio version.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Samples\console\console.sln /m /p:Configuration=Release /p:Platform=x64
```

Running with `Samples/console` as the working directory makes the default library path `../../Lib/`
and the existing tests' relative paths resolve correctly. Replace the script path with the file to run.

```powershell
Set-Location Samples\console
.\x64\Release\console.exe --file C:\scripts\main.ns
.\x64\Release\console.exe --file C:\scripts\main.ns --debug
.\x64\Release\console.exe --file C:\scripts\main.ns --asm --stats
```

An absolute path passed to `--file` does not make imports resolve beside that file.
Choose another library directory through the host loader or `libPath` in VS Code's `launch.json`.
See the [ReadMe](../ReadMe.md#vs-code-debugger-setup) for VS Code extension setup and features.

### A minimal self-checking example

```cpp
var failures = 0;
fun Check(var ok, var label)
{
    if (ok == false)
    {
        failures += 1;
        print("FAIL: " .. label);
    }
}

var values = [1, 2, 3];
values[1] = null;
Check(values.len() == 3, "list keeps length");
Check(values[1] == null, "list stores null");
var record = {"selected": values[2]};
Check(record.selected == 3, "map stores indexed value");
record.selected = null;
Check(record.len() == 0, "map removes null entry");
print("failures=" .. failures);     // failures=0
```

`Check` is defined in this example. Do not assume a standard `assert` function exists.
A harness that only prints failures may still exit with a successful process code.
Automation must inspect the failure count and output, or the host must reflect test results in the exit code.

### Distinguish compile errors from runtime errors

Compile errors include the source name and location, for example:

```text
.../settings.ns: Error (12, 8): unknown identifier 'missing'
```

Nested imports preserve the line and column in the file where the error occurred.
Concatenating files into one source string loses file boundaries.
Preserve imports or maintain source-location mappings in the harness when combining source text.
Recompile with debug information for runtime call stacks and debugger source navigation.

When host methods or properties fail without a detailed error, diagnostics identify the name and operation:
`invalid function call 'Method'`, `cannot read property 'Name'`, or `cannot write property 'Name'`.
These names remain available without debug information. Detailed errors and codes supplied by the host are preserved.
For unnamed function-value calls, the diagnostic reports the type of the non-callable value. IP, line, and call-stack details are attached as before.

| Symptom | What to check first |
| --- | --- |
| `unknown identifier` | Declaration order of variables, constants, and imports; spelling and capitalization |
| `forward declarations` | A leftover `fun F(...);` without a body |
| `no public member` | Access to a variable, constant, or function without `public` through an alias. Add `public` to expose it; `export` does not expose it to modules |
| `is only allowed at file scope` | `import`, `const`, `public`, or `export` inside a block or function. Move it to file scope |
| `invalid function call` | Exact API argument count and types; passing a list where a vector is required |
| String index error | Use `s.sub(i, 1)` instead of `s[i]` |
| List/array index error | Zero-based bounds and whether the container was grown first |
| Import failure | Loader libPath, lowercase filename, working directory, and file encoding |
| A documented example produces an old error | Whether the console/engine binary actually used was built from the current source |

When reporting an issue, include the binary used, build status, command and working directory, minimal source,
expected result, and complete actual error. Verify the behavior before concluding that it is a language limitation or bug.

<a id="checklist"></a>
## 14. Capacity limits and final checks

Do not cite the old limits of **roughly 15,000 constants / a 1 MiB compiled image** as current limits.
The compiler now supports larger constant indices and images, and global indices are independent of the constant count.
Some limits still apply, including:

- Up to 32,767 global variables, independent of the constant count.
- Runtime strings are limited to `2147483647` UTF-8 bytes; excessive concatenation or replacement is an error.
- Map/set capacities are powers of two up to `1073741824` buckets, with at most `805306368` entries.
  Available memory usually imposes a lower limit. Unrepresentable reserve requests fail without modifying the collection.
- Nested native-to-script calls, including nested sort comparators, are limited to 64 per worker.
- Compiler nesting is limited to 128 active parser levels across expressions, statements,
  blocks, functions, and constant expressions. Deeply nested source reports a compile error;
  this is a nesting budget, not a maximum line count.
- Each string or symbol name stored in an image is limited to **32,767 UTF-8 bytes**.
  Do not confuse this with the character counts used by string methods.
- Functions, local/temporary slots, arguments, relative jumps, and debug line numbers also have encoding limits.
  Compile large functions and generated source to verify them; line count alone is not sufficient.
- The full image no longer uses a fixed 1 MiB buffer; available memory and 32-bit offset limits apply.
  Capacity and buffer-write failures are reported as compile errors.

See [compiled-image limits](Embedding.md#compiled-image-capacity-format-0123) for exact limits and compatibility.
Function count and constant count are different quantities. Rebuild existing bytecode when the engine's image format changes.

Before submitting AI-generated code, check:

1. Did you consult this guide and the API reference for the required features, including existing `find`, interpolation, and vector functions?
2. Did you distinguish function declaration order from variable and constant declaration order?
3. Did you add `public` to variables, constants, and functions needed by other modules? `export` is for the C++ host.
4. Are integer division, bool conditions, zero-based indices, and string-length arguments correct?
5. Did you distinguish null list entries from map deletion, and shared references from value copies?
6. Did you avoid assuming that a lambda capture updates an outer scalar variable?
7. Does the test control any relevant iteration order, time, randomness, and host input?
8. Did a minimal runnable example produce the expected result, including empty values, failures, and boundary cases?
9. Does the report distinguish the validated environment from host behavior that has not been tested?

## 15. Keep documentation and implementation in sync

Reflect new features, changed boundary behavior, and removed syntax in both this guide's examples and the [API reference](API.md).
Explain how to write scripts, as well as how the compiler implements a feature.
Do not present workarounds for obsolete restrictions as the current default approach.
When documentation and behavior disagree, verify with a small script and determine which needs correction.

| What to verify | Reference |
| --- | --- |
| All functions, arguments, and return values | [API.md](API.md), [NeoLib.cpp](../NeoSource/NeoLib.cpp) |
| Syntax, declarations, and selectors | [ReadMe.md](../ReadMe.md), [NeoParser.cpp](../NeoSource/NeoParser.cpp) |
| Recent language behavior and errors | [language_regression.cpp](../Tests/language_regression.cpp) |
| Branches, returns, and null assignment | [control_flow_regression.cpp](../Tests/control_flow_regression.cpp) |
| Selectors after calls | [postfix_regression.cpp](../Tests/postfix_regression.cpp) |
| Indexed writes, evaluation order, and malformed selectors | [selector_regression.cpp](../Tests/selector_regression.cpp) |
| Operand order, side effects, and deferred assignment targets | [hunt_evaluation_regression.cpp](../Tests/hunt_evaluation_regression.cpp) |
| Runtime recovery and coroutine cleanup | [hunt_runtime_regression.cpp](../Tests/hunt_runtime_regression.cpp) |
| Standard-library argument and date errors | [hunt_library_regression.cpp](../Tests/hunt_library_regression.cpp) |
| Storage limits, integer keys, and numeric conversion | [hunt_storage_regression.cpp](../Tests/hunt_storage_regression.cpp) |
| Parser boundary and syntax regressions | [hunt_parser_regression.cpp](../Tests/hunt_parser_regression.cpp) |
| Compiler capacity limits | [compiler_limits_regression.cpp](../Tests/compiler_limits_regression.cpp) |
| Host calls, captures, and lifetimes | [Embedding.md](Embedding.md) |
| Repository workflow, builds, and regressions | [AGENTS.md](../AGENTS.md) |

Update the relevant regression tests and documentation when changing the compiler or standard library.
Report only tests actually run; do not claim validation of source that has not been built.
