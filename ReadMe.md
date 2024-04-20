# Neo Script Documentation
	- The grammar uses a C-like syntax, but it is somewhat similar to Lua script.
	- It was developed in Visual Studio Pro 2022 C++.
	- After some more features are added, port to C#

### License
	MIT license
	NeoScript is free software.
	The only requirement is that if you do use NeoScript, then you should give us credit by including the copyright notice somewhere in your product or its documentation.

### Performance test results
CPU : 12th Gen Intel(R) Core(TM) i7-12700F 2.10GHz  
RAM : 64GB  
OS  : Windows 10 Pro 64bit  
Build : Release Mode 64bit  

|               |Neo Script 1.0.6| [Lua Script 5.4.2](https://luabinaries.sourceforge.net/)| Visual C++ 2022 |
| :-----------  |:--------------:| :-------------:|:---------------:|
| Loop Sum (1~N)| 0.256 (12%↑)   | 0.286          | 0.043           |
| Math          | 1.427 (9%↑)    | 1.55           | 0.135           |
| Prime Count   | 9.517 (16%↑)   | 11.073         | 2.277           |
| fibonacci     | 4.629 (8%↑)    | 4.977          | 0.287           |

### Sample
	- console / hello: "hello" 문자열 출력
	- console / callback: C++ 와 Neo scripts 사이에 함수 호출
	- console / map_callback: map 자료구조를 통해서 script 에서 c++ 함수 호출, 변수 가져오기
	- console / 9_times: 구구단 1단 부터 9단 까지 출력
	- console / string: string 기능 예제
	- console / map: map 자료구조 예제
	- console / list: list 자료구제 예제
	- console / slice_run: 스크립트를 정해진 시간동안 실행하고, 이후에 이어서 실행을 진행
	- console / time_limit: 스크립트를 정해진 시간동안 실행하고, 이후에 이어서 실행을 진행
	- console / divide_by_zero: 0으로 나눗셈을 할 경우 예외 처리
	- console / delegate: 함수 포인터 예제
	- console / meta: meta 함수 예제
	- console / coroutine: coroutine 예제
	- console / mudule: mudule 임포트 및 사용 방법

### var data structure
	- null: 변수에 아무 값이 없을 경우 사용, 초기화 하지 않으면 null 상태가 됨
	- bool: 참(true) / 거짓(false) 상태를 저장
	- int: 4byte 정수를 저장
	- double: 8-byte or 4-byte 부동 소수점을 저장(NS_SINGLE_PRECISION 에 의해서 정의됨)
	- string : 문자열 저장
	- list : 배열(Array)형태의 자료구조
	- map : Key / Value 형태의 자료구조
	- set : Key 형태의 자료구조


### Neo Script reserved words
	- var: 변수를 선언
	- fun: 함수의 시작을 선언
	- import: Lib 폴더에서 module 임포트
	- export: var / fun 앞에 사용가능 하며, 변수 또는 함수를 c++ 에서 참조가 가능하게 됨
	- tostring (x): x 변수를 문자열로 변환한 변수를 리턴
	- toint (x): x 변수를 정수로 변환한 변수를 리턴
	- tofloat (x): x 변수를 부동소수점으로 변환한 변수를 리턴
	- tosize (x): 
		x  문자열일 경우 : 문자열 리턴
		x  가 list / map /set 일 경우 : 자료구조 Count를 리턴
		기타 : 0을  리턴
	- totype (x): x 변수의 자료구조 타입의 문자열을 리턴
	- sleep (x): x 시간 만큼 일시 정지 (1 second for 1000)
	- return [x]: 진행 중인 함수를 리턴, x 가 있으면 x 변수를 리턴
	- break: 실행중인 루프문을 빠져 나옴
	- if (x) / else / elif: 조건 문이 true 이면 if 다음을 false 이면 else 다음을 처리
	- for : for (var a in 1, 100, 1) 순서는 초기값, 최종값, 증가값
	- foreach : map, list, set 자료구조에 사용. Must be foreach (var a[, b] in map)
	- true / false: bool 타입 변수에 저장되는 값
	- null: 아무 값이 없음
	- ++ / -- : 변수 값을 1증가 또는 1감소
	- && / || : 논리 연산자 (c문법과 동일)
	- > / < / >= / <= : 논리 연산자 (c문법과 동일)
	- x..y: x와 y를 문자열해서 합친 문자열을 리턴

### Built-in system function use system.
	## Basic
	- print (x): x를 문자열로 출력

	## system
	- clock ():  현재 시간 리턴
	- load ():  스크립트를 로드 (컴파일 과정을 진행함)해서 모듈로 리턴
	- pcall (x) : x 모듈을 실행
	- meta(x,y) : bx 변수에 y meta 함수를 바인딩
	- set(x) : list 를 set 으로 변환해서 리턴
	
	## coroutine
	- create (): 코루틴을 생성해서 리턴 (suspended mode 로 생성)
	- resume (x): 코루틴을 Active 시킴
	- status (x): 코루틴 상태를 문자열로 리턴
	- close ([x]): 코루틴 Close

	## math
	- abs (x): x 의 양수를 리턴 (c함수과 동일)
	- acos (x): (c함수과 동일)
	- asin (x): (c함수과 동일)
	- atan (x): (c함수과 동일)
	- ceil (x): (c함수과 동일)
	- floor (x): (c함수과 동일)
	- sin (x): (c함수과 동일)
	- cos (x): (c함수과 동일)
	- tan (x): (c함수과 동일)
	- log (x): (c함수과 동일)
	- log10 (x): (c함수과 동일)
	- pow (x, y): (c함수과 동일)
	- deg (x): radian 값을 degree 값으로 리턴
	- radian (x): degree 값을 radian 값으로 리턴
	- sqrt (x): (c함수과 동일)
	- srand (x): (c함수과 동일)
	- rand (): (c함수과 동일)

    ## string
	- len (): 문자열 길이 리턴
	- find (x): 문자열을 찾아서 위치 인덱스를 리턴 (stl 함수과 동일)
	- sub (x, y): x 위치에서 부터 y 갯수만큼의 문자열을 리턴 (stl 함수과 동일)
	- upper () : 영문 소문자를 대문자로 변경해서 리턴
	- lower () : 영문 대문자를 소문자로 변경해서 리턴
	- trim () : 문자열 left / right 끝에 있는 공백을 모두 제거한 결과를 리턴
	- ltrim () : 문자열 left 끝에 있는 공백을 모두 제거한 결과를 리턴
	- rtrim () : 문자열 right 끝에 있는 공백을 모두 제거한 결과를 리턴
	- replace (x, y) : x 문자열을 y문자열로 치환해서 리턴
	- split (x) : x 문자열을 기준으로 문자열을 split 하고 list 형태로 리턴

	## list
	- len () : 리스트 아이템 갯수를 리턴
	- resize (x) : 리스트 아이템 갯수 변경
	- append (x, [y]) : 리스트에 아이템을 추가 (y 는 위치)

	## map
	- len () : map 아이템 갯수를 리턴
	- reserve (x) : 메모리를 x 갯수만큼 할당 (아이템 갯수는 증가하지 않음)
	- sort () : map 의 value를 정렬
	- keys() : map 의 key 들을 list 자료구조에 담아서 리턴
	- values() : map 의 value 들을 list 자료구조에 담아서 리턴

	## set
	- len () : set 의 아이템 갯수를 리턴

### Comment
	- // 한줄 주석
	- / * * / 여러줄 주석


#### Neo Script Code Image
![](/docs/img/vs001.png)

#### Neo Script Output Image
![](/docs/img/vs002.png)



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
