cmake_minimum_required(VERSION 3.24)

#cmake options
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING
		"CMAKE_BUILD_TYPE is not set, use release as default CMAKE_BUILD_TYPE" FORCE)
endif()
option(GPROF "enable gprof" OFF)
option(ASAN "enable asan" OFF)
option(TSAN "enable tsan" OFF)
option(FANALYZER "enable fanalyzer" OFF)

if(GPROF)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")
endif()

if(ASAN)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif()

if(TSAN)
	list(APPEND LINK_TARGET tsan)
	add_compile_options(-fsanitize=thread)
endif()

#target names
set(INTERPRETER_TARGET_NAME fakeLisp)
set(COMPILER_TARGET_NAME fakeLispc)
set(BYTECODE_PRINTER_TARGET_NAME fakeLispp)

set(LIBFKL_STATIC_TARGET_NAME fkl_a)
set(LIBFKL_TARGET_NAME fkl)

set(GRAMMER_GEN grammer)
set(PARSE parse)
set(VMPARSE vmparse)

#project setting
project(${INTERPRETER_TARGET_NAME})

if((CMAKE_C_COMPILER_ID STREQUAL "GNU")
    OR (CMAKE_C_COMPILER_ID STREQUAL "Clang"))
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
	set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
	list(APPEND LINK_TARGET m)
endif()

include(CMakePrintHelpers)
include_directories("${PROJECT_SOURCE_DIR}/include")

#copy lib/*.fkl
add_custom_target(COPY_LIB ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib/ ${PROJECT_BINARY_DIR}/lib/
	BYPRODUCTS ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

#copy tests/*.fkl
add_custom_target(COPY_TEST ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/tests/ ${PROJECT_BINARY_DIR}/tests/
	BYPRODUCTS ${PROJECT_BINARY_DIR}/tests/
	VERBATIM)

#warnnig setting
if(MSVC)
	add_compile_options(/W4 /experimental:c11atomics /utf-8)
else()
	add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wpointer-arith)
endif()

add_library(test_lib
	OBJECT
	"src/base.c"
	"src/utils.c"
	"src/grammer.c"
	"src/symbol.c"
	"src/nast.c"
	"src/regex.c"
	"src/bigint.c")

add_executable(test-lexer "main/tests/lexer.c")
target_link_libraries(test-lexer test_lib ${LINK_TARGET})

add_executable(test-lexer2 "main/tests/lexer2.c")
target_link_libraries(test-lexer2 test_lib ${LINK_TARGET})

add_executable(test-regex "main/tests/regex.c")
target_link_libraries(test-regex test_lib ${LINK_TARGET})

add_executable(test-grammer "main/tests/grammer.c")
target_link_libraries(test-grammer test_lib ${LINK_TARGET})

add_executable(test-parser "main/tests/parser.c" "src/parser.c" ${PARSE})
target_link_libraries(test-parser test_lib ${LINK_TARGET})

add_executable(test-bigint "main/tests/bi.c")
target_link_libraries(test-bigint test_lib ${LINK_TARGET})

add_executable(test-ins "main/tests/ins.c")
target_link_libraries(test-ins ${LIBFKL_TARGET_NAME} ${LINK_TARGET})

#grammer
file(GLOB GRAMMER_GEN_SOURCE
	"main/grammer.c")

add_executable(${GRAMMER_GEN} ${GRAMMER_GEN_SOURCE})
target_link_libraries(${GRAMMER_GEN} test_lib ${LINK_TARGET})

add_executable(test-dns "main/tests/dns.c")
add_executable(test-fs-event "main/tests/fs_event.c")

if(MSVC)
	target_link_libraries(test-dns $<LINK_LIBRARY:WHOLE_ARCHIVE,uv_a>)
	target_link_libraries(test-fs-event $<LINK_LIBRARY:WHOLE_ARCHIVE,uv_a>)
else()
	target_link_libraries(test-dns uv_a ${LINK_TARGET})
	target_link_libraries(test-fs-event uv_a ${LINK_TARGET})
endif()

#argtable3
include_directories("${PROJECT_SOURCE_DIR}/3rd/argtable3")
add_subdirectory("${PROJECT_SOURCE_DIR}/3rd/argtable3")

#replxx
include_directories("${PROJECT_SOURCE_DIR}/3rd/replxx/include")
add_subdirectory("${PROJECT_SOURCE_DIR}/3rd/replxx")
set_target_properties(replxx PROPERTIES POSITION_INDEPENDENT_CODE ON)

