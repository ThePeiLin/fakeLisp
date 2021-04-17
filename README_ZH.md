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
其中，被括号包围起来的字符串片段会匹配到相应的字符串并以这个字符创建语法分析树，  
并以该片段为符号绑定该语法分析树。  
如在表达式 " '(1 2 3) " 中，符号a绑定的语法分析树为 "(1 2 3)" ，最后进行展开并替换源字符串。

## 关于import

import的语法为

```
(import <模块名>)
```

import与load的区别在于import不产生字节码，而且只能加载.dll文件或.so文件，而load只能加载fakeLisp源代码文件，  
并且加载好的.dll文件或.so文件中可调用的函数的函数名会被添加到全局环境。（假设btk模块中有可调用的getch函数）  
```scheme
(import btk)
(setq getch "getch")
```
其中，getch是变量，调用getch会调用模块中对应的函数，直到该符号被重定义。
如下面的代码，  
```scheme
(import btk)
(define getch 9)
(setq getch 10)
```
另外，所谓可用函数即函数名有\"FAKE_\"前缀的函数。

---
## 目前可用内置函数及符号有：  
nil  
EOF  
stdin  
stdout  
stderr  
cons  
car  
cdr  
atom  
null  
aply  
type  
eq  
equal  
eqn  
gt  
ge  
lt  
le  
not  
dbl  
str  
sym  
chr  
int  
byt  
add  
sub  
mul  
div  
rem   
nth  
length  
appd  
file  
read  
getb  
write  
princ   
putb  
go  
chanl  
send  
recv  
clcc  

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
;=>9

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
### proc  
嵌入字节码，字节码表还在写，而且随时会有变化。  
且由于没有字节码验证器，直接写字节码有一定的安全风险。  
```scheme
(define cons (lambda (a b)
			  (proc
			   push_pair
			   push_var a
			   pop_car
			   push_var b
			   pop_cdr
			  )))
;=> <#proc>

(define a (lambda (a)
		   (proc
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

(cons 1 2)
;=> (1,2)

(a 1)
;=> 1

(a nil)
;=> nil
```

## 预处理指令（不产生字节码）:  
import  
defmacro  

---
基于消息的多线程。  
可以编译整个文件，有尾递归优化。  
可以愉快地开始写lisp了。  
没有面向对象的功能，但如果你把复合过程当作对象那就算有面向对象的功能，  
另外，如果觉得把复合过程当作对象写代码太麻烦的话可以写几个宏来方便实现面向对象的功能，  
这个解释器的宏展开过程是跑虚拟机，这种设置给宏的展开过程提供了极大的灵活性。  
另外，由于宏展开的虚拟机编号是-1，以此无法在宏展开过程中创建线程。  
我会找时间写调试器。还得实现一些编译时参数数量检查，不然太麻烦了。  
