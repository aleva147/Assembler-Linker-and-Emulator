#ifndef _section_table_h_
#define _section_table_h_


#include <unordered_map>
#include <vector>
#include "section.hpp"

#include <iostream>
using namespace std;


class SectionTable {
  unordered_map<string, Section> sectionTable;
  vector<string> sectionOrder;  // For iterating through sectionTable in the chronological order of sections.

public:

  // Fetches a section from the section table:
  Section* lookFor(string sectName);

  // Adds a section to the section table:
  void addSection(string sectName, Section section);

  // Updates a section in the map by swapping it with the given updated object:
  void updateSection(Section section);
  

  // Fill each literalTables with values (locations of lits/syms) after first cycle (that's when the lenght of machine code is known)
  //  and initialize the size of content vector so that we can insert literals' values into the pool locations during second cycle.
  void finalizeLiteralsTables();


  // Printing:
  void printSectionTables(FILE* outputFile);


  // Binary file support:
  void bWrite(std::ofstream& file);
  void bRead(std::ifstream& file);


  // Getters and Setters:
  unordered_map<string, Section>& getSections() { return sectionTable; }
  vector<string> getSectionOrder() { return sectionOrder; }


  // ---- For Linker: ----
  void setSectionsAsmFileId(int id) { 
    for (unordered_map<string, Section>::iterator i = sectionTable.begin(); i != sectionTable.end(); i++) {
      i->second.setAsmFileId(id);
    } 
  }
};


#endif