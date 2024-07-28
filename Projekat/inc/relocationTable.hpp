#ifndef _relocationTable_h_
#define _relocationTable_h_


#include <vector>
#include <algorithm> // To check if the entry already exists (both the symbol and location must match, checks for matching pair<>)
#include <fstream>

#include <iostream>
using namespace std;


class RelocationTable {
  vector<pair<string, uint>> entries;  // symbolName, offsetInSectionContentWhereToWriteItsValue

public:
  // Checks if the entry already exists (both the symbol and location must match)
  void addEntry(string symName, uint offsetInSection);

  // Printing:
  void printEntries(FILE* outputFile);

  // Binary file support:
  void bWrite(std::ofstream& file);
  void bRead(std::ifstream& file);

  // Getters and Setters:
  vector<pair<string, uint>> getEntries() { return entries; }
};


#endif