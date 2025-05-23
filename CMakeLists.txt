cmake_minimum_required(VERSION 3.24)

#cmake options
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if(NOT WIN32)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
	set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
	set(LINK_TARGET m)
else()
	set(LINK_TARGET "")
endif()

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

#ht.so
add_library(ht
	SHARED
	"src/ht/ht.c")
set_target_properties(ht PROPERTIES
	PREFIX "")
target_link_libraries(ht ${LIBFKL_TARGET_NAME})

set_target_properties(ht PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#os.so
add_library(os
	SHARED
	"src/os/os.c")
set_target_properties(os PROPERTIES
	PREFIX "")
target_link_libraries(os ${LIBFKL_TARGET_NAME})

set_target_properties(os PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#math.so
add_library(math
	SHARED
	"src/math/math.c")
set_target_properties(math PROPERTIES
	PREFIX "")
target_link_libraries(math ${LIBFKL_TARGET_NAME} ${LINK_TARGET})

set_target_properties(math PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#sync.so
add_library(sync
	SHARED
	"src/sync/sync.c")
set_target_properties(sync PROPERTIES
	PREFIX "")
target_link_libraries(sync ${LIBFKL_TARGET_NAME})

set_target_properties(sync PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#fs.so
add_library(fs
	SHARED
	"src/fs/fs.c")
set_target_properties(fs PROPERTIES
	PREFIX "")
target_link_libraries(fs ${LIBFKL_TARGET_NAME})

set_target_properties(fs PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#bdb.so
file(GLOB BDB_SOURCE "src/bdb/*.c")

add_library(bdb
	SHARED
	${BDB_SOURCE})
set_target_properties(bdb PROPERTIES
	PREFIX "")
target_link_libraries(bdb ${LIBFKL_TARGET_NAME} replxx)

set_target_properties(bdb PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#fuv.so
file(GLOB FUV_SOURCE "src/fuv/*.c")

add_library(fuv
	SHARED
	${FUV_SOURCE})
set_target_properties(fuv PROPERTIES
	PREFIX "")
target_link_libraries(fuv ${LIBFKL_TARGET_NAME})

#buf.so
add_library(buf
	SHARED
	"src/buf/buf.c")
set_target_properties(buf PROPERTIES
	PREFIX "")
target_link_libraries(buf ${LIBFKL_TARGET_NAME})

set_target_properties(buf PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#dvec.so
add_library(dvec
	SHARED
	"src/dvec/dvec.c")
set_target_properties(dvec PROPERTIES
	PREFIX "")
target_link_libraries(dvec ${LIBFKL_TARGET_NAME})

set_target_properties(dvec PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

#bvec.so
add_library(bvec
	SHARED
	"src/bvec/bvec.c")
set_target_properties(bvec PROPERTIES
	PREFIX "")
target_link_libraries(bvec ${LIBFKL_TARGET_NAME})

set_target_properties(bvec PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

if(MSVC)
target_link_libraries(fuv ws2_32)
endif()

set_target_properties(fuv PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

if(FANALYZER)
	set(FANALYZER_OPTIONS "-fanalyzer"
		"-Wno-analyzer-malloc-leak"
		"-Wno-analyzer-tainted-allocation-size"
		"-Wno-analyzer-tainted-size"
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
add_library(test-dll
	SHARED
	"main/tests/dll.c")
set_target_properties(test-dll PROPERTIES
	PREFIX "")
target_link_libraries(test-dll ${LIBFKL_TARGET_NAME})

set_target_properties(test-dll PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests")

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
add_custom_command(TARGET test-dll POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:test-dll> ${PROJECT_BINARY_DIR}/tests/
	COMMENT "Copying test-dll"
	VERBATIM)

add_custom_command(TARGET ht POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ht> ${PROJECT_BINARY_DIR}/lib/
	BYPRODUCTS ${PROJECT_BINARY_DIR}/lib/ht.dll
	VERBATIM)

add_custom_command(TARGET os POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:os> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

add_custom_command(TARGET math POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:math> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

add_custom_command(TARGET sync POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sync> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

add_custom_command(TARGET fs POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:fs> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

add_custom_command(TARGET bdb POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bdb> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

add_custom_command(TARGET fuv POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:fuv> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

add_custom_command(TARGET buf POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:buf> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

add_custom_command(TARGET dvec POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:dvec> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

add_custom_command(TARGET bvec POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:bvec> ${PROJECT_BINARY_DIR}/lib/
	VERBATIM)

# ����
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
include(CMakePrintHelpers)
list(APPEND CMAKE_MESSAGE_INDENT "    ")
cmake_print_variables(GPROF)
cmake_print_variables(ASAN)
cmake_print_variables(TSAN)
cmake_print_variables(FANALYZER)
cmake_print_variables(CMAKE_BUILD_TYPE)
list(POP_BACK CMAKE_MESSAGE_INDENT)
message("")

