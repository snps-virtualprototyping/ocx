cmake_minimum_required(VERSION 3.6)
project(ocx)

option(OCX_BUILD_TESTS "Build unit tests" ON)

set(CMAKE_CXX_STANDARD 11)

set(src "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(inc "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(runner_sources "${src}/ocx-runner.cpp"
                   "${src}/memory.cpp"
)
set(test_sources "${src}/test-runner.cpp")
set(lib_sources "${src}/dummy-core.cpp")

# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(googletest EXCLUDE_FROM_ALL)

add_executable(ocx-runner ${runner_sources})
add_executable(ocx-test-runner ${test_sources})
add_library(ocx-dummy-core MODULE ${lib_sources})
set_target_properties(ocx-dummy-core PROPERTIES OUTPUT_NAME "ocx-dummy")


target_link_libraries(ocx-runner gtest ${CMAKE_DL_LIBS})
target_link_libraries(ocx-test-runner
                      gtest gmock gtest_main ${CMAKE_DL_LIBS})
if (MSVC)
    target_sources(ocx-runner PRIVATE "${src}/getopt.cpp")
endif()

if (NOT MSVC)
	find_library(LIB_RT rt)
	target_link_libraries(ocx-test-runner ${LIB_RT})
endif()


target_include_directories(ocx-runner PUBLIC ${inc} ${src})
target_include_directories(ocx-test-runner PUBLIC ${inc} ${src})
target_include_directories(ocx-dummy-core PUBLIC ${inc})

if (MSVC)
    # warning level 3 and all warnings as errors
    target_compile_options(ocx-runner PRIVATE /W3 /WX)
    target_compile_options(ocx-test-runner PRIVATE /W3 /WX)
    target_compile_options(ocx-dummy-core PRIVATE /W3 /WX)
else()
    # lots of warnings and all warnings as errors
    target_compile_options(ocx-runner PRIVATE -Werror -Wall -Wextra)
    target_compile_options(ocx-test-runner PRIVATE -Werror -Wall -Wextra)
    target_compile_options(ocx-dummy-core PRIVATE -Werror -Wall -Wextra)
endif()

install(TARGETS ocx-dummy-core DESTINATION lib)
install(TARGETS ocx-runner DESTINATION bin)
install(TARGETS ocx-test-runner DESTINATION bin)
install(DIRECTORY ${inc}/ DESTINATION include)

if(OCX_BUILD_TESTS)
    enable_testing()
    add_test(NAME smoke COMMAND $<TARGET_FILE:ocx-test-runner>
                                --gtest_filter=ocx_basic.load_library
                                $<TARGET_FILE:ocx-dummy-core> test)
endif()
