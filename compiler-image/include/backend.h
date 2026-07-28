#ifndef BACKEND_H
#define BACKEND_H
#include "ir.h" 
//firma funzione che traduce IR in codice assemnly
void generate_x86_64(TacInstr* ir_head, const char* filename);

#endif