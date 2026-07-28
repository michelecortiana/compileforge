#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "../include/backend.h"
#include "../include/parser.h"
#include "../include/codegen.h"

//dichiarazioni esterne per accedere alle funzioni e alle strutture dati del frontend
extern char* get_var_struct_name(char* var_name);
extern char* get_var_struct_name(char* var_name);
extern int get_member_size_by_offset(char* struct_name, int offset);
extern int get_offset(char* name);
extern int get_offset_for_function(const char* func_name, const char* var_name);
extern int is_ptr(char* name);
extern DataType get_type(char* name);

extern DataType get_type_for_function(const char* func_name, const char* var_name);
extern int is_ptr_for_function(const char* func_name, const char* var_name);
extern char* get_struct_name_for_function(const char* func_name, const char* var_name);
extern int get_type_size(DataType type, int is_pointer);
extern const char* get_store_instr(DataType type, int is_pointer);
extern const char* get_load_instr(DataType type, int is_pointer);
extern const char* get_reg_a(DataType type, int is_pointer);
extern StructDef struct_table[];
extern int struct_count;

extern DataType get_member_type_by_offset(char* struct_name, int offset);
extern int get_member_is_pointer_by_offset(char* struct_name, int offset);

//separa la stringa combinata in offset numerico e nome della struttura
void extract_offset_and_struct(const char* combined, char* offset_str, char* struct_name) {
    if (!combined) {
        strcpy(offset_str, "0");
        struct_name[0] = '\0';
        return;
    }

    const char* colon = strchr(combined, ':');
    
    if (colon) {
        //estrae la parte numerica dell offset prima dei due punti
        int off_len = colon - combined;
        
        if (off_len > 31) off_len = 31; 
        
        strncpy(offset_str, combined, off_len);
        offset_str[off_len] = '\0';
        
        //estrae il nome della struct dopo i due punti
        strncpy(struct_name, colon + 1, 63);
        struct_name[63] = '\0';
    } else {
        //se non ci sono i due punti tratta tutto come un semplice offset
        strncpy(offset_str, combined, 31);
        offset_str[31] = '\0';
        
        struct_name[0] = '\0'; 
    }
}
//variabile globale per tenere traccia della funzione attualmente in traduzione
static char current_func_name[64] = "";

typedef struct {
    int id;
    char* text;
} StringLiteral;

extern StringLiteral str_table[];
extern int str_count;

//traduce un nome di variabile o parametro nella sua locazione di memoria o registro x86 64
const char* get_loc(const char* name) {
    if (!name) return "";
    static char buf[64];
    //se inizia con un numero lo tratta come un valore immediato
    if (name[0] >= '0' && name[0] <= '9') {
        sprintf(buf, "$%s", name);
        return buf;
    }

    //assegna i parametri interi ai registri standard secondo la convenzione system v
    if (strncmp(name, "PARAM_I_", 8) == 0) {
        int id = atoi(name + 8);
        const char* param_regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
        if (id < 6) return param_regs[id]; 
        else {
            sprintf(buf, "%d(%%rbp)", 16 + ((id - 6) * 8));
            return buf;
        }
    }
    //assegna i parametri float ai registri xmm fino a un massimo di 8
    if (strncmp(name, "PARAM_F_", 8) == 0) {
        int id = atoi(name + 8);
        const char* xmm_regs[] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"};
        if (id < 8) return xmm_regs[id]; 
        else {
            sprintf(buf, "%d(%%rbp)", 16 + ((id - 8) * 8));
            return buf;
        }
    }

    //gestisce le etichette per le stringhe o le costanti in virgola mobile
    if (name[0] == '.') {
        if (strncmp(name, ".LC_F", 5) == 0) {
            sprintf(buf, "%s(%%rip)", name); 
        } else {
            sprintf(buf, "$%s", name);
        }
        return buf;
    }

    //mappa i registri temporanei generati dal frontend nella parte bassa dello stack
    if (name[0] == 't') {
        int id = atoi(name + 1);
        sprintf(buf, "-%d(%%rbp)", 200 + (id * 8)); 
        return buf;
    }

    //calcola l indirizzo sullo stack per le variabili locali dichiarate dall utente
    int offset = get_offset_for_function(current_func_name, name);
    sprintf(buf, "-%d(%%rbp)", offset);
    return buf;
}

