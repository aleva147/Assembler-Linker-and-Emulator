#include "../inc/asembler.hpp"
#include "../inc/parser.tab.h"
#include "../inc/lexer.h"


const uint maxLit = 1 << 12; 
string outputFileName;

SymbolTable symbolTable = SymbolTable();
SectionTable sectionTable = SectionTable();
RelocationTables relocationTables = RelocationTables();

uint locCounter = 0;
Section curSection("UND");
RelocationTable curRelTable;


// Add labels to the SymbolTable: (or update value of symbol used before def, or throw multiple definition exception)
int processCommandLabels(lab* labels) {
  lab* labs = labels;

  while (labs) {
    SymbolTableEntry* ste = symbolTable.lookFor(labs->name);

    if (ste == nullptr) {
      symbolTable.createSymbolEntry(labs->name, curSection.getName(), locCounter, 'l');
    }
    else if (ste->getValue() != -1) {
      fprintf(stderr, "Multiple definitions of label: %s\n", labs->name);
      return -1;
    }
    else {
      ste->setValue(locCounter);
      ste->setSection(curSection.getName());
    }
    
    labs = labs->next;
  }

  return 0;
}

// Add used symbol to the SymbolTable (or do the necessary update to its SymbolTable entry).
//  If symbol is a section, it will be added to sectionTable as well. 
//  If this is the first use of this symbol in THIS section, it will be added to the section's pool as well.
int processCommandSymbol(arg* a, command* cmnd) {
  if (a->type == 3) {
    fprintf(stderr, "ERROR: Can't use syntax [reg + symbol] unless the symbol is defined using '.equ' and shorter than 12b.");
    return -1;
  }

  char* sym = a->sym;
  SymbolTableEntry* ste = symbolTable.lookFor(sym);

  // Special directives:
  if (cmnd->isDirective && strcmp(cmnd->name, "section") == 0) {
    // Add the old section to Section Table:
    curSection.setLength(locCounter);
    sectionTable.addSection(curSection.getName(), curSection);

    // Start a new section:
    locCounter = 0;
    curSection = Section(sym);
    symbolTable.createSymbolEntry(sym, curSection.getName(), 0, 'l');
  }
  if (cmnd->isDirective && strcmp(cmnd->name, "global") == 0) {
    if (ste != nullptr) {
      ste->setType('g');
    }
    else {
      symbolTable.createSymbolEntry(sym, "TBD", -1, 'g');
    }
  }
  else if (cmnd->isDirective && strcmp(cmnd->name, "extern") == 0) {
    if (ste != nullptr) {
      ste->setType('e');
    }
    else {
      symbolTable.createSymbolEntry(sym, "EXT", -1, 'e');
    }
  }
  else {
    // If this is the first occurance of the symbol, add it to the symbol table.
    if (ste == nullptr) {
      symbolTable.createSymbolEntry(sym, "TBD", -1, 'l');
    }

    // If this is the first occurence of the symbol in THIS section, add it to the section's pool.
    if (!cmnd->isDirective 
    && curSection.getPoolEntriesSym().find(sym) == curSection.getPoolEntriesSym().end()) {
      // cout << "Adding " << sym << " to pool of section " << curSection.getName() << endl;
      curSection.addPoolEntry(sym);
    }
  }

  return 0;
}

// Directives will write literal's value in place during the second cycle no matter the literal's length (during parsing we limited the lit to 32b, aka sizeof(int)).
// Instructions will do the same if the literal is shorter than 12b, otherwise they will add it to the section's pool here.  
int processCommandLiteral(arg* a, command* cmnd) {
  if (!cmnd->isDirective && ((uint)a->lit >= maxLit)) {
    if (a->type == 2) {
      fprintf(stderr, "ERROR: Can't use literals longer than 12b with syntax: [reg + literal]");
      return -1;
    }
    else {
      curSection.addPoolEntry(a->lit);
    }
  }

  return 0;
}


// Updates locCounter throughout assembler's first cycle.
void updateLocCounter(command* cmnd) {
  // Directives .skip and .word are the only ones that allocate space:
  if (cmnd->isDirective) {
    // Skip allocates given number of bytes:
    if (strcmp(cmnd->name, "skip") == 0 && cmnd->args) {
      locCounter += (uint)cmnd->args->lit;
    } 
    // Word allocates 4 bytes per argument:
    else if (strcmp(cmnd->name, "word") == 0) {
      arg* a = cmnd->args;
      while (a) {
        locCounter += 4;
        a = a->next;
      }
    }
  }
  // All machine instructions allocate 4 bytes: (but some asembler instructions will consist of more than one machine instructions)
  else {
    if (strcmp(cmnd->name, "iret") == 0) locCounter += 4;
    else if (strcmp(cmnd->name, "ld") == 0 && ((cmnd->args->type == 6 || cmnd->args->type == 7))) locCounter += 4;
    locCounter += 4;
  }
}


