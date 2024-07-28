#ifndef _linker_h_
#define _linker_h_


#include <set>
#include "string.h"
#include "../inc/symbolTable.hpp"
#include "../inc/sectionTable.hpp"
#include "../inc/relocationTables.hpp"
#include "../inc/memoryContent.hpp"

#include <iostream>
using namespace std;


// Remember inputFileNames, outputFileName, which sections were given the -place option:
int processCommandLineArguments(int argc, char* argv[]);

// Reads a single assembler's binary output: (curSymbolTable, curSectionTable, curRelocationTable)
int readAssemblerFile(string fileName);

// Moves bases of all placed sections starting form the given address for the given amount of bytes:
void moveSectionsAfterAddress(uint address, uint forAmount);

// Puts sections from curSectionTable in the correct position among all other sections:
int placeSection();


// Merge contents of successive sections:
int joinMemoryContents();


// Open output file for printing:
FILE* openOutputFile();

// Writes txt file in the specified format:
void hexPrint(FILE* outputFile);

// Write txt file:
int writeTxtFile();

// Write binary file:
int writeBinaryFile();


#endif