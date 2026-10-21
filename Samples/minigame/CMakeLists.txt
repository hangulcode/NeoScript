cmake_minimum_required(VERSION 3.21)

# This directory also builds on its own, without the console/test harness.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(NeoScriptMiniGame LANGUAGES CXX C)
    if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    set(NEOSCRIPT_BUILD_SMOKE OFF CACHE BOOL "Build API smoke tests" FORCE)
    set(NEOSCRIPT_BUILD_EXAMPLES OFF CACHE BOOL "Build examples from the root" FORCE)
    add_subdirectory(../.. neoscript)
    enable_testing()
endif()

find_package(Threads REQUIRED)
add_executable(neoscript_minigame main.cpp)
target_link_libraries(neoscript_minigame PRIVATE neoscript Threads::Threads)
set_target_properties(neoscript_minigame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(neoscript_minigame PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin")
endforeach()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/scripts")
configure_file(scripts/mover.ns "${CMAKE_BINARY_DIR}/bin/scripts/mover.ns" COPYONLY)
configure_file(viewer.html "${CMAKE_BINARY_DIR}/bin/viewer.html" COPYONLY)

# Verifies actual native bindings, callback invocation, and independent state
# in two instances sharing a single compiled program.
add_test(NAME neoscript_minigame
    COMMAND neoscript_minigame --verify)
set_tests_properties(neoscript_minigame PROPERTIES TIMEOUT 30)

install(TARGETS neoscript_minigame RUNTIME DESTINATION . COMPONENT MiniGame)
install(FILES scripts/mover.ns DESTINATION scripts COMPONENT MiniGame)
install(FILES viewer.html README.md
    "${CMAKE_CURRENT_SOURCE_DIR}/../../LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../THIRD_PARTY_NOTICES.txt"
    DESTINATION . COMPONENT MiniGame)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CPACK_GENERATOR ZIP)
    set(CPACK_PACKAGE_NAME neoscript-minigame)
    set(CPACK_PACKAGE_VERSION 1.0.0)
    set(CPACK_PACKAGE_FILE_NAME "neoscript-minigame-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    include(CPack)
endif()
