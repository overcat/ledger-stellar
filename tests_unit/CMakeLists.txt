cmake_minimum_required(VERSION 3.10)

if (${CMAKE_VERSION} VERSION_LESS 3.10)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif ()

# project information
project(tests_unit
        VERSION 0.1
        DESCRIPTION "Unit tests for Ledger Nano application"
        LANGUAGES C)


# guard against bad build-type strings
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

include(CTest)
ENABLE_TESTING()

# specify C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -pedantic -g -O0 --coverage")

set(GCC_COVERAGE_LINK_FLAGS "--coverage -lgcov")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")

# guard against in-source builds
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif ()

add_compile_definitions(TEST)
add_definitions("-DIO_SEPROXYHAL_BUFFER_SIZE_B=128") # cmake -DIO_SEPROXYHAL_BUFFER_SIZE_B=128

include_directories(../src)
include_directories(mock_includes)

add_executable(test_base58 test_base58.c)
add_executable(test_bip32 test_bip32.c)
add_executable(test_buffer test_buffer.c)
add_executable(test_format test_format.c)
add_executable(test_write test_write.c)
add_executable(test_utils test_utils.c)
add_executable(test_tx_parser test_tx_parser.c)
add_executable(test_tx_formatter test_tx_formatter.c)
add_executable(test_swap test_swap.c)

if (FUZZ)
    add_library(bsd STATIC IMPORTED)
    set_property(TARGET bsd PROPERTY IMPORTED_LOCATION /lib/x86_64-linux-gnu/libbsd.a)
endif()

add_library(base58 STATIC ../src/common/base58.c)
add_library(bip32 STATIC ../src/common/bip32.c)
add_library(buffer STATIC ../src/common/buffer.c)
add_library(read STATIC ../src/common/read.c)
add_library(write STATIC ../src/common/write.c)
add_library(format STATIC ../src/common/format.c)
add_library(varint STATIC ../src/common/varint.c)
add_library(utils STATIC ../src/utils.c)
add_library(tx_parser STATIC ../src/transaction/transaction_parser.c)
add_library(tx_formatter STATIC ../src/transaction/transaction_formatter.c)
add_library(swap STATIC ../src/swap/swap_lib_calls.c)


target_link_libraries(test_base58 PUBLIC cmocka gcov base58)
target_link_libraries(test_bip32 PUBLIC cmocka gcov bip32 read)
target_link_libraries(test_buffer PUBLIC cmocka gcov buffer bip32 varint write read)
target_link_libraries(test_format PUBLIC cmocka gcov format)
target_link_libraries(test_write PUBLIC cmocka gcov write)
target_link_libraries(test_utils PUBLIC cmocka gcov utils base58 format bsd)
target_link_libraries(test_tx_parser PUBLIC cmocka gcov tx_parser buffer bip32 varint write read)
target_link_libraries(test_tx_formatter PUBLIC cmocka gcov tx_parser buffer bip32 varint write read tx_formatter utils bsd format)
target_link_libraries(test_swap PUBLIC cmocka gcov swap tx_parser buffer bip32 varint write read tx_formatter utils bsd format)


add_test(test_base58 test_base58)
add_test(test_bip32 test_bip32)
add_test(test_buffer test_buffer)
add_test(test_format test_format)
add_test(test_write test_write)
add_test(test_utils test_utils)
add_test(test_tx_parser test_tx_parser)
add_test(test_tx_formatter test_tx_formatter)
add_test(test_swap test_swap)


if (FUZZ)
    if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Fuzzer needs to be built with Clang")
    endif()

    # compatible with ClusterFuzzLite
    if (NOT DEFINED ENV{LIB_FUZZING_ENGINE})
    set(COMPILATION_FLAGS_ "-fsanitize=address,fuzzer -g")
    else()
        set(COMPILATION_FLAGS_ "$ENV{LIB_FUZZING_ENGINE} $ENV{CXXFLAGS}")
    endif()

    string(REPLACE " " ";" COMPILATION_FLAGS ${COMPILATION_FLAGS_})
    message(${COMPILATION_FLAGS})

    add_executable(fuzz_tx fuzz_tx.c)
    target_compile_options(fuzz_tx PRIVATE ${COMPILATION_FLAGS})
    target_link_options(fuzz_tx PRIVATE ${COMPILATION_FLAGS})
    target_link_libraries(fuzz_tx PRIVATE tx_parser buffer bip32 varint write read tx_formatter utils bsd format bsd)
endif()
