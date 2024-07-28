#include "../inc/emulator.hpp"


enum GPR {
  r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, sp, pc
};
enum CSR {
  status, handler, cause
};

int gpr[16];  // Important to cast to (uint) if used as an address (when you are adding it to memory).
uint csr[3];

char* memory; // Starting address of host's 2^32 bytes of memory that will emulate the guest's memory.



// Write the linker's memory contents into the memory space used for emulation:
void initializeMemory(ifstream& in, char* memory) {
  uint len;
  in.read((char*)&len, sizeof(uint));
  //cout << "Len: " << len << endl;

  MemoryContent mc;
  for (uint i = 0; i < len; i++) {
    mc.bRead(in);
    //cout << "StartAddress: " << mc.startAddress << "  contentSize: " << mc.content.size() << endl; 

    for (uint j = 0; j < mc.getContent().size(); j++) {
      char* adr = (memory + mc.getStartAddress() + j);
      *adr = mc.getContent()[j];
    }
  }
}

// Allocate memory for emulation and initialize it:
int prepareMemory(int argc, char* argv[]) {
  /// Reserve space on disk with mmap that will represent the emulated 2^32 bytes of memory on the host machine:
  int prot = PROT_READ | PROT_WRITE;  // Enables reading and writing.
  int flags = MAP_PRIVATE | MAP_ANON; // Other processes won't see updates to the mapping. 
                                      //  Mapping isn't backed by any file. The content is initialized to 0.
                                      // MAP_ANON => fileDescriptor = -1, offset = 0. 
  memory = (char*)mmap(nullptr, (ulong)1 << 32, prot, flags, -1, 0);


  /// Open binary input file from linker:
  if (argc != 2)  {
    fprintf(stderr, "Emulator error: invalid command arguments given.\n   Expected './emulator filename'");
    return -1;
  }
  string prefix = "../tests/";
  string inputFileName = argv[1];    
  string fileName = prefix + inputFileName;   
  ifstream in(fileName); 
  if (in.fail()) {
    fprintf(stderr, "Emulator error: couldn't open a file with the given filename in the 'tests' directory.\n");
    return -1;
  }

  /// Read linker's MemoryContents and write them in the host's memory:
  initializeMemory(in, memory);
  in.close();

  return 0;
}


// Helper funs for the emulation process:
  // Used by INT.
void pushCSR(int id) {
  gpr[sp] -= 4;
  //cout << "   gpr[sp]: " << gpr[sp] << endl;
  *(uint*)(memory + (uint)gpr[sp]) = csr[id];
  //cout << "   mem[" << (uint)gpr[sp] << "]: " << *(uint*)(memory + (uint)gpr[sp]) << endl;
}
  // Used by INT, CALL, PUSH.
void pushGPR(int id) {
  gpr[sp] -= 4;
  //cout << "   gpr[sp]: " << gpr[sp] << endl;
  *(uint*)(memory + (uint)gpr[sp]) = gpr[id];
  //cout << "   mem[" << (uint)gpr[sp] << "]: " << *(uint*)(memory + (uint)gpr[sp]) << endl;
}
void popCSR(int id) {
  csr[id] = *(uint*)(memory + (uint)gpr[sp]);
  //cout << "   csr[" << id << "]:" << csr[id] << endl;
  gpr[sp] += 4;
  //cout << "   gpr[sp]: " << gpr[sp] << endl;
}
  // Used by RET, POP.
void popGPR(int id) {
  gpr[id] = *(uint*)(memory + (uint)gpr[sp]);
  //cout << "   gpr[" << id << "]:" << gpr[id] << endl;
  gpr[sp] += 4;
  //cout << "   gpr[sp]: " << gpr[sp] << endl;
}


