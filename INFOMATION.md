# 这是一份关于源文件结构的说明  

include/fakeLisp/:头文件
src/:源文件

include/fakeLisp/base.h 定义基础抽象数据结构及对应的函数  
include/fakeLisp/opcode.h 字节码表  
include/fakeLisp/utils.h 提供部分运行及编译时所需的部分通用函数  
include/fakeLisp/reader.h 字符串匹配模板的定义及读取器和读取宏相关函数  
include/fakeLisp/ast.h  提供语法分析树相关函数  
include/fakeLisp/vm.h 虚拟机相关定义及函数  
include/fakeLisp/compiler.h 编译器定义及编译器相关函数  
include/fakeLisp/builtin.h 内置函数的定义  
src/builtin.c 内置函数  
src/fakeLisp.c 解释器源文件  
src/fakeLispc.c 编译器源文件  
ffi/* 外部函数接口模块  
fklc/* 编译器模块  
