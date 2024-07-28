#include "../inc/sectionTable.hpp"


// Fetches a section from the section table:
Section* SectionTable::lookFor(string sectName) {
  unordered_map<string, Section>::iterator it = sectionTable.find(sectName);

  if (it == sectionTable.end()) return nullptr;
  else return &it->second;
}

// Adds a section to the section table:
void SectionTable::addSection(string sectName, Section section) {
  sectionTable.insert(make_pair(sectName, section));
  sectionOrder.push_back(sectName);
}

// Updates a section in the map by swapping it with the given updated object:
void SectionTable::updateSection(Section section) {
  sectionTable.at(section.getName()) = section;
}


// Fill each literalTables with values (locations of lits/syms) after first cycle (that's when the lenght of machine code is known)
//  and initialize the size of content vector so that we can insert literals' values into the pool locations during second cycle.
void SectionTable::finalizeLiteralsTables() {
  for(unordered_map<string, Section>::iterator it = sectionTable.begin(); it != sectionTable.end(); it++) {
    it->second.finalizeLiteralsTable();
  }
}


// Printing:
void SectionTable::printSectionTables(FILE* outputFile) {
  for (string sectName : sectionOrder) {
    unordered_map<string, Section>::iterator it = sectionTable.find(sectName);
    if (it->second.getContent().size() != 0) {
      it->second.printSection(outputFile);  
    }
  }
  fprintf(outputFile, "\n\n"); 
}


// Binary file support:
void SectionTable::bWrite(std::ofstream& file) {
  uint mapLen = sectionTable.size();
  file.write((char*)&mapLen, sizeof(uint));

  for (string sectName : sectionOrder) {
    unordered_map<string, Section>::iterator it = sectionTable.find(sectName);
    uint len = it->first.length();
    file.write((char*)&len, sizeof(uint));
    file.write((char*)it->first.c_str(), len);

    it->second.bWrite(file);
  }
}
void SectionTable::bRead(std::ifstream& file) {
  uint mapLen;
  file.read((char*)&mapLen, sizeof(uint));

  for (int i = 0; i < mapLen; i++) {
    uint len;
    file.read((char*)&len, sizeof(uint));

    char* tmp = new char[len+1];
    tmp[len] = '\0';
    file.read((char*)tmp, len);
    string sectName = tmp;
    delete tmp;

    Section section;
    section.bRead(file);

    sectionTable.insert(make_pair(sectName, section));
    sectionOrder.push_back(sectName);
  }
}