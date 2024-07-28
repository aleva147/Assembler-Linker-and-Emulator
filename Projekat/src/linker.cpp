#include "../inc/linker.hpp"


vector<string> inputFileNames;
string outputFileName = "";

SymbolTable curSymbolTable;
SectionTable curSectionTable;
RelocationTables curRelocationTables;

vector<SymbolTable> asmSymbolTables;    // SymbolTables from all input files in their argv order.
vector<RelocationTables> asmRelTables;  // RelocationTables from all input files in their argv order.

uint locCounter = 0;
uint maxPlacedAddress = 0;

unordered_map<string, uint> placedSections; // Names of sections that were given a '-place' option.  (sectName -> placedAddress)
multiset<Section> processedSections; // Sections sorted by base value, but their relocations aren't yet taken care of.
vector<Section> finishedSections;    // ProcessedSections that have their SymbolTable and RelocationTable taken care of.

SymbolTable resSymbolTable; // SymbolTable that is the result of linker's process (contains only global symbols). 

vector<MemoryContent> memoryContents; // Merged contents of successive sections. For writing binary output file.

ulong totalContentSize;  // To check if the total content size is larger than 2^32 bytes (the host's emulation memory).
const ulong maxTotalSize = (ulong)1 << 32; 


// Remember inputFileNames, outputFileName, which sections were given the -place option:
int processCommandLineArguments(int argc, char* argv[]) {
  bool inputErr = false;
  bool hexOption = false;

  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      // Input file:
      if (argv[i][0] != '-' && string(argv[i]).length() > 2 
      && strcmp(string(argv[i]).substr(string(argv[i]).length() - 2).c_str(), ".o") == 0) {
        inputFileNames.push_back(argv[i]);
      }
      // Option '-o':
      else if (strcmp(argv[i], "-o") == 0) {
        // If argument '-o' isn't followed by a file name: (if '-o' is the last argument or if it's followed by another option)  
        if (i == argc - 1 || argv[i+1][0] == '-' || strcmp(outputFileName.c_str(), "") != 0) {
          inputErr = true;
          break;
        }
        outputFileName = argv[++i];
      }
      // Option '-place':
      else if (strcmp(string(argv[i]).substr(0, 7).c_str(), "-place=") == 0) {
        string s = argv[i];
        int delimiter = s.find("@");
        string sectName = s.substr(7, delimiter - 7);
        uint address = stoul(s.substr(delimiter + 1), 0, 16);

        if (placedSections.find(sectName) != placedSections.end()) {
          fprintf(stderr, "Error: Multiple -place options for the same sectionName.\n");
          return -1;
        }
        placedSections.insert(make_pair(sectName, address));

        if (address > maxPlacedAddress) maxPlacedAddress = address;
      }
      // Option '-hex':
      else if (strcmp(argv[i], "-hex") == 0) {
        hexOption = true;
      }
      else inputErr = true;
    }
  }
  else inputErr = true;


  if (inputErr || inputFileNames.size() == 0) {
    fprintf(stderr, "Error: Invalid command arguments given to linker.\n");
    return -1;
  }
  else if (!hexOption) {
    fprintf(stderr, "Error: No '-hex' option given.\n");
    return -1;
  }

  return 0;
}

// Reads a single assembler's binary output: (curSymbolTable, curSectionTable, curRelocationTable)
int readAssemblerFile(string fileName) {
  string prefix = "../tests/";
  ifstream in(prefix + fileName);
  if (in.fail()) {
    fprintf(stderr, "Linker Error: couldn't find a file with the given filename in the 'tests' directory.");
    return -1;
  }

  curSymbolTable = SymbolTable();
  curSymbolTable.bRead(in);
  curSectionTable = SectionTable();
  curSectionTable.bRead(in);
  //curSectionTable.printSectionTables(stdout);
  curRelocationTables = RelocationTables();
  curRelocationTables.bRead(in);

  in.close();

  return 0;
}

// Moves bases of all placed sections starting form the given address for the given amount of bytes:
void moveSectionsAfterAddress(uint address, uint forAmount) {
  multiset<Section> set;
  multiset<Section>::iterator iter;
  for (iter = processedSections.begin(); iter->getBase() < address; iter++) {
    set.insert(*iter);
  }
  for (; iter != processedSections.end(); iter++) {
    Section sec = *iter;
    sec.setBase(sec.getBase() + forAmount);
    set.insert(sec);
  }

  processedSections = set;
  // cout<< "New order:  " << endl;
  // for (iter = set.begin(); iter != set.end(); iter++) {
  //   cout<< "name: " << iter->getName() << " base: " << iter->getBase() << " content: " << iter->getContent().size() << endl;
  // }
  locCounter += forAmount;
}

