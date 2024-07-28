#include "../inc/relocationTable.hpp"


// Checks if the entry already exists (both the symbol and location must match)
void RelocationTable::addEntry(string symName, uint offsetInSection) { 
  pair<string, uint> pair = make_pair(symName, offsetInSection);

  if (find(entries.begin(), entries.end(), pair) != entries.end()) return;
  entries.push_back(pair); 
}


// Printing:
void RelocationTable::printEntries(FILE* outputFile) {
  if (entries.size() > 0) {
    fprintf(outputFile, "|      Symbol      |     Location     |\n");
    for (uint i = 0; i < entries.size(); i++) {
      fprintf(outputFile, "%-20s %-20d\n", entries[i].first.c_str(), entries[i].second);
    }
  }
}


// Binary file support:
void RelocationTable::bWrite(std::ofstream& file) {
  uint len = entries.size();
  file.write((char*)&len, sizeof(uint));

  for (pair<string, uint> p : entries) {
    len = p.first.length();
    file.write((char*)&len, sizeof(uint));
    file.write((char*)p.first.c_str(), len);

    file.write((char*)&p.second, sizeof(uint));
  }
}
void RelocationTable::bRead(std::ifstream& file) {
  uint len;
  file.read((char*)&len, sizeof(uint));
  entries.resize(len);

  string key; uint value, keyLen;
  for (int i = 0; i < len; i++) {
    file.read((char*)&keyLen, sizeof(uint));

    char* tmp = new char[keyLen+1];
    tmp[keyLen] = '\0';
    file.read((char*)tmp, keyLen);
    key = tmp;
    delete tmp;

    file.read((char*)&value, sizeof(uint));

    entries[i] = make_pair(key, value);
  }
}