// Execute emulation of machine instructions starting from the pc address:
int emulate() {
  uint curWord;
  uint opCode, mode, regA, regB, regC, disp;

  //cout << endl << endl << endl;

  while(true) {
    // Read 4 bytes from memory in the little endian format:
    //cout << uppercase << hex << "PC: " << (uint)gpr[pc] << "  INSTRUCTION: "; 
    curWord = *(uint*)(memory + (uint)gpr[pc]);  
    gpr[pc] += 4;

    // Filter out opCode, mode, regs and disp from the 4 bytes:
    opCode = (curWord >> 28);
    mode = (curWord >> 24) & 0xf;
    regA = (curWord >> 20) & 0xf;
    regB = (curWord >> 16) & 0xf;
    regC = (curWord >> 12) & 0xf;
    disp = curWord & 0xfff;


    // Recognize the current machine instruction and execute it:
    // HALT:
    if (curWord == 0) {
      //cout << "HALT" << endl;
      break;
    }
    // INT:
    else if (curWord == 0x10000000) {
      //cout << "INT" << endl;
      pushCSR(status);
      pushGPR(pc);
      csr[cause] = 4;
      //cout << "   cause: " << csr[cause] << endl;
      csr[status] = csr[status] & (~0x1);
      //cout << "   status: " << csr[status] << endl;
      gpr[pc] = csr[handler];
      //cout << "   pc: " << gpr[pc] << endl;
    }
    // CALL:
    else if (opCode == 0x2) {
      //cout << "CALL" << endl;
      pushGPR(pc);
      if (mode == 0) {
        gpr[pc] = gpr[regA] + gpr[regB] + disp;
        //cout << "   pc:" << gpr[pc] << endl;
      }
      else if (mode == 1) {
        gpr[pc] = *(uint*)(memory + (uint)gpr[regA] + (uint)gpr[regB] + disp);
        //cout << "   pc:" << gpr[pc] << endl;
      }
    }
    // JMP, BRANCH:
    else if (opCode == 0x3) {
      switch (mode) {
        case 0:
          //cout << "JMP" << endl;
          gpr[pc] = gpr[regA] + disp;
          //cout << "   pc: " << gpr[pc] << endl;
          break;
        case 1:
          //cout << "BEQ" << endl;
          if (gpr[regB] == gpr[regC]) {
            gpr[pc] = gpr[regA] + disp;
            //cout << "   pc: " << gpr[pc] << endl;
          }
          break;
        case 2:
          //cout << "BNE" << endl;
          if (gpr[regB] != gpr[regC]) {
            gpr[pc] = gpr[regA] + disp;
            //cout << "   pc: " << gpr[pc] << endl;
          }
          break;
        case 3:
          //cout << "BGT" << endl;
          if (gpr[regB] > gpr[regC]) {
            gpr[pc] = gpr[regA] + disp;
            //cout << "   pc: " << gpr[pc] << endl;
          }
          break;
        case 8:
          //cout << "JMP" << endl;
          gpr[pc] = *(uint*)(memory + (uint)gpr[regA] + disp);
          //cout << "   pc: " << gpr[pc] << endl;
          break;
        case 9:
          //cout << "BEQ" << endl;
          if (gpr[regB] == gpr[regC]) {
            gpr[pc] = *(uint*)(memory + (uint)gpr[regA] + disp);
            //cout << "   pc: " << gpr[pc] << endl;
          }
          break;
        case 0xA:
          //cout << "BNE" << endl;
          if (gpr[regB] != gpr[regC]) {
            gpr[pc] = *(uint*)(memory + (uint)gpr[regA] + disp);
            //cout << "   pc: " << gpr[pc] << endl;
          }
          break;
        case 0xB:
          //cout << "BGT" << endl;
          if (gpr[regB] > gpr[regC]) {
            gpr[pc] = *(uint*)(memory + (uint)gpr[regA] + disp);
            //cout << "   pc: " << gpr[pc] << endl;
          }
          break;
        default:
          fprintf(stderr, "Emulator Error: Unrecognized machine instruction.\n");
          return -1;
      }
    }
    // XCHG:
    else if (opCode == 0x4) {
      //cout << "XCHG" << endl;
      long tmp = gpr[regB];
      gpr[regB] = gpr[regC];
      //cout << "   gpr[" << regB << "]:" << gpr[regB] << endl;
      gpr[regC] = tmp;
      //cout << "   gpr[" << regC << "]:" << gpr[regC] << endl;
    }
    // ADD, SUB, MUL, DIV:
    else if (opCode == 0x5) {
      switch (mode) {
        case 0:
          //cout << "ADD" << endl;
          gpr[regA] = gpr[regB] + gpr[regC];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 1:
          //cout << "SUB" << endl;
          gpr[regA] = gpr[regB] - gpr[regC];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 2:
          //cout << "MUL" << endl;
          gpr[regA] = gpr[regB] * gpr[regC];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 3:
          //cout << "DIV" << endl;
          gpr[regA] = gpr[regB] / gpr[regC];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        default:
          fprintf(stderr, "Emulator Error: Unrecognized machine instruction.\n");
          return -1;
      }
    }
    // NOT, AND, OR, XOR:
    else if (opCode == 0x6) {
      switch (mode) {
        case 0:
          //cout << "NOT" << endl;
          gpr[regA] = ~gpr[regB];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 1:
          //cout << "AND" << endl;
          gpr[regA] = gpr[regB] & gpr[regC];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 2:
          //cout << "OR" << endl;
          gpr[regA] = gpr[regB] | gpr[regC];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 3:
          //cout << "XOR" << endl;
          gpr[regA] = gpr[regB] ^ gpr[regC];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        default:
          fprintf(stderr, "Emulator Error: Unrecognized machine instruction.\n");
          return -1;
      }
    }
    // SHL, SHR:
    else if (opCode == 0x7) {
      if (mode == 0) {
        //cout << "SHL" << endl;
        gpr[regA] = gpr[regB] << gpr[regC];
        //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
      }
      else if (mode == 1) {
        //cout << "SHR" << endl;
        gpr[regA] = gpr[regB] >> gpr[regC];
        //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
      }
    }
    // PUSH, ST: 
    else if (opCode == 0x8) {
      if (mode == 0) {
        //cout << "ST" << endl;
        *(uint*)(memory + (uint)gpr[regA] + (uint)gpr[regB] + disp) = gpr[regC];
        //cout << "   mem[" << (uint)gpr[regA] + (uint)gpr[regB] + disp << "]: " <<  *(uint*)(memory + (uint)gpr[regA] + (uint)gpr[regB] + disp) << endl;
      }
      else if (mode == 1) {
        //cout << "PUSH" << endl;
        pushGPR(regC);
      }
      else if (mode == 2) {
        //cout << "ST" << endl;
        uint tmp = *(uint*)(memory + (uint)gpr[regA] + (uint)gpr[regB] + disp);
        *(uint*)(memory + tmp) = gpr[regC];
        //cout << "   mem[" << tmp << "]: " <<  *(uint*)(memory + tmp) << endl;
      }
    }
    // LD, CSRWR, CSRRD, POP, IRET(first csrrd then pop), RET(pop):
    else if (opCode == 0x9) {
      switch (mode) {
        case 0:
          //cout << "CSRRD" << endl;
          gpr[regA] = csr[regB];
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 1:
          //cout << "LD" << endl;
          gpr[regA] = gpr[regB] + disp;
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 2:
          //cout << "LD" << endl;
          gpr[regA] = *(uint*)(memory + (uint)gpr[regB] + (uint)gpr[regC] + disp);
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          break;
        case 3:
          //cout << "POP" << endl;
          //popGPR(regA); (can't use this because IRET uses disp 8, not 4)

          //In case this mode is later needed for something other than just POP, this code will do both:
          gpr[regA] = *(uint*)(memory + (uint)gpr[regB]);
          //cout << "   gpr[" << regA << "]:" << gpr[regA] << endl;
          gpr[regB] = gpr[regB] + disp;
          //cout << "   gpr[" << regB << "]:" << gpr[regB] << endl;
          break;
        case 4:
          //cout << "CSRWR" << endl;
          csr[regA] = gpr[regB];
          //cout << "   csr[" << regA << "]:" << csr[regA] << endl;
          break;
        case 5:
          csr[regA] = csr[regB] | disp;
          //cout << "   csr[" << regA << "]:" << csr[regA] << endl;
          break;
        case 6:
          //cout << "LD" << endl;
          csr[regA] = *(uint*)(memory + (uint)gpr[regB] + (uint)gpr[regC] + disp);
          //cout << "   csr[" << regA << "]:" << csr[regA] << endl;
          break;
        case 7:
          //cout << "POP" << endl;
          //popCSR(regA);

          // In case this mode is later needed for something other than just POP, this code will do both:
          csr[regA] = *(uint*)(memory + (uint)gpr[regB]);
          //cout << "   csr[" << regA << "]:" << csr[regA] << endl;
          gpr[regB] = gpr[regB] + disp;
          //cout << "   gpr[" << regB << "]:" << gpr[regB] << endl;
          break;
        default:
          fprintf(stderr, "Emulator Error: Unrecognized machine instruction.\n");
          return -1;
      }
    }
    // UNRECOGNIZED:
    else {
      fprintf(stderr, "Emulator Error: Unrecognized machine instruction.\n");
      return -1;
    }
  
    // Always keep r0 at zero:
    gpr[r0] = 0;
  }

  return 0;
}


