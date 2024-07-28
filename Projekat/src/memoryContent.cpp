#include "../inc/memoryContent.hpp"


// Binary file support:
void MemoryContent::bWrite(ofstream& file) {
  file.write((char*)&startAddress, sizeof(uint));

  uint contentSize = content.size();
  file.write((char*)&contentSize, sizeof(uint));

  for (char c : content) {
    file.write((char*)&c, sizeof(char));
  }
}
void MemoryContent::bRead(ifstream& file) {
  file.read((char*)&startAddress, sizeof(uint));
  
  uint contentSize;
  file.read((char*)&contentSize, sizeof(uint));
  content.clear();
  content.resize(contentSize);

  char c;
  for (int i = 0; i < contentSize; i++) {
    file.read((char*)&c, sizeof(char));
    content[i] = c;
  }
}