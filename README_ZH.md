# fakeLisp
fakeLisp是一个用c言编写的简单的lisp解释器。
语法与lisp类似，但点对表达式中的分割符是逗号，即：(ii,uu)；  
自定义函数示例：  

```scheme
(define gle (lambda (obj)  
       (cond ((null (cdr obj)) (car obj))  
       (1 (gle (cdr obj))))))  
```

## 宏：  
defmacro的语法比较特殊，为：
(defmacro <用于匹配的表达式> <用于返回的表达式>)  
宏没有名字，而是通过匹配表达式来实现表达式的替换，如：  
```scheme
(defmacro (c ?a ?b) (cons 'cons (cons a (cons b nil))))  
```
这个宏会匹配形如(c ?a ?b)的表达式，并用(cons 'cons (cons a (cons b nil)))的结果替换原来的表达式，  
即表达式(c 9 9)会被替换为(cons 9 9).  

匹配过程中，名字带"?"的符号会绑定源表达式中对应的部分，
如a绑定9，b绑定9。

下面是let宏:  
```scheme
	(defmacro
	 (let ?d,?b)
	 (begin
	  (define map
	   (lambda (f l)
		(define map-iter
		 (lambda (f c p)
		  (cond ((null c) (appd p nil))
		   (1 (map-iter f (cdr c) (appd p (cons (f (car c)) nil)))))))
		(map-iter f l nil)))
	  (define list (lambda ls ls))
	  (define args nil)
	  (define vals nil)
	  (cond ((eq (type d) (quote sym))
			 (setq
			  args
			  (map (lambda (sd) (nth 0 sd)) (car b)))
			 (setq
			  vals
			  (map (lambda (sd) (nth 1 sd)) (car b)))
			 (list (quote let)
			  (quote ())
			  (list (quote define) d (cons (quote lambda) (cons args (cdr b))))
			  (cons d vals)))
	   (1
		(setq args
		 (map (lambda (sd) (nth 0 sd)) d))
		(setq vals
		 (map (lambda (sd) (nth 1 sd)) d))
		(cons (cons (quote lambda) (cons args b)) vals)))))
```
如果用于匹配的表达式是字符串，则这个宏会在建立语法分析树时展开。
下面是用宏实现的quote的语法糖：
```scheme
(defmacro "'(a)" (cons (quote quote) (cons a nil)))
```
其中，被括号包围起来的字符串片段会匹配到相应的字符串并以这个字符串创建对象，  
并以该片段为符号绑定该对象。  
如在表达式 " '(1 2 3) " 中，符号a绑定的对象为 "(1 2 3)" ，最后进行展开并替换源字符串。

在编译环境环境的结构体中(CompEnv)，有一个成员名为proc，这个成员会将当前环境的符号的赋值或修改相关的字节码片段拼在一起，  
用于宏展开时初始化宏的环境，使得宏在编译完成前有一定的可以推导某个变量的值的能力，但是很拉就是了，比如这玩意现在连分支中的修改或赋值也会合并起来。  
其次，读取器宏的作用域是从该宏被定义的地方之后的第一个顶层结构开始。  
例如：  
```scheme
(begin
  (defmacro "'(a)"
    (cons (quote quote)
          (cons a nil)))
  'a) ;不合法

'a ;合法
```
## 可以内置读取器宏有：  

```scheme
'<some-thing>;=>(quote <some-thing>)
`<some-thing>;=>(qsquote <some-thing>)
~<some-thing>;=>(unquote <some-thing>)
~@<some-thing>;=>(unqtesp <some-thing>)
```

编译器宏如果在一个局部环境中定义，则它的作用域只在定义这个宏的环境中。  
如：
```scheme
;假定let宏已经在上文定义
(let ()
  (defmacro (q ?a)
    (cons (quote quote)
          (cons a nil)))
  (q a)) ;合法
(q a) ;不合法
```

---
## 目前可用内置符号有：  
### nil  
即空表：  
```scheme
nil
;=> ()
```
### stdin  
标准输入  
```scheme
stdin
;=> <#fp>
```
### stdout  
标准输出  
```scheme
stdout
;=> <#fp>
```
### stderr  
标准错误  
```scheme
stderr
;=> <#fp>
```

### car cdr cons
```scheme
(define i (cons 1 2))
;=> (1,2)

(car i)
;=> 1

(cdr i)
;=> 2
```
注意，car与cdr函数都是取对应部分的引用，而不是值。

### append
连接表、字符串和字节串，参数数量不定  
若一个参数是列表，则下一个参数的类型任意，  
若为字符串或字节串，则下一个参数只能为字符串或字节串  
```scheme
(append (quote (1 2 3)) (quote (4 5 6)) (quote (7 8 9)))
;=> (1 2 3 4 5 6 7 8 9)

(append (quote (1 2 3)) 4)
;=> (1 2 3,4)

(append "aaa" "bbb" "ccc")
;=> "aaabbbccc"

(append #b112233 #b445566 #b778899)
;=> #b112233445566778899
```

### atom
判断对象是否为原子  
```scheme
(atom 1)
;=> 1

(atom "aaa")
;=> 1

(atom (cons 1 2))
;=> ()

(atom ())
;=> 1
```

### null
判断对象是否为空表  
```scheme
(null ())
;=> 1

(null 1)
;=> ()
```

### not
返回值与null一致，只是为了方便区分谓词  

### eq
判断两个对象是否为同一个对象，通过比较指针  
由于符号、字符都用tagged pointer实现，所以只要字面量相同就是同一对象  
由于整数有32位和64位的版本，因此两个字面量相同的整数也可能是不同的对象  
```scheme
(eq 1.0 1.0)
;=> ()

(eq 1 1)
;=> 1

(eq (quote a) (quote a))
;=> 1

(eq "a" "a")
;=> ()

(eq #\a #\a)
;=> 1
```

### equal
判断两个对象是否完全相等
```scheme
(equal "aaa" "aaa")
;=> 1

(eq "aaa" "aaa")
;=> ()

(eq (cons 1 2) (cons 1 2))
;=> ()

(equal (cons 1 2) (cons 1 2))
;=> 1
```

### =
只能判断数字、字符串或字节串相等  
```scheme
(= 1 1.0)
;=> 1

(= "aaa" "aaa")
;=> 1

(= #b1111 #b1111)
;=> 1
```

### \+
返回若干个数字相加的结果  
```scheme
(+)
;=> 0

(+ 1 2 3)
;=> 6

(+ 1 2.0)
;=> 3.0
```

### \-
需要至少一个参数，如果只有一个参数，则返回参数的相反数，否则返回第一个参数减去剩余参数的和的结果  
```scheme
(- 2)
;=> -2

(- 2.0)
;=> -2.0

(- 1 2 3)
;=> -4

(- 3 1.0)
;=> 2.0
```

### \*
返回若干个参数相乘的结果  
```scheme
(*)
;=> 1

(* 1.0 2)
;=> 2

(* 1.0 2.0 3)
;=> 6
```

### /
至少需要一个参数，如果只有一个参数，返回参数的倒数，否则返回第一个参数除以剩余参数的积的结果  
```scheme
(/ 2)
;=> 0.5

(/ 2 4 2)
;=> 0.25

(/ 4 1.0)
;=> 4

(/ 4 2)
;=> 2
```

### %
返回两个参数的余  
```scheme
(% 4 2)
;=> 0

(% 4.0 2.0)
;=> 0.0

(% 1 2)
;=> 1
```

### \>
若前一个参数均大于后一个参数，则返回真，否则返回假  
```scheme
(> 1)
;=> 1

(> 3 2 1)
;=> 1

(> 3 2 3)
;=> ()

(> "ccc" "bbb" "aaa")
;=> 1
```

### <
若前一个参数均小于后一个参数，则返回真，否则返回假  
```scheme
(< 1)
;=> 1

(< 1 2 3)
;=> 1

(< 1 2 1)
;=> ()

(< "aaa" "bbb" "ccc")
;=> 1
```

### \>=
若前一个参数均大于等于后一个参数，则返回真，否则返回假  

### <=
若前一个参数均小于等于后一个参数，则返回真，否则返回假  

### chr int dbl str sym byts
返回用参数创建的对象  
```scheme
(chr #b61)
;=> #\a

(int #\a)
;=> 61

(dbl 15)
;=> 15.0

(str 11)
;=> "11"

(sym "aaa")
;=> aaa

(byts "aaa")
;=> #b616161
```

### type
返回参数的类型，如
```scheme
(type #\a)
;=> chr

(type list)
;=> proc

(type +)
;=> dlproc

(type 1)
;=> int

(type 1.0)
;=> dbl

(type "sss")
;=> str
```

### nth
第一个参数为索引，第二个参数为要取得其对应索引的引用的对象  
```scheme
(nth 0 ())
;=> ()

(nth 1 (quote (1 2 3)))
;=> 2

(nth 1 "abcd")
;=> #\b

(nth 1 #b616263)
;=> #\b
```

### length
返回对象的长度或管道中消息的数量
```scheme
(length "aaa")
;=> 3

(length (quote (1 2 3)))
;=> 3

(length ())
;=> 0

(length #b112233)
;=> 3
```

### clcc
参数为一个过程或continuation，用当前的continuation调用参数  

### file
第一个参数为文件名，第二个参数为打开的模式，返回文件指针  

### read
参数为需要读取的文件指针（默认为stdout），一次只读取一条表达式，如果参数为字符串，则以该字符串为输入流读取一条表达式。  

### getb
第一个参数为要读取的长度，第二个参数为文件指针，返回对应长度的字节串  

### prin1
第一个参数为需要写入的对象，第二个参数为文件指针（默认为stdout），以机器可读的形式将对象写入文件  

### princ
第一个参数为需要写入的对象，第二个参数为文件指针（默认为stdout），以人可读的形式将对象写入文件  

### putb
第一个参数为要写入的字节串，第二个参数为文件指针，将对象按字节写入文件  
### dll dlsym
dll的参数为要导入的动态库名，返回一个动态库对象。  
dlsym的第一个参数为动态库，第二个参数为要查找的函数名，返回一个过程  

### argv
返回由命令行参数构成的表  
### chanl
参数为管道的缓冲大小，参数为0则为无缓冲管道，返回一个管道对象。  

### send
第一个参数为要发送的对象，第二个参数为管道，将对象送入管道。  
如果送入对象后管道中的对象数量等于或大于缓冲数量，则回发生阻塞。  
如果管道为无缓冲管道则不会发生阻塞。  

### recv
参数为管道，从管道中取出一个对象，如果管道中没有对象，则会发生阻塞。

### error
第一个参数指明发生错误的位置或对象，第二个参数为错误的类型，第三个参数为对该错误的补充说明。第一个参数可以为符号或字符串，第二个参数必须为符号，第三个参数必须为字符串。  

### raise
参数为一个错误对象，该函数会使解释器发生错误对象对应的错误来终止程序的运行或触发错误处理的表达式。  

### newf
参数为内存的大小，申请一块大小为参数所指定的字节数的内存。

### delf
参数为一块内存，释放内存。

### lfdl
参数只能为字符串，字符串的内容为要加载的动态库的路径，如：
```scheme
(lfdl "libc.so.6")
```

---


## 可用特殊形式有:  
### define  
定义符号，如：(define i 1)，定义符号时必须绑定一个值；  
```scheme
(define i 1)
;=> 1
```

### setq  
为符号赋值，符号必须先定义，如：(setq i 8)；  
```scheme
(setq i 8)
;=> 8
```

### setf  
为引用赋值，由于解释器是通过引用传值的，所以可以为任意引用赋值，如:  
```scheme
(define i 10)
;=> 10

(setf i 9)  
;=> 9

i
;=> 9

(setf i (cons 1 2))
;=> (1,2)

(setf (car i) 10)
;=> 10

i
;=> (10,2)
```

### quote  
引用一个对象，使解释器不对该对象求值，如：  
```scheme
(quote (1 2 3))
;=> (1 2 3)  

(quote i)
;=> i  
```

### cond  
这个解释器唯一的一个分支跳转结构，if是用cond定义的宏，  
  
```scheme
;(cond (p1 e1...)
;      (p2 e2...)
;      ...)

(cond (1 2))
;=> 2

(cond (1 2 3))
;=> 3

(cond (nil 1) (1 2))
;=> 2
```
如上所示，如果p1为真，则执行e1并跳出cond，否则跳转到p2并求值确定p2是否为真，  
以此类推直到cond结束。  
这个解释器并没有boolean类型的值，一般用“1”代表真，“()”代表假。  
除了“()”以外，其他值均为真。  
其关系如下：  
```scheme
()
;=> ()

nil
;=> ()

(quote ())
;=> ()

(cons () ())
;=> (())

(quote (,))
;=> (())

(cons (quote ()) (quote ()))
;=> (()) ;这个值为真，因为不是空表
```
nil不是一个常量，只是一个绑定空表的符号，  
所以nil可以被重定义，但是空表求值结果仍然显示"()"。  
即：  
```scheme
(define nil 2)
;=> 2

()
;=> ()
```
### and  
(and e1 e2 e3 ...)  
按顺序求值，若所有表达式为真，则返回最后一个表达式的值，如果有表达式值为假，则停止求值并返回nil；  
如：  
```scheme
(and)
;=> 1

(and 1 2 3)
;=> 3

(and 1 nil 3)
;=> nil
```

### or  
(or e1 e2 e3...)  
按顺序求值，直到有一个表达式值为真，并返回该表达式的值，否则返回nil；  
如：  
```scheme
(or)
;=> nil

(or 1 2 3)
;=> 1

(or 3 nil 1)
;=> 3

(or nil nil nil)
;=> nil
```

### lambda  
(lambda args,body)
lambda表达式，返回一个参数列表为args，函数体为列表body的过程；  
如：  
```scheme
(define list (lambda ls ls))
;=> <#proc>

(list 1 2 3 4)
;=> (1 2 3 4)

(define + (lambda (a,b)
            (cond (b (add a (aply + b)))
                  (1 a))))
;=> <#proc>

(+ 1 2 3)
;=> 6
```
### load   
```scheme
(load filename)
```
加载文件filename到当前文件，其值为文件filename里的最后一个表达式的值；  

### import
加载模块用的特殊形式，表达式的值为该模块最后一个表达式的值；  
编译时编译器查找目标文件中的第一个library表达式，如果library后的第一个表达式与要import的模块所查找的路径不同，则查找文件中的下一个library表达式，  
如import的模块名为(tool test)，则tool/test.fkl文件中至少要有一个library表达式定义的模块名为(tool test)；  
即，(import (tool test))必须对应(library (tool test) ...)  
并且，export表达式必须紧跟模块名，library表达式中必须有且仅有一个export表达式；  
```scheme
;test.fkl
(library (test)
  (export (x y))
  (define x 2)
  (define y 1)
  (define c 9)
  (defmacro (c ?a)
    `(+ ~c ~a))
)

(import (test test1))  ;;加载相对路径为test/test1.fkl的文件
(import (test))  ;;加载相对路径为test.fkl的文件
(import (prefix test))  ;;加载相对路径为prefix/test.fkl的文件
(import (prefix (test) test:)  ;;加载相对路径为test.fkl的文件，并使文件中被export的符号带上前缀"test:"

test:x
;=> 2

test:y
;=> 1
```
### begin  
按顺序求值并返回最后一个值；  
```scheme
(begin
  (add 1 2)
  (add 2 3))
;=> 5
```
### unquote  
```scheme
(unquote a)
```
反引用，表达式与a等价。  
### qsquote  
准引用，与引用差不多，但是准引用中的被反引用的表达式依旧会被求值;  
```scheme
(qsquote (add 1 2))
;=> (add 1 2)

(qsquote (unquote (add 1 2)))
;=> 3

(qsquote (1 2 3 (unquote (add 2 2))))
;=> (1 2 3 4)
```
### unqtesp  
反引用且连接，只能用在准引用中，将表达式的值与准引用的表连接起来
```scheme
(define list (lambda ls ls))
;=> <#proc>

(qsquote (1 2 3 (unqtesp (list 4 5 6)) 7 8 9))
;=> (1 2 3 4 5 6 7 8 9)
```

### try
错误（异常）处理特殊形式，示例如下：
```scheme
(try (/ 1 0)
  (catch e
    (div-zero-error (princ "caught an error\n"))))
caught an error
;=> caught an error
```
一般语法为：
```scheme
(try <expression1>
  (catch <symbol-that-bind-the-error-occured>
    (<error-type> <expression2> ... <expression3>)
    (<other-error-type> <expression4> ... <expression5>)
    ...))
```
如果没有发生错误，则表达式的值为\<expression1\>中的值，否则其值为对应错误的处理表达式的值，如\<expression3\>的值或\<expression5\>的值，  
错误发生的时候，会产生一个错误对象，用符号\<symbol-that-bind-the-error-occured\>绑定；尖括号中的内容为可替代内容。  

### szof
格式为：  
```scheme
(szof <type>)
```
返回<type>的大小。  

### getf
格式为：  
```scheme
(getf <type> <path> <exp> <index(可选)>)
```

按照path返回一个内存的引用。
比如：  
```
(deftype Vec3
  (struct
    (x float)
    (y float)
    (z float)))

(define v1 (newf (szof Vec3)))
(setf (getf Vec3 (x) v1) 1.0)
(getf Vec3 (x) v1)
```
path中的“\&”表示取地址，“\*”表示解引用。  
只能取原始类型，数组，指针，外部函数的引用，引用结构体联合体将会导致编译错误。  
这些引用都是不安全的。  

### flsym
从加载的动态库中查找一个函数，并返回这个函数
格式为：
```scheme
(flsym <funtion-type> <name>)
(flsym (function (string) size_t) "strlen")
```


## 预处理指令（不产生字节码）:  
defmacro  

### deftype
给一个类型加一个别名，只支持原始数据类型，数组，指针，结构体，联合体。  
示例如下：
```scheme
(define LNode
   (struct LNode
     (data int)
     (next (ptr (struct LNode)))))

(define fiUnion
  (union
    (f float)
    (i int)))

(function (int int) int)
;         ~~~~~~~~~  ^
;             ^      |
;             |  函数返回值
;          参数列表

(deftype Mtx4 (array 16 float))

```
## 关于面向对象  
lisp系的编程语言大多都有词法闭包，  可以利用词法闭包来实现面向对象的功能，下面的宏实现了一个简单的对象系统：  
  
```scheme
(defmacro (class ?name,?body)
  (begin
    (define data-list nil)
    (define pre-method-list nil)
    (define method-list nil)
    (define construct-method nil)
    (define case-list nil)

    (let loop ((c body))
      (cond (c
              (letcc break
                     (define segmentId (car (car c)))
                     (define segmentData (cdr (car c)))
                     (if (and (not (eq segmentId 'data))
                              (not (eq segmentId 'method)))
                       (break nil))
                     (case segmentId
                       (('data) (setf data-list (appd data-list segmentData)))
                       (('method) (setf pre-method-list (appd pre-method-list segmentData)))))
              (loop (cdr c)))))

    (let loop ((c data-list))
      (cond (c (let ((name (car (car c))))
                 (setf case-list (cons `(((quote ~name)) ~name) case-list)))
               (loop (cdr c)))))

    (let loop ((c pre-method-list))
      (cond (c
              (let ((methodName (car (car c))))
                (cond ((not (eq methodName name))
                       (setf method-list (cons `(~methodName (lambda ~@(cdr (car c)))) method-list))
                       (setf case-list (cons `(((quote ~methodName)) ~methodName) case-list)))
                      (1
                       (setf construct-method (cdr (car c))))))
              (loop (cdr c)))))
    (setf case-list
          (appd case-list
                `((1
                   (raise (error 'invalid-selector (append "error:Invalid selector \"" (str selector) "\"\n")))))))

    (define local-env (appd data-list method-list))
    `(define ~name
       (lambda ~(car construct-method)
         (letrec ~local-env
           ~@(cdr construct-method)
           (define this
             (lambda (selector)
               (case selector
                 ~@case-list))))))))

(class Vec3
       (data
         (X 0.0)
         (Y 0.0)
         (Z 0.0))

       (method
         (Vec3 (x y z)
               (setf X (dbl x))
               (setf Y (dbl y))
               (setf Z (dbl z)))
         (setX (x)
               (setf X (dbl x)))
         (setY (y)
               (setf Y (dbl y)))
         (setZ (z)
               (setf Z (dbl z)))))
;=> <#proc>

(define i (Vec3 1 2 3))
;=> <#proc>

(i 'X)
;=> 1.0

(i 'Y)
;=> 2.0

(i 'Z)
;=> 3.0

((i 'setX) 9)
;=> 9.0

((i 'setY) 8)
;=> 8.0

((i 'setZ) 7)
;=> 7.0

(i 'X)
;=> 9.0

(i 'Y)
;=> 8.0

(i 'Z)
;=> 7.0
```
---
基于消息的多线程。  
没有面向对象的功能，但如果你把复合过程当作对象那就算有面向对象的功能，  
另外，如果觉得把复合过程当作对象写代码太麻烦的话可以写几个宏来方便实现面向对象的功能，  
这个解释器的宏展开过程是跑虚拟机，这种设置给宏的展开过程提供了极大的灵活性。  
另外，由于宏展开的虚拟机编号是-1，以此无法在宏展开过程中创建线程。  
我会找时间写调试器。还得实现一些编译时参数数量检查，不然太麻烦了。  
还有，我会找时间写一个好一些的repl，现在的repl简直不能叫repl。  
