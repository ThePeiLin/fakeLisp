# 这是一份关于源文件结构的说明  

src/fakedef.h 提供部分运行及编译时的通用数据结构的定义  
src/opcode.h 字节码表  
src/tool.\* 提供部分运行及编译时所需的部分通用函数  
src/reader.\* 提供读取器和读取宏相关函数  
src/ast.\*  提供语法分析树相关函数  
src/VMtool.\* 虚拟机运行用通用工具  
src/fakeVM.\*  提供运行字节码的虚拟机有关函数  
src/compiler.\* 提供编译器相关函数  
src/fakeLisp.\* 解释器源文件  
src/fakeLispc.\* 编译器源文件  
src/btk.c basic tool kit 基础工具套件，提供脚本调用的第三方函数  
