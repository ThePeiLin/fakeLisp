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
目前在展开宏的表达式中可用的特殊形式及函数有：  
add  
sub  
mul  
div  
mod  
car  
cdr  
cons  
quote  
define  
eq  
atom  
null  
and  
or  
not  
list  
append  
extend  
lambda  
print  
其中，defmacro的语法比较特殊，为：
(defmacro 用于匹配的表达式 用于返回的表达式)  
宏没有名字，而是通过匹配表达式来实现表达式的替换，如：  
```scheme
(defmacro (c ATOM#FIR ATOM#SEC) (list 'cons FIR SEC))  
```
这个宏会匹配形如(c ATOM ATOM)的表达式，并用(list 'cons FIR FIR)的结果替换原来的表达式，  
即表达式(c 9 9)会被替换为(cons 9 9).  

下面是defun宏:  
```scheme
(defmacro  
          (defun ATOM#FUNCNAME CELL#ARGS,CELL#EXPRESSION)  
          (list 'define FUNCNAME (list 'quote (cons 'lambda (cons ARGS EXPRESSION)))))  
```

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
app  
ischr  
isint  
isdbl  
isstr  
issym  
isprc  
isbyt  
eq  
equal  
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
strcat  
bytcat  
append  
rand  
open  
close  
getc  
getch  
ungetc  
read  
readb  
write  
princ   
writeb  
tell  
seek  
rewind  
exit  
go  
send  
accept  
getid  

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

基于消息的多线程。  
可以编译整个文件，有尾递归优化。
可以愉快地开始写lisp了。
没有面向对象的功能，但如果你把复合过程当作对象那就算有面向对象的功能，  
另外，如果觉得把复合过程当作对象写代码太麻烦的话可以写几个宏来方便实现面向对象的功能，  
还有，这个解释器对宏的支持有点孱弱，我会找个时间来完善并加强。（比如无法用宏实现scheme的case）
我会找时间写调试器。还得实现一些编译时参数数量检查，不然太麻烦了。
