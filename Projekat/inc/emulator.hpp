#ifndef _emulator_h_
#define _emulator_h_

#include "memoryContent.hpp"  // For writing MemoryContents from linker's output file into the host's memory addresses for emulation.
#include "sys/mman.h" // For mmap.
#include <sstream>    // For intToHex.
#include <iomanip>    // For intToHex.


#include <iostream>
using namespace std;


// Write the linker's memory contents into the memory space used for emulation:
void initializeMemory(ifstream& in, char* memory);
// Allocate memory for emulation and initialize it:
int prepareMemory(int argc, char* argv[]);

// Helper funs:
void pushCSR(int id);
void pushGPR(int id);
void popCSR(int id);
void popGPR(int id);
string intToHex(uint num, int hexLen);

// Emulate: (execute machine instruction starting from address memory+gpr[pc])
int emulate();

// Printing:
void printResults();


#endif