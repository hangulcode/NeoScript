cmake_minimum_required(VERSION 3.21)

# 콘솔과 테스트 없이 이 폴더만으로도 빌드할 수 있다.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(NeoScriptMiniGame LANGUAGES CXX C)
    if(NOT DEFINED CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 11)
    endif()
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
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
target_compile_features(neoscript_minigame PRIVATE cxx_std_11)
target_link_libraries(neoscript_minigame PRIVATE neoscript Threads::Threads)
if(WIN32)
    target_link_libraries(neoscript_minigame PRIVATE shell32)
endif()
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

# 같은 프로그램을 공유하는 두 인스턴스의 바인딩, 콜백과 독립 상태를 검증한다.
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
