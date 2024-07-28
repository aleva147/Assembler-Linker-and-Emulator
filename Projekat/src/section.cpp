#include "../inc/section.hpp"


// Only adds a lit/sym to literalTable if it's not already there.
void Section::addPoolEntry(int value) {
  if (poolEntriesLit.find(value) == poolEntriesLit.end()) {
    poolEntriesLit.insert(std::make_pair(value, -1)); // Location to be decided after the first assembler cycle.
    orderOfLits.push_back(make_pair(orderId++,value));
  } 
}
void Section::addPoolEntry(std::string symbol) {
  if (poolEntriesSym.find(symbol) == poolEntriesSym.end()) {
    poolEntriesSym.insert(std::make_pair(symbol, -1)); // Location to be decided after the first assembler cycle.
    orderOfSyms.push_back(make_pair(orderId++,symbol));
  } 
}

uint Section::getPoolEntryLocation(int key) { return poolEntriesLit.find(key)->second; }
uint Section::getPoolEntryLocation(string key) { return poolEntriesSym.find(key)->second; }


// Fill literalTables with values (locations of lits/syms) after first cycle (that's when the lenght of machine code is known)
//  and initialize the size of content vector so that we can insert literals' values into the pool locations during second cycle.
void Section::finalizeLiteralsTable() {
  uint id = this->length;

  // Assign locations: (in chronological order of use in machine instructions)
  uint iLit = 0, iSym = 0;
  for (uint i = 0; i < poolEntriesLit.size() + poolEntriesSym.size(); i++) {
    if (iSym == poolEntriesSym.size() || (iLit < poolEntriesLit.size() && orderOfLits[iLit].first < orderOfSyms[iSym].first)) {
      poolEntriesLit.find(orderOfLits[iLit].second)->second = id;
      iLit++;
      id += 4;
    }
    else {
      poolEntriesSym.find(orderOfSyms[iSym].second)->second = id;
      iSym++;
      id += 4;
    }
  }

  // At this moment, id is the number of bytes needed for this section's machine instructions and pool. 
  content = vector<char>(id, 0);

  // Initialize pool with literal values because we know them already:
  for (std::unordered_map<int, uint>::iterator it = this->poolEntriesLit.begin(); it != this->poolEntriesLit.end(); it++) {
    addContent(it->second, it->first);
  }
}


// Add int to the section's content: (position is given in bytes)
void Section::addContent(uint position, int item) {
  if (item == 0) return;

  // Write bytes of the given int into content in the little endian format:
  for (int i = 0; i < 4; i++) {
    char byte = ((item >> (8*i)) & 0xff);
    content[position + i] = byte;
  }
}
void Section::addContentInstruction(uint position, string machineInstr) {
  int rdId = 6, hex1, hex2;

  // Write bytes of the given machineInstr into content in the little endian format:
  for (int i = 0; i < 4; i++) {
    if (machineInstr[rdId] >= 'A') hex1 = machineInstr[rdId] - 'A' + 10;
    else hex1 = machineInstr[rdId] - '0';
    if (machineInstr[rdId+1] >= 'A') hex2 = machineInstr[rdId+1] - 'A' + 10;
    else hex2 = machineInstr[rdId+1] - '0';

    char byte = ((hex1 << 4) | hex2);

    content[position + i] = byte;
    rdId -= 2;
  }
}


void Section::printPoolEntries(){
  std::unordered_map<int, uint>::iterator itLit;
  std::unordered_map<std::string, uint>::iterator itSym;

  cout << endl << "Pool entries for section " << this->name << ":" << endl;
  cout << "|   Value   |   Location   |" << endl;
  // Todo: use chronological vector.
  for (itLit = poolEntriesLit.begin(); itLit != poolEntriesLit.end(); itLit++) {
    std::cout << itLit->first << "   " << itLit->second << std::endl;
  }
  for (itSym = poolEntriesSym.begin(); itSym != poolEntriesSym.end(); itSym++) {
    std::cout << itSym->first << "   " << itSym->second << std::endl;
  }
  cout << endl << endl;
}
void Section::printContent(FILE* outputFile) {
  if (content.size() == 0) return;

  //cout << "Content of section: " << name << endl;
  for (uint i = 0; i < content.size(); ) {
    char byte = content[i];

    int hex1 = ((byte >> 4) & 0xf);
    int hex2 = (byte & 0xf); 

    fprintf(outputFile, "%X%X ", hex1, hex2);

    i++;
    if (i % 8 == 0) fprintf(outputFile, "\n");
    else if (i % 4 == 0) fprintf(outputFile, " ");
  }
}
void Section::printSection(FILE* outputFile) {
  fprintf(outputFile, "#%s: \n", name.c_str());

  //printPoolEntries();
  printContent(outputFile);
  fprintf(outputFile, "\n");
}


// Binary file support:
void Section::bWrite(std::ofstream& file) {
  uint len = name.length();
  file.write((char*)&len, sizeof(uint));
  file.write((char*)name.c_str(), len);

  file.write((char*)&base, sizeof(uint));

  len = content.size();
  file.write((char*)&len, sizeof(uint));
  for (char c : content) {
    file.write((char*)&c, sizeof(char));
  }

  file.write((char*)&length, sizeof(uint));

  uint mapLen = poolEntriesLit.size();
  file.write((char*)&mapLen, sizeof(uint));
  
  for (unordered_map<int, uint>::iterator it = poolEntriesLit.begin(); it != poolEntriesLit.end(); it++) {
    file.write((char*)&it->first, sizeof(int));
    file.write((char*)&it->second, sizeof(uint));
  }

  mapLen = poolEntriesSym.size();
  file.write((char*)&mapLen, sizeof(uint));
  for (unordered_map<string, uint>::iterator it = poolEntriesSym.begin(); it != poolEntriesSym.end(); it++) {
    len = it->first.length();
    file.write((char*)&len, sizeof(int));
    file.write((char*)it->first.c_str(), len);

    file.write((char*)&it->second, sizeof(uint));
  }
}
void Section::bRead(std::ifstream& file) {
  uint len;
  file.read((char*)&len, sizeof(uint));

  char* tmp = new char[len+1];
  tmp[len] = '\0';
  file.read((char*)tmp, len);
  name = tmp;
  delete tmp;

  file.read((char*)&base, sizeof(uint));

  file.read((char*)&len, sizeof(uint));
  content.resize(len);
  for (int i = 0; i < len; i++) {
    file.read((char*)&content[i], sizeof(char));
  }

  file.read((char*)&length, sizeof(uint));

  uint mapLen;
  file.read((char*)&mapLen, sizeof(uint));
  for (uint i = 0; i < mapLen; i++) {
    int key; uint value;
    file.read((char*)&key, sizeof(int));
    file.read((char*)&value, sizeof(uint));
    poolEntriesLit.insert(make_pair(key, value));
  }

  file.read((char*)&mapLen, sizeof(uint));
  for (uint i = 0; i < mapLen; i++) {
    uint len;
    file.read((char*)&len, sizeof(uint));

    char* tmp = new char[len+1];
    tmp[len] = '\0';
    file.read((char*)tmp, len);
    string symName = tmp;
    delete tmp;

    uint value;
    file.read((char*)&value, sizeof(uint));

    poolEntriesSym.insert(make_pair(symName, value));
  }
}