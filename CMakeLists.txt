cmake_minimum_required(VERSION 3.18)

#cmake options
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
#set(CMAKE_BUILD_TYPE DEBUG)
#set(CMAKE_BUILD_TYPE RELEASE)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

if(NOT WIN32)
	set(LINK_TARGET m)# tsan)
else()
	set(LINK_TARGET "")
endif()

#target names
set(INTERPRETER_TARGET_NAME fakeLisp)
set(COMPILER_TARGET_NAME fakeLispc)
set(BYTECODE_PRINTER_TARGET_NAME fakeLispp)
set(LIBFKL_TARGET_NAME fkl)

set(LIBFKL_SHARED_TARGET_NAME fkl-shared)

set(LIBFKL_MOD_TARGET_NAME fkl-mod)

set(GRAMMER_GEN grammer)
set(PARSE parse)
set(VMPARSE vmparse)

#project setting
project(${INTERPRETER_TARGET_NAME})
include_directories("${PROJECT_SOURCE_DIR}/include")

#warnnig setting
if(MSVC)
	add_compile_options(/W4 /utf-8)
else()
	add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wpointer-arith)# -fsanitize=thread)
endif()

#testing
enable_testing()

add_library(test_lib
	OBJECT
	"src/base.c"
	"src/utils.c"
	"src/grammer.c"
	"src/symbol.c"
	"src/nast.c"
	"src/regex.c")

add_executable(test-lexer "main/test/lexer.c")
target_link_libraries(test-lexer test_lib ${LINK_TARGET})

add_executable(test-lexer2 "main/test/lexer2.c")
target_link_libraries(test-lexer2 test_lib ${LINK_TARGET})

add_executable(test-regex "main/test/regex.c")
target_link_libraries(test-regex test_lib ${LINK_TARGET})

add_executable(test-grammer "main/test/grammer.c")
target_link_libraries(test-grammer test_lib ${LINK_TARGET})

add_executable(test-parser "main/test/parser.c")
target_link_libraries(test-parser ${LIBFKL_TARGET_NAME})

#grammer
file(GLOB GRAMMER_GEN_SOURCE
	"main/grammer/grammer.c")

add_executable(${GRAMMER_GEN} ${GRAMMER_GEN_SOURCE})
target_link_libraries(${GRAMMER_GEN} test_lib ${LINK_TARGET})

#argtable3
include_directories("${PROJECT_SOURCE_DIR}/3rd/argtable3")
add_subdirectory("${PROJECT_SOURCE_DIR}/3rd/argtable3")

#replxx
include_directories("${PROJECT_SOURCE_DIR}/3rd/replxx/include")
add_subdirectory("${PROJECT_SOURCE_DIR}/3rd/replxx")
set_target_properties(replxx PROPERTIES POSITION_INDEPENDENT_CODE ON)

#libuv
set(LIBUV_BUILD_SHARED OFF)
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

#libfkl.a
file(GLOB LIBFKL_SOURCE "src/*.c")

add_library(${LIBFKL_TARGET_NAME}
	STATIC
	${LIBFKL_SOURCE})