#libuv
option(LIBUV_BUILD_SHARED OFF)
add_compile_definitions(USING_UV_SHARED)
include_directories("${PROJECT_SOURCE_DIR}/3rd/libuv/include")
add_subdirectory("${PROJECT_SOURCE_DIR}/3rd/libuv")
set_target_properties(uv_a PROPERTIES POSITION_INDEPENDENT_CODE ON)

#set source file for taget
file(GLOB INTERPRETER_SOURCE "main/fakeLisp.c")

file(GLOB COMPILER_SOURCE "main/fakeLispc.c")

file(GLOB BYTECODE_PRINTER_SOURCE "main/fakeLispp.c")

#parser.o
add_library(${PARSE}
	OBJECT
	"parse.c")
#vmparser.o
add_library(${VMPARSE}
	OBJECT
	"vmparse.c")

if(ASAN)
	set_target_properties(${PARSE} PROPERTIES
		POSITION_INDEPENDENT_CODE ON)
	set_target_properties(${VMPARSE} PROPERTIES
		POSITION_INDEPENDENT_CODE ON)
endif()

#libfkl_a.a
file(GLOB LIBFKL_SOURCE "src/*.c")

add_library(${LIBFKL_STATIC_TARGET_NAME}
	STATIC
	${LIBFKL_SOURCE})
