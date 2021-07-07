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
(defmacro (c $a $b) (cons 'cons (cons a (cons b nil))))  
```
这个宏会匹配形如(c $a $b)的表达式，并用(cons 'cons (cons a (cons b nil)))的结果替换原来的表达式，  
即表达式(c 9 9)会被替换为(cons 9 9).  

匹配过程中，名字带"$"的符号会绑定源表达式中对应的部分，
如a绑定9，b绑定9。

下面是let宏:  
```scheme
	(defmacro
	 (let $d,$b)
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
		(cons (cons (quote lambda) (cons args b)) vals)
	   ))))  
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
  'a ;不合法
  )

'a ;合法
```

编译器宏如果在一个局部环境中定义，则它的作用域只在定义这个宏的环境中。  
如：
```scheme
;假定let宏已经在上文定义
(let ()
  (defmacro (q $a)
    (cons (quote quote)
          (cons a nil)))
  (q a) ;合法
  )
(q a) ;不合法
```

---
## 目前可用内置符号有：  
nil  
EOF  
stdin  
stdout  
stderr  

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
为引用赋值，由于解释器是通过引用传值的，所以可以为任意引用赋值，包括字面量，如:  
```scheme
(setf i 9)  
;=> 9

(setf 1 9) ;合法，但是没有实际效果；  
;=> 9
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
;      ...
;      )

(cond (1 2))
;=> 2

(cond (1 2 3))
;=> 3

(cond (nil 1) (1 2))
;=> 2
```
如上所示，如果p1为真，则执行e1并跳出cond，否则跳转到p2并求值确定p2是否为真，  
以此类推直到cond结束。  
这个解释器并没有boolean类型的值，一般用“1”代表真，“nil”代表假。  
除了“nil”、“()”以外，其他值均为真。  
“nil”与“()”并不等价。其关系如下：  
```scheme
()
;=> nil

nil
;=> nil

(quote ())
;=> ()

(cons () ())
;=> ()

(cons (quote ()) (quote ()))
;=> ((),()) ;这个值为真，因为不是空表
```
nil不是一个常量，只是一个绑定空表求值结果的符号，  
所以nil可以被重定义，但是空表求值结果仍然显示nil。  
即：  
```scheme
(define nil 2)
;=> 2

()
;=> nil
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
  (define y 1))

(import (test test1))  ;;加载相对路径为test/test1.fkl的文件
(import (test))  ;;加载相对路径为test.fkl的文件
(import (prefix test))  ;;加载相对路径为prefix/test.fkl的文件
(import (prefix (test) test:)  ;;加载相对路径为test.fkl的文件，并使文件中被export的符号带上前缀"test:"
;=> 1

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
### progn  
嵌入字节码，字节码表还在写，而且随时会有变化。  
且由于没有字节码验证器，直接写字节码有一定的安全风险。  
```scheme
(define cons (lambda (a b)
			  (progn
			   push_pair
			   push_var a
			   pop_car
			   push_var b
			   pop_cdr
			  )))
;=> <#proc>

(define a (lambda (a)
		   (progn
			set_tp
			push_var a
			jmp_if_true t
			res_tp
			push_nil
			jmp u
		 :t res_tp
			push_int 1
		 :u pop_tp
		   )))
;=> <#proc>

(define clcc (lambda (f)
              (progn
               $cc
               pack_cc
               pop_var cc
               set_bp
               push_var cc
               push_var f
               invoke
              )))
;=> <#proc>

(cons 1 2)
;=> (1,2)

(a 1)
;=> 1

(a nil)
;=> nil

(define c nil)
;=> nil

(add 1 (clcc (lambda (cc) (setq c cc) 9)))
;=> 10

(c 10)
;=> 11
```

### try
错误（异常）处理特殊形式，示例如下：
```scheme
(try (div 1 0)
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

## 预处理指令（不产生字节码）:  
defmacro  

## 关于面向对象  
lisp系的编程语言大多都有词法闭包，  可以利用词法闭包来实现面向对象的功能，下面的宏实现了一个简单的对象系统：  
  
```scheme
(defmacro (class $name,$body)
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
                   (raise (error 'invalid-selector (append "error:Invalid selector \"" (str selector) "\"\n")))
                   ))))

    (define local-env (appd data-list method-list))
    `(define ~name
       (lambda ~(car construct-method)
         (letrec ~local-env
           ~@(cdr construct-method)
           (define this
             (lambda (selector)
               (case selector
                 ~@case-list)))
           (progn
             push_var this
             pop_env 1
             ))))))

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
