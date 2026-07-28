#include "../include/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ir.h"
//strutture per la gestione salti all'interno dei cicli annidati
#define MAX_LOOP_DEPTH 32
static char* break_label_stack[MAX_LOOP_DEPTH]; //stack etichette d'uscita per istruzioni break
static char* continue_label_stack[MAX_LOOP_DEPTH]; //stack etichette per istruzioni continue
static int loop_depth = 0; //livello di annidamento 
//struct che rappresenta variabile della symbol table
typedef struct {
    char name[64];
    int offset;  //posizione nello stack rispetto al frame pointer
    int is_pointer; //flag se puntatore
    DataType base_type; 
    char struct_name[64]; //se tipo di base è struct
} Symbol;
//tabella globale per registrazione delle dichiarazioni di struct 
StructDef struct_table[50];
int struct_count = 0;

//struct per gestire stringhe letterali per il segmento assembly
typedef struct {
    int id;          
    char* text;      
} StringLiteral;
StringLiteral str_table[100];
int str_count = 0;

//registra nuova stringa o restituisce id se gia presente
int add_string(char* text) {
    for (int i = 0; i < str_count; i++) {
        if (strcmp(str_table[i].text, text) == 0) {
            return str_table[i].id;
        }
    }
    if (str_count >= 100) {
        printf("\n[ERRORE FATALE] Superato il limite di 100 stringhe letterali!\n");
        exit(1);
    }
    //registra stringa
    str_table[str_count].id = str_count;
    str_table[str_count].text = text;
    return str_count++;
}

//symbol table globale e var. globali per tracciamento offset
Symbol sym_table[100];
int sym_count = 0;
int current_offset = 0; 
static int label_count = 0; 
//isola scope delle variabili per ogni funzione
typedef struct {
    char func_name[64];
    Symbol symbols[100]; //array variabili locali e parametri
    int count;
} FunctionSymbolTable;

FunctionSymbolTable func_sym_tables[50];
int func_sym_table_count = 0;

//cerca variabile nello scope di una specifica funzione e ne ritorna l'offset
int get_offset_for_function(const char* func_name, const char* var_name) {
    for (int f = 0; f < func_sym_table_count; f++) {
        if (strcmp(func_sym_tables[f].func_name, func_name) == 0) {
            for (int i = 0; i < func_sym_tables[f].count; i++) {
                if (strcmp(func_sym_tables[f].symbols[i].name, var_name) == 0) {
                    return func_sym_tables[f].symbols[i].offset;
                }
            }
        }
    }
    printf("\n[ERRORE SEMANTICO] Variabile '%s' non dichiarata nella funzione '%s'!\n",
           var_name, func_name ? func_name : "?");
    exit(1);
}
//trova il tipo base di una variabile all interno della symbol table di una funzione
DataType get_type_for_function(const char* func_name, const char* var_name) {
    for (int f = 0; f < func_sym_table_count; f++) {
        if (strcmp(func_sym_tables[f].func_name, func_name) == 0) {
            for (int i = 0; i < func_sym_tables[f].count; i++) {
                if (strcmp(func_sym_tables[f].symbols[i].name, var_name) == 0) {
                    return func_sym_tables[f].symbols[i].base_type;
                }
            }
        }
    }
    printf("\n[ERRORE SEMANTICO] Variabile '%s' non dichiarata nella funzione '%s'!\n",
           var_name, func_name ? func_name : "?");
    exit(1);
}

//verifica se una variabile locale e un puntatore interrogando lo scope della funzione
int is_ptr_for_function(const char* func_name, const char* var_name) {
    for (int f = 0; f < func_sym_table_count; f++) {
        if (strcmp(func_sym_tables[f].func_name, func_name) == 0) {
            for (int i = 0; i < func_sym_tables[f].count; i++) {
                if (strcmp(func_sym_tables[f].symbols[i].name, var_name) == 0) {
                    return func_sym_tables[f].symbols[i].is_pointer;
                }
            }
        }
    }
    printf("\n[ERRORE SEMANTICO] Variabile '%s' non dichiarata nella funzione '%s'!\n",
           var_name, func_name ? func_name : "?");
    exit(1);
}
//recupera il nome della struct associata a una variabile nello scope locale
char* get_struct_name_for_function(const char* func_name, const char* var_name) {
    for (int f = 0; f < func_sym_table_count; f++) {
        if (strcmp(func_sym_tables[f].func_name, func_name) == 0) {
            for (int i = 0; i < func_sym_tables[f].count; i++) {
                if (strcmp(func_sym_tables[f].symbols[i].name, var_name) == 0) {
                    return func_sym_tables[f].symbols[i].struct_name;
                }
            }
        }
    }
    return NULL;
}

//gestione delle costanti in virgola mobile da salvare nel segmento dati
double float_table[100];
int float_count = 0; 

int add_float(double val) {
    for (int i = 0; i < float_count; i++) {
        if (float_table[i] == val) {
            return i;
        }
    }
    
    if (float_count >= 100) {
        printf("\n[ERRORE FATALE] Superato il limite di 100 costanti float/double!\n");
        exit(1);
    }

    float_table[float_count] = val;
    return float_count++;
}

