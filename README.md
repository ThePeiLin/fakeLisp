# fakeLisp

This is a new dialect of LISP.  
This is a simple LISP interpreter that write with c language.  
The grammer of this languege looks like lisp,but I use comma to divide the dot pair expressions;  
For example: (ii,uu);  

The interpreter can compile the sourse code to byte code and excute the byte code.  

With only few built-in functions and special forms,   
fakeLisp supports various programming paradigms like   
funtional programming,  
Object Oriented programming (if you think that procedure is a kind of object),  
and so on.  

For this language, funtion or procedure are a kind of data and it can be operated just like other basic data.  

Using lambda expression and symbol-binding, you can define your own function.   

The expression follows will create a function and bind it with the symbol "gle". The function "gle" can get the last element of a list.  

```scheme
(define gle
  (lambda (obj)
    (cond ((null (cdr obj)) (car obj))
          (1 (gle (cdr obj))))))
```

To simplify some complex expression, macro is a good choice.  

The language support two kinds of macro, compiler macro and reader macro.  

Macro can use to simplify some complex expressions and improve the readability of your program.  
Here is compiler macro that use to simplify the process of define a new function.  

```scheme
(defmacro (define (name,args),body)
  ((lambda ()
     (define list (lambda ls ls))
     (list (quote define) name (cons (quote lambda) (cons args body))))))
```

With the macro above, you can define the "gle" function just like the expression follows.  

```scheme
(define (gle obj)
  (cond ((null (cdr obj)) (car obj))
        (1 (gle (cdr obj)))))
```

Available built-in symbols:
nil  
stdin  
stdout  
stderr  
car  
cdr  
cons  
append  
atom  
null  
not  
eq  
equal  
\=  
\+  
\-  
\*  
/  
%  
\>  
<  
\>=  
<=  
chr  
int  
dbl  
str  
sym  
byts  
type  
nth  
length  
clcc  
file  
read  
getb  
write  
putb  
princ  
dll  
dlsym  
argv  
go  
chanl  
send  
recv  
error  
raise  
newf  
delf  
lfdl  

Special forms:  

define  
setq  
setf  
quote  
cond  
and  
or  
lambda  
load  
begin  
unquote  
qsquote  
unqtesp  
proc  
import  
getf  
szof  
flsym  

Preprocesser commands:  
deftype  
defmacro  

##  Chinese version:  
[README\_ZH.md](./README\_ZH.md)
