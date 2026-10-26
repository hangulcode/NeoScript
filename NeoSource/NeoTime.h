#pragma once

#include <ctime>
#include <vector>

namespace NeoScript
{
class CNeoVMWorker;
struct VarInfo;
struct NeoBuiltinInfo;

namespace TimeLibrary
{
bool TimeMilliseconds(CNeoVMWorker* worker, VarInfo*, short args);
bool TimeSpan(CNeoVMWorker* worker, VarInfo*, short args);
bool TimeSpanMilliseconds(CNeoVMWorker* worker, VarInfo*, short args);
bool TimeSpanSeconds(CNeoVMWorker* worker, VarInfo*, short args);
bool UtcTimeMilliseconds(CNeoVMWorker* worker, VarInfo*, short args);
bool UtcParts(CNeoVMWorker* worker, VarInfo*, short args);
bool UtcFormatMilliseconds(CNeoVMWorker* worker, VarInfo*, short args);
bool UtcParseMilliseconds(CNeoVMWorker* worker, VarInfo*, short args);
bool Stopwatch(CNeoVMWorker* worker, VarInfo*, short args);
bool UtcCalendar(long long seconds, std::tm& calendar);
void GetBuiltins(std::vector<NeoBuiltinInfo>& out);
}
}
