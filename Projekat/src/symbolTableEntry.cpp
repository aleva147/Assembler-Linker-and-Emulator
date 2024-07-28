#include "../inc/symbolTableEntry.hpp"


// Binary file support:
void SymbolTableEntry::bWrite(std::ofstream& file) {
  uint len = section.length();
  file.write((char*)&len, sizeof(uint));
  file.write((char*)section.c_str(), len);
  file.write((char*)&value, sizeof(uint));
  file.write((char*)&type, sizeof(char));
}
void SymbolTableEntry::bRead(std::ifstream& file) {
  uint len;
  file.read((char*)&len, sizeof(uint));

  char* tmp = new char[len+1];
  tmp[len] = '\0';
  file.read((char*)tmp, len);
  section = tmp;
  delete tmp;

  file.read((char*)&value, sizeof(uint));
  file.read((char*)&type, sizeof(char));
}
