# Host-side unit tests for the platform independent audio code.
#
#   cmake -S tests/native -B build/native && cmake --build build/native
#   ctest --test-dir build/native --output-on-failure

cmake_minimum_required(VERSION 3.14)

project(voicesmith-tests CXX)

set(SRCDIR "${CMAKE_CURRENT_LIST_DIR}/../../voicesmith/src/main/cpp/de/jurihock/voicesmith")

include(FetchContent)

FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt
  GIT_TAG 12.0.0)

FetchContent_MakeAvailable(fmt)

add_executable(tests
  tests.cpp
  "${SRCDIR}/fx/DelayEffect.cpp")

target_compile_features(tests PRIVATE cxx_std_20)
target_compile_options(tests PRIVATE -Wall -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer)
target_link_options(tests PRIVATE -fsanitize=address,undefined)
target_include_directories(tests PRIVATE "${SRCDIR}/.." "${CMAKE_CURRENT_LIST_DIR}/stubs")
target_link_libraries(tests PRIVATE fmt)

enable_testing()
add_test(NAME tests COMMAND tests)
