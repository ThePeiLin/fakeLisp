functionAndForm.o: src/functionAndForm.c src/functionAndForm.h
	gcc -c src/functionAndForm.c
fake.o: src/fake.c src/fake.h
	gcc -c src/fake.c
floatAndString.o:src/floatAndString.c src/floatAndString.h
	gcc -c src/floatAndString.c
fakeLisp.o: src/fakeLisp.c
	gcc -c src/fakeLisp.c
fakeLisp: fakeLisp.o specialFunction.o fake.o floatAndString.o
	gcc -o fakeLisp fakeLisp.o floatAndString.o fake.o specialFunction.o
