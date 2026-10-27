cmake_minimum_required(VERSION 3.21)

project(NeoScript LANGUAGES CXX C)

# 상위 호스트가 더 높은 표준을 지정한 경우에는 유지하고, 단독 빌드는 C++11로 검사한다.
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# PCRE2 공급 소스는 C99이며, C++ 코어의 언어 표준은 바꾸지 않는다.
set_source_files_properties(NeoSource/NeoRegexVendor.c PROPERTIES LANGUAGE C)
if(NOT DEFINED CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 99)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)

# Single-config generators (for example Ninja and Unix Makefiles) otherwise
# leave CMAKE_BUILD_TYPE empty, which means no optimization flags by default.
# Multi-config generators select their configuration at build time, so leave
# those untouched.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

option(NEOSCRIPT_BUILD_SMOKE "Build the cross-platform API smoke test" ON)
set(NEOSCRIPT_EXAMPLES_DEFAULT ON)
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(NEOSCRIPT_EXAMPLES_DEFAULT OFF)
endif()
option(NEOSCRIPT_BUILD_EXAMPLES "Build the standalone game-host example" ${NEOSCRIPT_EXAMPLES_DEFAULT})
option(NEOSCRIPT_BUILD_SHARED "Build the flat-C shared library (Unity / P/Invoke hosts)" OFF)
option(NEOSCRIPT_BUILD_STATIC_C_ABI "Build the flat-C static library (iOS, where dynamic loading is not allowed)" OFF)

# C ABI는 C++ 호스트가 쓰는 코어와 분리한다. NeoSource/*.cpp를 직접 컴파일하는
# 호스트도 있으므로 P/Invoke 전용 코드를 이 디렉터리에 넣지 않는다.
set(NEOSCRIPT_C_ABI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Unity/Native)
set(NEOSCRIPT_C_ABI_SOURCES ${NEOSCRIPT_C_ABI_DIR}/NeoScriptC.cpp)

set(NEOSCRIPT_SOURCES
    NeoSource/NeoExport.cpp
    NeoSource/NeoJson.cpp
    NeoSource/NeoJsonVendor.cpp
    NeoSource/NeoRegex.cpp
    NeoSource/NeoRegexVendor.c
    NeoSource/NeoLib.cpp
    NeoSource/NeoTime.cpp
    NeoSource/NeoParser.cpp
    NeoSource/NeoScriptShim.cpp
    NeoSource/NeoTextLoader.cpp
    NeoSource/NeoVM.cpp
    NeoSource/NeoVMList.cpp
    NeoSource/NeoVMMap.cpp
    NeoSource/NeoVMProgram.cpp
    NeoSource/NeoVMSet.cpp
    NeoSource/NeoVMWorker.cpp
)

add_library(neoscript STATIC ${NEOSCRIPT_SOURCES})
target_include_directories(neoscript PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/NeoSource)
target_compile_features(neoscript PUBLIC cxx_std_11 PRIVATE c_std_99)

if(WIN32)
    target_compile_definitions(neoscript PRIVATE WIN_PRJ)
endif()

