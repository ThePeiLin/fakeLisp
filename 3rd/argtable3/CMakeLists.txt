cmake_minimum_required(VERSION 3.18)

set(CMAKE_C_STANDARD 11)

set(ARGTABLE3_TARGET_NAME argtable3)

project(${ARGTABLE3_TARGET_NAME})

if(MSVC)
	add_compile_options(/W4)
else()
	add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wpointer-arith)# -fsanitize=thread)
endif()

add_library(${ARGTABLE3_TARGET_NAME}
	STATIC
	"${PROJECT_SOURCE_DIR}/argtable3.c")