//ottiene l offset di una variabile dallo scope globale o corrente
int get_offset(char* name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) return sym_table[i].offset;
    }
    printf("\n[ERRORE SEMANTICO] Variabile '%s' non dichiarata!\n", name);
    exit(1);
}
//verifica se una variabile nello scope corrente e un puntatore
int is_ptr(char* name){
    for(int i = 0;i<sym_count;i++){
        if(strcmp(sym_table[i].name,name)==0) return sym_table[i].is_pointer;
    }
    printf("\n[ERRORE SEMANTICO] Variabile '%s' non dichiarata!\n", name);
    exit(1);
}
//calcola la dimensione in byte di un tipo di dato standard o puntatore
int get_type_size(DataType type, int is_pointer){
    //i puntatori occupano sempre 8 byte su architetture a 64 bit
    if (is_pointer) return 8; 
    if(type == TYPE_CHAR) return 1;
    if(type == TYPE_SHORT) return 2;
    if(type == TYPE_INT || type == TYPE_FLOAT) return 4;
    if(type == TYPE_LONG || type == TYPE_DOUBLE) return 8;

    return 8; 
}
//trova la dimensione di un membro di una struct conoscendone l offset
int get_member_size_by_offset(char* struct_name, int offset) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(struct_table[i].name, struct_name) == 0) {
            for (int j = 0; j < struct_table[i].member_count; j++) {
                if (struct_table[i].members[j].offset == offset) {
                    return get_type_size(struct_table[i].members[j].type, struct_table[i].members[j].is_pointer);
                }
            }
        }
    }
    return 8; 
}
//verifica se il tipo di dato e a virgola mobile 
int is_floating_point(DataType type) {
    return (type == TYPE_FLOAT || type == TYPE_DOUBLE);
}
//calcola l allineamento richiesto in memoria per un dato tipo
int get_alignment(DataType type, int is_pointer){
    if(type == TYPE_CHAR) return 1;
    if(type == TYPE_SHORT) return 2;
    if(type == TYPE_INT || type == TYPE_FLOAT) return 4;
    if(type == TYPE_LONG || type == TYPE_DOUBLE || is_pointer) return 8;

    return 8;
}
//registra una nuova definizione di struct calcolando offset e padding dei membri
void register_struct(char* name, AST_Node* fields) {
    if (struct_count >= 50) {
        printf("\n[ERRORE FATALE] Superato il limite di 50 struct!\n");
        exit(1);
    }

    StructDef* s = &struct_table[struct_count++];
    strncpy(s->name, name, 63);
    s->name[63] = '\0';
    s->member_count = 0;
    
    int current_struct_offset = 0;
    int max_align = 1; 
    
    AST_Node* curr = fields;
    while (curr != NULL) {
        if (s->member_count >= 20) {
            printf("\n[ERRORE FATALE] La struct '%s' ha più di 20 campi!\n", s->name);
            exit(1);
        }

        StructMember* m = &s->members[s->member_count++];
        strncpy(m->name, curr->data.var_decl.var_name, 63);
        m->name[63] = '\0';
        m->type = curr->data.var_decl.base_type;
        m->is_pointer = curr->data.var_decl.is_pointer;
        
        if (curr->data.var_decl.struct_name != NULL) {
            strncpy(m->struct_name, curr->data.var_decl.struct_name, 63);
            m->struct_name[63] = '\0';
        } else {
            strcpy(m->struct_name, "");
        }

        //calcola allineamento e dimensione del singolo membro
        int align = get_alignment(m->type, m->is_pointer);
        int size = 0;
        
        if (m->type == TYPE_STRUCT && !m->is_pointer) {
            for (int i = 0; i < struct_count; i++) {
                if (strcmp(struct_table[i].name, m->struct_name) == 0) {
                    size = struct_table[i].total_size;
                    break;
                }
            }
        } else {
            size = get_type_size(m->type, m->is_pointer);
        }
        //applica il padding necessario per l allineamento in memoria
        current_struct_offset = (current_struct_offset + align - 1) & ~(align - 1);
        m->offset = current_struct_offset; 
        current_struct_offset += size;     
    
        curr = curr->next;
    }
    //imposta la dimensione totale della struct inclusa la spaziatura finale
    s->total_size = (current_struct_offset + max_align - 1) & ~(max_align - 1);
    
    printf("\n[Memory Layout] Registrata Struct '%s':\n", s->name);
    for(int i = 0; i < s->member_count; i++) {
        printf("  ├─ Membro '%s' -> Offset: %d\n", s->members[i].name, s->members[i].offset);
    }
    printf("  └─ Dimensione in RAM: %d byte\n", s->total_size);
}
//costruisce la stringa dell istruzione assembly in virgola mobile
const char* get_fp_instr(DataType type, const char* operation) {
    
    static char buffer[16];
    if (type == TYPE_FLOAT) sprintf(buffer, "%sss", operation); 
    else sprintf(buffer, "%ssd", operation);                    
    return buffer;
}
//aggiunge una variabile allo scope corrente e ne calcola lo spazio sullo stack
void add_symbol(char* name, int is_pointer, int array_size, DataType type, char* struct_name) {
    if (sym_count >= 100) {
        printf("\n[ERRORE FATALE] Superato limite di 100 simboli nella funzione corrente!\n");
        exit(1);
    }

    int size = 0;
    if (type == TYPE_STRUCT) {
        for (int i = 0; i < struct_count; i++) {
            if (strcmp(struct_table[i].name, struct_name) == 0) {
                size = struct_table[i].total_size;
                break;
            }
        }
    } else {
        size = get_type_size(type, is_pointer);
    }
    //calcola lo spazio necessario tenendo conto di eventuali array
    if (array_size > 0) {
        current_offset += (size * array_size);
    } else {
        //le variabili locali semplici vengono allineate almeno a 8 byte per sicurezza
        if (size < 8) size = 8;
        current_offset += size;
    }
    //forza un allineamento generale a 8 byte sullo stack
    current_offset = (current_offset + 7) & ~7;
    
    strncpy(sym_table[sym_count].name, name, 63);
    sym_table[sym_count].name[63] = '\0';
    
    sym_table[sym_count].offset = current_offset;
    sym_table[sym_count].is_pointer = is_pointer;
    sym_table[sym_count].base_type = type;
    
    if (struct_name) {
        strncpy(sym_table[sym_count].struct_name, struct_name, 63);
        sym_table[sym_count].struct_name[63] = '\0';
    } else {
        strcpy(sym_table[sym_count].struct_name, "");
    }
    sym_count++;
}

//trova l offset di un singolo campo all interno di una struct dichiarata
int get_member_offset(char* struct_name, char* member_name) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(struct_table[i].name, struct_name) == 0) {
            for (int j = 0; j < struct_table[i].member_count; j++) {
                if (strcmp(struct_table[i].members[j].name, member_name) == 0) {
                    return struct_table[i].members[j].offset;
                }
            }
        }
    }
    return -1; 
}

//restituisce il nome della struttura se la variabile ne e una istanza
char* get_var_struct_name(char* var_name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, var_name) == 0) return sym_table[i].struct_name;
    }
    return NULL;
}
//seleziona l istruzione assembly di scrittura memoria piu adatta al tipo
const char* get_store_instr(DataType type, int is_pointer) {
    if (is_pointer) return "movq"; 
    if (type == TYPE_CHAR) return "movb";  
    if (type == TYPE_SHORT) return "movw"; 
    if (type == TYPE_INT) return "movl";   
    if (type == TYPE_LONG) return "movq";  
    return "movq";
}

