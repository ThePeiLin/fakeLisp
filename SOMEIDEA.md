# 这个文件主要记录我加特性时的想法
## 写编译器时的想法，大概在2020七月左右开始

这是关于生成字节码的一些想法  

生成字节码的主要流程如下：  
	对目标AST进行宏的展开  
  ->检查AST，分析是否有错误->修剪AST，将其中没有必要但语法没错的部分进行修剪  
  ->常量折叠->查找是否有需要进行符号替换,一般来说，需要替换符号的情况主要出现在每个列表的第一项，  
    由于内置函数可重命名，  
    有可能需要将列表第一项的符号进行替换，替换为其绑定的值  
  ->拆分AST，将其中列表的每一项进行拆分  
  ->从AST的最右端开始，根据列表第一项的内容生成字节码  
  ->插入到字节码文件中。  
   （在生成绑定lambda表达式的符号时，应检查是否为尾递归）  
   （不会生成宏对应的字节码，因为没必要）  

(define i 9)  
对应字节码:  
	0.push_num 9  
	1.pop_num_static 0  

(define ii (quote (9,2))=>  
(define ii (9,2))=>  
（常量折叠）  
define ii (9,2)=>   
（拆分AST）  
	0.push_num 9  
	1.push_num 2  
	3.push_num 2  
	4.push_num 1  
	5.cons  
	6.pop_list_static  

```scheme
(defmacro   
(defun ATOM CELL,CELL)  
(list (quote define) ATOM#0 (list (quote quote) (cons (quote lambda) (cons CELL#0 CELL#1)))))  

(defun gle (obj)   
(cond ((null (cdr obj)) (car obj))    
(1 (gle (cdr obj)))))  

```

=>
```scheme
(define gle  
(quote  
(lambda (obj)  
(cond ((null (cdr obj)) (car obj))  
(1 (gle (cdr obj)))))))  
```
（宏展开）

=>
```scheme
(define gle
(lambda (obj)
(cond ((null (car obj)) (car obj))
(1 (gle (cdr obj))))))
```
（折叠常量）

=>
```
define gle (lambda (obj) (cond ((null (car obj)) (car obj)) (1 (gle (cdr obj)))))
```
（拆分AST）

=>  
0.push_sym "obj"  
1.push_num 0  
2.push_num 1  
3.cons  
4.push_sym "car"  
5.push_num 2  
5.push_num 1  
6.cons  
7.push_0  
8.push_num 1  
9.cons  
10.push_sym "gle"  
11.push_num 2  
12.push_num 1  
13.cons  
14.push_num 1  
15.push_num 2  
16.push_num 1  
17.cons  
18.push_0  
19.push_num 1  
20.cons  
21.push_sym "obj"  
22.push_0  
23.push_num 1  
24.cons  
25.push_sym "car"  
26.push_num 2  
27.push_num 1  
28.cons  
29.push_0  
30.push_1  
31.cons  
32.push_sym "obj"  
33.push_num 0  
34.push_num 1  
35.cons  
36.push_sym "cdr"  
37.push_num 2  
38.push_num 1  
39.cons  
40.push_0  
41.push_num 1  
42.cons  
43.push_sym "null"  
44.push_num 2  
45.push_num 1  
46.cons  
47.push_num 8  
48.push_num 1  
49.cons  
50.push_num 14  
51.push_num 1  
52.cons  
53.push_sym "cond"  
54.push_num 2  
55.push_num 1  
56.cons  
57.push_0  
58.push_num 1  
59.cons  
60.push_sym "obj"  
61.push_0  
62.push_num 1  
63.cons  
64.push_num 3  
65.push_num 1  
66.cons  
67.push_sym "lambda"  
68.push_num 2  
69.push_num 1  
70.cons  
71.zip  
72.pop_list_static 0  
73.push_list_static 0  
74.lambda  
75.pop_function 0  


```
(gle (quote (9 0 9)))
```
=>  
```
(gle (9 0 9))
```
=>  
gle (9 0 9)
=>  
0.push_num 9  
1.push_0  
2.cons  
3.push_0  
4.push_num 2  
5.push_num 1  
6.cons  
7.push_num 9  
8.push_num 2  
9.push_num 1  
10.cons  
11.zip  
12.push_list_stack 0  
12.push_list_static 0  
13.lambda  
14.invole  
15.ret  

---
