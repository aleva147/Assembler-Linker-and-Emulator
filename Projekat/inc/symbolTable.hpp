#ifndef _symbol_table_h_
#define _symbol_table_h_


#include <unordered_map>
#include "string.h"
#include "symbolTableEntry.hpp"

#include <iostream>
using namespace std;


class SymbolTable {
  unordered_map<string, SymbolTableEntry> symbolTable;  // symName -> entry

public:
  // Constructors:
  SymbolTable() {}
  SymbolTable(unordered_map<string, SymbolTableEntry> symbolTable) { this->symbolTable = symbolTable; }


  // Creates a new SymbolEntry in the SymbolTable:
  void createSymbolEntry(string name, string section, uint value, char type);

  // Fetches a SymbolTableEntry:
  SymbolTableEntry* lookFor(string symName);

  // Checks if there are undefined non-extern symbol after the first assembler cycle:
  int validateSymbolTable();

  // Funs for printing:
  void printSymbolTable(FILE* outputFile);

  // Binary file support:
  void bWrite(std::ofstream& file);
  void bRead(std::ifstream& file);


  /// ---- Funs needed for linker: ----

  // Increases values of SymbolTableEntries that belong to the given section by section's base address:
  void updateLocalSymbolsValuesForSection(string sectName, uint sectBase);

  // Move global symbols from this SymbolTable to linker's resulting SymbolTable:
  int exportGlobalSymbols(SymbolTable& resSymbolTable);

  // Checks if every extern symbol in this SymbolTable is defined in the linker's resulting SymbolTable:
  int checkForUndefinedExtern(SymbolTable& resSymbolTable);
};


#endif