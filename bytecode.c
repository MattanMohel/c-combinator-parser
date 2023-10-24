#include "bytecode.h"

void emit_instruction (enum Instruction instr) {
  switch (instr) {
    case NOP:  printf("NO-OP\n");
    case HALT: printf("HALT\n");
    default:   printf("NA\n");
  }
}