set_target_properties(${LIBFKL_STATIC_TARGET_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE ON)

#libfkl.so
add_library(${LIBFKL_TARGET_NAME}
	SHARED
	${LIBFKL_SOURCE})

if(MSVC)
	set_target_properties(${LIBFKL_TARGET_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

#fakeLisp, fakeLispc, fakeLispp
add_executable(${INTERPRETER_TARGET_NAME} ${INTERPRETER_SOURCE})
add_executable(${COMPILER_TARGET_NAME} ${COMPILER_SOURCE})
add_executable(${BYTECODE_PRINTER_TARGET_NAME} ${BYTECODE_PRINTER_SOURCE})

function(add_fkl_native_module name)
    set(options)
    set(oneValueArgs OUTPUT_DIR)
    set(multiValueArgs SRC LIB)
    cmake_parse_arguments(MOD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    message(STATUS "module name: ${name}")
    list(APPEND CMAKE_MESSAGE_INDENT "    ")
    cmake_print_variables(MOD_OUTPUT_DIR)
    message(STATUS "sources:")
    list(APPEND CMAKE_MESSAGE_INDENT "    ")
    foreach(src ${MOD_SRC})
        message(STATUS "${src}")
    endforeach()
    list(POP_BACK CMAKE_MESSAGE_INDENT)

    message(STATUS "link libraries:")
    list(APPEND CMAKE_MESSAGE_INDENT "    ")
    foreach(lib ${MOD_LIB})
        message(STATUS "${lib}")
    endforeach()
    list(POP_BACK CMAKE_MESSAGE_INDENT)
    message("")
    list(POP_BACK CMAKE_MESSAGE_INDENT)
    add_library(${name} SHARED ${MOD_SRC})
    set_target_properties(${name} PROPERTIES PREFIX "")
    if(MSVC)
        add_custom_command(TARGET ${name} POST_BUILD
        	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${name}> "${MOD_OUTPUT_DIR}"
        	COMMENT "Copying ${name}"
        	VERBATIM)
        target_link_libraries(${name} ${LIBFKL_TARGET_NAME})
    else()
        set_target_properties(${name} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${MOD_OUTPUT_DIR}")
    endif()
    target_link_libraries(${name} ${MOD_LIB})
endfunction()

#ht.so
add_fkl_native_module(ht
    SRC "src/ht/ht.c"
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#os.so
add_fkl_native_module(os
    SRC "src/os/os.c"
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#math.so
add_fkl_native_module(math
    SRC "src/math/math.c"
    LIB ${LINK_TARGET}
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#sync.so
add_fkl_native_module(sync
    SRC "src/sync/sync.c"
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#fs.so
add_fkl_native_module(fs
    SRC "src/fs/fs.c"
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#bdb.so
file(GLOB BDB_SOURCE "src/bdb/*.c")
add_fkl_native_module(bdb
    SRC ${BDB_SOURCE}
    LIB replxx
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#fuv.so
file(GLOB FUV_SOURCE "src/fuv/*.c")
add_fkl_native_module(fuv
    SRC ${FUV_SOURCE}
    LIB $<$<BOOL:${MSVC}>:ws2_32>
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#buf.so
add_fkl_native_module(buf
    SRC "src/buf/buf.c"
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#dvec.so
add_fkl_native_module(dvec
    SRC "src/dvec/dvec.c"
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

#bvec.so
add_fkl_native_module(bvec
    SRC "src/bvec/bvec.c"
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/lib")

if(FANALYZER)
	set(FANALYZER_OPTIONS "-fanalyzer"
		"-Wno-analyzer-malloc-leak"
		"-Wno-analyzer-tainted-allocation-size"
		"-Wno-analyzer-tainted-size"
		"-Wno-analyzer-allocation-size"
	)
	target_compile_options(${LIBFKL_TARGET_NAME} PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(${LIBFKL_STATIC_TARGET_NAME} PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(${INTERPRETER_TARGET_NAME} PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(${COMPILER_TARGET_NAME} PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(${BYTECODE_PRINTER_TARGET_NAME} PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(${PARSE} PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(${VMPARSE} PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(ht PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(os PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(math PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(sync PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(fs PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(bdb PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(fuv PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(buf PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(dvec PRIVATE ${FANALYZER_OPTIONS})
	target_compile_options(bvec PRIVATE ${FANALYZER_OPTIONS})
endif()

#linking
target_link_libraries(${LIBFKL_STATIC_TARGET_NAME}
	PUBLIC ${PARSE} ${VMPARSE}
	PRIVATE ${LINK_TARGET} $<LINK_LIBRARY:WHOLE_ARCHIVE,uv_a>)

target_link_libraries(${LIBFKL_TARGET_NAME}
	PUBLIC ${PARSE} ${VMPARSE}
	PRIVATE ${LINK_TARGET} $<LINK_LIBRARY:WHOLE_ARCHIVE,uv_a>)

target_link_libraries(${INTERPRETER_TARGET_NAME} ${LIBFKL_TARGET_NAME} replxx argtable3 ${LINK_TARGET})
target_link_libraries(${COMPILER_TARGET_NAME} ${LIBFKL_TARGET_NAME} argtable3 ${LINK_TARGET})
target_link_libraries(${BYTECODE_PRINTER_TARGET_NAME} ${LIBFKL_TARGET_NAME} argtable3 ${LINK_TARGET})

#test-dll
add_fkl_native_module(test-dll
    SRC "main/tests/dll.c"
    OUTPUT_DIR "${PROJECT_BINARY_DIR}/tests")

add_custom_command(OUTPUT "parse.c"
	COMMAND $<TARGET_FILE:${GRAMMER_GEN}>
	"parse.c"
	${PROJECT_SOURCE_DIR}/src/grammer/action.c
	"fklNastTerminalCreate"
	"fklDestroyNastNode"
	"fklNastPushState0ToStack"

	DEPENDS ${GRAMMER_GEN} ${PROJECT_SOURCE_DIR}/src/grammer/action.c
	VERBATIM)

add_custom_command(OUTPUT "vmparse.c"
	COMMAND $<TARGET_FILE:${GRAMMER_GEN}>
	"vmparse.c"
	${PROJECT_SOURCE_DIR}/src/grammer/vmaction.c
	"fklVMvalueTerminalCreate"
	"fklVMvalueTerminalDestroy"
	"fklVMvaluePushState0ToStack"

	DEPENDS ${GRAMMER_GEN} ${PROJECT_SOURCE_DIR}/src/grammer/vmaction.c
	VERBATIM)

# copy dll in windows
if(MSVC)
# «Â¿Ì
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${PROJECT_BINARY_DIR}/lib/")
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES "${PROJECT_BINARY_DIR}/tests/")
endif()

add_test(NAME test-lexer
	COMMAND $<TARGET_FILE:test-lexer>)

add_test(NAME test-parser
	COMMAND $<TARGET_FILE:test-parser>)

add_test(NAME test-regex
	COMMAND $<TARGET_FILE:test-regex>)

add_test(NAME test-ins
	COMMAND $<TARGET_FILE:test-ins>)

add_test(NAME test-script
	COMMAND ${INTERPRETER_TARGET_NAME} tests ${PROJECT_SOURCE_DIR}/tests/test.json)

add_test(NAME test-pre-compile
	COMMAND $<TARGET_FILE:${COMPILER_TARGET_NAME}> -p tests -d tests-pre)

message(STATUS "options")
list(APPEND CMAKE_MESSAGE_INDENT "    ")
cmake_print_variables(GPROF)
cmake_print_variables(ASAN)
cmake_print_variables(TSAN)
cmake_print_variables(FANALYZER)
cmake_print_variables(CMAKE_BUILD_TYPE)
cmake_print_variables(CMAKE_C_COMPILER)
cmake_print_variables(CMAKE_CXX_COMPILER)
list(POP_BACK CMAKE_MESSAGE_INDENT)
message("")

