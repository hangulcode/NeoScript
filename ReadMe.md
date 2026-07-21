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
	- Supports .ns syntax highlighting and Debug Adapter Protocol debugging.
	- The debugger runs the currently selected .ns file as a top-level script.
	- Put the code you want to debug in the script body. If you want to debug an exported function, call it from top-level script code.
	- Build Samples/console in x64 Release, then use the "Debug Neo Script" launch configuration.
	- If the adapter executable is not in the default sample path, set neoScript.debugAdapterPath or add adapterPath to launch.json.
	- Set libPath in launch.json when the Neo Script Lib directory is outside the workspace folder.

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

### Compile-time defines
Host applications can provide C-style compile-time defines through `NeoCompilerParam::defines`.
This is useful for engine constants such as keyboard codes.

The define table is token based, so the host can prepare values before compilation.
During script compilation, identifiers such as `KEY_LEFT` are replaced before normal parsing.
This does not create a script global variable and has no runtime lookup cost.

```cpp
NeoScript::NeoCompileDefines defines;
defines.values["KEY_LEFT"]  = { NeoScript::NEO_DEFINE_TOKEN_INT, "37" };
defines.values["KEY_RIGHT"] = { NeoScript::NEO_DEFINE_TOKEN_INT, "39" };

NeoScript::NeoCompilerParam param(source, sourceLength);
param.defines = &defines;

// An execution context pool is required (see "Execution context pool" below).
NeoScript::NeoExecContextPool* pool = NeoScript::NeoExecContextPool_Create();
NeoScript::NeoLoadVMParam vparam;
vparam.execPool = pool;

NeoScript::INeoVM* vm = NeoScript::INeoVM::CompileAndLoadRunVM(param, &vparam);
```

Script code:
```cpp
if (key == KEY_LEFT)
{
    print("left");
}
```

Supported define token types:
	- `NEO_DEFINE_TOKEN_IDENTIFIER`
	- `NEO_DEFINE_TOKEN_INT`
	- `NEO_DEFINE_TOKEN_FLOAT`
	- `NEO_DEFINE_TOKEN_STRING`
	- `NEO_DEFINE_TOKEN_TRUE`
	- `NEO_DEFINE_TOKEN_FALSE`
	- `NEO_DEFINE_TOKEN_NULL`

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
CPU : 12th Gen Intel(R) Core(TM) i7-12700F 2.10GHz  
RAM : 64GB  
OS  : Windows 10 Pro 64bit  
Build : Release Mode 64bit  

