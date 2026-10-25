// 상류 PCRE2 C99 소스를 한 번역 단위로 빌드한다. JIT와 Unicode 확장은 사용하지 않는다.
#define HAVE_CONFIG_H 1
#define PCRE2_STATIC 1
#define PCRE2_CODE_UNIT_WIDTH 8
#include "../ThirdParty/pcre2/pcre2_auto_possess.c"
#include "../ThirdParty/pcre2/pcre2_chartables.c"
#include "../ThirdParty/pcre2/pcre2_chkdint.c"
#include "../ThirdParty/pcre2/pcre2_compile.c"
#include "../ThirdParty/pcre2/pcre2_compile_cgroup.c"
#include "../ThirdParty/pcre2/pcre2_compile_class.c"
#include "../ThirdParty/pcre2/pcre2_config.c"
#include "../ThirdParty/pcre2/pcre2_context.c"
#include "../ThirdParty/pcre2/pcre2_convert.c"
// 상류 DFA 구현의 size_t 축소 경고만 이 파일 포함 구간에 한정한다.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4267)
#endif
#include "../ThirdParty/pcre2/pcre2_dfa_match.c"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "../ThirdParty/pcre2/pcre2_error.c"
#include "../ThirdParty/pcre2/pcre2_extuni.c"
#include "../ThirdParty/pcre2/pcre2_find_bracket.c"
#include "../ThirdParty/pcre2/pcre2_jit_compile.c"
#include "../ThirdParty/pcre2/pcre2_maketables.c"
#include "../ThirdParty/pcre2/pcre2_match.c"
#include "../ThirdParty/pcre2/pcre2_match_data.c"
#include "../ThirdParty/pcre2/pcre2_match_next.c"
#include "../ThirdParty/pcre2/pcre2_newline.c"
#include "../ThirdParty/pcre2/pcre2_ord2utf.c"
#include "../ThirdParty/pcre2/pcre2_pattern_info.c"
#include "../ThirdParty/pcre2/pcre2_script_run.c"
#include "../ThirdParty/pcre2/pcre2_serialize.c"
#include "../ThirdParty/pcre2/pcre2_string_utils.c"
#include "../ThirdParty/pcre2/pcre2_study.c"
#include "../ThirdParty/pcre2/pcre2_substitute.c"
#include "../ThirdParty/pcre2/pcre2_substring.c"
#include "../ThirdParty/pcre2/pcre2_tables.c"
#include "../ThirdParty/pcre2/pcre2_ucd.c"
#include "../ThirdParty/pcre2/pcre2_valid_utf.c"
#include "../ThirdParty/pcre2/pcre2_xclass.c"
