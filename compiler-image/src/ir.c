#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ir.h"
#include "utils.h"
//puntatori globali per gestione lista concatenata istruzioni TAC
TacInstr* ir_head = NULL;
TacInstr* ir_tail = NULL;
//contatori per garantire univicità dei nomi generatori durante compilazione
static int temp_counter = 0;
static int label_counter = 0;
//genera nome di un nuovo registro univoco temporaneo
char* new_temp() {
    char buffer[32];
    sprintf(buffer, "t%d", temp_counter++);
    return my_strdup(buffer);
}
//genera nome di nuova etichetta univoca 
char* new_label() {
    char buffer[32];
    sprintf(buffer, ".L%d", label_counter++);
    return my_strdup(buffer);
}
//wrapper di sicurezza per my_strdup che previena crash in caso di puntatore nullo
static char* safe_my_strdup(const char* s) {
    if (!s) return NULL;
    return my_strdup(s);
}
//genera nuova istruzione TAC e accoda alla lista globale del programma
void emit_tac(TacOp op, char* dest, char* arg1, char* arg2) {
    //alloca dinamicamente memoria per nuovo nodo istruzione
    TacInstr* instr = malloc(sizeof(TacInstr));
    //popola i campi 
    instr->op = op;
    instr->dest = safe_my_strdup(dest);
    instr->arg1 = safe_my_strdup(arg1);
    instr->arg2 = safe_my_strdup(arg2);
    instr->next = NULL;
    //logica di inserimento in coda alla lista
    if (ir_head == NULL) {
        ir_head = instr;
        ir_tail = instr;
    } else {
        ir_tail->next = instr;
        ir_tail = instr;
    }
}