//verifica se un identificatore rappresenta una variabile reale e non un temporaneo o costante
int is_variable(const char* name) {
    if (!name) return 0;
    if (name[0] == 't') return 0; 
    if (name[0] == '.') return 0; 
    if (name[0] >= '0' && name[0] <= '9') return 0; 
    if (strncmp(name, "PARAM_I_", 8) == 0) return 0; 
    if (strncmp(name, "PARAM_F_", 8) == 0) return 0; 
    return 1; 
}

extern double float_table[];
extern int float_count;
extern int is_floating_point(DataType type);

//determina se l istruzione corrente coinvolge operazioni in virgola mobile
int is_float_op(TacInstr* curr) {
    
    if (curr->dest && is_variable(curr->dest) && is_floating_point(get_type_for_function(current_func_name, (char*)curr->dest)) && !is_ptr_for_function(current_func_name, (char*)curr->dest)) return 1;
    if (curr->arg1 && is_variable(curr->arg1) && is_floating_point(get_type_for_function(current_func_name, (char*)curr->arg1)) && !is_ptr_for_function(current_func_name, (char*)curr->arg1)) return 1;
    if (curr->arg2 && is_variable(curr->arg2) && is_floating_point(get_type_for_function(current_func_name, (char*)curr->arg2)) && !is_ptr_for_function(current_func_name, (char*)curr->arg2)) return 1;
    
    if (curr->arg1 && strncmp(curr->arg1, ".LC_F", 5) == 0) return 1;
    if (curr->arg2 && strncmp(curr->arg2, ".LC_F", 5) == 0) return 1;
    
    return 0; 
}
//funzione principale che itera sul codice intermedio e produce l assembly x86 64
void generate_x86_64(TacInstr* ir_head, const char* filename) {
    FILE* out = fopen(filename, "w");
    if (!out) {
        printf("Errore: Impossibile creare %s\n", filename);
        return;
    }
    //generazione della sezione rodata per costanti float e stringhe letterali
    if (str_count > 0 || float_count > 0) {
        fprintf(out, ".section .rodata\n");
        
        for (int i = 0; i < float_count; i++) {
            union { double d; unsigned long long u; } bit_hacker;
            bit_hacker.d = float_table[i];
            fprintf(out, ".LC_F%d:\n", i);
            fprintf(out, "    .quad %llu\n", bit_hacker.u);
        } 
        for (int i = 0; i < str_count; i++) {
            fprintf(out, ".LC%d:\n", str_table[i].id);
            fprintf(out, "    .string \"%s\"\n", str_table[i].text);
        }
        fprintf(out, "\n");
    }
    //inizio della sezione del codice eseguibile e dichiarazione del main globale
    fprintf(out, ".text\n");
    fprintf(out, ".globl main\n");

    TacInstr* curr = ir_head;
    const char* param_regs_int[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    const char* param_regs_float[] = {"%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"};
      
    int param_count_int = 0; 
    int param_count_float = 0;
    static char pending_float_loc[8][64];
    static char pending_int_loc[6][64];
    while (curr != NULL) {
        switch (curr->op) {
            case TAC_ASSIGN:
            {
                fprintf(out, "    # %s = %s\n", curr->dest, curr->arg1);
                
                //gestisce l assegnazione spostando dati tra memoria e registri xmm o generali
                if (is_float_op(curr)) {
                    fprintf(out, "    movsd %s, %%xmm0\n", get_loc(curr->arg1));
                    fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest));
                } else {
                    if (is_variable(curr->arg1)) {
                        DataType type_arg1 = get_type_for_function(current_func_name, (char*)curr->arg1);
                        int ptr_arg1 = is_ptr_for_function(current_func_name, (char*)curr->arg1);
                        fprintf(out, "    %s %s, %%rax\n", 
                                get_load_instr(type_arg1, ptr_arg1), 
                                get_loc(curr->arg1));
                    } else {
                        fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                    }

                    if (is_variable(curr->dest)) {
                        DataType type_dest = get_type_for_function(current_func_name, (char*)curr->dest);
                        int ptr_dest = is_ptr_for_function(current_func_name, (char*)curr->dest);
                        fprintf(out, "    %s %s, %s\n", 
                                get_store_instr(type_dest, ptr_dest), 
                                get_reg_a(type_dest, ptr_dest),       
                                get_loc(curr->dest));                
                    } else {
                        fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                    }
                }
                break;
            }
            //operazioni aritmetiche intere di base con registri a 64 bit
            case TAC_ADD:
                fprintf(out, "    # %s = %s + %s\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    addq %s, %%rax\n", get_loc(curr->arg2));
                fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                break;
            case TAC_SUB:
                fprintf(out, "    # %s = %s - %s\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    subq %s, %%rax\n", get_loc(curr->arg2));
                fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                break;
            case TAC_MUL:
                fprintf(out, "    # %s = %s * %s\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    imulq %s, %%rax\n", get_loc(curr->arg2));
                fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                break;
            case TAC_DIV:
            case TAC_MOD:
                //divisione e modulo richiedono l estensione del segno in rdx rax con cqo
                fprintf(out, "    # %s: %s = %s %s %s\n", 
                        curr->op == TAC_DIV ? "DIV" : "MOD", curr->dest, curr->arg1, 
                        curr->op == TAC_DIV ? "/" : "%", curr->arg2);
                
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    cqo\n"); 
                fprintf(out, "    movq %s, %%rcx\n", get_loc(curr->arg2));
                fprintf(out, "    idivq %%rcx\n");
                
                if (curr->op == TAC_DIV) {
                    fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest)); 
                } else {
                    fprintf(out, "    movq %%rdx, %s\n", get_loc(curr->dest)); 
                }
                break;
            //operazioni aritmetiche in virgola mobile a doppia precisione tramite sse
            case TAC_FADD:
                fprintf(out, "    # %s = %s + %s (float)\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movsd %s, %%xmm0\n", get_loc(curr->arg1));
                fprintf(out, "    addsd %s, %%xmm0\n", get_loc(curr->arg2));
                fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest));
                break;
            case TAC_FSUB:
                fprintf(out, "    # %s = %s - %s (float)\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movsd %s, %%xmm0\n", get_loc(curr->arg1));
                fprintf(out, "    subsd %s, %%xmm0\n", get_loc(curr->arg2));
                fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest));
                break;
            case TAC_FMUL:
                fprintf(out, "    # %s = %s * %s (float)\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movsd %s, %%xmm0\n", get_loc(curr->arg1));
                fprintf(out, "    mulsd %s, %%xmm0\n", get_loc(curr->arg2));
                fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest));
                break;
            case TAC_FDIV:
                fprintf(out, "    # %s = %s / %s (float)\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movsd %s, %%xmm0\n", get_loc(curr->arg1));
                fprintf(out, "    divsd %s, %%xmm0\n", get_loc(curr->arg2));
                fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest));
                break;
            //confronti di uguaglianza e disuguaglianza con istruzioni setcc
            case TAC_EQ:
            case TAC_NEQ:
                fprintf(out, "    # %s: %s = %s %s %s\n", 
                        curr->op == TAC_EQ ? "EQ" : "NEQ", 
                        curr->dest, curr->arg1, 
                        curr->op == TAC_EQ ? "==" : "!=", 
                        curr->arg2);
                
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    cmpq %s, %%rax\n", get_loc(curr->arg2));
                
                if (curr->op == TAC_EQ) {
                    fprintf(out, "    sete %%al\n");
                } else {
                    fprintf(out, "    setne %%al\n");
                }
                
                fprintf(out, "    movzbq %%al, %%rax\n");
                fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                break;
            //confronti relazionali maggiore minore con promozione del byte risultante
            case TAC_LT:
            case TAC_LE:
            case TAC_GT:
            case TAC_GE: {
                const char* op_str = "";
                const char* set_instr = "";
                
                if (curr->op == TAC_LT) { 
                    op_str = "<";  
                    set_instr = "setl";  
                } else if (curr->op == TAC_LE) { 
                    op_str = "<="; 
                    set_instr = "setle"; 
                } else if (curr->op == TAC_GT) { 
                    op_str = ">";  
                    set_instr = "setg";  
                } else if (curr->op == TAC_GE) { 
                    op_str = ">="; 
                    set_instr = "setge"; 
                }

                fprintf(out, "    # %s = %s %s %s\n", 
                        curr->dest, curr->arg1, op_str, curr->arg2);
                
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    cmpq %s, %%rax\n", get_loc(curr->arg2));
                fprintf(out, "    %s %%al\n", set_instr);
                fprintf(out, "    movzbq %%al, %%rax\n");
                fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                break;
            }
            //creazione di etichette e prologo delle funzioni con allocazione statica dello stack
            case TAC_LABEL:
                fprintf(out, "%s:\n", curr->dest);
                if (curr->dest != NULL && curr->dest[0] != '.') {
                    strncpy(current_func_name, curr->dest, sizeof(current_func_name) - 1);
                    current_func_name[sizeof(current_func_name) - 1] = '\0';

                    fprintf(out, "    pushq %%rbp\n");
                    fprintf(out, "    movq %%rsp, %%rbp\n");
                    fprintf(out, "    subq $4096, %%rsp\n\n");
                }
                break;
            //salti incondizionati e condizionati per i costrutti di controllo if while for
            case TAC_JMP:
                fprintf(out, "    # jmp %s\n", curr->dest);
                fprintf(out, "    jmp %s\n", curr->dest);
                break;
            case TAC_JMPZ:
                fprintf(out, "    # if %s == 0 jmp %s\n", curr->arg1, curr->dest);
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    cmpq $0, %%rax\n"); 
                fprintf(out, "    je %s\n", curr->dest); 
                break;
            //preparazione dei parametri per la chiamata a funzione usando registri o stack
            case TAC_PARAM:
                fprintf(out, "    # param %s (%s)\n", curr->arg1, curr->dest ? curr->dest : "INT");

                if (curr->dest != NULL && strcmp(curr->dest, "FLOAT") == 0) {
                    if (param_count_float < 8) {
                        strncpy(pending_float_loc[param_count_float], get_loc(curr->arg1), 63);
                        pending_float_loc[param_count_float][63] = '\0';
                    } else {
                        fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                        fprintf(out, "    pushq %%rax\n");
                    }
                    param_count_float++;
                } else {
                    if (param_count_int < 6) {
                        strncpy(pending_int_loc[param_count_int], get_loc(curr->arg1), 63);
                        pending_int_loc[param_count_int][63] = '\0';
                    } else {
                        fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                        fprintf(out, "    pushq %%rax\n");
                    }
                    param_count_int++;
                }
                break;
            //esecuzione della chiamata a funzione e recupero del valore di ritorno
            case TAC_CALL:
                fprintf(out, "    # call %s\n", curr->arg1);
                
                for (int i = 0; i < param_count_int && i < 6; i++) {
                    fprintf(out, "    movq %s, %%rax\n", pending_int_loc[i]);
                    fprintf(out, "    movq %%rax, %s\n", param_regs_int[i]);
                }
                for (int i = 0; i < param_count_float && i < 8; i++) {
                    fprintf(out, "    movsd %s, %s\n", pending_float_loc[i], param_regs_float[i]);
                }

                fprintf(out, "    movq $%d, %%rax\n", param_count_float);
                fprintf(out, "    call %s\n", curr->arg1);
                
                if (curr->dest != NULL && curr->dest[0] != '\0') {
                    char* dest_struct = get_var_struct_name(curr->dest);
                    if (dest_struct != NULL) {
                        fprintf(out, "    # struct return into %s\n", curr->dest);
                    } else {
                        if (is_variable(curr->dest) && is_floating_point(get_type_for_function(current_func_name, curr->dest)) && !is_ptr_for_function(current_func_name, curr->dest)) {
                            fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest));
                        } else {
                            fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                        }
                    }
                }
                
                param_count_int = 0;
                param_count_float = 0;
                break;
            //epilogo della funzione con ripristino dello stack frame tramite leave
            case TAC_RETURN:
                if (curr->arg1 != NULL) {
                    fprintf(out, "    # return %s\n", curr->arg1);
                    
                    char* struct_name = get_var_struct_name((char*)curr->arg1);
                    if (struct_name != NULL) {
                        int size = 8;
                        for (int i = 0; i < struct_count; i++) {
                            if (strcmp(struct_table[i].name, struct_name) == 0) {
                                size = struct_table[i].total_size;
                                break;
                            }
                        }
                        
                        if (size > 8 && size <= 16) {
                            fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                            char base_loc[64];
                            strcpy(base_loc, get_loc(curr->arg1)); 
                            fprintf(out, "    movq 8(%s), %%rdx\n", base_loc); 
                        } else {
                            fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                        }
                    } else {
                        fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                    }
                } else {
                    fprintf(out, "    # return (void)\n");
                    fprintf(out, "    movq $0, %%rax\n");
                }
                
                fprintf(out, "    leave\n");
                fprintf(out, "    ret\n");
                break;
            //calcolo dell indirizzo di una variabile con leaq
            case TAC_ADDR:
                fprintf(out, "    # %s = &%s\n", curr->dest, curr->arg1);
                fprintf(out, "    leaq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                break;
            //dereferenziazione di un puntatore per leggerne il valore
            case TAC_DEREF:
                fprintf(out, "    # %s = *%s\n", curr->dest, curr->arg1);
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    movq (%%rax), %%rcx\n");
                fprintf(out, "    movq %%rcx, %s\n", get_loc(curr->dest));
                break;
            //scrittura in memoria tramite puntatore dereferenziato
            case TAC_PTR_ASSIGN:
                fprintf(out, "    # *%s = %s\n", curr->dest, curr->arg1);
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    movq %s, %%rdx\n", get_loc(curr->dest));
                fprintf(out, "    movq %%rax, (%%rdx)\n");
                break;
            //accesso in lettura agli elementi di un array calcolando offset dinamico
            case TAC_ARRAY_READ:
            {
                fprintf(out, "    # %s = %s[%s]\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg2));
                
                DataType arr_type = get_type_for_function(current_func_name, curr->arg1);
                int is_ptr_arr = is_ptr_for_function(current_func_name, curr->arg1);
                int elem_size = get_type_size(arr_type, is_ptr_arr);
                
                fprintf(out, "    imulq $%d, %%rax\n", elem_size);
                
                if (is_ptr_arr) {
                    fprintf(out, "    movq %s, %%r10\n", get_loc(curr->arg1));
                } else {
                    fprintf(out, "    leaq %s, %%r10\n", get_loc(curr->arg1));
                }
                
                fprintf(out, "    addq %%rax, %%r10\n");
                
                if (arr_type == TYPE_FLOAT && !is_ptr_arr) {
                    fprintf(out, "    movss (%%r10), %%xmm0\n");          
                    fprintf(out, "    cvtss2sd %%xmm0, %%xmm0\n");        
                    fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest)); 
                } else {
                    if (elem_size == 1) {
                        fprintf(out, "    movzbq (%%r10), %%r11\n");
                    } else if (elem_size == 2) {
                        fprintf(out, "    movzwq (%%r10), %%r11\n");
                    } else if (elem_size == 4) {
                        fprintf(out, "    movslq (%%r10), %%r11\n");
                    } else {
                        fprintf(out, "    movq (%%r10), %%r11\n");
                    }
                    fprintf(out, "    movq %%r11, %s\n", get_loc(curr->dest));
                }
                break;
            }
            //scrittura negli elementi di un array eseguendo lo scale dell indice
            case TAC_ARRAY_WRITE:
            {
                fprintf(out, "    # %s[%s] = %s\n", curr->dest, curr->arg1, curr->arg2);
                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                
                DataType arr_type = get_type_for_function(current_func_name, curr->dest);
                int is_ptr_arr = is_ptr_for_function(current_func_name, curr->dest);
                int elem_size = get_type_size(arr_type, is_ptr_arr);
                
                fprintf(out, "    imulq $%d, %%rax\n", elem_size);
                
                if (is_ptr_arr) {
                    fprintf(out, "    movq %s, %%r10\n", get_loc(curr->dest));
                } else {
                    fprintf(out, "    leaq %s, %%r10\n", get_loc(curr->dest));
                }
                
                fprintf(out, "    addq %%rax, %%r10\n");
                
                
                if (arr_type == TYPE_FLOAT && !is_ptr_arr) {
                    fprintf(out, "    movsd %s, %%xmm0\n", get_loc(curr->arg2));   
                    fprintf(out, "    cvtsd2ss %%xmm0, %%xmm1\n");                 
                    fprintf(out, "    movss %%xmm1, (%%r10)\n");                   
                } else {
                    fprintf(out, "    movq %s, %%r11\n", get_loc(curr->arg2));
                    if (elem_size == 1) {
                        fprintf(out, "    movb %%r11b, (%%r10)\n");
                    } else if (elem_size == 2) {
                        fprintf(out, "    movw %%r11w, (%%r10)\n");
                    } else if (elem_size == 4) {
                        fprintf(out, "    movl %%r11d, (%%r10)\n");
                    } else {
                        fprintf(out, "    movq %%r11, (%%r10)\n");
                    }
                }
                break;
            }
            //scrittura di un campo di una struct risolto tramite indirizzo base e spiazzamento
            case TAC_MEMBER_WRITE_OBJ:
            case TAC_MEMBER_WRITE_PTR:
            {
                fprintf(out, "    # %s%s%s = %s\n", 
                        curr->dest, 
                        curr->op == TAC_MEMBER_WRITE_PTR ? "->" : ".", 
                        curr->arg1, curr->arg2);
                
                char offset_str[32], write_struct_name[64];
                extract_offset_and_struct(curr->arg1, offset_str, write_struct_name);
                
                fprintf(out, "    movq $%s, %%rax\n", offset_str); 
                
                int write_offset = atoi(offset_str);
                DataType mem_type = TYPE_INT;
                int mem_is_ptr = 0;
                
                if (strlen(write_struct_name) > 0) {
                    mem_type = get_member_type_by_offset(write_struct_name, write_offset);
                    mem_is_ptr = get_member_is_pointer_by_offset(write_struct_name, write_offset);
                }
                
                int is_float_val = 0;
                if (mem_type == TYPE_FLOAT && !mem_is_ptr) {
                    is_float_val = 1;
                    fprintf(out, "    movsd %s, %%xmm0\n", get_loc(curr->arg2));
                    fprintf(out, "    cvtsd2ss %%xmm0, %%xmm1\n"); 
                } else {
                    
                    fprintf(out, "    movq %s, %%r11\n", get_loc(curr->arg2)); 
                }
                
                if (curr->op == TAC_MEMBER_WRITE_OBJ) {
                    
                    fprintf(out, "    leaq %s, %%r10\n", get_loc(curr->dest)); 
                } else {
                    fprintf(out, "    movq %s, %%r10\n", get_loc(curr->dest)); 
                }
                
                int write_size = get_type_size(mem_type, mem_is_ptr);

                if (is_float_val) {
                    fprintf(out, "    movss %%xmm1, (%%r10,%%rax,1)\n");
                } else if (write_size == 1) {
                    fprintf(out, "    movb %%r11b, (%%r10,%%rax,1)\n");
                } else if (write_size == 2) {
                    fprintf(out, "    movw %%r11w, (%%r10,%%rax,1)\n");
                } else if (write_size == 4) {
                    fprintf(out, "    movl %%r11d, (%%r10,%%rax,1)\n");
                } else {
                    fprintf(out, "    movq %%r11, (%%r10,%%rax,1)\n");
                }
                break;
            }
            //lettura da un membro della struct valutando correttamente le dimensioni
            case TAC_MEMBER_READ_OBJ:
            case TAC_MEMBER_READ_PTR:
            {
                fprintf(out, "    # %s = %s%s%s\n", 
                        curr->dest, curr->arg1, 
                        curr->op == TAC_MEMBER_READ_PTR ? "->" : ".", 
                        curr->arg2);
                
                char offset_str[32], read_struct_name[64];
                extract_offset_and_struct(curr->arg2, offset_str, read_struct_name);
                
                if (curr->op == TAC_MEMBER_READ_OBJ) {
                    fprintf(out, "    leaq %s, %%r10\n", get_loc(curr->arg1)); 
                } else {
                    fprintf(out, "    movq %s, %%r10\n", get_loc(curr->arg1)); 
                }
                
                fprintf(out, "    movq $%s, %%rax\n", offset_str); 
                
                int read_offset = atoi(offset_str);
                DataType mem_type = TYPE_INT;
                int mem_is_ptr = 0;
                
                if (strlen(read_struct_name) > 0) {
                    mem_type = get_member_type_by_offset(read_struct_name, read_offset);
                    mem_is_ptr = get_member_is_pointer_by_offset(read_struct_name, read_offset);
                }
                
                int read_size = get_type_size(mem_type, mem_is_ptr);

                if (mem_type == TYPE_FLOAT && !mem_is_ptr) {
                    fprintf(out, "    movss (%%r10,%%rax,1), %%xmm0\n");
                    fprintf(out, "    cvtss2sd %%xmm0, %%xmm0\n");
                    fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest));
                } else {
                    if (read_size == 1) {
                        fprintf(out, "    movzbq (%%r10,%%rax,1), %%r11\n"); 
                    } else if (read_size == 2) {
                        fprintf(out, "    movzwq (%%r10,%%rax,1), %%r11\n"); 
                    } else if (read_size == 4) {
                        fprintf(out, "    movslq (%%r10,%%rax,1), %%r11\n"); 
                    } else {
                        fprintf(out, "    movq (%%r10,%%rax,1), %%r11\n");   
                    }
                    fprintf(out, "    movq %%r11, %s\n", get_loc(curr->dest));
                }
                break;
            }
            //operatori logici con valutazione stretta evitando risultati diversi da zero o uno
            case TAC_AND:
            case TAC_OR:
                fprintf(out, "    # %s: %s = %s %s %s\n", 
                        curr->op == TAC_AND ? "AND" : "OR", 
                        curr->dest, curr->arg1, 
                        curr->op == TAC_AND ? "&&" : "||", 
                        curr->arg2);

                fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                fprintf(out, "    movq %s, %%r11\n", get_loc(curr->arg2));
                fprintf(out, "    testq %%rax, %%rax\n    setne %%al\n    movzbq %%al, %%rax\n");
                fprintf(out, "    testq %%r11, %%r11\n    setne %%r11b\n    movzbq %%r11b, %%r11\n");
                
                if (curr->op == TAC_AND)
                    fprintf(out, "    andq %%r11, %%rax\n");
                else
                    fprintf(out, "    orq %%r11, %%rax\n");
                fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                break;
            //copia in blocco della memoria per restituire struct per valore tramite rep movsb
            case TAC_RETURN_STRUCT:
            {
                fprintf(out, "    # return struct %s\n", curr->arg1);
                
                int is_ptr = is_ptr_for_function(current_func_name, curr->arg1);
                int struct_size = 8; 
                
                if (!is_ptr) {
                    char* struct_name = get_struct_name_for_function(current_func_name, curr->arg1);
                    if (struct_name != NULL) {
                        for (int i = 0; i < struct_count; i++) {
                            if (strcmp(struct_table[i].name, struct_name) == 0) {
                                struct_size = struct_table[i].total_size;
                                break;
                            }
                        }
                    }
                } else {
                    struct_size = 8; 
                }
                
                if (is_ptr) {
                    fprintf(out, "    movq %s, %%rsi\n", get_loc(curr->arg1));
                } else {
                    fprintf(out, "    leaq %s, %%rsi\n", get_loc(curr->arg1));
                }
                
                fprintf(out, "    movq $%d, %%rcx\n", struct_size); 
                fprintf(out, "    cld\n");                          
                fprintf(out, "    rep movsb\n");                    
                
                break;
            }
            //istruzioni di conversione di tipo tra interi e float in entrambe le direzioni
            case TAC_INT_TO_FLOAT:
                fprintf(out, "    # %s = (float) %s\n", curr->dest, curr->arg1);
                
                if (is_variable(curr->arg1)) {
                    DataType type_arg1 = get_type_for_function(current_func_name, (char*)curr->arg1);
                    fprintf(out, "    %s %s, %%rax\n", get_load_instr(type_arg1, 0), get_loc(curr->arg1));
                } else {
                    fprintf(out, "    movq %s, %%rax\n", get_loc(curr->arg1));
                }
                
                fprintf(out, "    cvtsi2sdq %%rax, %%xmm0\n"); 
                fprintf(out, "    movsd %%xmm0, %s\n", get_loc(curr->dest));
                break;
            case TAC_FLOAT_TO_INT:
                fprintf(out, "    # %s = (int) %s\n", curr->dest, curr->arg1);
                fprintf(out, "    movsd %s, %%xmm0\n", get_loc(curr->arg1));
                fprintf(out, "    cvttsd2siq %%xmm0, %%rax\n"); 
                fprintf(out, "    movq %%rax, %s\n", get_loc(curr->dest));
                break;
        }
        
        curr = curr->next;
    }

    //uscita del programa a fine funzione main
    fprintf(out, "\n.L_end:\n");
    fprintf(out, "    leave\n");
    fprintf(out, "    ret\n");

    fclose(out);
    printf(">> Assembly x86-64 generato con successo in: %s\n", filename);
}