form.o: src/form.*
	gcc -g -c src/form.c
fake.o: src/fake.*
	gcc -g -c src/fake.c
tool.o: src/tool.*
	gcc -g -c src/tool.c
preprocess.o: src/preprocess.*
	gcc -g -c src/preprocess.c
fakeLisp.o: src/fakeLisp.*
	gcc -g -c src/fakeLisp.c
fakeLisp: fakeLisp.o form.o fake.o tool.o preprocess.o
	gcc -g -o fakeLisp fakeLisp.o tool.o fake.o form.o preprocess.o
