OS := $(shell uname -s)
IS_LINUX := $(shell echo $(OS)|grep -i linux)
objectOfFakeLisp=fakeLisp.o form.o tool.o preprocess.o syntax.o compiler.o fakeVM.o
objectOfFakeLispc=fakeLispc.o form.o tool.o preprocess.o syntax.o compiler.o
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
fakeLispc.o: src/fakeLispc.* src/fakedef.h
	gcc -g -c src/fakeLispc.c
fakeLisp: $(objectOfFakeLisp)
	gcc -g -o fakeLisp $(objectOfFakeLisp)
fakeLispc: $(objectOfFakeLispc)
	gcc -g -o fakeLispc $(objectOfFakeLispc)

.PHONY: clean
clean:
ifdef IS_LINUX
	rm *.o fakeLisp
else
	del *.o fakeLisp.exe
endif