// Assembler's first cycle: (filling up SymbolTable, SectionTables (and their poolEntries) whilst increasing locationCounter)
int firstCycle() {
  // Iterate through parsed commands:
  command* cmnd = commandsHead;
  while (cmnd) {
    if (processCommandLabels(cmnd->labs) == -1) return -1;

    arg* a = cmnd->args;
    while (a) {
      if (a->sym) {
        if (processCommandSymbol(a, cmnd) == -1) return -1;
      }
      else if (a->lit) {
        if (processCommandLiteral(a, cmnd) == -1) return -1;
      }

      a = a->next; 
    }

    // Update locCounter: (if the command generates content)
    updateLocCounter(cmnd);

    cmnd = cmnd->next;
  }

  // Add the last section to Section Table:
  curSection.setLength(locCounter);
  sectionTable.addSection(curSection.getName(), curSection);

  return 0;
}


// Helper funs used in second cycle:
bool isGPR(char* reg) {
  int len = strlen(reg);

  if (len == 2) {
    if (strcmp(reg, "sp") == 0 || strcmp(reg, "pc") == 0) return true;
    if (reg[0] == 'r' && reg[1] >= '0' && reg[1] <= '9') return true;
  }
  else if (len == 3) {
    if (reg[0] == 'r' && reg[1] == '1' && reg[2] >= '0' && reg[2] <= '5') return true;
  }

  return false;
}

bool isCSR(char* reg) {
  if (strcmp(reg, "status") == 0 || strcmp(reg, "handler") == 0 || strcmp(reg, "cause") == 0) return true;
  return false;
}

bool isReg(char* reg) {
  return isGPR(reg) || isCSR(reg);
}

char getRegId(char* reg) {
  int len = strlen(reg);

  if (len == 2) {
    if (strcmp(reg, "sp") == 0) return 'E';
    else if (strcmp(reg, "pc") == 0) return 'F';
    else return reg[1];
  }
  else if (len == 3) {
    if (reg[2] == '0') return 'A';
    else if (reg[2] == '1') return 'B';
    else if (reg[2] == '2') return 'C';
    else if (reg[2] == '3') return 'D';
    else if (reg[2] == '4') return 'E';
    else if (reg[2] == '5') return 'F';
  }
  else if (strcmp(reg, "status") == 0) return '0';
  else if (strcmp(reg, "handler") == 0) return '1';
  else if (strcmp(reg, "cause") == 0) return '2';

  return '?';
}

string intToHex(uint num, uint hexLen) {
  ostringstream ss;
  ss << uppercase << setfill('0') << std::setw(hexLen) << hex << num;
  return ss.str();
}


