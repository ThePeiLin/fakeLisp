objectOfFakeLisp=fakeLisp.o tool.o syntax.o compiler.o fakeVM.o VMtool.o ast.o reader.o
objectOfFakeLispc=fakeLispc.o tool.o syntax.o compiler.o fakeVM.o VMtool.o ast.o reader.o
ifeq ($(DEBUG),YES)
FLAG=-g -W
else
FLAG=-W
endif

ifeq ($(OS),WINDOWS)
LINK=-lpthread
BTK=btk.dll
else
LINK=-lpthread -ldl
BTK=btk.so
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
fakeLisp: $(objectOfFakeLisp) $(objectOfFakeLispc) $(BTK)
	gcc $(FLAG) -o fakeLisp $(objectOfFakeLisp) $(LINK)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)
fakeLispc: $(objectOfFakeLispc)
	gcc $(FLAG) -o fakeLispc $(objectOfFakeLispc) $(LINK)

btk.dll: src/btk.c src/VMtool.* src/tool.*
	gcc -fPIC -shared $(FLAG) -o btk.dll src/btk.c src/VMtool.c src/tool.c
btk.so: src/btk.c src/VMtool.* src/tool.*
	gcc -fPIC -shared $(FLAG) -o btk.so src/btk.c src/VMtool.c src/tool.c

.PHONY: clean
clean:
ifeq ($(OS),WINDOWS)
	del *.o fakeLisp.exe fakeLispc.exe btk.dll
else
	rm *.o fakeLisp fakeLispc btk.so
endif
