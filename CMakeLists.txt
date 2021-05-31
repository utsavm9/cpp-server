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
find_package(Boost 1.50 REQUIRED COMPONENTS system filesystem log_setup log regex iostreams)
find_package(ZLIB REQUIRED)
message(STATUS "Boost version: ${Boost_VERSION}")

# Enable OpenSSL
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

# Include our header files
include_directories(include)
add_library(parser src/parser.cc)
add_library(config src/config.cc)
add_library(sessions src/session.cc src/sessionTCP.cc src/sessionSSL.cc)
add_library(server src/server.cc)
add_library(logger src/logger.cc)
file(GLOB HANDLER_SOURCE_FILES src/handler.cc src/*Handler.cc)
add_library(handler ${HANDLER_SOURCE_FILES})

# Generate the server executable
add_executable(webserver src/main.cc)
target_link_libraries(webserver config parser logger server Boost::system pthread Boost::filesystem Boost::regex Boost::log_setup Boost::log)
target_link_libraries(server sessions handler OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(sessions parser OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(handler Boost::iostreams z)

# Generate the test executable
file(GLOB TEST_SOURCE_FILES tests/*.cc tests/handler_tests/*.cc)
add_executable(unit_tests ${TEST_SOURCE_FILES})
target_link_libraries(unit_tests
	parser config server logger
	gtest_main
	Boost::system Boost::filesystem Boost::regex Boost::log_setup Boost::log
	OpenSSL::SSL OpenSSL::Crypto)

# Unit tests
gtest_discover_tests(unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# Coverage Report
include(cmake/CodeCoverageReportConfig.cmake)
generate_coverage_report(TARGETS webserver parser config sessions server handler logger TESTS unit_tests)

# Integration tests
file(GLOB_RECURSE TestDriver */integration/testDriver.sh)
file(GLOB_RECURSE Certificate */certs/fullchain.pem)
add_test(NAME IntegrationTests
         COMMAND ${TestDriver} $<TARGET_FILE:webserver> 8080 8081 8082 8083 ${Certificate})
# Set Compiler Warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")