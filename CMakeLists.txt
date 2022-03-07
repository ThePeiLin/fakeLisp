cmake_minimum_required(VERSION 3.18)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_C_STANDARD 11)
set(INTERPRETER_TARGET_NAME fakeLisp)
set(COMPILER_TARGET_NAME fakeLispc)
set(BTK_TARGET_NAME btk)
project(${INTERPRETER_TARGET_NAME})
include_directories("${PROJECT_SOURCE_DIR}/include")
set(SOURCE "src/*.c" "src/*.h")
file(GLOB INTERPRETER_SOURCE "src/*.c")
file(GLOB COMPILER_SOURCE "src/*.c")
file(GLOB BTK_SOURCE "src/btk.c" "src/deftype.c" "src/syscall.c" "src/basicADT.c" "src/utils.c" "src/vmrun.c" "src/vmtype.c" "src/vmutils.c" "src/vmvalue.c")
list(REMOVE_ITEM INTERPRETER_SOURCE
	"${PROJECT_SOURCE_DIR}/src/fakeLispc.c"
	"${PROJECT_SOURCE_DIR}/src/btk.c")
list(REMOVE_ITEM COMPILER_SOURCE
	"${PROJECT_SOURCE_DIR}/src/fakeLisp.c"
	"${PROJECT_SOURCE_DIR}/src/btk.c")
add_library(${BTK_TARGET_NAME}
    SHARED
	${BTK_SOURCE})
set_target_properties(${BTK_TARGET_NAME} PROPERTIES PREFIX "")
set(LINK_TARGET m pthread dl ffi)
add_executable(${INTERPRETER_TARGET_NAME} ${INTERPRETER_SOURCE})
add_executable(${COMPILER_TARGET_NAME} ${COMPILER_SOURCE})
target_link_libraries(${INTERPRETER_TARGET_NAME} ${LINK_TARGET})
target_link_libraries(${COMPILER_TARGET_NAME} ${LINK_TARGET})
target_link_libraries(${BTK_TARGET_NAME} ${LINK_TARGET})
add_custom_target(COPY_STD_FKL ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lib ${CMAKE_BINARY_DIR}/lib)
add_custom_target(COPY_EXAMPLES ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/ ${CMAKE_BINARY_DIR}/examples/)
if(${DEBUG})
	target_compile_options(${BTK_TARGET_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/DEBUG /W4>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-g -Wall>)
    target_compile_options(${COMPILER_TARGET_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/DEBUG /W4>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-g -Wall>)
    target_compile_options(${INTERPRETER_TARGET_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/DEBUG /W4>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-g -Wall>)
else()
	target_compile_options(${BTK_TARGET_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>)
    target_compile_options(${COMPILER_TARGET_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>)
    target_compile_options(${INTERPRETER_TARGET_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/W4>
		$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>)
endif()
