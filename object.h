#ifndef _OBJECT_H_
#define _OBJECT_H_

enum ObjectType {
  SYMBOL   = 0,
  INTEGER  = 1,
  REAL     = 2,
}

struct Object {
  enum ObjectType type;
  
  union {
    int _integer;
    float  _real;
  }
}

#endif
