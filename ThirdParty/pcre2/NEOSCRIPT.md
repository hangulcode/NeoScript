# PCRE2 integration

Upstream: https://github.com/PCRE2Project/pcre2/releases/tag/pcre2-10.49
Source archive SHA-256: `929f0b20e62879252a15886b06c89f1edef61a363cbd5826fb041080a5e557ae`.
The upstream C sources and headers are unmodified. `pcre2.h` and `pcre2_chartables.c` are copied
from the upstream generic/distribution files. `config.h` enables 8-bit code units and stdint.h;
JIT and Unicode properties are disabled. `NeoRegexVendor.c` includes the required upstream C99
translation units. C++11 hosts compiling source directly must compile that file as C and link it
alongside `NeoSource/*.cpp`; CMake and the Windows static-library project already do so.
See `LICENCE.md` for the complete BSD-3-Clause WITH PCRE2-exception terms.