# Flat-C shared library.  Unity resolves DllImport("NeoScript") to NeoScript.dll
# on Windows and libNeoScript.so on Android, so the output name is fixed here.
# iOS links the static `neoscript` target instead and uses DllImport("__Internal").
if(NEOSCRIPT_BUILD_SHARED)
    add_library(NeoScriptShared SHARED ${NEOSCRIPT_SOURCES} ${NEOSCRIPT_C_ABI_SOURCES})
    target_include_directories(NeoScriptShared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/NeoSource
        ${NEOSCRIPT_C_ABI_DIR})
    target_compile_features(NeoScriptShared PUBLIC cxx_std_11 PRIVATE c_std_99)
    target_compile_definitions(NeoScriptShared PRIVATE NEOSCRIPT_C_BUILD_SHARED)
    # The DLL name is load-bearing: Unity resolves DllImport("NeoScript") by it.
    # Its Windows import library would then be NeoScript.lib, which is the same
    # file as the static neoscript.lib on a case-insensitive filesystem and
    # silently overwrites it -- leaving a 30KB stub that fails to link with a
    # wall of unresolved externals.  Rename the import library out of the way.
    set_target_properties(NeoScriptShared PROPERTIES
        OUTPUT_NAME NeoScript
        ARCHIVE_OUTPUT_NAME NeoScript_import)

    if(WIN32)
        target_compile_definitions(NeoScriptShared PRIVATE WIN_PRJ)
    else()
        # Export only what NS_API marks.  Unity loads many shared objects into one
        # process; leaking the whole C++ surface invites symbol collisions.
        set_target_properties(NeoScriptShared PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON)

        # Strip the release binary.  Unity does not strip plugin .so files, and an
        # unstripped one is ~19MB of debug symbols riding along in every APK.
        target_link_options(NeoScriptShared PRIVATE $<$<CONFIG:Release>:-s>)

        # Hidden visibility covers our own code, but the statically linked C++
        # runtime re-exports itself regardless -- measured at 1348 std:: symbols
        # on Android.  Unity loads libunity.so and libil2cpp.so with their own C++
        # runtime into the same process, so those exports let the dynamic linker
        # bind their calls to our copy and leave two libc++ instances sharing one
        # heap.  Keep everything that came out of a static archive to ourselves.
        target_link_options(NeoScriptShared PRIVATE -Wl,--exclude-libs,ALL)

        # yyjson explicitly requests default visibility; compiler flags alone
        # cannot hide it or libc++ template instantiations emitted in our TUs.
        # Keep the plugin's ELF export surface limited to the public C ABI.
        if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(NEOSCRIPT_EXPORT_MAP ${NEOSCRIPT_C_ABI_DIR}/neoscript.exports)
            target_link_options(NeoScriptShared PRIVATE "-Wl,--version-script=${NEOSCRIPT_EXPORT_MAP}")
            set_property(TARGET NeoScriptShared APPEND PROPERTY LINK_DEPENDS ${NEOSCRIPT_EXPORT_MAP})
        endif()

        if(ANDROID)
            # Google Play requires 16 KB page alignment for apps targeting Android 15
            # and later.  NDK r25 still links at 4 KB, and an unaligned .so fails the
            # upload check rather than the build -- far too late to find out.
            target_link_options(NeoScriptShared PRIVATE -Wl,-z,max-page-size=16384)
        endif()

        find_package(Threads REQUIRED)
        target_link_libraries(NeoScriptShared PUBLIC Threads::Threads)
    endif()
endif()

# Static flavour of the same shim, for iOS.  The platform forbids loading a
# dynamic library, so Unity links this .a into the player and the C# side
# resolves DllImport("__Internal") against the linked symbols.
if(NEOSCRIPT_BUILD_STATIC_C_ABI)
    add_library(NeoScriptStatic STATIC ${NEOSCRIPT_SOURCES} ${NEOSCRIPT_C_ABI_SOURCES})
    target_include_directories(NeoScriptStatic PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/NeoSource
        ${NEOSCRIPT_C_ABI_DIR})
    target_compile_features(NeoScriptStatic PUBLIC cxx_std_11 PRIVATE c_std_99)
    # NEOSCRIPT_C_BUILD_SHARED is deliberately absent: NS_API must stay empty so
    # the symbols are ordinary static-library symbols the linker can resolve.
    set_target_properties(NeoScriptStatic PROPERTIES OUTPUT_NAME NeoScriptUnity)
endif()

if(NEOSCRIPT_BUILD_SMOKE OR NEOSCRIPT_BUILD_EXAMPLES)
    enable_testing()
endif()

