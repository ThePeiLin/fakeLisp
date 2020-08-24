form.o: src/form.*
	gcc -g -c src/form.c
tool.o: src/tool.*
	gcc -g -c src/tool.c
preprocess.o: src/preprocess.*
	gcc -g -c src/preprocess.c
syntax.o: src/syntax.*
	gcc -g -c src/syntax.c
fakeLisp.o: src/fakeLisp.*
	gcc -g -c src/fakeLisp.c
compiler.o: src/compiler.* src/opcode.h
	gcc -g -c src/compiler.c
fakeLisp: fakeLisp.o form.o tool.o preprocess.o syntax.o compiler.o
	gcc -g -o fakeLisp fakeLisp.o tool.o form.o preprocess.o syntax.o compiler.o
