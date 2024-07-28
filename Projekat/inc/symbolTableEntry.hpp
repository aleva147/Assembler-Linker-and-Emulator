#ifndef _symbol_table_entry_h_
#define _symbol_table_entry_h_


#include <fstream>
#include <iostream>


class SymbolTableEntry {
  
public:
  std::string section;  // TBD - symbol used but wasn't previously declared as extern nor defined as a label.  EXT - declared extern.
  uint value;           // If its use appears before its definition (no matching name), the value will be set to -1 (meaning undefined).
  char type;            // g - global, l - local, e - extern.

  // Constructors:
  SymbolTableEntry() {}
  SymbolTableEntry(std::string section, uint value, char type) { this->section = section; this->value = value; this->type = type; }

  // Binary file support:
  void bWrite(std::ofstream& file);
  void bRead(std::ifstream& file);

  // Getters and Setters:
  std::string getSection() { return this->section; }
  void setSection(std::string section) { this->section = section; }

  uint getValue() { return this->value; }
  void setValue(uint value) { this->value = value; }

  char getType() { return this->type; }
  void setType(char type) { this->type = type; }
};


#endif