// Helper fun for printing results: (register value is given as a uint, this will use only the lower 32b)
string intToHex(uint num, int hexLen) {
  ostringstream ss;
  ss << uppercase << setfill('0') << std::setw(hexLen) << hex << num;
  return ss.str();
}

// Print the results of emulation in the specified format:
void printResults() {
  cout << "-----------------------------------------------------------------" << endl;
  cout << "Emulated processor executed halt instruction" << endl;
  cout << "Emulated processor state:" << endl;
  for (int i = 0; i < 16; ) {
    string reg = "r";
    if (i < 10) reg += (i+48);  // Ascii for '0' is 48. 
    else {
      reg += '1';
      reg += (i-10+48);
    }
    fprintf(stdout, "%3s=0x%s   ", reg.c_str(), intToHex(gpr[i], 8).c_str());
    if (++i % 4 == 0) fprintf(stdout, "\n");
  }
}




int main(int argc, char* argv[]) {

  // Allocate host's emulation memory and initialize it with linker's MemoryContents:
  if (prepareMemory(argc, argv) == -1) {
    munmap(memory, (ulong)1 << 32);
    return -1;
  }

  /// Initialize registers: (pc = 0x40000000)
  gpr[pc] = 0x40000000;

  /// Emulate: 
  if (emulate() == -1) {
    munmap(memory, (ulong)1 << 32);
    return -1;
  }

  /// Showcase results:
  printResults();

  /// Free memory:
  munmap(memory, (ulong)1 << 32);

  return 0;
}