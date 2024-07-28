#include "../inc/symbolTable.hpp"


// Creates a new SymbolEntry in the SymbolTable:
void SymbolTable::createSymbolEntry(string name, string section, uint value, char type) {
  SymbolTableEntry entry(section, value, type);

  symbolTable.insert(make_pair(name, entry));
}


// Fetches a SymbolTableEntry:
SymbolTableEntry* SymbolTable::lookFor(string symName) {
  unordered_map<string, SymbolTableEntry>::iterator it = symbolTable.find(symName);
  if (it == symbolTable.end()) return nullptr;
  else return &it->second;
}

// Checks if there are undefined non-extern symbol after the first assembler cycle:
int SymbolTable::validateSymbolTable() {
  bool err = false;

  for (unordered_map<string, SymbolTableEntry>::iterator it = symbolTable.begin(); it != symbolTable.end(); it++) {
    SymbolTableEntry entry = it->second;
    if (entry.getSection() == "TBD") {
      fprintf(stderr, "\nERROR: usage of an undefined non-extern symbol: %s\n", it->first.c_str());
      err = true; // Doesn't immediately return -1 so that it can print all undefined symbols, not just the first one.
    }
  } 

  if (err) return -1;
  return 0;
}


// Funs for printing:
void SymbolTable::printSymbolTable(FILE* outputFile) {
  fprintf(outputFile, "#SymbolTable\n");
  fprintf(outputFile, "|      SymName      |       SecName      |  Value  | Type |\n");

  for (unordered_map<string, SymbolTableEntry>::iterator it = symbolTable.begin(); it != symbolTable.end(); it++) {
    fprintf(outputFile, "%-20s %-20s %-10d %c\n", 
    it->first.c_str(), it->second.getSection().c_str(), it->second.getValue(), it->second.getType());
  }
  fprintf(outputFile, "\n\n");
}


// Binary file support:
void SymbolTable::bWrite(std::ofstream& file) {
  uint mapLen = symbolTable.size();
  file.write((char*)&mapLen, sizeof(uint));

  for (unordered_map<string, SymbolTableEntry>::iterator it = symbolTable.begin(); it != symbolTable.end(); it++) {
    uint len = it->first.length();
    file.write((char*)&len, sizeof(uint));
    file.write((char*)it->first.c_str(), len);

    it->second.bWrite(file);
  }
}
void SymbolTable::bRead(std::ifstream& file) {
  uint mapLen;
  file.read((char*)&mapLen, sizeof(uint));

  for (uint i = 0; i < mapLen; i++) {
    uint len;
    file.read((char*)&len, sizeof(uint));

    char* tmp = new char[len+1];
    tmp[len] = '\0';
    file.read((char*)tmp, len);
    string symName = tmp;
    delete tmp;

    SymbolTableEntry ste;
    ste.bRead(file);

    symbolTable.insert(make_pair(symName, ste));
  }
}


/// ---- Funs needed for linker: ----

// Increases values of SymbolTableEntries that belong to the given section by section's base address:
void SymbolTable::updateLocalSymbolsValuesForSection(string sectName, uint sectBase) {
  for (unordered_map<string, SymbolTableEntry>::iterator i = symbolTable.begin(); i != symbolTable.end(); i++) {
    if (strcmp(i->second.getSection().c_str(), sectName.c_str()) == 0) {
      i->second.setValue(i->second.getValue() + sectBase);
    }
  }
}

// Move global symbols from this SymbolTable to linker's resulting SymbolTable:
int SymbolTable::exportGlobalSymbols(SymbolTable& resSymbolTable) {
  for (unordered_map<string, SymbolTableEntry>::iterator it = symbolTable.begin(); it != symbolTable.end(); it++) {
    if (it->second.getType() != 'g') continue;
    
    if (resSymbolTable.lookFor(it->first)) {
      fprintf(stderr, "Multiple global definitions of symbol %s.\n", it->first.c_str());
      return -1;
    }

    resSymbolTable.createSymbolEntry(it->first, it->second.getSection(), it->second.getValue(), it->second.getType());
  }

  return 0;
}

// Checks if every extern symbol in this SymbolTable is defined in the linker's resulting SymbolTable:
int SymbolTable::checkForUndefinedExtern(SymbolTable& resSymbolTable) {
  for (unordered_map<string, SymbolTableEntry>::iterator it = symbolTable.begin(); it != symbolTable.end(); it++) {
    if (it->second.getType() != 'e') continue;
  
    if (!resSymbolTable.lookFor(it->first)) {
      fprintf(stderr, "Usage of non-defined extern symbol %s\n", it->first.c_str());
      return -1;
    }
  }

  return 0;
}