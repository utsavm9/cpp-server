cmake_minimum_required(VERSION 3.10.0)
project(koko)

# Make sure developers do not run cmake in the main project directory, to keep
# build artifacts from becoming clutter
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed.
    Please make a new directory (called a build directory) and run CMake from there.
    You may need to remove CMakeCache.txt." )
endif()

# Turn on debug builds if we are building in a devel shell
if (CMAKE_BUILD_TYPE STREQUAL "" AND "$ENV{DEVEL_SHELL}" STREQUAL "1")
    message(STATUS "Setting debug build type by default in devel shell")
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Output binaries to a sub directory "bin"
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Enable cmake testing
include(CTest)
enable_testing()

# Enable GoogleTest
include(GoogleTest)
add_subdirectory(/usr/src/googletest googletest)

# Enable Boost
# Using static libraries so binaries can be deployed without a full boost install
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.50 REQUIRED COMPONENTS system filesystem log_setup log regex)
message(STATUS "Boost version: ${Boost_VERSION}")

# Enable OpenSSL
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

# Include our header files
include_directories(include)
add_library(parser src/parser.cc)
add_library(config src/config.cc)
add_library(session src/session.cc)
add_library(server src/server.cc)
add_library(logger src/logger.cc)
add_library(handler src/handler.cc src/echoHandler.cc src/fileHandler.cc src/notFoundHandler.cc src/proxyRequestHandler.cc src/statusHandler.cc 
    src/healthHandler.cc src/sleepEchoHandler.cc)

# Generate the server executable
add_executable(webserver src/main.cc)
target_link_libraries(webserver config parser logger server Boost::system pthread Boost::filesystem Boost::regex Boost::log_setup Boost::log)
target_link_libraries(server session handler OpenSSL::Crypto)
target_link_libraries(session parser)

# Generate the test executable
add_executable(unit_tests tests/parser_test.cc tests/config_test.cc tests/server_test.cc tests/logger_test.cc tests/session_test.cc
	tests/handler_tests/handler_test.cc tests/handler_tests/echo_test.cc tests/handler_tests/file_test.cc tests/handler_tests/health_test.cc
	tests/handler_tests/notFound_test.cc tests/handler_tests/proxy_test.cc tests/handler_tests/status_test.cc tests/handler_tests/sleepEcho_test.cc)
target_link_libraries(unit_tests 
	parser config server logger 
	gtest_main 
	Boost::system Boost::filesystem Boost::filesystem Boost::regex Boost::log_setup Boost::log
)

# Unit tests
gtest_discover_tests(unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Coverage Report
include(cmake/CodeCoverageReportConfig.cmake)
generate_coverage_report(TARGETS webserver parser config session server handler logger TESTS unit_tests)

# Integration tests
file(GLOB_RECURSE TestDriver */integration/testDriver.sh)
add_test(NAME IntegrationTests 
         COMMAND ${TestDriver} $<TARGET_FILE:webserver> 8080 8081)

# Set Compiler Warnings
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra")