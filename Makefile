form.o: src/form/*
	gcc -g -c src/form/form.c
fake.o: src/fake/*
	gcc -g -c src/fake/fake.c
tool.o: src/tool/*
	gcc -g -c src/tool/tool.c
pretreatment.o: src/pretreatment/*
	gcc -g -c src/pretreatment/pretreatment.c
fakeLisp.o: src/fakeLisp/*
	gcc -g -c src/fakeLisp/fakeLisp.c
fakeLisp: fakeLisp.o form.o fake.o tool.o pretreatment.o
	gcc -g -o fakeLisp fakeLisp.o tool.o fake.o form.o pretreatment.o
