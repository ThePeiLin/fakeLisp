OS := $(shell uname -s)
IS_LINUX := $(shell echo $(OS)|grep -i linux)
objectOfFakeLisp=fakeLisp.o form.o tool.o preprocess.o syntax.o compiler.o fakeVM.o
objectOfFakeLispc=fakeLispc.o form.o tool.o preprocess.o syntax.o compiler.o
ifeq ($(DEBUG),YES)
FLAG=-g -W
else
FLAG=-W
endif

ifdef IS_LINUX
LINK=-lpthread -ldl
else
LINK=-lpthread
endif

form.o: src/form.* src/fakedef.h
	gcc $(FLAG) -c src/form.c
tool.o: src/tool.* src/fakedef.h
	gcc $(FLAG) -c src/tool.c
preprocess.o: src/preprocess.* src/fakedef.h
	gcc $(FLAG) -c src/preprocess.c
syntax.o: src/syntax.* src/fakedef.h
	gcc $(FLAG) -c src/syntax.c
fakeLisp.o: src/fakeLisp.* src/fakedef.h
	gcc $(FLAG) -c src/fakeLisp.c
compiler.o: src/compiler.* src/opcode.h src/fakedef.h
	gcc $(FLAG) -c src/compiler.c
fakeVM.o: src/fakeVM.* src/fakedef.h src/opcode.h
	gcc $(FLAG) -c src/fakeVM.c
fakeLispc.o: src/fakeLispc.* src/fakedef.h
	gcc $(FLAG) -c src/fakeLispc.c
fakeLisp: $(objectOfFakeLisp) $(objectOfFakeLispc)
	gcc $(FLAG) -o fakeLisp $(objectOfFakeLisp) $(LINK)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)
fakeLispc: $(objectOfFakeLispc)
	gcc -o fakeLispc $(objectOfFakeLispc)

.PHONY: clean
clean:
ifdef IS_LINUX
	rm *.o fakeLisp fakeLispc
else
	del *.o fakeLisp.exe fakeLispc.exe
endif
