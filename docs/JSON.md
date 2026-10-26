# JSON conversion

Scope: standalone script JSON and the shared native conversion API.
Updated: 2026-10-18.
Validation: build with CMake and run `ctest -C Release -R neoscript_json_regression --output-on-failure`.

`import system;` exposes `jsonEncode(value, sortKeys = false)`, `jsonDecode(text)`,
and `jsonError()`. These work in the console without any engine services.
The converter uses yyjson 0.13.0 (MIT); redistribute `THIRD_PARTY_NOTICES.txt`.

## Values and errors

Strings, int64, float64, bool, null, string-keyed maps, and lists are supported.
The root may be any supported value. Map null values are omitted; list nulls keep their positions.
Shared containers are serialized by value and decoded as independent containers.
Cycles, functions, vectors, native objects (including null user data), dense arrays,
sets, modules, and other opaque runtime values fail; nothing is silently replaced with null.
Maximum container depth is 64, with a root map/list at depth 1.
No additional document-byte or element-count policy limit is imposed; VM index and memory limits still apply.

Invalid UTF-8, embedded NUL (including escaped U+0000), NaN, and infinity fail.
Input may start with a UTF-8 BOM. Output has no BOM.
Duplicate input object keys use the last value; a last null removes an earlier key.
A malformed earlier value still fails validation even if a duplicate would replace it.

Failure returns null and sets `jsonError()` to a path and reason. Paths use `$.a[3]`
or `$["unusual.key"]`. Syntax errors also report a byte offset.
Errors belong to the execution instance. Successful conversion clears the error;
reading the error does not clear it. Decoding the valid JSON text `null` succeeds with
null and an empty error, so inspect the error to distinguish this from failure.

## Numbers and deterministic output

Integer tokens without a decimal point or exponent must fit int64.
Decimal/exponent tokens become float64 and must be finite and within its range.
Writing preserves numeric kind: integer `100` versus float `100.0`.
Float output uses a short spelling that round-trips to the same float64, including
finite extremes and subnormal values. Negative floating zero is retained.

`sortKeys=true` sorts every map by UTF-8 byte order, without Unicode normalization;
list order is unchanged. This is useful for same-value test summaries, not a general
canonical JSON signature standard. Equivalent int and float values remain distinct.

## Native hosts

`CallContext::argToJson(argument, text, result, sortKeys)` and
`CallContext::retJson(text, result, strictIntegers, requireMap)` share the implementation
used by the script functions. `JsonResult` contains an error and optional warnings.
The default read mode is strict and permits any root. A host may require a map or
allow out-of-int64 integer tokens to become float64 with warnings; float range checks
still apply. File storage, encoding success, and durable I/O success are separate concerns.

Encoding is synchronous and borrows VM string bytes only until writing finishes;
there are no script callbacks or VM mutation during that interval. Decoding copies
strings into VM-owned storage before releasing the document. In-situ parsing is not used.
Each conversion owns its document; there is no global mutable JSON error or parser state.

## Build and portability

Compile both `NeoJson.cpp` and `NeoJsonVendor.cpp` alongside the other NeoSource files.
The latter compiles the unmodified portable yyjson source exactly once. Hosts must not
also compile `ThirdParty/yyjson/yyjson.c`, which would duplicate symbols.
No platform SIMD requirement, extra runtime DLL, or RTTI is introduced.
