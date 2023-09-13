cmake_minimum_required(VERSION 3.18)

#cmake options
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
#set(CMAKE_BUILD_TYPE DEBUG)
#set(CMAKE_BUILD_TYPE RELEASE)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")

#warnnig setting
if(MSVC)
	add_compile_options(/Wall)
else()
	add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wpointer-arith)# -fsanitize=thread)
endif()
set(CMAKE_C_STANDARD 11)

if(WIN32)
	set(LINK_TARGET m pthread)
else()
	set(LINK_TARGET m pthread dl)# tsan)
endif()

#target names
set(INTERPRETER_TARGET_NAME fakeLisp)
set(COMPILER_TARGET_NAME fakeLispc)
set(BYTECODE_PRINTER_TARGET_NAME fakeLispp)
set(LIBFKL_TARGET_NAME fkl)
set(GRAMMER_GEN grammer)
set(PARSE parse)
set(VMPARSE vmparse)

#project setting
project(${INTERPRETER_TARGET_NAME})
include_directories("${PROJECT_SOURCE_DIR}/include")

#set source file for taget
file(GLOB INTERPRETER_SOURCE "main/fakeLisp.c")

file(GLOB COMPILER_SOURCE "main/fakeLispc.c")

file(GLOB BYTECODE_PRINTER_SOURCE "main/fakeLispp.c")

file(GLOB GRAMMER_GEN_SOURCE
	"main/grammer/grammer.c"
	"src/grammer.c"
	"src/base.c"
	"src/utils.c"
	"src/symbol.c"
	"src/nast.c")

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

#libfkl.so
set(FKL_SHARED_LIB_NAME ${LIBFKL_TARGET_NAME}-shared)
add_library(${FKL_SHARED_LIB_NAME}
	SHARED
	$<TARGET_OBJECTS:${LIBFKL_TARGET_NAME}>)
set_target_properties(${FKL_SHARED_LIB_NAME} PROPERTIES OUTPUT_NAME ${LIBFKL_TARGET_NAME})
target_link_libraries(${FKL_SHARED_LIB_NAME} ${LINK_TARGET})

set_property(TARGET ${LIBFKL_TARGET_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)


#fakeLisp, fakeLispc, fakeLispp, grammer
add_executable(${INTERPRETER_TARGET_NAME} ${INTERPRETER_SOURCE})
add_executable(${COMPILER_TARGET_NAME} ${COMPILER_SOURCE})
add_executable(${BYTECODE_PRINTER_TARGET_NAME} ${BYTECODE_PRINTER_SOURCE})
add_executable(${GRAMMER_GEN} ${GRAMMER_GEN_SOURCE})


#linking
target_link_libraries(${LIBFKL_TARGET_NAME} ${LINK_TARGET} ${PARSE} ${VMPARSE})
target_link_libraries(${INTERPRETER_TARGET_NAME} ${LIBFKL_TARGET_NAME})
target_link_libraries(${COMPILER_TARGET_NAME} ${LIBFKL_TARGET_NAME})
target_link_libraries(${BYTECODE_PRINTER_TARGET_NAME} ${LIBFKL_TARGET_NAME})
target_link_libraries(${GRAMMER_GEN} ${LINK_TARGET})

#testing
enable_testing()

add_executable(test-lexer "main/test/lexer.c")
target_link_libraries(test-lexer ${LIBFKL_TARGET_NAME})

add_executable(test-lexer2 "main/test/lexer2.c")
target_link_libraries(test-lexer2 ${LIBFKL_TARGET_NAME})

add_executable(test-parser "main/test/parser.c")
target_link_libraries(test-parser ${LIBFKL_TARGET_NAME})

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

add_test(NAME test-script
	COMMAND ${INTERPRETER_TARGET_NAME} test ${PROJECT_SOURCE_DIR}/test/test.json)

#copy lib/*.fkl
add_custom_target(COPY_LIB ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib ${CMAKE_BINARY_DIR}/lib)

#copy test
add_custom_target(COPY_TEST ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/test/ ${CMAKE_BINARY_DIR}/test/)

