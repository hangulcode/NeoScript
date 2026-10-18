// Keep the portable upstream implementation in one translation unit, including
// hosts that compile NeoSource/*.cpp directly. Upstream supports C and C++.
#define YYJSON_DISABLE_READER 0
#define YYJSON_DISABLE_WRITER 0
#include "../ThirdParty/yyjson/yyjson.c"
