# fakeLisp
fakeLisp是一个用c言编写的简单的lisp解释器，由于技术原因，暂时不能解释文件，只能用控制台运行。  
语法与lisp类似，但点对表达式中的分割符是逗号，即：(ii,uu)；  
由于没有复合过程，所以自定义函数实际上是将一个列表与一个符号绑定；  
自定义函数示例：  
(define gle (quote  
	(lambda (x)  
		(cons 9 x))))  
支持宏；  
可在src/fakeLisp.c文件中查看可用内置函数；
