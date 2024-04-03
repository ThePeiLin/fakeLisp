# fakeLisp

> 这篇文档包含多个语言版本：[**中文**](./README_ZH.md) | [**English**](./README.md)

fakeLisp是一个用c语言编写的轻量的LISP解释器。  
本解释器的大量关键字和内建函数抄袭自`scheme`和`common lisp`。用于创建线程的函数`go`和内建用于线程间通信的管道抄袭自`go`语言。子函数对于外部函数的变量的引用的实现抄袭自[`QuickJs`](https://bellard.org/quickjs)。`libuv`的绑定，模块`fuv`的测试抄袭自[`luv`](https://github.com/luvit/luv.git)。  
本项目欢迎任何人的贡献。  

## 特性

- 静态词法作用域，支持闭包，不允许运行时添加变量
- 函数可以作为参数传入函数也可以作为返回值返回
- 尾递归优化
- 轻量，可以嵌入其他的项目中
- 多线程，使用`libuv`实现了多线程接口
- 内置`libuv`的绑定模块`fuv`，允许用户通过`libuv`实现异步io
- 无自动类型转换，除了算术函数外基本没有自动类型转换
- 自带调试器，模块`bdb`提供基本调试接口，包`fdb`则是调试器本身
- 宏，支持3种宏，分别为简单的符号替换宏、基于模板匹配的编译器宏和基于LALR分析法的读取器宏
- 支持包和模块
- 可以像`lua`一样使用C语言编写扩展模块
- 使用逗号而不是小数点来分隔序对的两部分

## 编译

为了方便编译，部分依赖的源码直接储存在仓库中，编译前不需要进行任何配置  

被内置在仓库的依赖：  
- [replxx](https://github.com/AmokHuginnsson/replxx.git)，提供`repl`
- [argtable3](https://github.com/argtable/argtable3.git)，提供命令行参数解析
- [libuv](https://github.com/libuv/libuv.git)，提供异步io，多线程以及一些系统调用

### Linux

大致流程如下

1. 将项目源码下载到本地
1. 使用CMake生成目标构建系统的构建文件
1. 编译这个项目

简单的示例（使用`Makefile`作为构建文件）：
```
cd path/to/fakeLisp
mkdir build
cd build
cmake ..
make
```

### Windows

将项目源码下载到本地，然后使用`visual studio`打开  
由于使用了标准库提供的原子操作，因而要求`visual studio`的版本至少要2022  

本项目使用CMake来实现跨平台构建，因而可以使用CMake生成`visual studio`解决方案或者也可以直接使用 `visual studio 2022`编译

## 简单的示例

### 打印`hello, world`

```
(define (f n)
  (when (< n 3)
    (println "hello, world")
    (f (1+ n))))
(f 0)
```

### 模块和包

假定有如下文件结构

```
.
├── mod.fkl
├── pac
│   ├── main.fkl
│   ├── mod1.fkl
│   └── mod2.fkl
└── test.fkl
```

其中，单个文件的为模块，带有一个`main`文件的为包。文件的内容分别如下

`test.fkl`
```
;; 导入包`pac`
(import (pac))
;; 实际上通过`(import (pac mod1))`可以只导入模块`mod1`，
;; 编写包时，如果不想要子模块被外部引用，可以令子模块带有底线前缀，
;; 如`_mod1.fkl`，
;; 此时如果`main.fkl`要引用子模块就要写`(import (_mod1))`。
;; 特殊形式`load`可以无视这种限制，
;; 因为这个特殊形式本质和C语言的`include`差不多

;; 导入模块`mod`
(import (mod))

;; 打印来自模块`mod`的符号`foobar`
(println foobar)

;; 打印来自包`pac`的子模块`mod1`的符号`foobar1`
(println foobar1)

;; 打印来自包`pac`的子模块`mod2`的符号`foobar2`
(println foobar2)
```

`mod.fkl`
```
;; 使用特殊形式`export`导出定义
(export (define foobar 'foobar))
```

`pac/main.fkl`
```
;; 使用特殊形式`export`导出模块`mod1`和`mod2`
(export (import (mod1) (mod2)))
```

`pac/mod1.fkl`
```
(export (define foobar1 'foobar1))
```

`pac/mod2.fkl`
```
(export (define foobar2 'foobar2))
```

我们执行如下命令
```
path/to/fakeLisp test.fkl
```

得到的输出结果为

```
foobar
foobar1
foobar2
```

### 简单的宏
```
(defmacro ~(assert ~exp)
  `(unless ~exp
     (throw 'assert-fail
            ~(append! "expression "
                      (stringify (stringify exp))
                      " failed"))))

;; 断言宏，在断言失败时会报错
(assert (eq 'foobar 'barfoo))

;; 符号替换宏
(defmacro foo 'bar)

;; 只是执行简单的替换
(println foo)

;; 读取器宏，该宏定义了特殊形式`quote`的一个语法糖
(defmacro
  #[() #["#quote'" *s-exp*] builtin quote]
  ())

(println #quote'foobar)
```

## 注意

- 本解释器不支持32位设备  
- 目前的字节码文件没有考虑大小端等问题，不具有分发到其他平台的能力  
- 本项目目前只是纯纯的处于好玩的个人项目，代码的质量低下。本人不对任何的代码的可靠性做出保证

## TODO

- [ ] 清晰易懂的文档
- [ ] 一个不允许修改长度的数组类型
- [ ] 改进虚拟机指令格式
- [ ] 标准化字节码格式
- [ ] 完整的unicode支持
- [ ] 更好的正则表达式支持
- [ ] JIT