// Puts sections from curSectionTable in the correct position among all other sections:
int placeSection() {

  for (string sectName : curSectionTable.getSectionOrder()) {
    /* getSections return a map reference, not a copy */
    unordered_map<string, Section>::iterator it = curSectionTable.getSections().find(sectName);
    Section curSec = it->second;

    // If a section has no content, skip it:
    if (curSec.getContent().size() == 0) {
      continue; 
    }
    // Check if the resulting content will be too large for host's emulation memory:
    totalContentSize += curSec.getContent().size();
    if (totalContentSize > maxTotalSize) {
      fprintf(stderr, "Linker Error: resulting content size won't fit into the host's emulation memory (it's larger than 2^32 bytes).\n");
      return -1;
    }
    
    unordered_map<std::string, uint>::iterator itPlac = placedSections.find(curSec.getName());  // secName -> address

    // There was NO -PLACE OPTION for this section:
    if (itPlac == placedSections.end()) {
      // Look for the last processed section with the same name.
      multiset<Section>::reverse_iterator itProc = processedSections.rbegin();
      uint address = -1;
      for (; itProc != processedSections.rend(); itProc++) {
        if (strcmp(curSec.getName().c_str(), itProc->getName().c_str()) == 0) {
          address = itProc->getBase() + itProc->getContent().size();
          break;
        }
      }
      //  If found, move all sections after it (they are certainly non -placed sections) and insert curSection.
      if (address != -1) {
        if (itProc != processedSections.rbegin()) moveSectionsAfterAddress(address, curSec.getContent().size());
        it->second.setBase(address);
      }
      //  If not found, insert curSection at the end (on locCounter).
      else {
        it->second.setBase(locCounter);
        locCounter += it->second.getContent().size();
      }
    }
    // There was a -PLACE OPTION for this section:
    else {
      uint initialAddress = itPlac->second;
      uint address = initialAddress;

      // Look for the last processed section with the same name, and increase the address accordingly:
      //  If there is no processed section with the same name, stop at the first section with <= base address.
      multiset<Section>::reverse_iterator itProc = processedSections.rbegin();
      for (; itProc != processedSections.rend(); itProc++) {
        if (strcmp(curSec.getName().c_str(), itProc->getName().c_str()) == 0) {
          address = itProc->getBase() + itProc->getContent().size();
          break;
        }
        if (itProc->getBase() <= address) {
          break;
        }
      }

      // Check for overlap with a further -place section:
      for (unordered_map<std::string, uint>::iterator itPlaced = placedSections.begin(); itPlaced != placedSections.end(); itPlaced++) {
        if (strcmp(itPlaced->first.c_str(), curSec.getName().c_str()) != 0) {
          if (itPlaced->second > initialAddress && address + curSec.getContent().size() > itPlaced->second) {
            fprintf(stderr, "Section %s will overlap with section %s.\n", curSec.getName().c_str(), itPlaced->first.c_str());
            return -1;
          } 
        }
      }

      // Before placing this section at its address, check for overlap with the previous and next section in the processed set.
      // Check for overlap with the previous only if there weren't processed section with the same name in the for loop above.
      //   If there is overlap with the previous, throw an error (previous can only be a section with a -place option).
      if (itProc != processedSections.rend() && itProc->getBase() + itProc->getContent().size() > address) {
        if (placedSections.find(itProc->getName()) != placedSections.end()) {
          cout << "Section " << curSec.getName() << " overlaps with section " << itProc->getName() << " which precedes it." << endl;
          return -1;
        }
        // If we first placed some sections without -place option and then the furthest one with -place, this happens:
        else {
          moveSectionsAfterAddress(address, curSec.getContent().size());
        }
      }
      // If there is overlap with the next, check if the initialAddress == maxPlacedAddress.
      //   If it is, move all the sections in processed set after current for current.content.size(). As well as locCounter. 
      //   If it isn't, throw an error (because the current sections has reached content of a latter section with a -place option).
      else if (itProc != processedSections.rbegin()) {
        itProc--;
        if (address + curSec.getContent().size() > itProc->getBase()) {
          if (initialAddress != maxPlacedAddress) {
            cout << "Section " << curSec.getName() << " overlaps with section " << itProc->getName() << " which starts after it." << endl;
            return -1;
          }
          else {
            moveSectionsAfterAddress(address, curSec.getContent().size());
          }
        }
      }
      // This is the furthest -place section and we haven't placed any non -place sections already:
      if (address + curSec.getContent().size() > locCounter) locCounter += curSec.getContent().size();

      // Set the base of the current section
      it->second.setBase(address);
    }

    // Add section to processed sections:
    processedSections.insert(it->second);      
    //cout << "Processed section:  secName: " << it->second.getName() << ", base: " << it->second.getBase() << ", contentSize: " << it->second.getContent().size() << " bytes." << endl;
  }
    
  return 0;
}


