objectOfFakeLisp=fakeLisp.o common.o syntax.o compiler.o fakeVM.o VMtool.o ast.o reader.o
objectOfFakeLispc=fakeLispc.o common.o syntax.o compiler.o fakeVM.o VMtool.o ast.o reader.o
ifeq ($(DEBUG),YES)
FLAG=-g -Wall
else
FLAG=-O3 -Wall
endif

ifeq ($(OS),WINDOWS)
LINK=-lpthread
BTK=btk.dll
else
LINK=-lpthread -ldl
BTK=btk.so
endif

fakeLisp: $(objectOfFakeLisp) $(objectOfFakeLispc) $(BTK)
	gcc $(FLAG) -o fakeLisp $(objectOfFakeLisp) $(LINK)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)
reader.o: src/reader.* src/fakedef.h common.o
	gcc $(FLAG) -c src/reader.c
ast.o: src/ast.* src/fakedef.h fakeVM.o
	gcc $(FLAG) -c src/ast.c
common.o: src/common.* src/fakedef.h src/opcode.h
	gcc $(FLAG) -c src/common.c
syntax.o: src/syntax.* src/fakedef.h common.o
	gcc $(FLAG) -c src/syntax.c
fakeLisp.o: src/fakeLisp.* src/fakedef.h
	gcc $(FLAG) -c src/fakeLisp.c
compiler.o: src/compiler.* src/opcode.h src/fakedef.h ast.o
	gcc $(FLAG) -c src/compiler.c
VMtool.o: src/VMtool.* src/fakedef.h common.o
	gcc $(FLAG) -c src/VMtool.c
fakeVM.o: src/fakeVM.* src/fakedef.h src/opcode.h VMtool.o reader.o
	gcc $(FLAG) -c src/fakeVM.c
fakeLispc.o: src/fakeLispc.* src/fakedef.h
	gcc $(FLAG) -c src/fakeLispc.c
fakeLispc: $(objectOfFakeLispc)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)

btk.dll: src/btk.c src/VMtool.* src/common.*
	gcc -fPIC -shared $(FLAG) -o btk.dll src/btk.c src/VMtool.c src/common.c
btk.so: src/btk.c VMtool.o common.o
	gcc -fPIC -shared $(FLAG) -o btk.so src/btk.c src/VMtool.c src/common.c

.PHONY: clean
clean:
ifeq ($(OS),WINDOWS)
	del *.o fakeLisp.exe fakeLispc.exe btk.dll
else
	rm *.o fakeLisp fakeLispc btk.so
endif
