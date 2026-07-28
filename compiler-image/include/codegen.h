#ifndef CODEGEN_H
#define CODEGEN_H
#include "parser.h"
//struttura che rappresenta un singolo campo di una struct
typedef struct {
    char name[64];
    DataType type;
    int is_pointer; //flag che indica che campo è puntatore
    int offset; //distanza in byte dall'inizio della struct
    char struct_name[64]; //caso in cui membro è una struct
} StructMember;
//struttura mem di una struct
typedef struct {
    char name[64];
    StructMember members[20]; //array di campi interni max 20
    int member_count; //num campi
    int total_size; //dim. totale in byte della struct
} StructDef;

//firme funzioni per gestione principale della generazione codice 
void generate_ir_program(AST_Node* root); //per traduccre l'intero AST nell lista istruzioni TAC
void print_ir_to_file(const char* filename);
//funzioni per interrogare symbol table
int get_offset_for_function(const char* func_name, const char* var_name);
DataType get_type_for_function(const char* func_name, const char* var_name);
int is_ptr_for_function(const char* func_name, const char* var_name);
char* get_struct_name_for_function(const char* func_name, const char* var_name);
//funzioni per risolvere gli accessi ai membri delle struct durante generazione IR 
DataType get_member_type_by_offset(char* struct_name, int offset);
int get_member_is_pointer_by_offset(char* struct_name, int offset);
int get_member_is_pointer(char* struct_name, char* member_name);
#endif