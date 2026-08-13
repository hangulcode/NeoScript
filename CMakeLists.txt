cmake_minimum_required(VERSION 3.21)

project(NeoScript LANGUAGES CXX)

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

set(NEOSCRIPT_SOURCES
    NeoSource/NeoExport.cpp
    NeoSource/NeoLib.cpp
    NeoSource/NeoParser.cpp
    NeoSource/NeoScriptShim.cpp
    NeoSource/NeoTextLoader.cpp
    NeoSource/NeoVM.cpp
    NeoSource/NeoVMImpl.cpp
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

if(NEOSCRIPT_BUILD_SMOKE)
    find_package(Threads REQUIRED)

    add_executable(neoscript_ci_smoke
        Tests/ci_smoke.cpp
    )
    target_link_libraries(neoscript_ci_smoke PRIVATE neoscript Threads::Threads)

    enable_testing()
    add_test(NAME neoscript_ci_smoke COMMAND neoscript_ci_smoke)
endif()
