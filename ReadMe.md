<p align="center">
  <img src="/docs/img/Neo_Icon.png" alt="My Image">
</p>

# Neo Script Documentation
	- The grammar uses a C-like syntax, but it is somewhat similar to Lua script.
	- It was developed in Visual Studio 2026 C++.
	- After some more features are added, port to C#

### Script API reference

**[docs/API.md](docs/API.md)**

Every function a script can call — the keyword intrinsics, `print`, the `math` / `system` /
`coroutine` modules, and the `string` / `list` / `map` / `async` method sets — with exact
signatures, argument types and return types. Look there first when you need a function; this
file covers the language itself and the C++ host API.

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

### Embedding the engine

The public v2 C++ host API in `NeoScript.h` — Runtime / Program / Instance, host↔script calls,
captured lambdas, cycle collection, and the internal execution-context pool — has its own document:

**[docs/Embedding.md](docs/Embedding.md)**

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
| `loop_sum`      | integer loop, VM dispatch floor | **171** | 197 |  13.1 | 0.87x | 13.1x |
| `float_math`    | float mul/add/sub chain         | **160** | 316 |  59.8 | 0.51x |  2.7x |
| `func_call`     | script function call overhead   | **128** | 161 |   4.4 | 0.80x | 29.4x |
| `fib_recursive` | recursion, fib(32)              |  **89** |  90 |   6.8 | 0.99x | 13.1x |
| `array_rw`      | sequential array write + read   |  **42** |  47 |   2.5 | 0.89x | 17.0x |
| `map_str`       | string-key hash lookup          |  46 |  **40** |  72.7 | 1.15x |  0.6x |
| `string_ops`    | string build + length           |  **80** | 153 |  13.6 | 0.52x |  5.9x |
| `particles`     | game-style float + array sim    |  **45** |  50 |   3.5 | 0.90x | 12.7x |
| **total**       |                                 | **761** | 1054 | 176.3 | 0.72x |  4.3x |

- Neo leads on 7 of 8 benchmarks; `map_str` is the one Lua wins, because Lua interns every short
  string so a table lookup is a pointer compare.
- `fib_recursive` is where the lead is thinnest — 0.99x here and an exact tie on the median, against
  0.72x overall. Almost all of its work is call and return, so there is no loop body for Neo's
  cheaper dispatch to win back.
- C++ is a reference ceiling, not a peer. The exception is `map_str`, where
  `std::unordered_map<std::string,…>` is slower than both VMs — the interpreters cache the string
  hash; the C++ map rehashes on every lookup.
- These are ordinary `/LTCG` figures, the same build anyone gets by default.

**Method.** The three files implement the same algorithm and each returns a checksum that must match
across languages — that is what proves they did the same work. Each language times only the measured
region with its own clock, so start-up and compilation are excluded. Figures are best-of-5 in-process
repetitions, minimum across 7 runs, with the three languages interleaved inside each run.

Two rules follow from how noisy this suite is. **Compare within one session, not across sessions** —
machine load moved *Lua's* numbers 20-30% with its source untouched. And **A/B every change**:
build-to-build variation is code layout, not measurement noise, so a move under ~10% on `float_math`,
`particles` or `map_str` is layout rather than a real regression.

Layout moves for reasons that have nothing to do with the code being measured — moving a function
between translation units, or marking one cold, reshuffles block placement across the whole
interpreter. Those three rows react and `loop_sum` / `array_rw` do not, which is the signature to
look for before calling something a regression. The durable fix is PGO, since `/LTCG` places blocks
from heuristics while it has no idea which branches actually run.

All compiler and VM changes are covered by the regression suites: `console.exe --smoke` (2,785
compiler cases) and `console.exe --v2smoke` (host API, closure lifetime, leak counters).

### var data structure
	- null: represents no value; uninitialized variables are null
	- bool: stores true or false
	- int: stores a 4-byte integer
	- float: stores a 4-byte single-precision float (`NS_FLOAT` is `typedef float`).
	  `type()` reports "float"; there is no double type
	- string: stores UTF-8 text; length and index positions count characters, not bytes
	- list: an array-like container
	- map: a key/value container
	- set: a key-only container, built with `system.set(list)`
	- Vector2 / Vector3 / Vector4 / Quaternion: inline value types, not lists
	  - see "Vector value types" above
	- function: a named function or a captured anonymous function
	- coroutine / module / async: engine object handles
	- native object: a symbol the host bound with `RegisterObject`/`BindObject`
	  (`GameObject`, `Host`, ...). `type()` reports "null" for one and `== null` is true,
	  so it cannot be told apart from a real null - call a method on it instead


### Neo Script reserved words
	- var: declares a variable
	- const: declares a compile-time constant - see "Script `const`" above
	- fun: declares a function. `fun(...) { ... }` without a name is an anonymous function; it
	  captures the enclosing function's locals **by value** — see "Captured anonymous functions"
	  in [docs/Embedding.md](docs/Embedding.md) for what that does and does not share
	- import: imports a module from the Lib directory
	- export: makes a variable or function available to C++
	- tostring (x) / toint (x) / tofloat (x) / tosize (x) / type (x) / sleep (x):
	  built-in intrinsics. Return types and exact semantics:
	  [`docs/API.md`](docs/API.md) section 1
	- return [x]: returns from the current function, optionally with x
	- break: exits the current loop
	- yield: suspends the current coroutine; execution resumes at the next
	  `coroutine.resume`
	- continue: starts the next loop iteration
	- if (x) / else / else if: C-style conditional chain; the legacy `elif` syntax is removed.
	  **x must be a bool** - only `true` takes the branch. `if (1)`, `if ("a")` and
	  `if (someMap)` are all false; there is no truthiness conversion. Same for `while`
	- switch / case / default: see "switch statement" below
	- for: `for (var a in start, end [, step])` — see "for loop" below. `end` is **exclusive**
	  and `step` is optional (defaults to 1)
	- foreach: iterates a map, list, or set
		- map: `foreach (var key, value in map)` or `foreach (var key in map)`
		- list / set: single variable only, `foreach (var value in list)`
		  (two-variable form is not supported and reports a runtime error)
	- while: `while (condition) { ... }`, C semantics. There is no `do ... while`
	- true / false: boolean values
	- null: no value
	- __LINE__: the current source line, substituted at compile time
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
See **[`docs/API.md`](docs/API.md)** — the full list with signatures and argument types, kept in
step with `NeoSource/NeoLib.cpp`. It is deliberately not repeated here.

### Comment
	- //: single-line comment
	- /* */: multi-line comment

### Benchmark code

All three files run the **same eight benchmarks with the same algorithm**; only the syntax differs.
Each returns a checksum that must match across languages.

- [`Samples/bench/bench.ns`](Samples/bench/bench.ns) — NeoScript
- [`Samples/bench/bench.lua`](Samples/bench/bench.lua) — Lua
- [`Samples/bench/bench.cpp`](Samples/bench/bench.cpp) — C++ (build with `build_cpp.bat`)

Run: `console.exe --file Samples/bench/bench.ns`
