#ifndef _memory_content_h_
#define _memory_content_h_


#include <vector>
#include <fstream>

#include <iostream>
using namespace std;


class MemoryContent {
  uint startAddress;
  vector<char> content;

public:
  // Constructors:
  MemoryContent() {};
  MemoryContent(uint startAddress, vector<char> content) {
    this->startAddress = startAddress;
    this->content = content;
  }

  // Binary file support:
  void bWrite(ofstream& file);
  void bRead(ifstream& file);

  // Getters and Setters:
  vector<char> getContent() { return content; }   // Currently no need to return by reference.
  uint getStartAddress() { return startAddress; } // Currently no need to return by reference.
};


#endif