if(NEOSCRIPT_BUILD_SMOKE)
    find_package(Threads REQUIRED)

    add_executable(neoscript_ci_smoke
        Tests/ci_smoke.cpp
    )
    target_link_libraries(neoscript_ci_smoke PRIVATE neoscript Threads::Threads)

    # Deliberately compiled as C: if the ABI header ever grows something only a
    # C++ compiler accepts, a P/Invoke host would find out much later than this.
    add_executable(neoscript_c_abi_smoke
        Tests/c_abi_smoke.c
        ${NEOSCRIPT_C_ABI_SOURCES}
    )
    set_target_properties(neoscript_c_abi_smoke PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(neoscript_c_abi_smoke PRIVATE ${NEOSCRIPT_C_ABI_DIR})
    target_link_libraries(neoscript_c_abi_smoke PRIVATE neoscript Threads::Threads)

    enable_testing()
    add_executable(neoscript_array32_regression Tests/array32_regression.cpp)
    target_link_libraries(neoscript_array32_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_array32_regression COMMAND neoscript_array32_regression)
    add_executable(neoscript_double_regression Tests/double_regression.cpp)
    target_link_libraries(neoscript_double_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_double_regression COMMAND neoscript_double_regression)
    add_executable(neoscript_time_regression Tests/time_regression.cpp)
    target_link_libraries(neoscript_time_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_time_regression COMMAND neoscript_time_regression)
    set_tests_properties(neoscript_time_regression PROPERTIES TIMEOUT 30)
    add_executable(neoscript_http_regex_regression Tests/http_regex_regression.cpp)
    target_link_libraries(neoscript_http_regex_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_http_regex_regression COMMAND neoscript_http_regex_regression)
    set_tests_properties(neoscript_http_regex_regression PROPERTIES TIMEOUT 30)

    add_executable(neoscript_json_regression Tests/json_regression.cpp)
    target_link_libraries(neoscript_json_regression PRIVATE neoscript Threads::Threads)
    if(MSVC)
        target_compile_options(neoscript_json_regression PRIVATE /utf-8 /GR-)
    endif()
    add_test(NAME neoscript_json_regression COMMAND neoscript_json_regression)
    add_executable(neoscript_integer_division_regression Tests/integer_division_regression.cpp)
    target_link_libraries(neoscript_integer_division_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_integer_division_regression COMMAND neoscript_integer_division_regression)
    set_tests_properties(neoscript_integer_division_regression PROPERTIES TIMEOUT 30)

    add_executable(neoscript_integer_shift_regression Tests/integer_shift_regression.cpp)
    target_link_libraries(neoscript_integer_shift_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_integer_shift_regression COMMAND neoscript_integer_shift_regression)
    set_tests_properties(neoscript_integer_shift_regression PROPERTIES TIMEOUT 30)

    add_executable(neoscript_collection_api_regression Tests/collection_api_regression.cpp)
    target_link_libraries(neoscript_collection_api_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_collection_api_regression COMMAND neoscript_collection_api_regression)
    set_tests_properties(neoscript_collection_api_regression PROPERTIES TIMEOUT 30)

    add_executable(neoscript_runtime_edge_regression Tests/runtime_edge_regression.cpp)
    target_link_libraries(neoscript_runtime_edge_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_runtime_edge_regression COMMAND neoscript_runtime_edge_regression)
    set_tests_properties(neoscript_runtime_edge_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_math_boundary_regression Tests/math_boundary_regression.cpp)
    target_link_libraries(neoscript_math_boundary_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_math_boundary_regression COMMAND neoscript_math_boundary_regression)
    set_tests_properties(neoscript_math_boundary_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_hunt_runtime_regression Tests/hunt_runtime_regression.cpp)
    target_link_libraries(neoscript_hunt_runtime_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_hunt_runtime_regression COMMAND neoscript_hunt_runtime_regression)
    set_tests_properties(neoscript_hunt_runtime_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_hunt_library_regression Tests/hunt_library_regression.cpp)
    target_link_libraries(neoscript_hunt_library_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_hunt_library_regression COMMAND neoscript_hunt_library_regression)
    set_tests_properties(neoscript_hunt_library_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_hunt_parser_regression Tests/hunt_parser_regression.cpp)
    target_link_libraries(neoscript_hunt_parser_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_hunt_parser_regression COMMAND neoscript_hunt_parser_regression)
    set_tests_properties(neoscript_hunt_parser_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_hunt_storage_regression Tests/hunt_storage_regression.cpp)
    target_link_libraries(neoscript_hunt_storage_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_hunt_storage_regression COMMAND neoscript_hunt_storage_regression)
    set_tests_properties(neoscript_hunt_storage_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_hunt_evaluation_regression Tests/hunt_evaluation_regression.cpp)
    target_link_libraries(neoscript_hunt_evaluation_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_hunt_evaluation_regression COMMAND neoscript_hunt_evaluation_regression)
    set_tests_properties(neoscript_hunt_evaluation_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_read_arith_regression Tests/read_arith_regression.cpp)
    target_link_libraries(neoscript_read_arith_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_read_arith_regression COMMAND neoscript_read_arith_regression)
    set_tests_properties(neoscript_read_arith_regression PROPERTIES TIMEOUT 120)

    add_executable(neoscript_host_api_regression Tests/host_api_regression.cpp)
    target_link_libraries(neoscript_host_api_regression PRIVATE neoscript Threads::Threads)
    if(MSVC)
        target_compile_options(neoscript_host_api_regression PRIVATE /utf-8)
    endif()
    add_test(NAME neoscript_host_api_regression COMMAND neoscript_host_api_regression)
    set_tests_properties(neoscript_host_api_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_host_runtime_regression Tests/host_runtime_regression.cpp)
    target_link_libraries(neoscript_host_runtime_regression PRIVATE neoscript Threads::Threads)
    if(MSVC)
        target_compile_options(neoscript_host_runtime_regression PRIVATE /utf-8)
    endif()
    add_test(NAME neoscript_host_runtime_regression COMMAND neoscript_host_runtime_regression)
    set_tests_properties(neoscript_host_runtime_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_host_argument_buffer_regression Tests/host_argument_buffer_regression.cpp)
    target_link_libraries(neoscript_host_argument_buffer_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_host_argument_buffer_regression COMMAND neoscript_host_argument_buffer_regression)
    set_tests_properties(neoscript_host_argument_buffer_regression PROPERTIES TIMEOUT 30)

    add_executable(neoscript_host_c_abi_regression Tests/host_c_abi_regression.cpp ${NEOSCRIPT_C_ABI_SOURCES})
    target_include_directories(neoscript_host_c_abi_regression PRIVATE ${NEOSCRIPT_C_ABI_DIR})
    target_link_libraries(neoscript_host_c_abi_regression PRIVATE neoscript Threads::Threads)
    if(MSVC)
        target_compile_options(neoscript_host_c_abi_regression PRIVATE /utf-8)
    endif()
    add_test(NAME neoscript_host_c_abi_regression COMMAND neoscript_host_c_abi_regression)
    set_tests_properties(neoscript_host_c_abi_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_signed_zero_regression Tests/signed_zero_regression.cpp)
    target_link_libraries(neoscript_signed_zero_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_signed_zero_regression COMMAND neoscript_signed_zero_regression)
    set_tests_properties(neoscript_signed_zero_regression PROPERTIES TIMEOUT 30)

    add_executable(neoscript_local_initialization_regression Tests/local_initialization_regression.cpp)
    target_link_libraries(neoscript_local_initialization_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_local_initialization_regression COMMAND neoscript_local_initialization_regression)
    set_tests_properties(neoscript_local_initialization_regression PROPERTIES TIMEOUT 30)

    add_executable(neoscript_callback_reentry_regression Tests/callback_reentry_regression.cpp)
    target_link_libraries(neoscript_callback_reentry_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_callback_reentry_regression COMMAND neoscript_callback_reentry_regression)
    set_tests_properties(neoscript_callback_reentry_regression PROPERTIES TIMEOUT 30)

    add_executable(neoscript_property_stack_regression Tests/property_stack_regression.cpp)
    target_link_libraries(neoscript_property_stack_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_property_stack_regression COMMAND neoscript_property_stack_regression)
    set_tests_properties(neoscript_property_stack_regression PROPERTIES TIMEOUT 60)

    add_executable(neoscript_call_error_regression Tests/call_error_regression.cpp)
    target_link_libraries(neoscript_call_error_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_call_error_regression COMMAND neoscript_call_error_regression)
    set_tests_properties(neoscript_call_error_regression PROPERTIES TIMEOUT 30)
    add_executable(neoscript_foreach_source_regression Tests/foreach_source_regression.cpp)
    target_link_libraries(neoscript_foreach_source_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_foreach_source_regression COMMAND neoscript_foreach_source_regression)
    set_tests_properties(neoscript_foreach_source_regression PROPERTIES TIMEOUT 30)
    add_executable(neoscript_reference_equality_regression Tests/reference_equality_regression.cpp)
    target_link_libraries(neoscript_reference_equality_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_reference_equality_regression COMMAND neoscript_reference_equality_regression)
    set_tests_properties(neoscript_reference_equality_regression PROPERTIES TIMEOUT 30)
    add_executable(neoscript_collection_reader_regression Tests/collection_reader_regression.cpp)
    target_link_libraries(neoscript_collection_reader_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_collection_reader_regression COMMAND neoscript_collection_reader_regression)
    add_test(NAME neoscript_ci_smoke COMMAND neoscript_ci_smoke)
    add_test(NAME neoscript_c_abi_smoke COMMAND neoscript_c_abi_smoke)

    add_executable(neoscript_int64_regression Tests/int64_regression.cpp)
    target_link_libraries(neoscript_int64_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_int64_regression COMMAND neoscript_int64_regression)

    add_executable(neoscript_control_flow_regression Tests/control_flow_regression.cpp)
    target_link_libraries(neoscript_control_flow_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_control_flow_regression COMMAND neoscript_control_flow_regression)

    add_executable(neoscript_postfix_regression Tests/postfix_regression.cpp)
    target_link_libraries(neoscript_postfix_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_postfix_regression COMMAND neoscript_postfix_regression)

    add_executable(neoscript_compiler_limits_regression Tests/compiler_limits_regression.cpp)
    target_link_libraries(neoscript_compiler_limits_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_compiler_limits_regression COMMAND neoscript_compiler_limits_regression)
    set_tests_properties(neoscript_compiler_limits_regression PROPERTIES TIMEOUT 180)

    add_executable(neoscript_language_regression Tests/language_regression.cpp)
    target_link_libraries(neoscript_language_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_language_regression COMMAND neoscript_language_regression)
    set_tests_properties(neoscript_language_regression PROPERTIES TIMEOUT 60)
    add_executable(neoscript_vector_component_regression Tests/vector_component_regression.cpp)
    target_link_libraries(neoscript_vector_component_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_vector_component_regression COMMAND neoscript_vector_component_regression)
    set_tests_properties(neoscript_vector_component_regression PROPERTIES TIMEOUT 60)
    add_executable(neoscript_vector_array_regression Tests/vector_array_regression.cpp)
    target_link_libraries(neoscript_vector_array_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_vector_array_regression COMMAND neoscript_vector_array_regression)
    set_tests_properties(neoscript_vector_array_regression PROPERTIES TIMEOUT 60)
    add_executable(neoscript_vector_ref_regression Tests/vector_ref_regression.cpp)
    target_link_libraries(neoscript_vector_ref_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_vector_ref_regression COMMAND neoscript_vector_ref_regression)
    set_tests_properties(neoscript_vector_ref_regression PROPERTIES TIMEOUT 60)
    add_executable(neoscript_selector_regression Tests/selector_regression.cpp)
    target_link_libraries(neoscript_selector_regression PRIVATE neoscript Threads::Threads)
    add_test(NAME neoscript_selector_regression COMMAND neoscript_selector_regression)
    set_tests_properties(neoscript_selector_regression PROPERTIES TIMEOUT 120)
endif()

# A desktop host example; disabled by default for Android/iOS cross builds.
if(NEOSCRIPT_BUILD_EXAMPLES)
    add_subdirectory(Samples/minigame)
endif()

# Keep licenses beside standalone host artifacts, including multi-config builds.
foreach(json_host neoscript NeoScriptShared NeoScriptStatic)
    if(TARGET ${json_host})
        add_custom_command(TARGET ${json_host} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt"
                "$<TARGET_FILE_DIR:${json_host}>/NEOSCRIPT_THIRD_PARTY_NOTICES.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
                "$<TARGET_FILE_DIR:${json_host}>/NEOSCRIPT_LICENSE.txt")
    endif()
endforeach()
