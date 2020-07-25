# fakeLisp
fakeLisp是一个用c言编写的简单的lisp解释器，由于技术原因，暂时不能解释文件，只能用控制台运行。  
语法与lisp类似，但点对表达式中的分割符是逗号，即：(ii,uu)；  
由于没有复合过程（其实是自己不会），所以自定义函数实际上是将一个列表与一个符号绑定；  
自定义函数示例：  
(defun gle (obj)  
       (cond ((null (cdr obj)) (car obj))  
       (1 (gle (cdr obj)))))  
  
支持宏；  
目前可用函数及特殊形式有：  
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
lambda  
list  
defmacro  
print  
undef（用于取消宏的定义）  
其中，defmacro的语法比较特殊，为：
(defmacro 用于匹配的表达式 用于返回的表达式)  
宏没有名字，而是通过匹配表达式来实现宏的嵌入，如：  
(defmacro (c ATOM#FIR ATOM#SEC) (list 'cons FIR SEC))  
这个宏会匹配形如(c ATOM ATOM)的表达式，并用(list 'cons FIR FIR)的结果替换原来的表达式，  
即表达式(c 9 9)会被替换为(cons 9 9).  

下面是defun宏:  
(defmacro  
         (defun ATOM#FUNCNAME CELL#ARGS,CELL#EXPRESSION)  
         (list 'define FUNCNAME (list 'quote (cons 'lambda (cons ARGS EXPRESSION)))))  

我应该会添加复合过程类型
