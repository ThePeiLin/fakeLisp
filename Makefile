form.o: src/form.*
	gcc -g -c src/form.c
fake.o: src/fake.*
	gcc -g -c src/fake.c
tool.o: src/tool.*
	gcc -g -c src/tool.c
pretreatment.o: src/pretreatment.*
	gcc -g -c src/pretreatment.c
fakeLisp.o: src/fakeLisp.*
	gcc -g -c src/fakeLisp.c
fakeLisp: fakeLisp.o form.o fake.o tool.o pretreatment.o
	gcc -g -o fakeLisp fakeLisp.o tool.o fake.o form.o pretreatment.o
