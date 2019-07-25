specialFunction.o: src/specialFunction.c src/specialFunction.h
	gcc -c src/specialFunction.c
fake.o: src/fake.c src/fake.h
	gcc -c src/fake.c
floatAndString.o:src/floatAndString.c src/floatAndString.h
	gcc -c src/floatAndString.c
fakeLisp.o: src/fakeLisp.c
	gcc -c src/fakeLisp.c
fakeLisp: fakeLisp.o specialFunction.o fake.o floatAndString.o
	gcc -o fakeLisp fakeLisp.o floatAndString.o fake.o specialFunction.o
