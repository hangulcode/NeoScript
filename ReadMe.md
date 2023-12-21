# Neo Script Documentation
	- The grammar uses a C-like syntax, but it is somewhat similar to Lua script.
	- It was developed in Visual Studio Pro 2022 C++.
	- After some more features are added, port to C#

### License
	MIT license
	NeoScript is free software.
	The only requirement is that if you do use NeoScript, then you should give us credit by including the copyright notice somewhere in your product or its documentation.

### Sample
	- console / hello: print hello.
	- console / callback: Function calls between C ++ and Neo scripts.
	- console / map_callback: maps the function to the Neo script table and finds and calls the function at runtime.
	- console / 9_times: This is a sample of a Multiplication table.
	- console / string: this is an example of string.
	- console / map: Sample Dictionary usage.
	- console / list: Sample List usage.
	- console / slice_run: Executes every part of the script that is produced by the script in sequential order of a certain time or command.
	- console / time_limit: Processes a script function call to exit after a certain period of time.
		Unlike console / slice_run, console / time_limit does not execute after the next frame.
	- console / divide_by_zero: Exception handling.
	- console / delegate: Function pointer support.
	- console / meta: this is an example of using meta
	- console / coroutine: an example of using coroutines.
	- console / mudule: this is an example of using module.

### var data structure
	- null: the variable has no value (if the variable is not initialized or "var a;" or "var a = null;")
	- bool: store true / false value
	- int: Save 4-byte signed integer value
	- double: 8-byte floating point storage
	- string
	- list
	- map : Dictionary


### Neo Script reserved words
	- var: Save the value as a variable name later
	- fun: Notice the start of the function
	- import: prefixed to fun and declared to call a c ++ function
	- export: can be prepended to fun and the function can be called from external (c ++)
	- tostring (x): change the value of x variable to string
	- toint (x): change the value of x variable to int
	- tofloat (x): change the value of x variable to double
	- tosize (x): Returns the number in the table if the variable x is a table, or 0 if it is not a table
	- totype (x): return the data type of the variable x as a string
	- sleep (x): Pause as much as the value of x (1 second for 1000)
	- return [x]: Return x value
	- break: causes for / foreach / while loop to exit
	- if (x) / else: same as c ++
	- for : for (var a int 1, 100, 1)
	- foreach : map, list, set variable only. Must be foreach (var a, b in map)
	- true / false: value stored in a variable of type bool
	- null: no value
	- ++ / - Same as c ++ (variable value is incremented by 1 or decremented by 1)
	- && / || : Same as c ++
	- > / < / >= / <= : same as c ++
	- x..y: return the combined string of x and y strings

### Built-in system function use system.
	## Basic
	- print (x): print string x

	## system
	- clock ():  return time
	- load ():  load module scipt
	- pcall (x) : run mdule script
	- meta(x,y) : bind the y function of var x
	- set(x) : list to set
	
	## coroutine
	- create (): coroutine create and return (suspended mode )
	- resume (x): suspended coroutine active
	- status (x): coroutine state string return
	- close ([x]): coroutine close

	## math
	- abs (x): same as c ++
	- acos (x): same as c ++
	- asin (x): same as c ++
	- atan (x): same as c ++
	- ceil (x): same as c ++
	- floor (x): same as c ++
	- sin (x): same as c ++
	- cos (x): same as c ++
	- tan (x): same as c ++
	- log (x): same as c ++
	- log10 (x): same as c ++
	- pow (x, y): same as c ++
	- deg (x): radian -> degree
	- radian (x): degree -> radian
	- sqrt (x): same as c ++
	- srand (x): same as c ++
	- rand (): same as c ++

    ## string
	- len (): Return the length of the string
	- find (x): Retrieve x string return position (same as c ++)
	- sub (x, y): convert x string  offset to y number of strings and return
	- upper () : convert to uppercase and return
	- lower () : convert to lowercase and return
	- trim () : trim space erase (left, right)
	- ltrim () : trim space erase (left)
	- rtrim () : trim space erase (right)
	- replace (x, y) : find x and change y
	- split (x) : string split from x and return list 

	## list
	- len () : return the length of list item count
	- resize (x) : list item resize
	- append (x, [y]) : list item insert (y value is index)

	## map
	- reserve (x) : allocate x number of buffers.
	- sort () : sort table value
	- keys() : put the keys into a set data structure and return it.
	- values() : put the values into a set data structure and return it.

	## set
	- len () : return count set item

### Comment
	- // one-line comment
	- / * * / is a multi-line range comment


#### Neo Script Code Image
![](/docs/img/vs001.png)

#### Neo Script Output Image
![](/docs/img/vs002.png)

### Performance test results
CPU : 12th Gen Intel(R) Core(TM) i7-12700F 2.10GHz  
RAM : 64GB  
OS  : Windows 10 Pro 64bit  
Build : Release Mode 64bit  

|               |Neo Script 1.0.6| [Lua Script 5.4.2](https://luabinaries.sourceforge.net/)| Visual C++ 2022 |
| :-----------  |:--------------:| :-------------:|:---------------:|
| Loop Sum (1~N)| 0.256          | 0.286          | 0.043           |
| Math          | 1.427          | 1.55           | 0.135           |
| Prime Count   | 9.517          | 11.073         | 2.277           |
| fibonacci     | 4.629          | 4.977          | 0.287           |


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
print("PrimeCount :" .. PrimeCount(10000001));
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

### Lua Script Test Code
```lua
local startTime
print("Start ...")

function calculateSum(n)
    local sum = 0
    for i = 0, n do
        sum = sum + i
    end
    return sum
end

startTime = os.clock()
print("calculateSum:" .. calculateSum(100000000))
print("Time:" .. (os.clock() - startTime))

function calculateMath(n)
    local sum = 0
    for i = 0, n do
        sum = sum + math.sqrt(i)
    end
    return sum
end

startTime = os.clock()
print("calculateMath:" .. calculateMath(100000000))
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
	for i = 1, num do
		if isPrime(i) then
			cnt = cnt + 1
		end
	end
	return cnt
end

startTime = os.clock()
print("PrimeCount :" .. PrimeCount(10000000));
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

### Visual C++ 2022 Test Code
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

	for (int i = 2; i < sqrt(num) + 1; i++)
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
	printf("\nPrimeCount : %d", PrimeCount(10000001));
	printf("\nTime : %lf", (Clock() - start_time));

	start_time = Clock();
	printf("\nfibonacci : %d", fibonacci_recursive(40));
	printf("\nTime :%lf", (Clock() - start_time));
}
```
