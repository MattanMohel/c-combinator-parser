#ifndef _BYTECODE_H_
#define _BYTECODE_H_

#include "object.h"

typedef Instruction* (*instruction) (enum Instruction*, struct* VM);

enum Instruction {
  NOP  = 'n',
  HALT = 'h',
};

struct VM {
  enum Instruction* ip;
  enum Instruction* code;
  Object* stack;
};

void emit_instruction (enum Instruction instr);

#endif
