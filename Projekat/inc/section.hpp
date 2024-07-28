#ifndef _section_h_
#define _section_h_

#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "relocationTable.hpp"
#include "symbolTable.hpp"
#include "symbolTableEntry.hpp"

#include <iostream>
using namespace std;


class Section {
  std::string name;
  uint base;
  std::vector<char> content;   // Bytes representing machineInstruction and pool afterwards.
  uint length;                 // Length of machineInstructions content (for total length of content use content.size()). 

  std::unordered_map<int, uint> poolEntriesLit;         // literalTabel: value -> location
  std::unordered_map<std::string, uint> poolEntriesSym; // literalTabel: symbol -> location

  // Needed to assign location values in literals table chronologically:
  uint orderId = 0;
  vector<pair<uint,int>> orderOfLits;    // <orderId,litValue> 
  vector<pair<uint,string>> orderOfSyms;  // <orderId,symName>

  int asmFileId;  // When linker has to deal with multiple section's with the same name from different asm files this will be used
                  //  to get the one we need. Linker will initialize this value as it reads an asm file.

public:
  // Constructors:
  Section() {}
  Section(std::string name) { this->name = name; }


  // Operator overload:
  bool operator< (const Section& sec) const { return base < sec.base; }


  // Only adds a lit/sym to literalTable if it's not already there.
  void addPoolEntry(int value);
  void addPoolEntry(std::string symbol);

  uint getPoolEntryLocation(int key);
  uint getPoolEntryLocation(string key);
  

  // Fill literalTables with values (locations of lits/syms) after first cycle (that's when the lenght of machine code is known)
  //  and initialize the size of content vector so that we can insert literals' values into the pool locations during second cycle.
  void finalizeLiteralsTable();


  // Add int to the section's content: (position is given in bytes)
  void addContent(uint position, int item);
  void addContentInstruction(uint position, string machineInstr);


  // Printing:
  void printPoolEntries();
  void printContent(FILE* outputFile);
  void printSection(FILE* outputFile);


  // Binary file support:
  void bWrite(std::ofstream& file);
  void bRead(std::ifstream& file);


  // Getters and Setters:
  std::string getName() const { return this->name; }
  void setName(std::string name) { this->name = name; }

  uint getBase() const { return this->base; }
  void setBase(uint base) { this->base = base; }

  std::vector<char> getContent() const { return this->content; }
  void setContent(std::vector<char> content) { this->content = content; }

  uint getLength() const { return this->length; }
  void setLength(uint length) { this->length = length; }

  std::unordered_map<int,uint> getPoolEntriesLit() const { return this->poolEntriesLit; }
  void setPoolEntriesLit(std::unordered_map<int,uint> poolEntriesLit) { this->poolEntriesLit = poolEntriesLit; }

  std::unordered_map<std::string,uint> getPoolEntriesSym() const { return this->poolEntriesSym; }
  void setPoolEntriesSym(std::unordered_map<std::string,uint> poolEntriesSym) { this->poolEntriesSym = poolEntriesSym; }


  /// ---- For Linker: ----

  // Section needs an attribute asmFileId because there can be multiple sections of the same name in different asm files. 
  //  Linker will initialize this values as it is processing the asm files.
  int getAsmFileId() const { return asmFileId; }
  void setAsmFileId(int id) { asmFileId = id; }

  // When linker makes an executable file, it will write final symbols' values into the section's pool:
  void writeRelocations(RelocationTable relTable, SymbolTable* sectionSymbolTable, SymbolTable linkerSymbolTable) {
    vector<pair<string, uint>> relEntries = relTable.getEntries();

    for (uint i = 0; i < relEntries.size(); i++) { 
      string symName = relEntries[i].first;
      int position = relEntries[i].second;

      SymbolTableEntry* localSymbol = sectionSymbolTable->lookFor(symName);

      // For local symbol, get its value from section's SymbolTable: 
      if (localSymbol && localSymbol->getType() != 'e') {
        addContent(position, localSymbol->getValue());
      } 
      // For extern symbol, get its value from linker's resulting SymbolTables:
      else {
        addContent(position, linkerSymbolTable.lookFor(symName)->getValue());
      }
    }
  }
};

#endif