// Writes machine instructions into the section's content (and checks for correct syntax). 
//  Fills the section's relocation table when needed.
int secondCycle() {
  locCounter = 0;
  curSection = *sectionTable.lookFor("UND");

  command* cmnd = commandsHead;
  while (cmnd) {
    // For directives, parser made sure that there can't be any %,[,] and other unexpected syntaxes. Only lit or symName.
    //  But not every directive allows both lit and symNames as its args, nor does every directive allow optional number of args.
    if (cmnd->isDirective) {
      // SKIP:
      if (strcmp(cmnd->name, "skip") == 0) {
        // Check for irregular formats:
        if (!cmnd->args || cmnd->args->next || cmnd->args->sym) {
          fprintf(stderr, "\nERROR: directive .skip expects a single literal as its argument.");
          return -1;
        }

        // No need to write zeros in the section content, it's already initialized with zeros.
        locCounter += (uint)cmnd->args->lit;
      }

      // WORD:
      else if (strcmp(cmnd->name, "word") == 0) {
        if (!cmnd->args) {
          fprintf(stderr, "\nERROR: directive .word expects at least one argument.");
          return -1;
        }

        arg* a = cmnd->args;
        while (a) {
          if (!a->sym) {
            // Write the given literal into section's content:
            curSection.addContent(locCounter, a->lit);
          }
          else {
            // Create a RealocationTableEntry for 4 bytes starting from the current value of locCounter.
            curRelTable.addEntry(a->sym, locCounter);
          }  

          locCounter += 4;
          a = a->next;
        }
      }

      // SECTION:
      else if (strcmp(cmnd->name, "section") == 0) {
        if (!cmnd->args || !cmnd->args->sym || cmnd->args->next) {
          fprintf(stderr, "\nERROR: directive .section expects a single identifier as its argument.");
          return -1;
        }

        // Add previous section's relocation table to the map of relocation tables:
        relocationTables.addOrUpdateTable(curSection.getName(), curRelTable); 
        // Start a new Relocation table:
        curRelTable = RelocationTable();

        // Write the updated version of the previous section to the SectionTable map instead of the old one.
        sectionTable.updateSection(curSection);

        // Grab the newly started section:
        locCounter = 0;
        curSection = *sectionTable.lookFor(cmnd->args->sym);
      }

      // OTHER: GLOBAL, EXTERN  (only check if the args are as expected, no additional work)
      else if (strcmp(cmnd->name, "global") == 0 || strcmp(cmnd->name, "extern") == 0) {
        if (!cmnd->args) {
          fprintf(stderr, "\nERROR: directive .%s must be given one or more symbol arguments.", cmnd->name);
          return -1;
        }

        arg* a = cmnd->args;
        while (a) {
          if (!a->sym) {
            fprintf(stderr, "\nERROR: directive .%s can't work with literals as arguments, only symbols.", cmnd->name);
            return -1;
          }
          a = a->next;
        }
      }

      // UNRECOGNIZED: (throw an error)
      else {
        fprintf(stderr, "\nERROR: Unrecognized directive '.%s'", cmnd->name);
        return -1;
      }
    }

    // INSTRUCTIONS:
    else {
      string machineInstr;
      
      // HALT, INT, RET: (no arg)
      if (strcmp(cmnd->name, "halt") == 0 
      || strcmp(cmnd->name, "int") == 0
      || strcmp(cmnd->name, "ret") == 0) {
        if (cmnd->args) {
          fprintf(stderr, "\nERROR: Instruction %s can't have arguments.", cmnd->name);
          return -1;
        }

        if (strcmp(cmnd->name, "halt") == 0) machineInstr = "00000000";
        else if (strcmp(cmnd->name, "int") == 0) machineInstr = "10000000";
        else machineInstr = "93FE0004";
      }

      // IRET: (no arg, two machine instr)
      else if (strcmp(cmnd->name, "iret") == 0) {
        if (cmnd->args) {
          fprintf(stderr, "\nERROR: Instruction %s can't have arguments.", cmnd->name);
          return -1;
        }

        machineInstr = "960E0004";
        curSection.addContentInstruction(locCounter, machineInstr);
        locCounter += 4;

        machineInstr = "93FE0008";
      }

      // PUSH, POP, NOT: (one arg: gpr)
      else if (strcmp(cmnd->name, "push") == 0 
      || strcmp(cmnd->name, "pop") == 0
      || strcmp(cmnd->name, "not") == 0) {
        if (!cmnd->args || !cmnd->args->reg || cmnd->args->next || cmnd->args->type != 0) {
          fprintf(stderr, "\nERROR: Instruction %s expects a single GPR register argument.", cmnd->name);
          return -1;
        }
        else if (!isGPR(cmnd->args->reg)) {
          fprintf(stderr, "\nERROR: In instruction %s, invalid register name: %s.", cmnd->name, cmnd->args->reg);
          return -1;
        }

        if (strcmp(cmnd->name, "push") == 0) {
          machineInstr = "81E0";
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "FFC";
        }
        else if (strcmp(cmnd->name, "pop") == 0) {
          machineInstr = "93";
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += 'E';
          machineInstr += "0004";
        }
        else {
          machineInstr = "60";
          char regId = getRegId(cmnd->args->reg);
          machineInstr += regId;
          machineInstr += regId;
          machineInstr += "0000";
        }
      }

      // XCHG, ADD, SUB, MUL, DIV
      // AND, OR, XOR, SHL, SHR: (two args: gpr gpr)
      else if (strcmp(cmnd->name, "xchg") == 0
      || strcmp(cmnd->name, "add") == 0
      || strcmp(cmnd->name, "sub") == 0
      || strcmp(cmnd->name, "mul") == 0
      || strcmp(cmnd->name, "div") == 0
      || strcmp(cmnd->name, "and") == 0
      || strcmp(cmnd->name, "or") == 0
      || strcmp(cmnd->name, "xor") == 0
      || strcmp(cmnd->name, "shl") == 0
      || strcmp(cmnd->name, "shr") == 0) {
        if (!cmnd->args || !cmnd->args->reg || !cmnd->args->next || !cmnd->args->next->reg || cmnd->args->next->next
        || cmnd->args->type != 0 || cmnd->args->next->type != 0) {
          fprintf(stderr, "\nERROR: Instruction %s expects two GPR registers as its arguments.", cmnd->name);
          return -1;
        }
        else if (!isGPR(cmnd->args->reg))  {
          fprintf(stderr, "\nERROR: In instruction %s, invalid register name: %s.", cmnd->name, cmnd->args->reg);
          return -1;
        }
        else if (!isGPR(cmnd->args->next->reg))  {
          fprintf(stderr, "\nERROR: In instruction %s, invalid register name: %s.", cmnd->name, cmnd->args->next->reg);
          return -1;
        }

        if (strcmp(cmnd->name, "xchg") == 0) {
          machineInstr = "400";
          machineInstr += getRegId(cmnd->args->next->reg);
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "000";
        }
        else {
          if (strcmp(cmnd->name, "add") == 0) machineInstr = "50";
          else if (strcmp(cmnd->name, "sub") == 0) machineInstr = "51";
          else if (strcmp(cmnd->name, "mul") == 0) machineInstr = "52";
          else if (strcmp(cmnd->name, "div") == 0) machineInstr = "53";
          else if (strcmp(cmnd->name, "and") == 0) machineInstr = "61";
          else if (strcmp(cmnd->name, "or") == 0) machineInstr = "62";
          else if (strcmp(cmnd->name, "xor") == 0) machineInstr = "63";
          else if (strcmp(cmnd->name, "shl") == 0) machineInstr = "70";
          else if (strcmp(cmnd->name, "shr") == 0) machineInstr = "71";

          machineInstr += getRegId(cmnd->args->next->reg);
          machineInstr += getRegId(cmnd->args->next->reg);
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "000";
        }
      }

      // CSRRD: (two args: csr, gpr)
      else if (strcmp(cmnd->name, "csrrd") == 0) {
        if (!cmnd->args || !cmnd->args->reg || !cmnd->args->next || !cmnd->args->next->reg || cmnd->args->next->next
        || cmnd->args->type != 0 || cmnd->args->next->type != 0) {
          fprintf(stderr, "\nERROR: Instruction %s expects a CSR and a GPR register as its arguments.", cmnd->name);
          return -1;
        }
        else if (!isCSR(cmnd->args->reg))  {
          fprintf(stderr, "\nERROR: In instruction %s, invalid csr register name: %s.", cmnd->name, cmnd->args->reg);
          return -1;
        }
        else if (!isGPR(cmnd->args->next->reg))  {
          fprintf(stderr, "\nERROR: In instruction %s, invalid gpr register name: %s.", cmnd->name, cmnd->args->next->reg);
          return -1;
        }

        machineInstr = "90";
        machineInstr += getRegId(cmnd->args->next->reg);
        machineInstr += getRegId(cmnd->args->reg);
        machineInstr += "0000";
      }

      // CSRWR: (two args: gpr, csr)
      else if (strcmp(cmnd->name, "csrwr") == 0) {
        if (!cmnd->args || !cmnd->args->reg || !cmnd->args->next || !cmnd->args->next->reg || cmnd->args->next->next
        || cmnd->args->type != 0 || cmnd->args->next->type != 0) {
          fprintf(stderr, "\nERROR: Instruction %s expects a GPR and a CSR register as its arguments.", cmnd->name);
          return -1;
        }
        else if (!isGPR(cmnd->args->reg))  {
          fprintf(stderr, "\nERROR: In instruction %s, invalid gpr register name: %s.", cmnd->name, cmnd->args->reg);
          return -1;
        }
        else if (!isCSR(cmnd->args->next->reg))  {
          fprintf(stderr, "\nERROR: In instruction %s, invalid csr register name: %s.", cmnd->name, cmnd->args->next->reg);
          return -1;
        }

        machineInstr = "94";
        machineInstr += getRegId(cmnd->args->next->reg);
        machineInstr += getRegId(cmnd->args->reg);
        machineInstr += "0000";
      }

      // CALL, JMP: (one arg: operand)
      else if (strcmp(cmnd->name, "call") == 0
      || strcmp(cmnd->name, "jmp") == 0) {
        if (!cmnd->args || cmnd->args->next) {
          fprintf(stderr, "\nERROR: Instruction %s expects a single operand as its arguments.", cmnd->name);
          return -1;
        }
        else if (cmnd->args->type == 4 || cmnd->args->type == 5) {
          fprintf(stderr, "\nERROR: Instructions jmp, br, call can't use operand syntax '$sym' or '$lit'. Use 'sym' or 'lit' for that same effect.");
          return -1;
        }
        else if ((cmnd->args->type == 0 || cmnd->args->type == 1 || cmnd->args->type == 2) 
        && !isReg(cmnd->args->reg)) {
          fprintf(stderr, "\nERROR: Instruction %s is given an invalid register name.", cmnd->name);
          return -1;
        } 

        // 0 - %reg, 1 - [%reg], 2 - [%reg + literal], 3 - [%reg + symbol] (first cycle doesn't allow this one)
        // 4 - $literal, 5 - $symbol, 6 - literal, 7 - symbol   (jmp,br,call use 6,7 instead of 4,5. there are no mem[sym] or mem[lit])
        int argType = cmnd->args->type;

        if (argType == 0 || (argType == 6 && (uint)cmnd->args->lit < maxLit)) {
          if (strcmp(cmnd->name, "call") == 0) machineInstr = "20";
          else if (strcmp(cmnd->name, "jmp") == 0) machineInstr = "30";
        }
        else {
          if (strcmp(cmnd->name, "call") == 0) machineInstr = "21";
          else if (strcmp(cmnd->name, "jmp") == 0) machineInstr = "38";
        }

        if (argType == 0) {
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "00000";   
        }
        else if (argType == 1) {
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "00000";
        }
        else if (argType == 2) {
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "00";
          machineInstr += intToHex(cmnd->args->lit, 3);
        }
        else if (argType == 6) {
          // Literal is in the pool:
          if ((uint)cmnd->args->lit >= maxLit) {
            uint dispToLit = curSection.getPoolEntryLocation(cmnd->args->lit); // Disp from the start of this section to the pool loc.
            dispToLit = dispToLit - (locCounter + 4); // Disp from the current pc value (start of the next machineInstr) to the location in the pool where the literal's value is. 
            machineInstr += "F00";  // gpr[A]=pc
            machineInstr += intToHex(dispToLit, 3);
          }
          // Literal isn't in the pool:
          else {
            machineInstr += "000";  // gpr[A]=r0=0
            machineInstr += intToHex(cmnd->args->lit, 3);
          }
        }
        else if (argType == 7) {
          if (strcmp(cmnd->name, "call") == 0) machineInstr += "F00";
          else if (strcmp(cmnd->name, "jmp") == 0) machineInstr += "F00";

          uint dispToSymVal = curSection.getPoolEntryLocation(cmnd->args->sym); // Disp from the start of this section to the pool loc.

          curRelTable.addEntry(cmnd->args->sym, dispToSymVal);  // Add a relocation entry to the section's relocation table.

          dispToSymVal = dispToSymVal - (locCounter + 4); // Disp from the current pc value (start of the next machineInstr) to the location in the pool where the symbol's value is. 
          machineInstr += intToHex(dispToSymVal, 3);
        }
      }

      // BRANCH: (three args: gpr, gpr, operand)
      else if (strcmp(cmnd->name, "beq") == 0
      || strcmp(cmnd->name, "bne") == 0
      || strcmp(cmnd->name, "bgt") == 0) {
        if (!cmnd->args || !cmnd->args->next || !cmnd->args->next->next || cmnd->args->next->next->next
        || !cmnd->args->reg || !cmnd->args->next->reg || cmnd->args->type != 0 || cmnd->args->next->type != 0) {
          fprintf(stderr, "\nERROR: Instruction %s expects the following arguments: gpr, gpr, operand.", cmnd->name);
          return -1;
        }
        else if (!isGPR(cmnd->args->reg) || !isGPR(cmnd->args->next->reg))  {
          fprintf(stderr, "\nERROR: In instruction %s, invalid gpr register name.", cmnd->name);
          return -1;
        }
        else if (cmnd->args->next->next->type == 4 || cmnd->args->next->next->type == 5) {
          fprintf(stderr, "\nERROR: Instructions jmp, br, call can't use operand syntax '$sym' or '$lit'. Use 'sym' or 'lit' for that same effect.");
          return -1;
        }
        else if (!isReg(cmnd->args->reg) || !isReg(cmnd->args->next->reg)
        || ((cmnd->args->next->next->type == 0 || cmnd->args->next->next->type == 1 || cmnd->args->next->next->type == 2) 
        && !isReg(cmnd->args->next->next->reg))) {
          fprintf(stderr, "\nERROR: Instruction %s is given an invalid register name.", cmnd->name);
          return -1;
        } 

        int argType = cmnd->args->next->next->type;
        char* reg1 = cmnd->args->reg;
        char* reg2 = cmnd->args->next->reg;
        
        // 0 - %reg, 1 - [%reg], 2 - [%reg + literal], 3 - [%reg + symbol] (first cycle doesn't allow this one)
        // 4 - $literal, 5 - $symbol, 6 - literal, 7 - symbol   (jmp,br,call use 6,7 instead of 4,5. there are no mem[sym] or mem[lit])
        machineInstr = "3";

        if (argType == 0 || (argType == 6 && (uint)cmnd->args->next->next->lit < maxLit)) {
          if (strcmp(cmnd->name, "beq") == 0) machineInstr += "1";
          else if (strcmp(cmnd->name, "bne") == 0) machineInstr += "2";
          else machineInstr += "3";
        }
        else {
          if (strcmp(cmnd->name, "beq") == 0) machineInstr += "9";
          else if (strcmp(cmnd->name, "bne") == 0) machineInstr += "A";
          else machineInstr += "B";
        }

        if (argType == 0 || argType == 1) {
          machineInstr += getRegId(cmnd->args->next->next->reg);
          machineInstr += getRegId(reg1);
          machineInstr += getRegId(reg2);
          machineInstr += "000";   
        }
        else if (argType == 2) {
          machineInstr += getRegId(cmnd->args->next->next->reg);
          machineInstr += getRegId(reg1);
          machineInstr += getRegId(reg2);
          machineInstr += intToHex(cmnd->args->next->next->lit, 3);
        }
        else if (argType == 6) {
          // Literal is in the pool:
          if ((uint)cmnd->args->next->next->lit >= maxLit) {
            uint dispToLit = curSection.getPoolEntryLocation(cmnd->args->next->next->lit); // Disp from the start of this section to the pool loc.
            dispToLit = dispToLit - (locCounter + 4); // Disp from the current pc value (start of the next machineInstr) to the location in the pool where the literal's value is. 
            machineInstr += "F";  // gpr[A]=pc
            machineInstr += getRegId(reg1);
            machineInstr += getRegId(reg2);
            machineInstr += intToHex(dispToLit, 3);
          }
          // Literal isn't in the pool:
          else {
            machineInstr += "0";  // gpr[A]=r0=0
            machineInstr += getRegId(reg1);
            machineInstr += getRegId(reg2);
            machineInstr += intToHex(cmnd->args->next->next->lit, 3);
          }
        }
        else if (argType == 7) {
          uint dispToSym = curSection.getPoolEntryLocation(cmnd->args->next->next->sym); // Disp from the start of this section to the pool loc.
          
          curRelTable.addEntry(cmnd->args->next->next->sym, dispToSym);  // Add a relocation entry to the section's relocation table.
          
          dispToSym = dispToSym - (locCounter + 4); // Disp from the current pc value (start of the next machineInstr) to the location in the pool where the symbol's value is. 
          machineInstr += "F";  // gpr[A]=pc
          machineInstr += getRegId(reg1);
          machineInstr += getRegId(reg2);
          machineInstr += intToHex(dispToSym, 3);
        }
      }

      // LD: (two args: operand, gpr)
      else if (strcmp(cmnd->name, "ld") == 0) {
        if (!cmnd->args || !cmnd->args->next || !cmnd->args->next->reg || cmnd->args->next->next || cmnd->args->next->type != 0) {
          fprintf(stderr, "\nERROR: Instruction %s expects an operand (but not a CSR) and a GPR as its arguments.", cmnd->name);
          return -1;
        }
        else if (!isGPR(cmnd->args->next->reg)
        || ((cmnd->args->type == 0 || cmnd->args->type == 1 || cmnd->args->type == 2) && !isGPR(cmnd->args->reg))) {
          fprintf(stderr, "\nERROR: Instruction %s is given an invalid GPR register name.", cmnd->name);
          return -1;
        } 

        int argType = cmnd->args->type;
        char* reg = cmnd->args->next->reg;
        // 0 - %reg, 1 - [%reg], 2 - [%reg + literal], 3 - [%reg + symbol] (first cycle doesn't allow this one)
        // 4 - $literal, 5 - $symbol, 6 - literal, 7 - symbol   
        // pazi, sada 4 i 5 rade ono sto su gore radili 6 i 7!
        if (argType == 0) {
          machineInstr = "91";
          machineInstr += getRegId(reg);
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "0000";   
        }
        else if (argType == 1) {
          machineInstr = "92";
          machineInstr += getRegId(reg);
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "0000"; 
        }
        else if (argType == 2) {
          machineInstr = "92";
          machineInstr += getRegId(reg);
          machineInstr += getRegId(cmnd->args->reg);
          machineInstr += "0";
          machineInstr += intToHex(cmnd->args->lit, 3);
        }
        // 1) gpr[A] <= $literal
        else if (argType == 4 || argType == 6) {
          if ((uint)cmnd->args->lit >= maxLit) {    
            machineInstr = "92";
            machineInstr += getRegId(reg);
            machineInstr += "F0"; // gpr[B]=pc=15
            uint dispToLit = curSection.getPoolEntryLocation(cmnd->args->lit); // Disp from the start of this section to the pool loc.
            dispToLit = dispToLit - (locCounter + 4); // Disp from the current pc value (start of the next machineInstr) to the location in the pool where the literal's value is. 
            machineInstr += intToHex(dispToLit, 3);    
          }
          else {
            machineInstr = "91";
            machineInstr += getRegId(reg);  
            machineInstr += "00"; // gpr[B]=r0=0
            machineInstr += intToHex(cmnd->args->lit, 3); 
          }
        }
        // 1) gpr[A] <= $symbol
        else if (argType == 5 || argType == 7) {    
          machineInstr = "92";
          machineInstr += getRegId(reg);
          machineInstr += "F0"; // gpr[B]=pc=15
          uint dispToSymVal = curSection.getPoolEntryLocation(cmnd->args->sym); // Disp from the start of this section to the pool loc.
        
          curRelTable.addEntry(cmnd->args->sym, dispToSymVal);  // Add a relocation entry to the section's relocation table.
        
          dispToSymVal = dispToSymVal - (locCounter + 4); // Disp from the current pc value (start of the next machineInstr) to the location in the pool where the symbol's value is. 
          machineInstr += intToHex(dispToSymVal, 3);
        }
        // 2) gpr[A] <= mem[gpr[A]]
        if (argType == 6 || argType == 7) {
          curSection.addContentInstruction(locCounter, machineInstr);
          locCounter += 4;
          
          machineInstr = "92";
          machineInstr += getRegId(reg);
          machineInstr += getRegId(reg); // gpr[B]=gpr[A]
          machineInstr += "0000"; 
        }
      }

      // ST: (two args: gpr, operand)
      else if (strcmp(cmnd->name, "st") == 0) {
        if (!cmnd->args || !cmnd->args->next || !cmnd->args->reg || cmnd->args->next->next) {
          fprintf(stderr, "\nERROR: Instruction %s expects a GPR and an operand as its arguments.", cmnd->name);
          return -1;
        }
        else if (!isGPR(cmnd->args->reg)
        || ((cmnd->args->next->type == 0 || cmnd->args->next->type == 1 || cmnd->args->next->type == 2) && !isReg(cmnd->args->next->reg))) {
          fprintf(stderr, "\nERROR: Instruction %s is given an invalid register name.", cmnd->name);
          return -1;
        } 

        int argType = cmnd->args->next->type;
        char* reg = cmnd->args->reg;
        // 0 - %reg, 1 - [%reg], 2 - [%reg + literal], 3 - [%reg + symbol] (first cycle doesn't allow this one)
        // 4 - $literal, 5 - $symbol, 6 - literal, 7 - symbol   
        // pazi, sada 4 i 5 rade ono sto su gore radili 6 i 7!
        if (argType == 0 || argType == 4 || argType == 5) {
          // Todo: proveri da li sme.. Mada ja msm da se ld koristi u ove svrhe, a st je samo za upis u mem.
          fprintf(stderr, "\nERROR: Instruction %s can't use reg, $sym or $lit as its operand.", cmnd->name);
          return -1;
        }
        else if (argType == 1) {
          machineInstr = "80";
          machineInstr += getRegId(cmnd->args->next->reg);
          machineInstr += "0";
          machineInstr += getRegId(reg);
          machineInstr += "000"; 
        }
        else if (argType == 2) {
          machineInstr = "80";
          machineInstr += getRegId(cmnd->args->next->reg);
          machineInstr += "0";
          machineInstr += getRegId(reg);
          machineInstr += intToHex(cmnd->args->next->lit, 3);
        }
        else if (argType == 6) {
          if ((uint)cmnd->args->next->lit >= maxLit) {     
            machineInstr = "82";
            machineInstr += "F0"; // gpr[A]=pc=15, gpr[B]=r0=0
            machineInstr += getRegId(reg);  // gpr[C]=reg
            uint dispToLit = curSection.getPoolEntryLocation(cmnd->args->next->lit); // Disp from the start of this section to the pool loc.
            dispToLit = dispToLit - (locCounter + 4); // Disp from the current pc value (start of the next machineInstr) to the location in the pool where the literal's value is. 
            machineInstr += intToHex(dispToLit, 3);      
          }
          else {
            machineInstr = "80";
            machineInstr += "00"; // gpr[A]=r0=0, gpr[B]=r0=0
            machineInstr += getRegId(reg);  // gpr[C]=reg
            machineInstr += intToHex(cmnd->args->next->lit, 3); 
          }
        } 
        else if (argType == 7) {
          machineInstr = "82";
          machineInstr += "F0"; // gpr[A]=pc=15, gpr[B]=r0=0
          machineInstr += getRegId(reg);  // gpr[C]=reg
          uint dispToSymVal = curSection.getPoolEntryLocation(cmnd->args->next->sym); // Disp from the start of this section to the pool loc.
        
          curRelTable.addEntry(cmnd->args->next->sym, dispToSymVal);  // Add a relocation entry to the section's relocation table.
        
          dispToSymVal = dispToSymVal - (locCounter + 4); // Disp from the current pc value (start of the next machineInstr) to the location in the pool where the symbol's value is. 
          machineInstr += intToHex(dispToSymVal, 3);
        }
      }

      // UNRECOGNIZED:
      else {
        fprintf(stderr, "\nERROR: Unrecognized instruction '%s'", cmnd->name);
        return -1;
      }

      curSection.addContentInstruction(locCounter, machineInstr);
      locCounter += 4;
    }


    cmnd = cmnd->next;
  }



  // Add previous section's relocation table to the map of relocation tables:
  relocationTables.addOrUpdateTable(curSection.getName(), curRelTable);

  // Write the updated version of the previous section to the SectionTable map instead of the old one.
  sectionTable.updateSection(curSection);

  return 0;
} 


