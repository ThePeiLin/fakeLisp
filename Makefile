objectOfFakeLisp=fakeLisp.o common.o syntax.o compiler.o fakeVM.o VMtool.o ast.o reader.o syscall.o fakeFFI.o
objectOfFakeLispc=fakeLispc.o common.o syntax.o compiler.o fakeVM.o VMtool.o ast.o reader.o syscall.o fakeFFI.o
exeFile=fakeLisp fakeLispc
ifeq ($(DEBUG),YES)
FLAG=-g -Wall
else
FLAG=-O3 -Wall
endif

ifeq ($(OS),WINDOWS)
exeFile=fakeLisp.exe fakeLispc.exe
DLLAPPENDFIX= .dll
LINK=-lpthread
BTK=btk.dll
else
DLLAPPENDFIX= .so
LINK=-lpthread -ldl -lm -lffi
BTK=btk.so
endif

CC=gcc

fakeLisp: $(objectOfFakeLisp) $(objectOfFakeLispc) $(BTK)
	gcc $(FLAG) -o fakeLisp $(objectOfFakeLisp) $(LINK)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)
syscall.o: src/syscall.* include/fakeLisp/fakedef.h VMtool.o common.o
	gcc $(FLAG) -c src/syscall.c -Iinclude/
reader.o: src/reader.* include/fakeLisp/fakedef.h common.o
	gcc $(FLAG) -c src/reader.c -Iinclude/
ast.o: src/ast.* include/fakeLisp/fakedef.h fakeVM.o
	gcc $(FLAG) -c src/ast.c -Iinclude/
common.o: src/common.* include/fakeLisp/fakedef.h include/fakeLisp/opcode.h
	gcc $(FLAG) -c src/common.c -Iinclude/
syntax.o: src/syntax.* include/fakeLisp/fakedef.h common.o
	gcc $(FLAG) -c src/syntax.c -Iinclude/
fakeLisp.o: src/fakeLisp.* include/fakeLisp/fakedef.h
	gcc $(FLAG) -c src/fakeLisp.c -Iinclude/
compiler.o: src/compiler.* include/fakeLisp/opcode.h include/fakeLisp/fakedef.h ast.o
	gcc $(FLAG) -c src/compiler.c -Iinclude/
VMtool.o: src/VMtool.* include/fakeLisp/fakedef.h common.o
	gcc $(FLAG) -c src/VMtool.c -Iinclude/
fakeVM.o: src/fakeVM.* include/fakeLisp/fakedef.h include/fakeLisp/opcode.h VMtool.o reader.o
	gcc $(FLAG) -c src/fakeVM.c -Iinclude/
fakeFFI.o: src/fakeFFI.* include/fakeLisp/fakedef.h src/fakeFFI.*
	gcc $(FLAG) -c src/fakeFFI.c -Iinclude/
fakeLispc.o: src/fakeLispc.* include/fakeLisp/fakedef.h
	gcc $(FLAG) -c src/fakeLispc.c -Iinclude/
fakeLispc: $(objectOfFakeLispc)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)

btk.dll: src/btk.c src/VMtool.* src/common.*
	gcc -fPIC -shared $(FLAG) -o btk.dll src/btk.c src/VMtool.c src/common.c -Iinclude/
btk.so: src/btk.c VMtool.o common.o
	gcc -fPIC -shared $(FLAG) -o btk.so src/btk.c src/VMtool.c src/common.c -Iinclude/

.PHONY: clean
clean:
	rm *.o $(exeFile) btk$(DLLAPPENDFIX)