// Merge contents of successive sections:
int joinMemoryContents() {
  if (finishedSections.size() == 0) {
    fprintf(stderr, "Linker has no sections to output.\n");
    return -1;
  }

  uint startingAddress = finishedSections[0].getBase();
  vector<char> memContent = finishedSections[0].getContent();

  for (uint i = 1; i < finishedSections.size(); i++) {
    uint sectionBase = finishedSections[i].getBase();
    vector<char> sectionContent = finishedSections[i].getContent();

      // Join continuous sections' contents:
    if (sectionBase == startingAddress + memContent.size()) {
      uint wi = memContent.size();

      memContent.resize(memContent.size() + sectionContent.size());

      for (uint j = 0; j < sectionContent.size(); j++) {
        memContent[wi++] = sectionContent[j];
      }
    }
    else {
      // Create the joined memory content object:
      MemoryContent mc = MemoryContent(startingAddress, memContent);
      memoryContents.push_back(mc);

      // Start a new joined content:
      startingAddress = sectionBase;
      memContent.clear();
      memContent = sectionContent;
    }
  }

  // Create the last joined content object:
  MemoryContent mc = MemoryContent(startingAddress, memContent);
  memoryContents.push_back(mc);

  return 0;
}


// Open output file for printing:
FILE* openOutputFile() {
  string prefix = "../tests/";
  string fileName = prefix + outputFileName;
  fileName += ".txt";

  FILE* outputFile = fopen(fileName.c_str(), "w");
  if (!outputFile) {
    fprintf(stderr, "Error: Couldn't open the requested outputFile.\n");
  }

  return outputFile;
}

// Writes txt file in the specified format:
void hexPrint(FILE* outputFile) {
  uint addressCount = 0;
  uint stPos = 0;  // Indicates on what byte in line did we stop printing the previous section

  for (vector<Section>::iterator it = finishedSections.begin(); it != finishedSections.end(); it++) {
    // If the content of the next section should start in the same line where the previous section's ended:
    if (it->getBase() < addressCount + 8 && stPos != 0) {
      int byteInLine = it->getBase() % 8;
      // Print '--' for the unused bytes in the line between the end of the previous and the start of the next section:
      while (stPos < byteInLine) {
        fprintf(outputFile, "-- ");
        stPos++;
      }
    }
    // Otherwise, skip to the 8byte including the starting byte of the next section:
    else {
      if (stPos != 0) fprintf(outputFile, "\n");
      stPos = 0;  // this is necessary!

      int byteInLine = it->getBase() % 8;
      addressCount += (it->getBase() - byteInLine) - addressCount;

      // If byteInLine == 0, in the for loop below the address and the line will be printed normally.
      if (byteInLine != 0) {  
        ostringstream ss;
        ss << uppercase << setfill('0') << std::setw(4)<< hex << addressCount;
        string address = ss.str() + ": ";
        fprintf(outputFile, "%s", address.c_str());

        // Print '--' for the unused bytes at the start of the 8byte address until the beggining of the section:
        while (stPos < byteInLine) {
          fprintf(outputFile, "-- ");
          stPos++;
        }
      }
    }


    // Print the content of the section:
    vector<char> sectionContent = it->getContent();

    for (uint i = 0; i < sectionContent.size(); i++) {
      if (stPos == 0) {
        ostringstream ss;
        ss << uppercase << setfill('0') << std::setw(4) << hex << addressCount;
        string address = ss.str() + ": ";
        fprintf(outputFile, "%s", address.c_str());
      }

      char byte = sectionContent[i];
      int hex1 = ((byte >> 4) & 0xf);
      int hex2 = (byte & 0xf); 

      fprintf(outputFile, "%X%X ", hex1, hex2); // Todo: check.
      stPos += 1;

      if (stPos % 8 == 0) {
        stPos = 0;
        addressCount += 8;
        fprintf(outputFile, "\n");
      }
    }
  }
}