// Opening the input '.s' file and the output '.o' file:
FILE* openInputFile(int argc, char* argv[]) {
  string prefix = "../tests/";
  string fileName;

  if (argc == 2) fileName = argv[1];
  else if (argc == 4 && strcmp("-o", argv[1]) == 0) fileName = argv[3];
  else {
    fprintf(stderr, "Error: expected syntax './asembler -o outputName inputName' or './asembler inputName'\n");
    return nullptr;
  }
  
  string path = prefix + fileName;
  FILE* inputFile = fopen(path.c_str(), "r");
  if (!inputFile) {
    fprintf(stderr, "Error: Couldn't open the requested inputFile.\n");
  }

  return inputFile;
}
FILE* openOutputFile(int argc, char* argv[]) {
  string fileName = "../tests/";
  
  if (argc == 2) {
    fileName += argv[1];
    fileName = fileName.substr(0, fileName.length()-2);  // Removes '.s' suffix
    fileName += ".o"; 
  }
  else if (argc == 4) {
    fileName += argv[2];
  }
  outputFileName = fileName;
  fileName += ".txt";

  FILE* outputFile = fopen(fileName.c_str(), "w");
  if (!outputFile) {
    fprintf(stderr, "Error: Couldn't open the requested outputFile.\n");
  }

  return outputFile;
}




