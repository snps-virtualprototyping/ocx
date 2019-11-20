cmake_minimum_required(VERSION 3.6)
project(ocx)

option(OCX_BUILD_TESTS "Build unit tests" ON)

set(CMAKE_CXX_STANDARD 11)

set(src "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(inc "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(test_sources "${src}/test-runner.cpp")
set(lib_sources "${src}/dummy-core.cpp")

add_subdirectory(googletest EXCLUDE_FROM_ALL)

add_executable(ocx-test-runner ${test_sources})
add_library(ocx-dummy-core MODULE ${lib_sources})
set_target_properties(ocx-dummy-core PROPERTIES OUTPUT_NAME "ocx-dummy")

find_library(LIB_RT rt)

target_link_libraries(ocx-test-runner
                      gtest gmock gtest_main ${CMAKE_DL_LIBS} ${LIB_RT})

target_include_directories(ocx-test-runner PUBLIC ${inc} ${src})
target_include_directories(ocx-dummy-core PUBLIC ${inc})

target_compile_options(ocx-test-runner PRIVATE -Werror -Wall -Wextra)
target_compile_options(ocx-dummy-core PRIVATE -Werror -Wall -Wextra)

install(TARGETS ocx-dummy-core DESTINATION lib)
install(TARGETS ocx-test-runner DESTINATION bin)
install(DIRECTORY ${inc}/ DESTINATION include)

if(OCX_BUILD_TESTS)
    enable_testing()
    add_test(NAME smoke COMMAND $<TARGET_FILE:ocx-test-runner>
                                --gtest_filter=ocx_basic.load_library
                                $<TARGET_FILE:ocx-dummy-core> test)
endif()
