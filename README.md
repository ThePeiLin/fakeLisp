# fakeLisp
This is a simple LISP interpreter.  
The grammer of command that can be run by This interpreter like scheme Lisp but it's not lisp,so I call it fakeLisp.  
This interpreter only can't compile a whole file,but it can compile expressions that you input in console.  
The "defmacro" key word is able to use,and some special form is also able to use in expanding macro.   


I use comma to divide car and cdr but not dot.Such as (1,2) and (1,(1,2)).  
And you only can call function in brakets.For exmple,(add 1 1) and (add 1 (sub 2 3)).  
If you want more information about this interpreter please read [README_ZH.md](./README_ZH.md),because my English is really bad.
