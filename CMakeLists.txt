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
	add_compile_options(/W4)
else()
	add_compile_options(-Wall)# -fsanitize=thread)
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
set(BTK_TARGET_NAME btk)
set(LIBFKL_TARGET_NAME fkl)
set(FFI_TARGET_NAME fklffi)
set(FKLC_TARGET_NAME fklc)

#project setting
project(${INTERPRETER_TARGET_NAME})
include_directories("${PROJECT_SOURCE_DIR}/include")

#set source file for taget
file(GLOB INTERPRETER_SOURCE "src/fakeLisp.c")

file(GLOB COMPILER_SOURCE "src/fakeLispc.c")

file(GLOB BYTECODE_PRINTER_SOURCE "src/fakeLispp.c")

file(GLOB BTK_SOURCE "src/btk.c")

file(GLOB FFI_SOURCE "ffi/*.c")

file(GLOB FKLC_SOURCE "fklc/*.c")

#btk.so
add_library(${BTK_TARGET_NAME}
	MODULE
	${BTK_SOURCE})
set_target_properties(${BTK_TARGET_NAME} PROPERTIES PREFIX "")

#fklffi.so
add_library(${FFI_TARGET_NAME}
	MODULE
	${FFI_SOURCE})
set_target_properties(${FFI_TARGET_NAME} PROPERTIES PREFIX "")

#fklc.so
add_library(${FKLC_TARGET_NAME}
	MODULE
	${FKLC_SOURCE})
set_target_properties(${FKLC_TARGET_NAME} PROPERTIES PREFIX "")

#libfkl.a
file(GLOB LIBFKL_SOURCE "src/*.c")
list(REMOVE_ITEM LIBFKL_SOURCE
	"${PROJECT_SOURCE_DIR}/src/fakeLisp.c"
	"${PROJECT_SOURCE_DIR}/src/fakeLispc.c"
	"${PROJECT_SOURCE_DIR}/src/fakeLispp.c"
	"${PROJECT_SOURCE_DIR}/src/btk.c")

add_library(${LIBFKL_TARGET_NAME}
	STATIC
	${LIBFKL_SOURCE})

#libfkl.so
set(ORIGINAL_SHARED_LIB_NAME ${LIBFKL_TARGET_NAME}-shared)
add_library(${ORIGINAL_SHARED_LIB_NAME}
	SHARED
	$<TARGET_OBJECTS:${LIBFKL_TARGET_NAME}>)
set_target_properties(${ORIGINAL_SHARED_LIB_NAME} PROPERTIES OUTPUT_NAME ${LIBFKL_TARGET_NAME})
target_link_libraries(${ORIGINAL_SHARED_LIB_NAME} ${LINK_TARGET})

set_property(TARGET ${LIBFKL_TARGET_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)


#fakeLisp and fakeLispc
add_executable(${INTERPRETER_TARGET_NAME} ${INTERPRETER_SOURCE})
add_executable(${COMPILER_TARGET_NAME} ${COMPILER_SOURCE})
add_executable(${BYTECODE_PRINTER_TARGET_NAME} ${BYTECODE_PRINTER_SOURCE})

#linking
target_link_libraries(${LIBFKL_TARGET_NAME} ${LINK_TARGET})
target_link_libraries(${INTERPRETER_TARGET_NAME} ${LIBFKL_TARGET_NAME})
target_link_libraries(${COMPILER_TARGET_NAME} ${LIBFKL_TARGET_NAME})
target_link_libraries(${BYTECODE_PRINTER_TARGET_NAME} ${LIBFKL_TARGET_NAME})

target_link_libraries(${BTK_TARGET_NAME} ${LIBFKL_TARGET_NAME})
target_link_libraries(${FFI_TARGET_NAME} ${LIBFKL_TARGET_NAME} ffi)
target_link_libraries(${FKLC_TARGET_NAME} ${LIBFKL_TARGET_NAME})

#copy lib/*.fkl
add_custom_target(COPY_LIB_FKL ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lib ${CMAKE_BINARY_DIR}/lib)

#copy examples
add_custom_target(COPY_EXAMPLES ALL
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/examples/ ${CMAKE_BINARY_DIR}/examples/)


