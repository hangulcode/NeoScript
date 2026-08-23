// ============================================================================
// C++ 성능 벤치마크 — bench.ns / bench.lua 와 완전히 동일한 알고리즘
// 네이티브 상한선(reference ceiling) 참조용.
//
// 빌드: cl /O2 /EHsc /std:c++17 bench.cpp
// 출력: name|microseconds|checksum
// ============================================================================
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

static const int REPS = 5;

using Clock = std::chrono::steady_clock;
static double Now()
{
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

// ---------------------------------------------------------------- 1. 정수 루프
static int64_t LoopSum(int outer, int inner)
{
    int64_t last = 0;
    for (int o = 0; o < outer; ++o)
    {
        const int bias = (int)(last & 1);  // 직전 결과에 의존(진짜 데이터 의존성) → 축약 불가
        int64_t sum = 0;
        for (int i = 0; i < inner; ++i)
            sum += i + bias;
        last = sum;
    }
    return last;
}

// ------------------------------------------------------------- 2. 부동소수 산술
static int64_t FloatMath(int outer, int inner)
{
    double last = 0.0;
    for (int o = 0; o < outer; ++o)
    {
        const int bias = (int)((int64_t)last & 1);  // 직전 결과 의존
        double acc = 0.0;
        for (int i = 0; i < inner; ++i)
        {
            double x = ((i + bias + (int64_t)acc) & 15) * 0.5;   // acc 의존 → 예측 불가
            acc += (x * 1.5 - x * 0.25);
        }
        last = acc;
    }
    return (int64_t)last;
}

// ---------------------------------------------------------------- 3. 함수 호출
// noinline: 스크립트의 "함수 호출" 비용과 대응시키기 위해 인라인 제거.
// (인라인 허용 시 호출 자체가 사라져 '함수 호출' 측정이 성립하지 않는다.)
__declspec(noinline) static int64_t Inc(int64_t x)
{
    return x + 1;
}
static int64_t FuncCall(int outer, int inner)
{
    int64_t last = 0;
    for (int o = 0; o < outer; ++o)
    {
        const int bias = (int)(last & 1);  // 직전 결과 의존
        int64_t sum = 0;
        for (int i = 0; i < inner; ++i)
            sum += Inc(i + bias);
        last = sum;
    }
    return last;
}

// ------------------------------------------------------------------- 4. 재귀
static int64_t Fib(int n)
{
    if (n < 2)
        return n;
    return Fib(n - 1) + Fib(n - 2);
}

// -------------------------------------------------------- 5. 배열 순차 쓰기/읽기
static int64_t ArrayRW(int size, int reps)
{
    std::vector<int64_t> a(size, 0);
    int64_t last = 0;
    for (int r = 0; r < reps; ++r)
    {
        const int bias = (int)(last & 1);  // 직전 결과 의존
        for (int i = 0; i < size; ++i)
            a[i] = (i + bias) & 255;
        int64_t sum = 0;
        for (int i = 0; i < size; ++i)
            sum += a[i];
        last = sum;
    }
    return last;
}

// ------------------------------------------------------- 6. 해시맵(문자열 키) 조회
static int64_t MapStr(int outer, int inner)
{
    std::unordered_map<std::string, int64_t> m;
    m["alpha"] = 1;
    m["bravo"] = 2;
    m["charlie"] = 3;
    m["delta"] = 4;
    m["echo"] = 5;
    m["foxtrot"] = 6;
    m["golf"] = 7;
    m["hotel"] = 8;
    // 스크립트의 상수 문자열(미리 만들어진 VM 문자열)에 대응 — 조회마다 임시 string 생성 방지.
    static const std::string kAlpha = "alpha";
    static const std::string kHotel = "hotel";
    static const std::string kCharlie = "charlie";
    static const std::string kFoxtrot = "foxtrot";

    int64_t last = 0;
    for (int o = 0; o < outer; ++o)
    {
        int64_t sum = 0;
        for (int i = 0; i < inner; ++i)
        {
            sum += m.find(kAlpha)->second;
            sum += m.find(kHotel)->second;
            sum += m.find(kCharlie)->second;
            sum += m.find(kFoxtrot)->second;
        }
        last = sum;
    }
    return last;
}

// ----------------------------------------------------------- 7. 문자열 생성/길이
static int64_t StringOps(int outer, int inner)
{
    int64_t last = 0;
    for (int o = 0; o < outer; ++o)
    {
        int64_t total = 0;
        for (int i = 0; i < inner; ++i)
        {
            std::string s = "item" + std::to_string(i);
            total += (int64_t)s.size();
        }
        last = total;
    }
    return last;
}

// ------------------------------------------- 8. 파티클 시뮬(게임형 부동소수+배열)
static int64_t Particles(int count, int steps)
{
    std::vector<double> px(count), py(count), vx(count), vy(count);
    for (int i = 0; i < count; ++i)
    {
        px[i] = (i & 63) * 1.0;
        py[i] = (i & 31) * 1.0;
        vx[i] = ((i & 7) - 4) * 0.25;
        vy[i] = ((i & 15) - 8) * 0.125;
    }

    const double dt = 0.015625;   // 1/64 (이진 정확값)
    for (int s = 0; s < steps; ++s)
    {
        for (int i = 0; i < count; ++i)
        {
            double nx = px[i] + vx[i] * dt;
            double ny = py[i] + vy[i] * dt;
            if (nx < 0.0 || nx > 64.0)
                vx[i] = -vx[i];
            if (ny < 0.0 || ny > 32.0)
                vy[i] = -vy[i];
            px[i] = nx;
            py[i] = ny;
        }
    }

    int64_t chk = 0;
    for (int i = 0; i < count; ++i)
        chk += (int64_t)(px[i] + py[i] + 1000.0);
    return chk;
}

// ============================================================================
// 하네스
// ============================================================================
static void Report(const char* name, double best, int64_t chk)
{
    printf("%s|%lld|%lld\n", name, (long long)(best * 1000000.0), (long long)chk);
}

// volatile 인자: 반복마다 인자를 새로 읽어 컴파일러가 "같은 인자의 순수 호출"로 보고
// 공통 부분식 제거(CSE)로 1회만 실행하는 것을 막는다. 인터프리터(Neo/Lua)는 항상 매번
// 실행하므로, 이 장치가 있어야 세 언어가 실제로 같은 작업량을 수행한 비교가 된다.
static volatile int g_one = 1;

template <typename Fn>
static void Measure(const char* name, Fn fn, int a, int b)
{
    fn(a > 2 ? 2 : a, b);                       // warmup(작게)
    double best = 1000000.0;
    int64_t chk = 0;
    for (int r = 0; r < REPS; ++r)
    {
        const int aa = a * g_one;               // 매 반복 volatile 재읽기
        const int bb = b * g_one;
        double t0 = Now();
        chk = fn(aa, bb);
        double dt = Now() - t0;
        if (dt < best) best = dt;
    }
    Report(name, best, chk);
}

int main()
{
    printf("# cpp bench begin\n");

    // volatile: 인자를 런타임 값으로 만들어 컴파일타임 상수 폴딩(계산 자체가 사라지는 것)을 막는다.
    // 스크립트는 언제나 런타임 계산이므로, 이렇게 해야 같은 일을 실제로 수행한 비교가 된다.
    volatile int vN = 1;

    Measure("loop_sum", LoopSum, 10000 * vN, 5000);       // 5천만 회
    Measure("float_math", FloatMath, 2000 * vN, 5000);    // 1천만 회
    Measure("func_call", FuncCall, 2000 * vN, 5000);      // 1천만 회

    {
        volatile int vFib = 32;
        Fib(10);
        double best = 1000000.0;
        int64_t chk = 0;
        for (int r = 0; r < REPS; ++r)
        {
            double t0 = Now();
            chk = Fib((int)vFib);
            double dt = Now() - t0;
            if (dt < best) best = dt;
        }
        Report("fib_recursive", best, chk);
    }

    Measure("array_rw", ArrayRW, 200000 * vN, 20);        // 400만 쓰기 + 400만 읽기
    Measure("map_str", MapStr, 500 * vN, 5000);           // 1천만 조회
    Measure("string_ops", StringOps, 20 * vN, 50000);     // 100만 문자열 생성
    Measure("particles", Particles, 10000 * vN, 200);     // 200만 업데이트

    printf("# cpp bench end\n");
    return 0;
}

