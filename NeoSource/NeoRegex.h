#pragma once

#include <memory>
#include <string>
#include <vector>

namespace NeoScript {
struct RegexResult {
    bool matched = false;
    std::string text;
    std::vector<std::string> captures;
};

class RegexEngine {
public:
    RegexEngine();
    ~RegexEngine();
    RegexResult Run(const std::string& subject, const std::string& pattern,
        const std::string& replacement, int operation, bool ignoreCase);
private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
}
