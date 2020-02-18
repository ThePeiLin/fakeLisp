form.o: src/form.c src/form.h
	gcc -c src/form.c
fake.o: src/fake.c src/fake.h
	gcc -c src/fake.c
tool.o: src/tool.c src/tool.h
	gcc -c src/tool.c
fakeLisp.o: src/fakeLisp.c
	gcc -c src/fakeLisp.c
fakeLisp: fakeLisp.o functionAndForm.o fake.o numAndString.o
	gcc -o fakeLisp fakeLisp.o tool.o fake.o form.o
