form.o: src/form.* src/fakedef.h
	gcc -g -c src/form.c
tool.o: src/tool.* src/fakedef.h
	gcc -g -c src/tool.c
preprocess.o: src/preprocess.* src/fakedef.h
	gcc -g -c src/preprocess.c
syntax.o: src/syntax.* src/fakedef.h
	gcc -g -c src/syntax.c
fakeLisp.o: src/fakeLisp.* src/fakedef.h
	gcc -g -c src/fakeLisp.c
compiler.o: src/compiler.* src/opcode.h src/fakedef.h
	gcc -g -c src/compiler.c
fakeVM.o: src/fakeVM.* src/fakedef.h
	gcc -g -c src/fakeVM.c
fakeLisp: fakeLisp.o form.o tool.o preprocess.o syntax.o compiler.o
	gcc -g -o fakeLisp fakeLisp.o tool.o form.o preprocess.o syntax.o compiler.o
