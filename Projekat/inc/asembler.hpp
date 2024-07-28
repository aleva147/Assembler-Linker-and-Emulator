#ifndef _asembler_h_
#define _asembler_h_


#include <unordered_map>
#include <vector>

#include "string.h"
#include "parserHelper.hpp"
#include "symbolTable.hpp"
#include "sectionTable.hpp"
#include "relocationTables.hpp"

#include <iostream>
using namespace std;


// Funs used in assembler's first cycle:
int processCommandLabels(lab* labels);
int processCommandSymbol(arg* a, command* cmnd);
int processCommandLiteral(arg* a, command* cmnd);

void updateLocCounter(command* cmnd);

int firstCycle();


// Funs used in assembler's second cycle:
bool isGPR(char* reg);
bool isCSR(char* reg);
bool isReg(char* reg);
char getRegId(char* reg);
string intToHex(uint num, uint hexLen);

int secondCycle();


#endif