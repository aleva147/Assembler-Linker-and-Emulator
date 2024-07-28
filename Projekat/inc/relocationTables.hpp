#ifndef _relocationTables_h_
#define _relocationTables_h_


#include "relocationTable.hpp"
#include <unordered_map>
#include <fstream>

#include <iostream>
using namespace std;


class RelocationTables {
  unordered_map<string, RelocationTable> relocationTables;  // sectionName -> Secition's relocation table. 
  vector<string> sectionOrder; // For iterating through relocationTables in the chronological order of sections.

public:
  
  // Will create a new map key if it doesn't find an existing one.        
  void addOrUpdateTable(string sectName, RelocationTable& relTable);

  // Printing:
  void printRelocationTables(FILE* outputFile);

  // Binary file support:
  void bWrite(std::ofstream& file);
  void bRead(std::ifstream& file);


  /// ---- For Linker: ----

  // Get a section's RelocationTable
  RelocationTable getRelTable(string sectName);
  vector<string> getSectionOrder() { return sectionOrder; }
};


#endif 