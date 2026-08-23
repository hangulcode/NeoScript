-- ============================================================================
-- Lua 5.5 성능 벤치마크 — bench.ns / bench.cpp 와 완전히 동일한 알고리즘
-- 측정: warmup 1회 + REPS 회 중 최소값. 출력: name|microseconds|checksum
-- ============================================================================
local REPS = 5
local clock = os.clock

-- ---------------------------------------------------------------- 1. 정수 루프
local function LoopSum(outer, inner)
    local last = 0
    for o = 1, outer do
        local bias = last & 1          -- 직전 결과에 의존(진짜 데이터 의존성) → 축약 불가
        local sum = 0
        for i = 0, inner - 1 do
            sum = sum + i + bias
        end
        last = sum
    end
    return last
end

-- ------------------------------------------------------------- 2. 부동소수 산술
local function FloatMath(outer, inner)
    local last = 0.0
    for o = 1, outer do
        local bias = math.floor(last) & 1  -- 직전 결과 의존
        local acc = 0.0
        for i = 0, inner - 1 do
            local x = ((i + bias + math.floor(acc)) & 15) * 0.5   -- acc 의존 → 예측 불가
            acc = acc + (x * 1.5 - x * 0.25)
        end
        last = acc
    end
    return math.floor(last)
end

-- ---------------------------------------------------------------- 3. 함수 호출
local function Inc(x)
    return x + 1
end
local function FuncCall(outer, inner)
    local last = 0
    for o = 1, outer do
        local bias = last & 1          -- 직전 결과 의존
        local sum = 0
        for i = 0, inner - 1 do
            sum = sum + Inc(i + bias)
        end
        last = sum
    end
    return last
end

-- ------------------------------------------------------------------- 4. 재귀
local function Fib(n)
    if n < 2 then
        return n
    end
    return Fib(n - 1) + Fib(n - 2)
end

-- -------------------------------------------------------- 5. 배열 순차 쓰기/읽기
local function ArrayRW(size, reps)
    local a = {}
    for i = 1, size do a[i] = 0 end          -- resize 상당(배열부 미리 확보)
    local last = 0
    for r = 1, reps do
        local bias = last & 1          -- 직전 결과 의존
        for i = 0, size - 1 do
            a[i + 1] = (i + bias) & 255
        end
        local sum = 0
        for i = 0, size - 1 do
            sum = sum + a[i + 1]
        end
        last = sum
    end
    return last
end

-- ------------------------------------------------------- 6. 해시맵(문자열 키) 조회
local function MapStr(outer, inner)
    local m = {}
    m["alpha"] = 1
    m["bravo"] = 2
    m["charlie"] = 3
    m["delta"] = 4
    m["echo"] = 5
    m["foxtrot"] = 6
    m["golf"] = 7
    m["hotel"] = 8
    local last = 0
    for o = 1, outer do
        local sum = 0
        for i = 0, inner - 1 do
            sum = sum + m["alpha"]
            sum = sum + m["hotel"]
            sum = sum + m["charlie"]
            sum = sum + m["foxtrot"]
        end
        last = sum
    end
    return last
end

-- ----------------------------------------------------------- 7. 문자열 생성/길이
local function StringOps(outer, inner)
    local last = 0
    for o = 1, outer do
        local total = 0
        for i = 0, inner - 1 do
            local s = "item" .. i
            total = total + #s
        end
        last = total
    end
    return last
end

-- ------------------------------------------- 8. 파티클 시뮬(게임형 부동소수+배열)
local function Particles(count, steps)
    local px, py, vx, vy = {}, {}, {}, {}
    for i = 0, count - 1 do
        px[i + 1] = (i & 63) * 1.0
        py[i + 1] = (i & 31) * 1.0
        vx[i + 1] = ((i & 7) - 4) * 0.25
        vy[i + 1] = ((i & 15) - 8) * 0.125
    end

    local dt = 0.015625                       -- 1/64 (이진 정확값)
    for s = 1, steps do
        for i = 1, count do
            local nx = px[i] + vx[i] * dt
            local ny = py[i] + vy[i] * dt
            if nx < 0.0 or nx > 64.0 then
                vx[i] = -vx[i]
            end
            if ny < 0.0 or ny > 32.0 then
                vy[i] = -vy[i]
            end
            px[i] = nx
            py[i] = ny
        end
    end

    local chk = 0
    for i = 1, count do
        chk = chk + math.floor(px[i] + py[i] + 1000.0)
    end
    return chk
end

-- ============================================================================
-- 하네스
-- ============================================================================
local function report(name, best, chk)
    print(string.format("%s|%d|%d", name, math.floor(best * 1000000.0), chk))
end

local function measure(name, fn, a, b)
    fn(a > 2 and 2 or a, b)                   -- warmup(작게)
    local best = 1000000.0
    local chk = 0
    for r = 1, REPS do
        local t0 = clock()
        chk = fn(a, b)
        local dt = clock() - t0
        if dt < best then best = dt end
    end
    report(name, best, chk)
end

print("# lua bench begin")

measure("loop_sum",   LoopSum,   10000, 5000)     -- 5천만 회
measure("float_math", FloatMath,  2000, 5000)     -- 1천만 회
measure("func_call",  FuncCall,   2000, 5000)     -- 1천만 회

-- fib 은 인자 1개라 별도 처리
do
    Fib(10)
    local best, chk = 1000000.0, 0
    for r = 1, REPS do
        local t0 = clock()
        chk = Fib(32)
        local dt = clock() - t0
        if dt < best then best = dt end
    end
    report("fib_recursive", best, chk)
end

measure("array_rw",   ArrayRW,  200000, 20)       -- 400만 쓰기 + 400만 읽기
measure("map_str",    MapStr,      500, 5000)     -- 1천만 조회
measure("string_ops", StringOps,    20, 50000)    -- 100만 문자열 생성
measure("particles",  Particles, 10000, 200)      -- 200만 업데이트

print("# lua bench end")

