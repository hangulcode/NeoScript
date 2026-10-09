cmake_minimum_required(VERSION 3.21)

project(NeoScript LANGUAGES CXX C)

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
option(NEOSCRIPT_BUILD_SHARED "Build the flat-C shared library (Unity / P/Invoke hosts)" OFF)
option(NEOSCRIPT_BUILD_STATIC_C_ABI "Build the flat-C static library (iOS, where dynamic loading is not allowed)" OFF)

# The flat-C shim lives outside NeoSource on purpose.  The engine repository
# compiles NeoSource/*.cpp directly and embeds the C++ facade, so dropping a
# P/Invoke shim in there would push a shim it never calls into every engine build.
set(NEOSCRIPT_C_ABI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Unity/Native)
set(NEOSCRIPT_C_ABI_SOURCES ${NEOSCRIPT_C_ABI_DIR}/NeoScriptC.cpp)

set(NEOSCRIPT_SOURCES
    NeoSource/NeoExport.cpp
    NeoSource/NeoLib.cpp
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
target_compile_features(neoscript PUBLIC cxx_std_17)

if(WIN32)
    target_compile_definitions(neoscript PRIVATE WIN_PRJ)
    target_link_libraries(neoscript PUBLIC ws2_32)
endif()

# Flat-C shared library.  Unity resolves DllImport("NeoScript") to NeoScript.dll
# on Windows and libNeoScript.so on Android, so the output name is fixed here.
# iOS links the static `neoscript` target instead and uses DllImport("__Internal").
if(NEOSCRIPT_BUILD_SHARED)
    add_library(NeoScriptShared SHARED ${NEOSCRIPT_SOURCES} ${NEOSCRIPT_C_ABI_SOURCES})
    target_include_directories(NeoScriptShared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/NeoSource
        ${NEOSCRIPT_C_ABI_DIR})
    target_compile_features(NeoScriptShared PUBLIC cxx_std_17)
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
        target_link_libraries(NeoScriptShared PUBLIC ws2_32)
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
    target_compile_features(NeoScriptStatic PUBLIC cxx_std_17)
    # NEOSCRIPT_C_BUILD_SHARED is deliberately absent: NS_API must stay empty so
    # the symbols are ordinary static-library symbols the linker can resolve.
    set_target_properties(NeoScriptStatic PROPERTIES OUTPUT_NAME NeoScriptUnity)
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
    add_test(NAME neoscript_ci_smoke COMMAND neoscript_ci_smoke)
    add_test(NAME neoscript_c_abi_smoke COMMAND neoscript_c_abi_smoke)

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
endif()