int main(int argc, char* argv[]) {
  /// Parse the input file:
  FILE* inputFile = openInputFile(argc, argv);
  if (inputFile == nullptr) return -1;

  yyin = inputFile;
	yyparse();

  fclose(inputFile);


  /// Assembler's first cycle:
  if (firstCycle() == -1) return -1;


  /// Additional work between the two cycles:
  if (symbolTable.validateSymbolTable() == -1) return -1; 

  sectionTable.finalizeLiteralsTables();


  /// Assembler's second cycle:
  if (secondCycle() == -1) {
    fprintf(stderr, "\n\nStopping the assembler's process due to error.");
    return -1;
  }


  /// Printing:
  FILE* outputFile = openOutputFile(argc, argv);
  if (outputFile == nullptr) return -1;

  symbolTable.printSymbolTable(outputFile);
  sectionTable.printSectionTables(outputFile);
  relocationTables.printRelocationTables(outputFile);

  fclose(outputFile);


  /// Creating a binary output:
  ofstream out(outputFileName);  
  if (out.fail()) {
    fprintf(stderr, "Asembler Error: couldn't write the binary output in the 'tests' directory.\n");
    return -1;
  }
  
  symbolTable.bWrite(out);
  sectionTable.bWrite(out);
  relocationTables.bWrite(out);
  
  out.close();


  /// Free allocated memory:
  freeCommands(commandsHead); 


	return 0;
}