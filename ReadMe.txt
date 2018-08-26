Neo Script 설명서
	- 문법은 C와 비슷한 문법을 사용 하지만, Lua 와도 일부 비슷 합니다.


※ Visual Studio Pro 2017 C++ 로 개발 되었습니다.
	- 추후 어느 정도 기능 추가가 완료 되면 C# 으로 포팅을 합니다.
	

	
※ 샘플
	- console_callback : C++ 와 Neo 스크립트 서로 함수 호출 합니다.
	- console_9_times : 간단한 구구단 샘플 입니다.
	- console_table : 테이블 사용법 샘플 입니다.
	- console_slice_run : 스크립트로 제작한 부분을 일정시간 or 명령어 만큼 순차적으로 매프레임 실행합니다.
	- console_time_limit : 스크립트 함수 호출을 일정 시간이 지나면 빠져 나오게 처리 합니다.
		console_slice_run 와 다르게 console_time_limit은 다음 프레임에 이어서 실행 하지 않습니다.

※ var 자료 구조
	- null : 변수에 아무 값도 없는 상태 (변수를 초기화 하지 않거나 "var a;" or "var a = null;" 인 경우)
	- bool : true / false 값을 저장
	- int : 4바이트 signed 정수 값 저장
	- double : 8바이트 부동 소수점 저장 
	- string : 문자열 저장 
	- table : 테이블 저장 (사용법은 Lua 와 비슷함)

	- string / table : 메모리 절약및 성능을 위해서 레퍼런스로 관리
	- int / double : "a = 1;" 하면 int 가 되며, "a = 1.0" 하면 double 값이 저장됨.
	- int 와 double 를 4칙 연산 하면 double 값이 됨.




※ Neo Scrit 예약어
	- var : 이후에 나오는 변수이름으로 값을 저장
	- fun : 함수의 시작을 알림
	- import : fun 앞에 붙일 수 있고 c++ 함수를 호출 하기 위해서 선언
	- export : fun 앞에 붙일 수 있고 해당 함수는 외부(c++)에서 호출 될 수 있음
	- tostring(x) : x 변수의 값을 string 으로 변경
	- toint(x) : x 변수의 값을 int 으로 변경
	- tofloat(x) : x 변수의 값을 double 으로 변경
	- tosize(x) : x 변수가 테이블이면 테이블안의 갯수를 리턴, 테이블이 아니면 0 리턴
	- totype(x) : x 변수의 데이터 타입을 스트링으로 리턴
	- sleep(x) : x 변수 값 만큼 일시 정지 (1000 일경우 1초)
	- return [x]: x 값 리턴
	- break : for / foreach / while 루프문에서 나오게 함
	- if(x) / else : c++ 과 동일
	- for / while : c++ 과 동일
	- foreach : talbe 변수에서만 사용 되며 반드시 foreach(var a, b in table) 형태여야 함
	- true / false : bool 타입의 변수에 저장되는 값
	- null : 아무 값도 없는 상태
	- ++ / -- : c++ 과 동일 (변수 값을 1 증가 하거나 1감소)
	- && / || : c++ 과 동일
	- > / < / >= / <= : c++ 과 동일
	- x..y : x문자열과 y문자열 합친 문자열을 리턴

※ 내장 system 함수 (호출시 앞에 system. 을 붙여서 사용)
	- abs(x) : c++ 과 동일
	- acos(x) : c++ 과 동일
	- asin(x) : c++ 과 동일
	- atan(x) : c++ 과 동일
	- ceil(x) : c++ 과 동일
	- floor(x) : c++ 과 동일
	- sin(x) : c++ 과 동일
	- cos(x) : c++ 과 동일
	- tan(x) : c++ 과 동일
	- log(x) : c++ 과 동일
	- log10(x) : c++ 과 동일
	- pow(x, y) : c++ 과 동일
	- deg(x) : radian -> degree
	- radian(x) : degree -> radian
	- sqrt(x) : c++ 과 동일
	- rand(x) : c++ 과 동일

	- str_sub(x,y,z) : x문자열을 y 오프셋 부터 z갯수 만큼의 문자열로 변환해서 리턴
	- str_len(x) : x문자열 길이를 리턴
	- str_find(x, y) : x문자열 에서 y문자열을 검색하고 위치를 리턴 (c++ 과 동일)
	- print(x) : x문자열 출력

※ 주석 (Comment)
	- C 문법과 동일하게 동작
	- // 은 한줄 주석
	- /* */ 은 멀티라인 범위 주석

- END
