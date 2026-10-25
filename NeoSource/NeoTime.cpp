#include "NeoTime.h"
#include "NeoVMInternal.h"
#include "NeoVMWorker.h"
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>

namespace NeoScript
{
namespace TimeLibrary
{
namespace
{
const char spanType = 0;
const char watchType = 0;
const NS_INT intMin = (std::numeric_limits<NS_INT>::min)();
const NS_INT intMax = (std::numeric_limits<NS_INT>::max)();
typedef std::chrono::steady_clock MonotonicClock;

struct Span
{
    const NS_INT milliseconds;
    explicit Span(NS_INT value) : milliseconds(value) {}
};

struct Watch
{
    MonotonicClock::time_point start;
    Watch() : start(MonotonicClock::now()) {}
};

bool SpanMethod(INeoVMWorker*, void*, const VMString*, short);
bool WatchMethod(INeoVMWorker*, void*, const VMString*, short);

bool Error(CNeoVMWorker* worker, const char* text)
{
    worker->SetError(text);
    return false;
}

bool Add(NS_INT a, NS_INT b, NS_INT& result)
{
    if ((b > 0 && a > intMax - b) || (b < 0 && a < intMin - b)) return false;
    result = a + b;
    return true;
}

bool Subtract(NS_INT a, NS_INT b, NS_INT& result)
{
    if ((b > 0 && a < intMin + b) || (b < 0 && a > intMax + b)) return false;
    result = a - b;
    return true;
}

template<class T>
bool ReturnObject(CNeoVMWorker* worker, const std::shared_ptr<T>& owner,
    const void* type, Neo_NativeFunction method)
{
    VarInfo* result = worker->GetReturnVar();
    worker->ResetVarType(result, VAR_FP_NATIVE, 0);
    return NeoVMSystem::RegisterTableCallBack(result, owner.get(), method, nullptr,
        type, owner.get(), owner);
}

bool ReturnSpan(CNeoVMWorker* worker, NS_INT milliseconds)
{
    return ReturnObject(worker, std::make_shared<Span>(milliseconds), &spanType, SpanMethod);
}

const Span* ReadSpan(VarInfo* value)
{
    if (value->GetType() != VAR_FP_NATIVE || value->_fpNative->_identityType != &spanType)
        return nullptr;
    return static_cast<const Span*>(value->_fpNative->_pUserData);
}

struct Parts
{
    int sign;
    NS_INT days, hours, minutes, seconds, milliseconds;
    explicit Parts(NS_INT value)
    {
        sign = value < 0 ? -1 : (value > 0 ? 1 : 0);
        // INT64_MIN도 부호 있는 정수에서 직접 부정하지 않고 분해한다.
        uint64_t magnitude = value < 0 ? uint64_t(0) - static_cast<uint64_t>(value)
                                      : static_cast<uint64_t>(value);
        milliseconds = static_cast<NS_INT>(magnitude % 1000); magnitude /= 1000;
        seconds = static_cast<NS_INT>(magnitude % 60); magnitude /= 60;
        minutes = static_cast<NS_INT>(magnitude % 60); magnitude /= 60;
        hours = static_cast<NS_INT>(magnitude % 24);
        days = static_cast<NS_INT>(magnitude / 24);
    }
};

bool ReturnParts(CNeoVMWorker* worker, NS_INT value)
{
    const Parts parts(value);
    const char* keys[] = {"sign", "days", "hours", "minutes", "seconds", "milliseconds"};
    const NS_INT values[] = {parts.sign, parts.days, parts.hours, parts.minutes, parts.seconds, parts.milliseconds};
    VarInfo* result = worker->GetReturnVar();
    worker->ResetVarType(result, VAR_MAP, 6);
    for (int i = 0; i < 6; ++i)
    {
        VarInfo key;
        worker->Var_SetString(&key, keys[i]);
        // 맵 할당이 예외를 던져도 임시 키의 참조를 반납한다.
        struct KeyGuard
        {
            CNeoVMWorker* worker;
            VarInfo* key;
            ~KeyGuard() { worker->Var_Release(key); }
        } guard = {worker, &key};
        VarInfo* slot = result->_tbl->Insert(&key);
        if (!slot) return false;
        worker->Var_SetInt(slot, values[i]);
    }
    return true;
}

bool SpanMethod(INeoVMWorker* base, void* data, const VMString* name, short args)
{
    CNeoVMWorker* worker = static_cast<CNeoVMWorker*>(base);
    // 반환 슬롯을 교체하면서 마지막 별칭이 해제될 수 있으므로 값을 먼저 복사한다.
    const NS_INT value = static_cast<const Span*>(data)->milliseconds;
    const std::string& method = name->_str;
    if (method == "total_milliseconds" && args == 0)
    {
        worker->ReturnValue(value);
        return true;
    }
    if (method == "total_seconds" && args == 0)
    {
        worker->ReturnValue(static_cast<NS_FLOAT>(static_cast<double>(value) / 1000.0));
        return true;
    }
    if (method == "parts" && args == 0) return ReturnParts(worker, value);
    if (method == "format" && args == 0)
    {
        const Parts parts(value);
        std::ostringstream text;
        text.imbue(std::locale::classic());
        if (parts.sign < 0) text << '-';
        text << parts.days << '.' << std::setfill('0') << std::setw(2) << parts.hours << ':'
             << std::setw(2) << parts.minutes << ':' << std::setw(2) << parts.seconds << '.'
             << std::setw(3) << parts.milliseconds;
        worker->ReturnValue(text.str().c_str());
        return true;
    }
    if ((method == "add" || method == "subtract" || method == "compare") && args == 1)
    {
        const Span* other = ReadSpan(worker->GetStack(1));
        if (!other) return Error(worker, "time_span operation expects a time_span");
        const NS_INT operand = other->milliseconds;
        if (method == "compare")
        {
            worker->ReturnValue(value < operand ? -1 : (value > operand ? 1 : 0));
            return true;
        }
        NS_INT result;
        const bool valid = method == "add" ? Add(value, operand, result) : Subtract(value, operand, result);
        if (!valid) return Error(worker, "time_span milliseconds overflow int64");
        return ReturnSpan(worker, result);
    }
    return false;
}

bool WatchMethod(INeoVMWorker* base, void* data, const VMString* name, short args)
{
    if (args != 0) return false;
    CNeoVMWorker* worker = static_cast<CNeoVMWorker*>(base);
    Watch* watch = static_cast<Watch*>(data);
    const std::string& method = name->_str;
    if (method == "restart")
    {
        watch->start = MonotonicClock::now();
        worker->ReturnValue();
        return true;
    }
    if (method != "elapsed" && method != "elapsed_ms") return false;
    const NS_INT elapsed = static_cast<NS_INT>(std::chrono::duration_cast<std::chrono::milliseconds>(
        MonotonicClock::now() - watch->start).count());
    if (method == "elapsed") return ReturnSpan(worker, elapsed);
    worker->ReturnValue(elapsed);
    return true;
}

long long DaysBeforeYear(int year)
{
    const long long preceding = year - 1;
    return preceding * 365 + preceding / 4 - preceding / 100 + preceding / 400;
}

bool LeapYear(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
}

bool TimeMilliseconds(CNeoVMWorker* worker, VarInfo*, short args)
{
    if (args != 0) return false;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    worker->ReturnValue(static_cast<NS_INT>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()));
    return true;
}

bool TimeSpanMilliseconds(CNeoVMWorker* worker, VarInfo*, short args)
{
    if (args != 1 || worker->GetStack(1)->GetType() != VAR_INT) return false;
    return ReturnSpan(worker, worker->GetStack(1)->_int);
}

bool TimeSpan(CNeoVMWorker* worker, VarInfo*, short args)
{
    if (args != 5) return false;
    const NS_INT multipliers[] = {86400000, 3600000, 60000, 1000, 1};
    NS_INT total = 0;
    for (int i = 0; i < 5; ++i)
    {
        VarInfo* value = worker->GetStack(i + 1);
        if (value->GetType() != VAR_INT) return false;
        const NS_INT scale = multipliers[i];
        if (value->_int < intMin / scale || value->_int > intMax / scale ||
            !Add(total, value->_int * scale, total))
            return Error(worker, "time_span milliseconds overflow int64");
    }
    return ReturnSpan(worker, total);
}

bool Stopwatch(CNeoVMWorker* worker, VarInfo*, short args)
{
    if (args != 0) return false;
    return ReturnObject(worker, std::make_shared<Watch>(), &watchType, WatchMethod);
}

bool UtcCalendar(long long seconds, std::tm& calendar)
{
    // CRT의 time_t 폭과 gmtime 음수 제한에 의존하지 않는다. strftime의 공통 연도 범위만 쓴다.
    if (seconds < -2208988800LL || seconds > 253402300799LL) return false;
    long long days = seconds / 86400;
    int daySeconds = static_cast<int>(seconds % 86400);
    if (daySeconds < 0) { daySeconds += 86400; --days; }
    const long long absoluteDays = days + DaysBeforeYear(1970);
    int lower = 1900, upper = 10000;
    while (lower + 1 < upper)
    {
        const int middle = lower + (upper - lower) / 2;
        if (DaysBeforeYear(middle) <= absoluteDays) lower = middle;
        else upper = middle;
    }
    calendar = std::tm();
    calendar.tm_year = lower - 1900;
    calendar.tm_yday = static_cast<int>(absoluteDays - DaysBeforeYear(lower));
    int monthDay = calendar.tm_yday;
    const int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    while (calendar.tm_mon < 11)
    {
        const int count = monthDays[calendar.tm_mon] + (calendar.tm_mon == 1 && LeapYear(lower) ? 1 : 0);
        if (monthDay < count) break;
        monthDay -= count;
        ++calendar.tm_mon;
    }
    calendar.tm_mday = monthDay + 1;
    calendar.tm_hour = daySeconds / 3600;
    calendar.tm_min = daySeconds / 60 % 60;
    calendar.tm_sec = daySeconds % 60;
    calendar.tm_wday = static_cast<int>((days + 4) % 7);
    if (calendar.tm_wday < 0) calendar.tm_wday += 7;
    return true;
}

void GetBuiltins(std::vector<NeoBuiltinInfo>& out)
{
    const auto add = [&out](const char* module, const char* name, const char* ret, const char* parameter = nullptr)
    {
        NeoBuiltinInfo info;
        info.module = module; info.name = name; info.ret = ret;
        info.argCount = parameter ? 1 : 0;
        if (parameter) info.params.push_back(parameter);
        out.push_back(info);
    };
    add("time_span", "total_milliseconds", "int");
    add("time_span", "total_seconds", "float");
    add("time_span", "parts", "map");
    add("time_span", "format", "string");
    add("time_span", "add", "time_span", "time_span other");
    add("time_span", "subtract", "time_span", "time_span other");
    add("time_span", "compare", "int", "time_span other");
    add("stopwatch", "elapsed", "time_span");
    add("stopwatch", "elapsed_ms", "int");
    add("stopwatch", "restart", "void");
}
}
}