// Write txt file:
int writeTxtFile() {
  FILE* outputFile = openOutputFile();
  if (outputFile == nullptr) {
    fprintf(stderr, "Linker Error: couldn't write the requested txt file in the 'tests' directory.");
    return -1;
  }

  hexPrint(outputFile);

  fclose(outputFile);

  return 0;
}

// Write binary file:
int writeBinaryFile() {
  string prefix = "../tests/";
  ofstream out(prefix + outputFileName); 
  if (out.fail()) {
    fprintf(stderr, "Linker Error: couldn't write a binary output file in the 'tests' directory.\n");
    return -1;
  } 

  uint memContentsCount = memoryContents.size();
  out.write((char*)&memContentsCount, sizeof(uint));

  for (uint i = 0; i < memContentsCount; i++) {
    memoryContents[i].bWrite(out);
  }

  out.close();

  return 0;
}




int main(int argc, char* argv[]) {
  /// Process command line arguments:
  if (processCommandLineArguments(argc, argv) == -1) return -1;
  locCounter = maxPlacedAddress;


  for (int i = 0; i < inputFileNames.size(); i++) {
    /// Reading assembler's binary outputs:
    if (readAssemblerFile(inputFileNames[i]) == -1) return -1;

    // Remember to which SymbolTable and RelocationTable the sections belong to before you start sorting them all together:
    curSectionTable.setSectionsAsmFileId(i);
    // Save this asm's file SymbolTable and RelocationTable for later use:
    asmSymbolTables.push_back(curSymbolTable);
    asmRelTables.push_back(curRelocationTables);

    // Place assembler's sections in the resulting sections order: (in the processedSections multiset)
    if (placeSection() == -1) return -1;
  }


  // Print final order of sections for debugging:
  // cout << endl << "-----------------" << endl;
  // cout << "Printing final order of sections:" << endl;
  // for (multiset<Section>::iterator itProc = processedSections.begin(); itProc != processedSections.end(); itProc++) {
  //   fprintf(stdout, "SecName: %s, base: %X, contentSize: %ld bytes\n", itProc->getName().c_str(), itProc->getBase(), itProc->getContent().size());
  // }



  for (multiset<Section>::iterator itProc = processedSections.begin(); itProc != processedSections.end(); itProc++) {
    // Put section's names into the resulting SymbolTable: (only the first occurance of the section name and it's base)
    resSymbolTable.createSymbolEntry(itProc->getName(), itProc->getName(), itProc->getBase(), 'l');   // Will not overwrite existing entry with the same name.

    // Increase local symbols' values in intern SymbolTable for section's base value: 
    asmSymbolTables[itProc->getAsmFileId()].updateLocalSymbolsValuesForSection(itProc->getName(), itProc->getBase());
  }

  // Put all global syms from the intern SymbolTable into the resulting SymbolTable: (while checking for multiple definitions error)
  for (int i = 0; i < asmSymbolTables.size(); i++) {
    if (asmSymbolTables[i].exportGlobalSymbols(resSymbolTable) == -1) { // Passing resSymbolTable by reference.
      return -1;  
    }
  }



  for (multiset<Section>::iterator itProc = processedSections.begin(); itProc != processedSections.end(); itProc++) {
    int asmId = itProc->getAsmFileId();

    // Check for usage of extern non-defined symbols:
    if (asmSymbolTables[asmId].checkForUndefinedExtern(resSymbolTable) == -1) {
      return -1;
    }

    // Write symbol values in the section's pool: (on the locations specified with RelocationTables)
    Section s = *itProc;

    RelocationTable sectRelocationTable = asmRelTables[asmId].getRelTable(itProc->getName());
    if (sectRelocationTable.getEntries().size() != 0) {
      s.writeRelocations(sectRelocationTable, &asmSymbolTables[asmId], resSymbolTable);
    }

    finishedSections.push_back(s);
  }


  // cout << endl << "Resulting SymbolTable: " << endl;
  // resSymbolTable.printSymbolTable(stdout);
  // cout << "----------------" << endl;

  // cout << "Finished sections after linker: " << endl;
  // for (vector<Section>::iterator i = finishedSections.begin(); i != finishedSections.end(); i++) {
  //   i->printSection(stdout);
  // }

  /// Open and write the txt output file:
  writeTxtFile();

  /// Create joined memory contents for binary output:
  if (joinMemoryContents() == -1) return -1;

  // Write binary output:
  writeBinaryFile();


	return 0;
}