set_target_properties(${LIBFKL_TARGET_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE ON)

#libfkldll.so
add_library(${LIBFKL_SHARED_TARGET_NAME}
	SHARED
	${LIBFKL_SOURCE})
set_target_properties(${LIBFKL_SHARED_TARGET_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE ON)

#libfkl-mod.a
file(GLOB LIBFKL_MOD_SOURCE
	"src/base.c"
	"src/symbol.c"
	"src/utils.c"
	"src/vm*.c")

add_library(${LIBFKL_MOD_TARGET_NAME}
	STATIC
	${LIBFKL_MOD_SOURCE})
set_target_properties(${LIBFKL_MOD_TARGET_NAME} PROPERTIES
	POSITION_INDEPENDENT_CODE ON)

#fakeLisp, fakeLispc, fakeLispp
add_executable(${INTERPRETER_TARGET_NAME} ${INTERPRETER_SOURCE})
add_executable(${COMPILER_TARGET_NAME} ${COMPILER_SOURCE})
add_executable(${BYTECODE_PRINTER_TARGET_NAME} ${BYTECODE_PRINTER_SOURCE})

#ht.so
add_library(ht
	SHARED
	"main/ht.c")
set_target_properties(ht PROPERTIES
	PREFIX "")
target_link_libraries(ht ${LIBFKL_MOD_TARGET_NAME})

if(MSVC)
add_custom_command(TARGET ht
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:ht>
	"${CMAKE_BINARY_DIR}/lib/"
	DEPENDS ht
	VERBATIM)
else()
set_target_properties(ht PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

#os.so
add_library(os
	SHARED
	"main/os.c")
set_target_properties(os PROPERTIES
	PREFIX "")
target_link_libraries(os ${LIBFKL_MOD_TARGET_NAME})

if(MSVC)
add_custom_command(TARGET os
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:os>
	"${CMAKE_BINARY_DIR}/lib/"
	DEPENDS os
	VERBATIM)
else()
set_target_properties(os PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

#math.so
add_library(math
	SHARED
	"main/math.c")
set_target_properties(math PROPERTIES
	PREFIX "")
target_link_libraries(math ${LIBFKL_MOD_TARGET_NAME} ${LINK_TARGET})

if(MSVC)
add_custom_command(TARGET math
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:math>
	"${CMAKE_BINARY_DIR}/lib/"
	DEPENDS math
	VERBATIM)
else()
set_target_properties(math PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

#sync.so
add_library(sync
	SHARED
	"main/sync.c")
set_target_properties(sync PROPERTIES
	PREFIX "")
target_link_libraries(sync ${LIBFKL_MOD_TARGET_NAME} ${LINK_TARGET} uv_a)

if(MSVC)
add_custom_command(TARGET sync
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:sync>
	"${CMAKE_BINARY_DIR}/lib/"
	DEPENDS sync
	VERBATIM)
else()
set_target_properties(sync PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

#fs.so
add_library(fs
	SHARED
	"main/fs.c")
set_target_properties(fs PROPERTIES
	PREFIX "")
target_link_libraries(fs ${LIBFKL_MOD_TARGET_NAME})

if(MSVC)
add_custom_command(TARGET fs
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:fs>
	"${CMAKE_BINARY_DIR}/lib/"
	DEPENDS fs
	VERBATIM)
else()
set_target_properties(fs PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

#bdb.so
add_library(bdb
	SHARED
	"main/bdb.c")
set_target_properties(bdb PROPERTIES
	PREFIX "")
target_link_libraries(bdb ${LIBFKL_TARGET_NAME} uv_a replxx)

if(MSVC)
add_custom_command(TARGET bdb
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:bdb>
	"${CMAKE_BINARY_DIR}/lib/fdb/bin"
	DEPENDS bdb
	VERBATIM)
else()
set_target_properties(bdb PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/fdb/bin")
endif()

#linking
target_link_libraries(${LIBFKL_TARGET_NAME} ${LINK_TARGET} ${PARSE} ${VMPARSE} uv_a)
target_link_libraries(${INTERPRETER_TARGET_NAME} ${LIBFKL_TARGET_NAME} replxx)
target_link_libraries(${COMPILER_TARGET_NAME} ${LIBFKL_TARGET_NAME} argtable3)
target_link_libraries(${BYTECODE_PRINTER_TARGET_NAME} ${LIBFKL_TARGET_NAME})

#test-dll
add_library(test-dll
	SHARED
	"main/test/dll.c")
set_target_properties(test-dll PROPERTIES
	PREFIX "")
target_link_libraries(test-dll ${LIBFKL_TARGET_NAME})

if(MSVC)
add_custom_command(TARGET test-dll
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:test-dll>
	"${CMAKE_BINARY_DIR}/test/"
	DEPENDS test-dll
	VERBATIM)
else()
set_target_properties(test-dll PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
endif()


add_custom_command(OUTPUT "parse.c"
	COMMAND ${GRAMMER_GEN}
	"parse.c"
	${PROJECT_SOURCE_DIR}/main/grammer/action.c
	"<fakeLisp/parser.h>"
	"fklNastTerminalCreate"
	"fklDestroyNastNode"
	"fklNastPushState0ToStack"

	DEPENDS ${GRAMMER_GEN} ${PROJECT_SOURCE_DIR}/main/grammer/action.c
	VERBATIM)

add_custom_command(OUTPUT "vmparse.c"
	COMMAND ${GRAMMER_GEN}
	"vmparse.c"
	${PROJECT_SOURCE_DIR}/main/grammer/vmaction.c
	"<fakeLisp/vm.h>"
	"fklVMvalueTerminalCreate"
	"fklVMvalueTerminalDestroy"
	"fklVMvaluePushState0ToStack"

	DEPENDS ${GRAMMER_GEN} ${PROJECT_SOURCE_DIR}/main/grammer/vmaction.c
	VERBATIM)

add_test(NAME test-lexer
	COMMAND $<TARGET_FILE:test-lexer>)

add_test(NAME test-parser
	COMMAND $<TARGET_FILE:test-parser>)

add_test(NAME test-regex
	COMMAND $<TARGET_FILE:test-regex>)

add_test(NAME test-script
	COMMAND ${INTERPRETER_TARGET_NAME} test ${PROJECT_SOURCE_DIR}/test/test.json)

add_test(NAME test-pre-compile
	COMMAND $<TARGET_FILE:${COMPILER_TARGET_NAME}> -p test -d test-pre)

#copy lib/*.fkl
add_custom_target(COPY_LIB ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib ${CMAKE_BINARY_DIR}/lib
	VERBATIM)

#copy test
add_custom_target(COPY_TEST ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/test/ ${CMAKE_BINARY_DIR}/test/
	VERBATIM)