//seleziona l istruzione assembly di lettura memoria con estensione di segno o zero
const char* get_load_instr(DataType type, int is_pointer) {
    if (is_pointer) return "movq"; 
    if (type == TYPE_CHAR) return "movzbq";  
    if (type == TYPE_SHORT) return "movzwq"; 
    if (type == TYPE_INT) return "movslq";   
    if (type == TYPE_LONG) return "movq";    
    return "movq";
}
//seleziona il registro accumulatore x86 della dimensione corretta
const char* get_reg_a(DataType type, int is_pointer) {
    if (is_pointer) return "%rax";
    if (type == TYPE_CHAR) return "%al";
    if (type == TYPE_SHORT) return "%ax";
    if (type == TYPE_INT) return "%eax";
    if (type == TYPE_LONG) return "%rax";
    return "%rax";
}
//seleziona il registro base x86 della dimensione corretta
const char* get_reg_b(DataType type, int is_pointer) {
    if (is_pointer) return "%rbx";
    if (type == TYPE_CHAR) return "%bl";
    if (type == TYPE_SHORT) return "%bx";
    if (type == TYPE_INT) return "%ebx";
    if (type == TYPE_LONG) return "%rbx";
    return "%rbx";
}
//recupera il tipo base di una variabile dichiarata nello scope corrente
DataType get_type(char* name){
    for(int i = 0; i < sym_count; i++){
        if(strcmp(sym_table[i].name, name) == 0) return sym_table[i].base_type;
    }
    printf("\n[ERRORE SEMANTICO] Variabile '%s' non dichiarata!\n", name);
    exit(1);
}

//dichiarazioni anticipate delle funzioni per l analisi semantica delle espressioni
char* get_expr_struct_name(AST_Node* node);
DataType get_member_type(char* struct_name, char* member_name);
int get_member_is_pointer(char* struct_name, char* member_name);

//analizza ricorsivamente l albero sintattico per determinare il tipo risultante di un espressione
DataType get_expr_type(AST_Node* node) {
    if (!node) return TYPE_INT; 
    if (node->type == AST_NUM) return TYPE_INT;
    if (node->type == AST_FLOAT) return TYPE_DOUBLE; 
    if (node->type == AST_CAST) return node->data.cast.target_type;
    
    if (node->type == AST_VAR) return get_type(node->data.var_name);
    if (node->type == AST_ARRAY_ACCESS) return get_type(node->data.array_access.array_name);
    if (node->type == AST_MEMBER_ACCESS) {
        char* struct_name = get_expr_struct_name(node->data.member_access.object);
        if (struct_name) return get_member_type(struct_name, node->data.member_access.member_name);
    }
    //promozione implicita a doppia precisione se uno degli operandi e a virgola mobile
    if (node->type == AST_BINOP) {
        DataType left_t = get_expr_type(node->data.binop.left);
        DataType right_t = get_expr_type(node->data.binop.right);
        if (is_floating_point(left_t) || is_floating_point(right_t)) return TYPE_DOUBLE;
        return left_t;
    }
    return TYPE_INT; 
}

//verifica se un nodo dell albero sintattico restituisce un tipo puntatore
int is_expr_pointer(AST_Node* node) {
    if (!node) return 0;
    if (node->type == AST_VAR) return is_ptr(node->data.var_name);
    if (node->type == AST_ADDR) return 1;
    if (node->type == AST_ARRAY_ACCESS) return 0; 
    if (node->type == AST_MALLOC) return 1; 
    if (node->type == AST_MEMBER_ACCESS) {
        char* struct_name = get_expr_struct_name(node->data.member_access.object);
        if (struct_name) return get_member_is_pointer(struct_name, node->data.member_access.member_name);
    }
    return 0;
}
//interroga la tabella delle struct per ottenere il tipo di un membro specifico
DataType get_member_type(char* struct_name, char* member_name) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(struct_table[i].name, struct_name) == 0) {
            for (int j = 0; j < struct_table[i].member_count; j++) {
                if (strcmp(struct_table[i].members[j].name, member_name) == 0) {
                    return struct_table[i].members[j].type;
                }
            }
        }
    }
    printf("[ERRORE] Membro '%s' non trovato nella struct '%s'!\n", member_name, struct_name);
    exit(1);
}

//ottiene il tipo di un membro di una struct conoscendone soltanto l offset
DataType get_member_type_by_offset(char* struct_name, int offset) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(struct_table[i].name, struct_name) == 0) {
            for (int j = 0; j < struct_table[i].member_count; j++) {
                if (struct_table[i].members[j].offset == offset) {
                    return struct_table[i].members[j].type;
                }
            }
        }
    }
    return TYPE_INT; 
}
//verifica se un membro di struct e un puntatore tramite il suo offset
int get_member_is_pointer_by_offset(char* struct_name, int offset) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(struct_table[i].name, struct_name) == 0) {
            for (int j = 0; j < struct_table[i].member_count; j++) {
                if (struct_table[i].members[j].offset == offset) {
                    return struct_table[i].members[j].is_pointer;
                }
            }
        }
    }
    return 0; 
}
//verifica se un membro di struct e un puntatore tramite il suo nome
int get_member_is_pointer(char* struct_name, char* member_name) {
    for (int i = 0; i < struct_count; i++) {
        if (strcmp(struct_table[i].name, struct_name) == 0) {
            for (int j = 0; j < struct_table[i].member_count; j++) {
                if (strcmp(struct_table[i].members[j].name, member_name) == 0) {
                    return struct_table[i].members[j].is_pointer;
                }
            }
        }
    }
    return 0;
}
//dichiarazioni delle funzioni principali del generatore IR Three Address Code
char* generate_ir(AST_Node* node);
void generate_ir_if(AST_Node* node);
void generate_ir_while(AST_Node* node);
void generate_ir_for(AST_Node* node);

//funzione di supporto per generare il codice intermedio di un intero blocco di istruzioni
void generate_ir_block(AST_Node* head) {
    AST_Node* current = head;
    while (current != NULL) {
        generate_ir(current);
        current = current->next;
    }
}
//utility per verificare se una variabile esiste gia nella symbol table corrente
int symbol_exists(char* name) {
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(sym_table[i].name, name) == 0) return 1;
    }
    return 0;
}

