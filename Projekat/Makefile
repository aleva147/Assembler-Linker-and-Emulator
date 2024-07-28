emulator:	linker
	g++ ./src/memoryContent.cpp ./src/emulator.cpp -o emulator
	mv emulator ./misc

linker: asembler
	g++ ./src/symbolTableEntry.cpp ./src/symbolTable.cpp ./src/section.cpp ./src/sectionTable.cpp ./src/relocationTable.cpp ./src/relocationTables.cpp ./src/memoryContent.cpp ./src/linker.cpp -o linker
	mv linker ./misc

asembler:	lexer.c parser.tab.c 
	g++ ./src/parser.tab.c ./src/lexer.c ./src/parserHelper.cpp ./src/symbolTableEntry.cpp ./src/symbolTable.cpp ./src/section.cpp ./src/sectionTable.cpp ./src/relocationTable.cpp ./src/relocationTables.cpp ./src/asembler.cpp -lfl -o asembler
	mv asembler ./misc

lexer.c: parser.tab.c
	flex ./misc/lexer.l
	mv lexer.c ./src
	mv lexer.h ./inc

parser.tab.c: ./misc/parser.y
	bison -d ./misc/parser.y
	mv parser.tab.c ./src
	mv parser.tab.h ./inc

clean:
	rm  ./inc/lexer.h ./inc/parser.tab.h ./src/lexer.c ./src/parser.tab.c ./misc/asembler ./misc/linker ./misc/emulator