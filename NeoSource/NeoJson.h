#pragma once
#include <string>
#include <vector>

namespace NeoScript
{
struct JsonResult
{
    std::string error;
    std::vector<std::string> warnings;
    bool Fail(const std::string& path, const std::string& reason)
    { if (error.empty()) error = path + ": " + reason; return false; }
};

class CNeoVMWorker;
struct VarInfo;
namespace JsonInternal
{
bool Encode(const VarInfo& value, std::string& text, JsonResult& result, bool sortKeys);
bool Decode(CNeoVMWorker& worker, const char* text, size_t size, VarInfo& value,
    JsonResult& result, bool strictIntegers = true, bool requireMap = false);
}
}
