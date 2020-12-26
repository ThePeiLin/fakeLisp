objectOfFakeLisp=fakeLisp.o tool.o syntax.o compiler.o fakeVM.o VMtool.o ast.o
objectOfFakeLispc=fakeLispc.o tool.o syntax.o compiler.o fakeVM.o VMtool.o ast.o
ifeq ($(DEBUG),YES)
FLAG=-g -W
else
FLAG=-W
endif

ifeq ($(OS),WINDOWS)
LINK=-lpthread
else
LINK=-lpthread -ldl
endif


reader.o: src/reader.* src/fakedef.h src/tool.*
	gcc $(FLAG) -c src/reader.c
ast.o: src/ast.* src/fakedef.h src/tool.* src/VMtool.*
	gcc $(FLAG) -c src/ast.c
tool.o: src/tool.* src/fakedef.h src/opcode.h
	gcc $(FLAG) -c src/tool.c
syntax.o: src/syntax.* src/fakedef.h
	gcc $(FLAG) -c src/syntax.c
fakeLisp.o: src/fakeLisp.* src/fakedef.h
	gcc $(FLAG) -c src/fakeLisp.c
compiler.o: src/compiler.* src/opcode.h src/fakedef.h src/ast.h
	gcc $(FLAG) -c src/compiler.c
VMtool.o: src/VMtool.* src/fakedef.h src/tool.*
	gcc $(FLAG) -c src/VMtool.c
fakeVM.o: src/fakeVM.* src/fakedef.h src/opcode.h src/VMtool.*
	gcc $(FLAG) -c src/fakeVM.c src/VMtool.c
fakeLispc.o: src/fakeLispc.* src/fakedef.h
	gcc $(FLAG) -c src/fakeLispc.c
fakeLisp: $(objectOfFakeLisp) $(objectOfFakeLispc)
	gcc $(FLAG) -o fakeLisp $(objectOfFakeLisp) $(LINK)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)
fakeLispc: $(objectOfFakeLispc)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)
btk: src/tool.* src/VMtool.*
ifeq ($(OS),WINDOWS)
	gcc -fPIC -shared $(FLAG) -o btk.dll src/btk.c src/VMtool.c src/tool.c
else
	gcc -fPIC -shared $(FLAG) -o btk.so src/btk.c src/VMtool.c src/tool.c
endif

.PHONY: clean
clean:
ifeq ($(OS),WINDOWS)
	del *.o fakeLisp.exe fakeLispc.exe
else
	rm *.o fakeLisp fakeLispc
endif
