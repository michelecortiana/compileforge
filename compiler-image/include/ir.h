#ifndef IR_H
#define IR_H

//enum di tutte le operazioni supportate nell intermediate representation
typedef enum {
    TAC_ASSIGN,
    TAC_ADD,
    TAC_SUB,
    TAC_MUL,
    TAC_DIV,
    TAC_MOD,

    TAC_EQ,           
    TAC_NEQ,          
    TAC_LT,           
    TAC_LE,           
    TAC_GT,           
    TAC_GE,  

    TAC_LABEL,
    TAC_JMP,
    TAC_JMPZ,

    TAC_PARAM,        
    TAC_CALL,         
    TAC_RETURN,       
    TAC_RETURN_STRUCT, 

    TAC_ADDR,         
    TAC_DEREF,        
    TAC_PTR_ASSIGN,   
    TAC_ARRAY_READ,   
    TAC_ARRAY_WRITE, 

    TAC_MEMBER_READ_OBJ,  
    TAC_MEMBER_READ_PTR,  
    TAC_MEMBER_WRITE_OBJ, 
    TAC_MEMBER_WRITE_PTR,  

    TAC_AND,    
    TAC_OR,
    //op. specifiche per virgola mobile
    TAC_FADD,
    TAC_FSUB,
    TAC_FMUL,
    TAC_FDIV,

    TAC_INT_TO_FLOAT, 
    TAC_FLOAT_TO_INT
} TacOp;

//struttura che rappresenta singola istruzione TAC
typedef struct TacInstr {
    TacOp op; //tipo op
    char* dest; //operando di destinazione
    char* arg1; 
    char* arg2;
    struct TacInstr* next; //puntatore istruzione successiva
} TacInstr;
//puntatore globale alla head della lista istruzione IR generate dal compilatore
extern TacInstr* ir_head;

//firme supporto per generazione IR
char* new_temp(); //genera registro temporaneo
char* new_label(); //genera etichetta per salti
void emit_tac(TacOp op, char* dest, char* arg1, char* arg2); //crea nuova istruzione TAC e la accoda alla lista globale

#endif