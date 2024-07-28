#include "../inc/relocationTables.hpp"


// Will create a new map key if it doesn't find an existing one.        
void RelocationTables::addOrUpdateTable(string sectName, RelocationTable& relTable) {
  // If adding, add to chronological order:
  if (relocationTables.find(sectName) == relocationTables.end()) {
    sectionOrder.push_back(sectName);
  }

  // Add or update:
  relocationTables[sectName] = relTable;
}


// Printing:
void RelocationTables::printRelocationTables(FILE* outputFile) {
  for (string sectName : sectionOrder) {
    unordered_map<string, RelocationTable>::iterator it = relocationTables.find(sectName);
    if (it->second.getEntries().size() != 0) {
      fprintf(outputFile, "#relo.%s: \n", it->first.c_str());  
      it->second.printEntries(outputFile);
      fprintf(outputFile, "\n"); 
    }
  }
}


// Binary file support:
void RelocationTables::bWrite(std::ofstream& file) {
  uint len = relocationTables.size();
  file.write((char*)&len, sizeof(uint));

  for (string sectName : sectionOrder) {
    pair<string, RelocationTable> p = *relocationTables.find(sectName);

    len = p.first.length();
    file.write((char*)&len, sizeof(uint));
    file.write((char*)p.first.c_str(), len);

    p.second.bWrite(file);
  }
}
void RelocationTables::bRead(std::ifstream& file) {
  uint len;
  file.read((char*)&len, sizeof(uint));
  relocationTables.clear();

  string key; uint keyLen;
  for (uint i = 0; i < len; i++) {
    file.read((char*)&keyLen, sizeof(uint));

    char* tmp = new char[keyLen+1];
    tmp[keyLen] = '\0';
    file.read((char*)tmp, keyLen);
    key = tmp;
    delete tmp;

    RelocationTable rt = RelocationTable();
    rt.bRead(file);

    relocationTables.insert(make_pair(key, rt));
    sectionOrder.push_back(key);
  }
}



/// ---- For Linker: ----

// Get a section's RelocationTable:
RelocationTable RelocationTables::getRelTable(string sectName) {
  if (relocationTables.find(sectName) == relocationTables.end()) {
    fprintf(stderr, "Tried to access relocation table from RelocationTables which doesn't exist.\n");
    return RelocationTable();
  }
  return relocationTables.find(sectName)->second;
}