//gestione speciale per le chiamate a funzione che restituiscono una struct per valore
void generate_struct_call(char* dest_var_name, AST_Node* call_node) {
    char* hidden_ptr = new_temp();
    emit_tac(TAC_ADDR, hidden_ptr, dest_var_name, NULL);
    
    //passa l indirizzo della struct di destinazione come parametro nascosto secondo la convenzione C
    emit_tac(TAC_PARAM, "INT", hidden_ptr, NULL);

    AST_Node* arg = call_node->data.func_call.args;
    while (arg != NULL) {
        char* arg_val = generate_ir(arg);
        DataType t = get_expr_type(arg);
        if (is_floating_point(t) && !is_expr_pointer(arg)) {
            emit_tac(TAC_PARAM, "FLOAT", arg_val, NULL);
        } else {
            emit_tac(TAC_PARAM, "INT", arg_val, NULL);
        }
        arg = arg->next;
    }

    emit_tac(TAC_CALL, dest_var_name, call_node->data.func_call.func_name, NULL);
}
//risolve il nome della struct a cui appartiene un espressione complessa nell AST
char* get_expr_struct_name(AST_Node* node) {
    if (!node) return NULL;
    if (node->type == AST_VAR) {
        return get_var_struct_name(node->data.var_name);
    }
    if (node->type == AST_ARRAY_ACCESS) {
        return get_var_struct_name(node->data.array_access.array_name);
    }
    if (node->type == AST_DEREF) {
        return get_expr_struct_name(node->data.unary.expr);
    }
    //analisi ricorsiva per gli accessi a membri di struct annidate
    if (node->type == AST_MEMBER_ACCESS) {
        char* parent_struct = get_expr_struct_name(node->data.member_access.object);
        
        if (parent_struct != NULL) {
            for (int i = 0; i < struct_count; i++) {
                if (strcmp(struct_table[i].name, parent_struct) == 0) {
                    for (int j = 0; j < struct_table[i].member_count; j++) {
                        if (strcmp(struct_table[i].members[j].name, node->data.member_access.member_name) == 0) {
                            return struct_table[i].members[j].struct_name;
                        }
                    }
                }
            }
        }
    }
    return NULL;
}
//calcola l indirizzo di memoria di un espressione utilizzabile come l value a sinistra di un uguale
char* get_lvalue_address(AST_Node* node) {
    if (!node) return NULL;
    if (node->type == AST_VAR) {
        char* dest = new_temp();
        emit_tac(TAC_ADDR, dest, node->data.var_name, NULL);
        return dest;
    }
    if (node->type == AST_DEREF) {
        //il valore dereferenziato e gia un indirizzo di memoria valido
        return generate_ir(node->data.unary.expr);
    }
    if (node->type == AST_MEMBER_ACCESS) {
        char* base_addr;
        if (node->data.member_access.is_pointer) {
            //se l oggetto e un puntatore basta valutarlo per ottenere l indirizzo base
            base_addr = generate_ir(node->data.member_access.object);
        } else {
            //altrimenti bisogna calcolare esplicitamente l indirizzo dell oggetto
            base_addr = get_lvalue_address(node->data.member_access.object);
        }
        if (!base_addr) {
            semantic_error(node, "Impossibile determinare l'indirizzo di memoria dell'oggetto a sinistra del membro");
        }
        
        char* struct_name = get_expr_struct_name(node->data.member_access.object);
        int offset = get_member_offset(struct_name, node->data.member_access.member_name);
        //aggiunge l offset del membro all indirizzo base dell oggetto
        char* offset_reg = new_temp();
        char offset_str[32];
        sprintf(offset_str, "%d", offset);
        emit_tac(TAC_ASSIGN, offset_reg, offset_str, NULL);
        
        char* dest_addr = new_temp();
        emit_tac(TAC_ADD, dest_addr, base_addr, offset_reg);
        
        return dest_addr;
    }
    semantic_error(node, "Espressione non valida a sinistra di un'assegnazione (richiesto L-value)");
    return NULL; 
}
//cuore del generatore di codice intermedio che visita l albero sintattico e produce il TAC
char* generate_ir(AST_Node* node) {
    if (!node) return NULL;
    switch (node->type) {
        case AST_NUM:
        {   //gestione dei valori interi letterali
            char* dest = new_temp();
            char num_str[32];
            sprintf(num_str, "%d", node->data.num.value.int_val);
            emit_tac(TAC_ASSIGN, dest, num_str, NULL);
            return dest;
        }

        case AST_FLOAT:
        {
            //gestione dei valori float letterali salvati nella sezione dati ed etichettati
            int float_id = add_float(node->data.num.value.float_val);
            char label_str[32];
            sprintf(label_str, ".LC_F%d", float_id); 
            char* dest = new_temp();
            emit_tac(TAC_ASSIGN, dest, label_str, NULL);
            return dest;
        }
        case AST_VAR:
        {
            char* var_name = node->data.var_name;
            if (!symbol_exists(var_name)) {
                char err_msg[128];
                sprintf(err_msg, "Uso di una variabile non dichiarata: '%s'", var_name);
                semantic_error(node, err_msg);
            }
            
            char* dest = new_temp();
            emit_tac(TAC_ASSIGN, dest, var_name, NULL);
            return dest;
        }

        case AST_VAR_DECL:
        {
            //controlli di tipo e casting impliciti durante l inizializzazione delle variabili
            if (node->data.var_decl.array_size == 0 && node->data.var_decl.expr != NULL) {
                DataType dest_t = node->data.var_decl.base_type;
                DataType src_t = get_expr_type(node->data.var_decl.expr);
                int dest_ptr = node->data.var_decl.is_pointer;
                int src_ptr = is_expr_pointer(node->data.var_decl.expr);

                if (!dest_ptr && !src_ptr) {
                    if (is_floating_point(dest_t) && !is_floating_point(src_t)) {
                        AST_Node* cast_node = malloc(sizeof(AST_Node));
                        cast_node->type = AST_CAST;
                        cast_node->next = NULL;
                        cast_node->line = node->data.var_decl.expr->line;
                        cast_node->col = node->data.var_decl.expr->col;
                        cast_node->data.cast.target_type = dest_t; 
                        cast_node->data.cast.expr = node->data.var_decl.expr;
                        node->data.var_decl.expr = cast_node;
                    } else if (!is_floating_point(dest_t) && is_floating_point(src_t)) {
                        AST_Node* cast_node = malloc(sizeof(AST_Node));
                        cast_node->type = AST_CAST;
                        cast_node->next = NULL;
                        cast_node->line = node->data.var_decl.expr->line;
                        cast_node->col = node->data.var_decl.expr->col;
                        cast_node->data.cast.target_type = TYPE_INT; 
                        cast_node->data.cast.expr = node->data.var_decl.expr;
                        node->data.var_decl.expr = cast_node;
                    }
                }
            }

            //registrazione del simbolo nella tabella corrente con allocazione stack
            add_symbol(node->data.var_decl.var_name, 
                       node->data.var_decl.is_pointer, 
                       node->data.var_decl.array_size, 
                       node->data.var_decl.base_type,
                       node->data.var_decl.struct_name);
            
            //generazione dell eventuale assegnazione del valore iniziale
            if (node->data.var_decl.array_size == 0 && node->data.var_decl.expr != NULL) {
                if (node->data.var_decl.expr->type == AST_FUNC_CALL && 
                    node->data.var_decl.base_type == TYPE_STRUCT) {
                    generate_struct_call(node->data.var_decl.var_name, node->data.var_decl.expr);
                } else {
                    char* right_val = generate_ir(node->data.var_decl.expr);
                    emit_tac(TAC_ASSIGN, node->data.var_decl.var_name, right_val, NULL);
                }
            }
            return NULL; 
        }

        case AST_ASSIGN:
        {
            char* var_name = node->data.assign.var_name;
            if (!symbol_exists(var_name)) {
                char err_msg[128];
                sprintf(err_msg, "Impossibile assegnare un valore: variabile '%s' non dichiarata", var_name);
                semantic_error(node, err_msg);
            }

            char* struct_name = get_var_struct_name(var_name);
            if (struct_name != NULL && node->data.assign.expr->type == AST_FUNC_CALL) {
                generate_struct_call(var_name, node->data.assign.expr);
                return NULL;
            }
            //risoluzione dei conflitti di tipo con casting automatico int float durante l assegnazione
            DataType dest_t = get_type(var_name);
            DataType src_t = get_expr_type(node->data.assign.expr);
            int dest_ptr = is_ptr(var_name);
            int src_ptr = is_expr_pointer(node->data.assign.expr);

            if (!dest_ptr && !src_ptr) {
                if (is_floating_point(dest_t) && !is_floating_point(src_t)) {
                    
                    AST_Node* cast_node = malloc(sizeof(AST_Node));
                    cast_node->type = AST_CAST;
                    cast_node->next = NULL;
                    cast_node->data.cast.target_type = dest_t; 
                    cast_node->data.cast.expr = node->data.assign.expr;
                    node->data.assign.expr = cast_node;
                } else if (!is_floating_point(dest_t) && is_floating_point(src_t)) {
                    
                    AST_Node* cast_node = malloc(sizeof(AST_Node));
                    cast_node->type = AST_CAST;
                    cast_node->next = NULL;
                    cast_node->data.cast.target_type = TYPE_INT; 
                    cast_node->data.cast.expr = node->data.assign.expr;
                    node->data.assign.expr = cast_node;
                }
            }

            char* right_val = generate_ir(node->data.assign.expr);
            emit_tac(TAC_ASSIGN, var_name, right_val, NULL);
            
            return right_val;
        }

        case AST_STRING:
        {
            //gestione delle stringhe come etichette da risolvere nella sezione rodata
            int str_id = add_string(node->data.string_val);
            char label_str[32];
            sprintf(label_str, ".LC%d", str_id);
            
            char* dest = new_temp();
            emit_tac(TAC_ASSIGN, dest, label_str, NULL);
            
            return dest;
        }

        case AST_BINOP:
        {
            //controllo dei tipi per operazioni binarie ed eventuale promozione degli operandi
            DataType left_t = get_expr_type(node->data.binop.left);
            DataType right_t = get_expr_type(node->data.binop.right);
            int left_ptr = is_expr_pointer(node->data.binop.left);
            int right_ptr = is_expr_pointer(node->data.binop.right);
            if (!left_ptr && !right_ptr) {
                if (is_floating_point(left_t) && !is_floating_point(right_t)) {
                    AST_Node* cast_node = malloc(sizeof(AST_Node));
                    cast_node->type = AST_CAST;
                    cast_node->next = NULL;
                    cast_node->line = node->data.binop.right->line;
                    cast_node->col = node->data.binop.right->col;
                    cast_node->data.cast.target_type = TYPE_DOUBLE;
                    cast_node->data.cast.expr = node->data.binop.right;
                    node->data.binop.right = cast_node; 
                    
                } else if (!is_floating_point(left_t) && is_floating_point(right_t)) {
                    AST_Node* cast_node = malloc(sizeof(AST_Node));
                    cast_node->type = AST_CAST;
                    cast_node->next = NULL;
                    cast_node->line = node->data.binop.left->line;
                    cast_node->col = node->data.binop.left->col;
                    cast_node->data.cast.target_type = TYPE_DOUBLE;
                    cast_node->data.cast.expr = node->data.binop.left;
                    node->data.binop.left = cast_node; 
                }
            }
            char* left_val = generate_ir(node->data.binop.left);
            char* right_val = generate_ir(node->data.binop.right);
            
            //implementazione dell aritmetica dei puntatori moltiplicando l offset per la dimensione del tipo
            if (node->data.binop.op == TOKEN_PLUS || node->data.binop.op == TOKEN_MINUS) {
                if (left_ptr && !right_ptr) {
                    int elem_size = get_type_size(left_t, 0); 
                    if (elem_size > 1) {
                        char size_str[32];
                        sprintf(size_str, "%d", elem_size);
                        char* scaled_right = new_temp();
                        emit_tac(TAC_MUL, scaled_right, right_val, size_str); 
                        right_val = scaled_right; 
                    }
                } 
                else if (!left_ptr && right_ptr && node->data.binop.op == TOKEN_PLUS) {
                    int elem_size = get_type_size(right_t, 0);
                    if (elem_size > 1) {
                        char size_str[32];
                        sprintf(size_str, "%d", elem_size);
                        char* scaled_left = new_temp();
                        emit_tac(TAC_MUL, scaled_left, left_val, size_str);
                        left_val = scaled_left; 
                    }
                }
            }

            char* dest = new_temp();
            int is_float = is_floating_point(left_t) || is_floating_point(right_t);
            
            //l aritmetica dei puntatori impedisce l uso delle istruzioni floating point
            if (left_ptr || right_ptr) is_float = 0;
            
            
            TacOp op;
            switch(node->data.binop.op) {
                case TOKEN_PLUS:          op = is_float ? TAC_FADD : TAC_ADD; break;
                case TOKEN_MINUS:         op = is_float ? TAC_FSUB : TAC_SUB; break;
                case TOKEN_STAR:          op = is_float ? TAC_FMUL : TAC_MUL; break;
                case TOKEN_SLASH:         op = is_float ? TAC_FDIV : TAC_DIV; break;
                case TOKEN_MODULUS:       op = TAC_MOD; break;
                case TOKEN_EQUAL:         op = TAC_EQ; break;
                case TOKEN_NOT_EQUAL:     op = TAC_NEQ; break;
                case TOKEN_LESS:          op = TAC_LT; break;
                case TOKEN_LESS_EQUAL:    op = TAC_LE; break;
                case TOKEN_GREATER:       op = TAC_GT; break;
                case TOKEN_GREATER_EQUAL: op = TAC_GE; break;
                case TOKEN_AND:           op = TAC_AND; break;
                case TOKEN_OR:            op = TAC_OR;  break;
                default:
                    semantic_error(node, "Operatore binario non supportato nella generazione IR!");
            }
            emit_tac(op, dest, left_val, right_val);
            return dest;
        }
        case AST_RETURN:
        {
            char* ret_val = NULL;
            int is_struct = 0;
            //gestione differenziata se il valore di ritorno e scalare o e un intera struct
            if (node->data.ret.expr != NULL) {
                if (get_expr_type(node->data.ret.expr) == TYPE_STRUCT) {
                    is_struct = 1;
                    if (node->data.ret.expr->type == AST_VAR) {
                        ret_val = node->data.ret.expr->data.var_name;
                    } else {
                        
                        ret_val = generate_ir(node->data.ret.expr);
                    }
                } else {
                    ret_val = generate_ir(node->data.ret.expr);
                }
            }

            if (is_struct) {
                emit_tac(TAC_RETURN_STRUCT, NULL, ret_val, NULL);
            } else {
                emit_tac(TAC_RETURN, NULL, ret_val, NULL);
            }
            
            return NULL;
        }

        case AST_IF:      
            generate_ir_if(node); 
            return NULL;
            
        case AST_WHILE:   
            generate_ir_while(node); 
            return NULL;
            
        case AST_FOR: 
            generate_ir_for(node); 
            return NULL;
            
        case AST_PRINTF:
        {
            //risolve il formato e passa correttamente i parametri in base al loro tipo
            int str_id = add_string(node->data.printf_stmt.format_str);
            char label_str[32];
            sprintf(label_str, ".LC%d", str_id);

            emit_tac(TAC_PARAM, "INT", label_str, NULL);

            AST_Node* arg = node->data.printf_stmt.expr;
            while (arg != NULL) {
                char* expr_val = generate_ir(arg);
                DataType t = get_expr_type(arg);
                if (is_floating_point(t) && !is_expr_pointer(arg)) {
                    emit_tac(TAC_PARAM, "FLOAT", expr_val, NULL);
                } else {
                    emit_tac(TAC_PARAM, "INT", expr_val, NULL);
                }
                arg = arg->next;
            }
            
            emit_tac(TAC_CALL, NULL, "printf@PLT", NULL);
            return NULL;
        }
        case AST_FUNC_DECL:
        {
            //pulisce lo scope corrente e alloca i parametri nei registri virtuali
            sym_count = 0;
            current_offset = 0;
            emit_tac(TAC_LABEL, node->data.func_decl.func_name, NULL, NULL);
            
            AST_Node* param = node->data.func_decl.params;
            int p_int_index = 0; 
            int p_float_index = 0; 
            
            while(param != NULL) {
                add_symbol(param->data.var_decl.var_name, 
                           param->data.var_decl.is_pointer, 
                           0, 
                           param->data.var_decl.base_type, 
                           param->data.var_decl.struct_name); 
                
                char param_reg[32];
                if (is_floating_point(param->data.var_decl.base_type) && !param->data.var_decl.is_pointer) {
                    sprintf(param_reg, "PARAM_F_%d", p_float_index++);
                } else {
                    sprintf(param_reg, "PARAM_I_%d", p_int_index++);
                }
                
                emit_tac(TAC_ASSIGN, param->data.var_decl.var_name, param_reg, NULL);
                param = param->next; 
            }

            //genera l interno del corpo della funzione e aggiunge un return implicito di sicurezza
            generate_ir_block(node->data.func_decl.func_body);
            emit_tac(TAC_RETURN, NULL, NULL, NULL);
            //salva l intera tabella dei simboli per questa funzione in modo da costruire lo stack frame nel backend
            {
                if (func_sym_table_count >= 50) {
                    printf("\n[ERRORE FATALE] Superato il limite massimo di 50 funzioni nel programma!\n");
                    exit(1);
                }
                FunctionSymbolTable* fst = &func_sym_tables[func_sym_table_count++];

                strncpy(fst->func_name, node->data.func_decl.func_name, 63);
                fst->func_name[63] = '\0';
                
                memcpy(fst->symbols, sym_table, sizeof(Symbol) * sym_count);
                fst->count = sym_count;
            }
            return NULL;
        }

        case AST_FUNC_CALL:
        {
            //valuta e passa gli argomenti alla funzione distinguendo registri interi e floating point
            AST_Node* arg = node->data.func_call.args;
            while(arg != NULL) {
                char* arg_val = generate_ir(arg);
                DataType t = get_expr_type(arg);
                
                if (is_floating_point(t) && !is_expr_pointer(arg)) {
                    emit_tac(TAC_PARAM, "FLOAT", arg_val, NULL);
                } else {
                    emit_tac(TAC_PARAM, "INT", arg_val, NULL);
                }
                
                arg = arg->next;
            }

            char* dest = new_temp();
            emit_tac(TAC_CALL, dest, node->data.func_call.func_name, NULL);
            return dest;
        }

        case AST_ADDR:
        {
            //restituisce l indirizzo di memoria di una variabile tramite l operatore ampersand
            AST_Node* child = node->data.unary.expr;
            if (child->type != AST_VAR) {
                semantic_error(node, "L'operatore '&' richiede una variabile, non un valore o un'espressione!");
            }
            if (!symbol_exists(child->data.var_name)) {
                char err_msg[128];
                sprintf(err_msg, "Impossibile ottenere l'indirizzo: variabile '%s' non dichiarata", child->data.var_name);
                semantic_error(node, err_msg);
            }
            
            char* dest = new_temp();
            char* var_name = child->data.var_name;
            emit_tac(TAC_ADDR, dest, var_name, NULL);
            return dest;
        }

        case AST_DEREF:
        {
            //accede al valore puntato risolvendo l operatore asterisco unario
            char* ptr_val = generate_ir(node->data.unary.expr);       
            char* dest = new_temp();
            emit_tac(TAC_DEREF, dest, ptr_val, NULL);
            return dest;
        }

        case AST_PTR_ASSIGN:
        {
            //scrittura in memoria all indirizzo contenuto in un puntatore
            char* value_to_write = generate_ir(node->data.ptr_assign.expr);
            char* ptr_name = node->data.ptr_assign.ptr_name;
            emit_tac(TAC_PTR_ASSIGN, ptr_name, value_to_write, NULL);
            return value_to_write; 
        }

        case AST_ARRAY_ASSIGN:
        {
            //scrittura di un valore in una specifica cella dell array
            char* index_val = generate_ir(node->data.array_write.index_expr);
            char* value_val = generate_ir(node->data.array_write.value_expr);
            char* arr_name = node->data.array_write.array_name;
            emit_tac(TAC_ARRAY_WRITE, arr_name, index_val, value_val);
            return value_val; 
        }

        case AST_ARRAY_ACCESS:
        {
            //lettura dal vettore e salvataggio in un registro temporaneo
            char* index_val = generate_ir(node->data.array_access.index_expr);
            char* arr_name = node->data.array_access.array_name;
            char* dest = new_temp();
            emit_tac(TAC_ARRAY_READ, dest, arr_name, index_val);
            return dest;
        }

        case AST_STRUCT_DEF:
        {
            //attiva la logica di registrazione del layout della nuova struct
            register_struct(node->data.struct_def.struct_name, node->data.struct_def.fields);
            return NULL; 
        }

        case AST_MEMBER_ASSIGN:
        {
            //scrittura di un campo interno di una struct risolvendo indirizzo base e offset
            AST_Node* obj_node = node->data.member_assign.object;
            char* mem_name = node->data.member_assign.member_name;

            char* obj_val;
            if (node->data.member_assign.is_pointer) {
                obj_val = generate_ir(obj_node);
            } else {
                obj_val = get_lvalue_address(obj_node);
            }

            char* struct_name = get_expr_struct_name(obj_node);
            if (!struct_name) semantic_error(node, "L'oggetto non è una struct valida");

            DataType dest_t = get_member_type(struct_name, mem_name);
            DataType src_t = get_expr_type(node->data.member_assign.expr);
            int dest_ptr = get_member_is_pointer(struct_name, mem_name);
            int src_ptr = is_expr_pointer(node->data.member_assign.expr);
            //cast implicito in caso di incompatibilita tra interi e floating point
            if (!dest_ptr && !src_ptr) {
                if (is_floating_point(dest_t) && !is_floating_point(src_t)) {
                    AST_Node* cast_node = malloc(sizeof(AST_Node));
                    cast_node->type = AST_CAST;
                    cast_node->next = NULL;
                    cast_node->data.cast.target_type = TYPE_DOUBLE; 
                    cast_node->data.cast.expr = node->data.member_assign.expr;
                    node->data.member_assign.expr = cast_node;
                } else if (!is_floating_point(dest_t) && is_floating_point(src_t)) {
                    AST_Node* cast_node = malloc(sizeof(AST_Node));
                    cast_node->type = AST_CAST;
                    cast_node->next = NULL;
                    cast_node->data.cast.target_type = TYPE_INT; 
                    cast_node->data.cast.expr = node->data.member_assign.expr;
                    node->data.member_assign.expr = cast_node;
                }
            }

            char* value_val = generate_ir(node->data.member_assign.expr);
            
            int offset = get_member_offset(struct_name, mem_name);
            if (offset == -1) semantic_error(node, "Membro non trovato nella struct");

            char offset_str[128];
            sprintf(offset_str, "%d:%s", offset, struct_name);
            emit_tac(TAC_MEMBER_WRITE_PTR, obj_val, offset_str, value_val);
            return value_val;
        }

        case AST_MEMBER_ACCESS:
        {
            //lettura del campo di una struct risolvendone l offset in memoria
            AST_Node* obj_node = node->data.member_access.object;
            char* mem_name = node->data.member_access.member_name;
            
            char* obj_val;
            if (node->data.member_access.is_pointer) {
                obj_val = generate_ir(obj_node);
            } else {
                obj_val = get_lvalue_address(obj_node);
            }

            char* struct_name = get_expr_struct_name(obj_node);
            if (!struct_name || strlen(struct_name) == 0) {
                semantic_error(node, "Impossibile determinare il tipo di struct in questo accesso");
            }

            int offset = get_member_offset(struct_name, mem_name);
            if (offset == -1) semantic_error(node, "Membro non trovato");

            char* dest = new_temp();
            char offset_str[128];
            sprintf(offset_str, "%d:%s", offset, struct_name);
            
            emit_tac(TAC_MEMBER_READ_PTR, dest, obj_val, offset_str);
            
            return dest;
        }
        case AST_CAST:
        {
            //generazione dell istruzione IR per la conversione esplicita tra tipi
            char* child_val = generate_ir(node->data.cast.expr);
            char* dest = new_temp();
            
            DataType src_t = get_expr_type(node->data.cast.expr);
            DataType dst_t = node->data.cast.target_type;
            
            if (!is_floating_point(src_t) && is_floating_point(dst_t)) {
                emit_tac(TAC_INT_TO_FLOAT, dest, child_val, NULL);
            } else if (is_floating_point(src_t) && !is_floating_point(dst_t)) {
                emit_tac(TAC_FLOAT_TO_INT, dest, child_val, NULL);
            } else {
                return child_val; 
            }
            return dest;
        }
        case AST_BREAK:
        {
            //salto incondizionato all etichetta di uscita del ciclo corrente
            if (loop_depth == 0) {
                semantic_error(node, "L'istruzione 'break' può essere usata solo all'interno di un ciclo!");
            }
            emit_tac(TAC_JMP, break_label_stack[loop_depth - 1], NULL, NULL);
            return NULL;
        }

        case AST_CONTINUE:
        {
            //salto incondizionato all etichetta di ripresa del ciclo corrente
            if (loop_depth == 0) {
                semantic_error(node, "L'istruzione 'continue' può essere usata solo all'interno di un ciclo!");
            }
            emit_tac(TAC_JMP, continue_label_stack[loop_depth - 1], NULL, NULL);
            return NULL;
        }

        case AST_MALLOC:
        {
            //generazione chiamata alla funzione di libreria standard malloc
            char* size_val = generate_ir(node->data.malloc_expr.size_expr);
            emit_tac(TAC_PARAM, "INT", size_val, NULL);
            char* dest = new_temp();
            emit_tac(TAC_CALL, dest, "malloc@PLT", NULL);
            return dest;
        }

        case AST_FREE:
        {
            //generazione chiamata alla funzione di libreria standard free
            char* ptr_val = generate_ir(node->data.free_stmt.ptr_expr);
            emit_tac(TAC_PARAM, "INT", ptr_val, NULL);
            emit_tac(TAC_CALL, NULL, "free@PLT", NULL);
            return NULL;
        }

        case AST_SIZEOF:
        {
            //valuta l operatore sizeof a tempo di compilazione e ne sostituisce il valore
            int size = 0;
            if (node->data.sizeof_expr.is_pointer) {
                size = 8;
            } else if (node->data.sizeof_expr.type_val == TYPE_STRUCT) {
                for (int i = 0; i < struct_count; i++) {
                    if (strcmp(struct_table[i].name, node->data.sizeof_expr.struct_name) == 0) {
                        size = struct_table[i].total_size;
                        break;
                    }
                }
                if (size == 0) {
                    semantic_error(node, "Impossibile calcolare 'sizeof': Struct non trovata");
                }
            } else {
                size = get_type_size(node->data.sizeof_expr.type_val, 0);
            }
            
            
            char* dest = new_temp();
            char size_str[32];
            sprintf(size_str, "%d", size);
            
            emit_tac(TAC_ASSIGN, dest, size_str, NULL);
            return dest;
        }

        default:
            printf("\n[WARNING] Nodo AST non supportato nell'IR!\n");
            return NULL;
    }
    return NULL; 
}
//generazione codice IR per il costrutto condizionale if else
void generate_ir_if(AST_Node* node) {
    char* cond_val = generate_ir(node->data.if_stmt.condition);

    //ottimizzazione preliminare salta il blocco se la condizione e gia un intero noto
    if (node->data.if_stmt.condition != NULL && node->data.if_stmt.condition->type == AST_NUM) {
        int cond_val_int = node->data.if_stmt.condition->data.num.value.int_val;

        if (cond_val_int != 0) {
            generate_ir_block(node->data.if_stmt.true_branch);
        } else {
            if (node->data.if_stmt.false_branch != NULL) {
                generate_ir_block(node->data.if_stmt.false_branch);
            }
        }
        return; 
    }

    //crea le etichette necessarie per il flusso condizionale
    int my_label = label_count++;
    char* label_else = malloc(32);
    char* label_end = malloc(32);
    sprintf(label_else, ".L_ELSE_%d", my_label);
    sprintf(label_end, ".L_END_%d", my_label);

    //se la condizione e falsa salta all else
    emit_tac(TAC_JMPZ, label_else, cond_val, NULL);
    //ramo True
    generate_ir_block(node->data.if_stmt.true_branch);
    emit_tac(TAC_JMP, label_end, NULL, NULL);
    //ramo False Else
    emit_tac(TAC_LABEL, label_else, NULL, NULL);
    if (node->data.if_stmt.false_branch != NULL) {
        generate_ir_block(node->data.if_stmt.false_branch);
    }

    emit_tac(TAC_LABEL, label_end, NULL, NULL);
}
//generazione codice IR per il ciclo while con inserimento nello stack delle etichette
void generate_ir_while(AST_Node* node) {
    int my_label = label_count++;
    char* label_start = malloc(32);
    char* label_end = malloc(32);
    sprintf(label_start, ".L_WHILE_START_%d", my_label);
    sprintf(label_end, ".L_WHILE_END_%d", my_label);

    //salva le etichette per le chiamate break e continue in questo livello
    break_label_stack[loop_depth] = label_end;
    continue_label_stack[loop_depth] = label_start; 
    loop_depth++;
    //emette l etichetta di testa e verifica la condizione d uscita
    emit_tac(TAC_LABEL, label_start, NULL, NULL);
    char* cond_val = generate_ir(node->data.while_stmt.condition);
    emit_tac(TAC_JMPZ, label_end, cond_val, NULL);
    
    generate_ir_block(node->data.while_stmt.loop_body);
    //torna all inizio per la prossima iterazione o chiude il blocco
    emit_tac(TAC_JMP, label_start, NULL, NULL);
    emit_tac(TAC_LABEL, label_end, NULL, NULL);

    loop_depth--;
}
//generazione codice IR per il ciclo for gestendo correttamente inizializzazione condizione e incremento
void generate_ir_for(AST_Node* node) {
    int my_label = label_count++;
    char* label_start = malloc(32);
    char* label_inc = malloc(32); 
    char* label_end = malloc(32);
    sprintf(label_start, ".L_FOR_START_%d", my_label);
    sprintf(label_inc, ".L_FOR_INC_%d", my_label);
    sprintf(label_end, ".L_FOR_END_%d", my_label);

    //il continue salta all incremento non all inizio della condizione
    break_label_stack[loop_depth] = label_end;
    continue_label_stack[loop_depth] = label_inc; 
    loop_depth++;

    if (node->data.for_stmt.init) generate_ir(node->data.for_stmt.init);
    emit_tac(TAC_LABEL, label_start, NULL, NULL);

    if (node->data.for_stmt.condition) {
        char* cond_val = generate_ir(node->data.for_stmt.condition);
        emit_tac(TAC_JMPZ, label_end, cond_val, NULL);
    }

    if (node->data.for_stmt.loop_body) {
        generate_ir_block(node->data.for_stmt.loop_body);
    }
    //etichetta di salto per l istruzione continue
    emit_tac(TAC_LABEL, label_inc, NULL, NULL);

    if (node->data.for_stmt.increment) {
        generate_ir(node->data.for_stmt.increment);
    }

    emit_tac(TAC_JMP, label_start, NULL, NULL);
    emit_tac(TAC_LABEL, label_end, NULL, NULL);

    loop_depth--;
}

