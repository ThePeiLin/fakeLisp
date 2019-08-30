functionAndForm.o: src/functionAndForm.c src/functionAndForm.h
	gcc -c src/functionAndForm.c
fake.o: src/fake.c src/fake.h
	gcc -c src/fake.c
numAndString.o:src/numAndString.c src/numAndString.h
	gcc -c src/numAndString.c
fakeLisp.o: src/fakeLisp.c
	gcc -c src/fakeLisp.c
fakeLisp: fakeLisp.o functionAndForm.o fake.o numAndString.o
	gcc -o fakeLisp fakeLisp.o numAndString.o fake.o functionAndForm.o
