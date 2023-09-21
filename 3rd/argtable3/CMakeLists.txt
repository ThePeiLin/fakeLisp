cmake_minimum_required(VERSION 3.18)

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

set(ARGTABLE3_TARGET_NAME argtable3)

project(${ARGTABLE3_TARGET_NAME})

file(GLOB ARGTABLE3_SOURCE "${PROJECT_SOURCE_DIR}/*.c")

add_library(${ARGTABLE3_TARGET_NAME}
	STATIC
	${ARGTABLE3_SOURCE})