//punto di ingresso principale che avvia la traduzione dell intero albero sintattico
void generate_ir_program(AST_Node* root) {
    AST_Node* current = root;
    while (current != NULL) {
        generate_ir(current);
        current = current->next;
    }
}

//esporta il codice intermedio su file con formattazione chiara per ispezione e debug
void print_ir_to_file(const char* filename) {
    FILE* out = fopen(filename, "w");
    if (!out) {
        printf("[ERRORE] Impossibile creare il file IR %s\n", filename);
        exit(1);
    }
    fprintf(out, "=== THREE-ADDRESS CODE (IR) ===\n\n");
    
    TacInstr* current = ir_head; 
    while (current != NULL) {
        //stampa le etichette senza indentazione mentre le istruzioni vengono spaziate
        if (current->op == TAC_LABEL) {
            fprintf(out, "%s:\n", current->dest);
        } else {
            fprintf(out, "    "); 
            fprintf(out, "OP(%d)\t", current->op);
            if (current->dest) fprintf(out, "%s ", current->dest);
            if (current->arg1) fprintf(out, ", %s ", current->arg1);
            if (current->arg2) fprintf(out, ", %s", current->arg2);
            fprintf(out, "\n");
        }
        current = current->next;
    }
    //se ci sono costanti numeriche o testuali aggiunge la sezione dati alla fine del file
    if (str_count > 0 || float_count > 0) {
        fprintf(out, "\n=== DATA SECTION ===\n"); 
        for (int i = 0; i < float_count; i++) {
            union { double d; unsigned long long u; } bit_hacker;
            bit_hacker.d = float_table[i];
            fprintf(out, ".LC_F%d:\tFLOAT %f\t(raw: %llu)\n", i, float_table[i], bit_hacker.u);
        }

        for (int i = 0; i < str_count; i++) {
            fprintf(out, ".LC%d:\tSTRING \"%s\"\n", str_table[i].id, str_table[i].text); 
        }
    }

    fclose(out);
    printf(">> Codice Intermedio (IR) generato con successo in: %s\n", filename);
}