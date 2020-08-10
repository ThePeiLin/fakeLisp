form.o: src/form.*
	gcc -g -c src/form.c
fake.o: src/fake.*
	gcc -g -c src/fake.c
tool.o: src/tool.*
	gcc -g -c src/tool.c
preprocess.o: src/preprocess.*
	gcc -g -c src/preprocess.c
syntax.o: src/syntax.*
	gcc -g -c src/syntax.c
fakeLisp.o: src/fakeLisp.*
	gcc -g -c src/fakeLisp.c
compiler.o: src/compiler.*
	gcc -g -c src/compiler.c
fakeLisp: fakeLisp.o form.o fake.o tool.o preprocess.o syntax.o
	gcc -g -o fakeLisp fakeLisp.o tool.o fake.o form.o preprocess.o syntax.o
