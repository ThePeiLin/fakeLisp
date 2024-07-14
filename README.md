# fakeLisp

> This document is also available in: [**中文**](./README_ZH.md) | [**English**](./README.md)

fakeLisp is a lightweight LISP interpreter written in C.  
Many keywords and builtin functions of the interpreter are copied from `scheme` and `common lisp`. The function used to create thread `go` and the way of inter-thread communication is copied from go-lang. The implementation of variable reference from subfunction to its parents is copied from [`QuickJs`](https://bellard.org/quickjs). Test of module `fuv` which is the binding of `libuv` is copied from [`luv`](https://github.com/luvit/luv.git).  
PRs welcome.  

## Feature

- lexical scope, support closure, and not allow to define new variable when running
- functions can be arguements and be returned from functions.
- tail call optimization
- lightweight, able to imbed to other project
- multi-thread, implement the multi-thread interface with `libuv`
- built-in module `fuv` which is the language binding of `libuv`, allow user to implement asynchronous io with `libuv`
- almost no automatic type conversion, except some mathematical functions
- has a debugger, the module `bdb` provide some basic debugging interface and the package `fdb` is the debugger itself.
- macro, symbol macro, compiler macro and reader macro
- support module and package
- allow to write extension with C language like `lua`
- use comma instead of dot to divide the two parts of pairs

## Compile

To let the compiling of the project more easy, source files of dependences are contained into the repository. No configuration is needed before compiling.  

Dependences that be contained into the repository:  
- [replxx](https://github.com/AmokHuginnsson/replxx.git), for `repl`
- [argtable3](https://github.com/argtable/argtable3.git), for command-line options parsing
- [libuv](https://github.com/libuv/libuv.git), provide asynchronous io, multi-thread and some system calls

### Linux

The general process is as follows:

1. Download the source code
1. generate the build system with CMake
1. compile the project

Here is a example(use `Makefile` as build system):  

```
cd path/to/fakeLisp
mkdir build
cd build
cmake ..
make
```

### Windows

Download the source code, then open it with `visual studio` directly or generate the project solution file with CMake.
Since the project use `stdatomic.h`, the version of `visual studio` should at least be 2022.  

## Some simple examples

### print `hello, world`

```
(define (f n)
  (when (< n 3)
    (println "hello, world")
    (f (1+ n))))
(f 0)
```

### Package and module

Assume that we have such some file as follows.

```
.
├── mod.fkl
├── pac
│   ├── main.fkl
│   ├── mod1.fkl
│   └── mod2.fkl
└── test.fkl
```

The single file is a module, and the directory with a `main` file is a package. Contents of files is as follws.

`test.fkl`
```
;; import package `pac`
(import (pac))
;; It is enable to import module `mod1` only by `(import (pac mod1))`.
;; Let the name of submodule with a base line as prefix to prevent
;; submodule be import by other packages or outer modules.
;; Like: `_mod1.fkl`.
;; Now the module `_mod1` can be import by `(import (_mod1))` in that package.
;; But module `_mod1` still can be include in other packages or outer modules,
;; because special `load` essentially is like `include` in C language.

;; import module `mod`
(import (mod))

;; print symbol `foobar` which is from `mod`
(println foobar)

;; print symbol from `foobar1` submodule `mod1` of package `pac`
(println foobar1)

;; print symbol from `foobar2` submodule `mod2` of package `pac`
(println foobar2)
```

`mod.fkl`
```
;; export definition with special form `export`
(export (define foobar 'foobar))
```

`pac/main.fkl`
```
;; export module `mod1` and module `mod2` with special form `export`
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

Then we execute such a command

```
path/to/fakeLisp test.fkl
```

The output is

```
foobar
foobar1
foobar2
```

### Simple macros
```
(defmacro ~(assert ~exp)
  `(unless ~exp
     (throw 'assert-fail
            ~(append! "expression "
                      (stringify (stringify exp))
                      " failed"))))

;; Macro `assert`, let the program raises error when the expression
;; is false.

(assert (eq 'foobar 'barfoo))

;; Symbol replace macro `foo`
(defmacro foo 'bar)

;; It actually just replaces the symbol in expression
(println foo)

;; Reader macro, this macro defines another syntactic sugar for
;; special form `quote`.

(defmacro
  #[() #["#quote'" *s-exp*] builtin quote]
  ())

(println #quote'foobar)
```

## Attention

- The interpreter does not support 32-bit devices
- The bytecode file is not considered the big-endian and little-endian problems, which make it unable to be use in other platforms.
- The project is a just purely personal project for fun at present with low code quality. The reliability is not guaranteed.

## TODO
- [ ] Explicit documents
- [ ] An array type of which length is not allowed to be modified
- [X] Impove the instruction format of visual machine
- [ ] Instructions' optimization
- [ ] Standardize the bytecode format
- [ ] Full unicode support
- [ ] Better regex support
- [ ] JIT

