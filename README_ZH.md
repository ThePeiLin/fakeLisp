# fakeLisp
fakeLisp是一个用c言编写的简单的lisp解释器。
语法与lisp类似，但点对表达式中的分割符是逗号，即：(ii,uu)；  
自定义函数示例：  

```scheme
(define gle (lambda (obj)  
       (cond ((null (cdr obj)) (car obj))  
       (1 (gle (cdr obj))))))  
```

支持宏；  
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
  ((lambda ()
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
     (cond ((eq (type d) 'sym)
            (setq
              args
              (map (lambda (sd) (nth 0 sd)) (car b)))
            (setq
              vals
              (map (lambda (sd) (nth 1 sd)) (car b)))
            (list 'let
                  '()
                  (list 'define d (cons 'lambda (cons args (cdr b))))
                  (cons d vals)))
           (1
            (setq args
                  (map (lambda (sd) (nth 0 sd)) d))
            (setq vals
                  (map (lambda (sd) (nth 1 sd)) d))
            (cons (cons 'lambda (cons args b)) vals)
            )))))  
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

import与load的区别在于import不产生字节码，而且只能加载.dll文件或.so文件，二而load只能加载fakeLisp源代码文件，  
并且加载好的.dll文件或.so文件中可调用的函数会作为常量供调用，如下面的代码错误的，（假设btk模块中有可调用的getch函数）  
```scheme
(import btk)
(setq getch "getch")
```
其中，getch是一常量而不是变量，它会一直是常量直到getch符号被定义。  
如下面的代码，  
```scheme
(import btk)
(define getch 9)
(setq getch 10)
```
并且如上的定义只在当前的环境有效，所以最好不要试图重定义.dll文件或.so文件中可用的函数。  
另外，所谓可用函数即函数名有\"FAKE_\"前缀的函数。

---
目前可用内置函数及符号有：  
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
mod  
nth  
length  
appd  
open  
close  
read  
readb  
write  
princ   
writeb  
go  
send  
recv  
getid  
clcc  

可用特殊形式有:  
define  
setq  
setf  
quote  
cond  
and  
or  
lambda  
load   
unquote  
qsquote  
unqtesp  

预处理指令（不产生字节码）:  
import  
defmacro  

setf可以给任何值赋值，因为setf是修改引用，  
因此你给一个不是变量的值赋值是没有问题的，只不过没什么用就是了。  
基于消息的多线程。  
可以编译整个文件，有尾递归优化。  
可以愉快地开始写lisp了。  
没有面向对象的功能，但如果你把复合过程当作对象那就算有面向对象的功能，  
另外，如果觉得把复合过程当作对象写代码太麻烦的话可以写几个宏来方便实现面向对象的功能，  
这个解释器的宏求值过程是跑虚拟机，这种设定给宏的展开过程提供了极大的灵活性。  
另外，由于宏展开的虚拟机编号是-1，以此无法在宏展开过程中创建线程。  
我会找时间写调试器。还得实现一些编译时参数数量检查，不然太麻烦了。  
暂时没有quasiquote、unquote和unquote-splicing等方便写宏的特殊形式。  