|               |Neo Script 1.0.9| [Lua Script 5.5.0](https://www.lua.org/)| Visual C++ 2026 |
| :-----------  |:--------------:| :-------------:|:---------------:|
| Loop Sum (1~N)| 0.275 (4% faster) | 0.285       | 0.043           |
| Math          | 1.634 (3% slower) | 1.584       | 0.129           |
| Prime Count   | 3.784 (13% faster)| 4.278       | 0.867           |
| fibonacci     | 4.066 (24% faster)| 5.047       | 0.315           |

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
	- double: stores an 8-byte or 4-byte floating-point value, depending on `NS_SINGLE_PRECISION`
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
	- for: `for (var a in 1, 100, 1)` specifies start, end, and increment values
	- foreach: iterates a map, list, or set; use `foreach (var a[, b] in map)`
	- true / false: boolean values
	- null: no value
	- ++ / --: increments or decrements a variable by one
	- && / ||: logical operators, equivalent to C semantics
	- > / < / >= / <=: comparison operators, equivalent to C semantics
	- x..y: converts x and y to strings and concatenates them

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


#### Neo Script Code Image
![](/docs/img/vs001.png)

#### Neo Script Output Image
![](/docs/img/vs002.png)

#### Neo Script Debugger Image
![](/docs/img/code_debugger.png)

### Neo Script Test Code
```cpp
import math;
import system;

print("Start ...");
var start_time;

fun calculateSum(var n)
{
    var sum = 0.0;
    for(var i in 0, n, 1)
        sum += i;
    return sum;
}
start_time = system.clock();
print("Loop Sum :" .. calculateSum(100000001));
print("Time :" .. (system.clock() - start_time));

fun calculateMath(var n)
{
    var sum = 0.0;
    for(var i in 0, n, 1)
        sum += math.sqrt(i);
    return sum;
}
start_time = system.clock();
print("Math :" .. calculateMath(100000001));
print("Time :" .. (system.clock() - start_time));

fun isPrime(var num)
{
    if( num < 2)
        return false;
	for(var i in 2, math.sqrt(num) + 1, 1)
	{
		if(num % i == 0)
			return false;
	}
    return true;
}
fun PrimeCount(var num)
{
	var cnt = 0;
	for(var i in 1, num, 1)
	{
		if(isPrime(i))
			cnt++;
	}
	return cnt;
}
start_time = system.clock();
print("PrimeCount :" .. PrimeCount(5000001));
print("Time : " .. (system.clock() - start_time));

fun fibonacci_recursive(var n)
{
    if( n <= 1)
        return n;
    else
        return fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2);
}
start_time = system.clock();
print("fibonacci :" .. fibonacci_recursive(40));
print("Time :" .. (system.clock() - start_time));
```

### Lua Script 5.5.0 Test Code
```lua
local startTime
print("Start ...")

function calculateSum(n)
    local sum = 0
    for i = 0, n - 1 do
        sum = sum + i
    end
    return sum
end

startTime = os.clock()
print("Loop Sum :" .. calculateSum(100000001))
print("Time:" .. (os.clock() - startTime))

function calculateMath(n)
    local sum = 0
    for i = 0, n - 1 do
        sum = sum + math.sqrt(i)
    end
    return sum
end

startTime = os.clock()
print("Math :" .. calculateMath(100000001))
print("Time:" .. (os.clock() - startTime))

function isPrime(num)
    if num < 2 then
        return false
    end
    for i = 2, math.sqrt(num) do
        if num % i == 0 then
            return false
        end
    end
    return true
end
function PrimeCount(num)
	local cnt = 0
	for i = 1, num - 1 do
		if isPrime(i) then
			cnt = cnt + 1
		end
	end
	return cnt
end

startTime = os.clock()
print("PrimeCount :" .. PrimeCount(5000001));
print("Time:" .. (os.clock() - startTime))

function fibonacci_recursive(n)
    if n <= 1 then
        return n
    else
        return fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2)
    end
end

start_time = os.clock()
print("fibonacci:" .. fibonacci_recursive(40))
print("Time:", os.clock() - start_time)
```

### Visual C++ 2026 Test Code
```cpp
#include <iostream>

double Clock()
{
	return (double)clock() / (double)CLOCKS_PER_SEC;
}
double calculateSum(int n)
{
	double sum = 0.0;
	for (int i  = 0; i <  n; i++)
	{
		sum += i;
	}
	return sum;
}
double calculateMath(int n)
{
	double sum = 0.0;
	for (int i = 0; i < n; i++)
	{
		sum += sqrt(i);
	}
	return sum;
}
bool isPrime(int num)
{
	if (num < 2)
		return false;

	int end = (int)(sqrt(num) + 1);
	for (int i = 2; i < end; i++)
	{
		if (num % i == 0)
			return false;
	}
	return true;
}
int PrimeCount(int num)
{
	int cnt = 0;
	for (int i = 1; i < num; i++)
	{
		if (isPrime(i))
			cnt++;
	}
	return cnt;
}
int fibonacci_recursive(int n)
{
	if (n <= 1)
		return n;
	else
		return fibonacci_recursive(n - 1) + fibonacci_recursive(n - 2);
}
int main()
{
	printf("\nStart ...");
	double start_time;

	start_time = Clock();
	printf("\nLoop Sum : %lf", calculateSum(100000001));
	printf("\nTime : %lf", (Clock() - start_time));

	start_time = Clock();
	printf("\nMath : %lf", calculateMath(100000001));
	printf("\nTime : %lf", (Clock() - start_time));


	start_time = Clock();
	printf("\nPrimeCount : %d", PrimeCount(5000001));
	printf("\nTime : %lf", (Clock() - start_time));

	start_time = Clock();
	printf("\nfibonacci : %d", fibonacci_recursive(40));
	printf("\nTime :%lf", (Clock() - start_time));
}
```

### YoutTube
[![YoutTube](https://i9.ytimg.com/vi_webp/iEpmlLcxBVg/mq2.webp?sqp=CLTL6L8G-oaymwEmCMACELQB8quKqQMa8AEB-AH-CYAC0AWKAgwIABABGEkgZSg2MA8=&rs=AOn4CLAwiLyh_R2B056F-Eej9t5PiuJhmw)](https://www.youtube.com/watch?v=iEpmlLcxBVg)
