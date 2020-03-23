form.o: src/form.c src/form.h
	gcc -g -c src/form.c
fake.o: src/fake.c src/fake.h
	gcc -g -c src/fake.c
tool.o: src/tool.c src/tool.h
	gcc -g -c src/tool.c
pretreatment.o: src/pretreatment.c src/pretreatment.h
	gcc -g -c src/pretreatment.c
fakeLisp.o: src/fakeLisp.c src/fakeLisp.h
	gcc -g -c src/fakeLisp.c
fakeLisp: fakeLisp.o form.o fake.o tool.o pretreatment.o
	gcc -g -o fakeLisp fakeLisp.o tool.o fake.o form.o pretreatment.o
