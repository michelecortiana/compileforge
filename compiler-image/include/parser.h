#ifndef PARSER_H
#define PARSER_H
#include "lexer.h"

//tipi di nodo dell AST
typedef enum{
    AST_NUM,       
    AST_VAR,      
    AST_STRING,
    AST_BINOP,     
    AST_ASSIGN,    
    AST_VAR_DECL,
    AST_RETURN,   
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_BREAK,      
    AST_CONTINUE, 
    AST_MALLOC,     
    AST_FREE,       
    AST_SIZEOF,     
    AST_PRINTF,
    AST_FUNC_DECL,
    AST_FUNC_CALL,
    AST_ADDR, 
    AST_DEREF, 
    AST_PTR_ASSIGN,
    AST_ARRAY_ACCESS,
    AST_ARRAY_ASSIGN,
    AST_FLOAT,
    AST_STRUCT_DEF,
    AST_MEMBER_ACCESS,
    AST_MEMBER_ASSIGN,
    AST_CAST
} AST_NodeType;

//tipi di dato supportati
typedef enum {
    TYPE_CHAR, TYPE_SHORT, TYPE_INT, TYPE_LONG, TYPE_FLOAT, TYPE_DOUBLE,
    TYPE_STRUCT
} DataType;

typedef struct AST_Node AST_Node;  
//struttura nodo
struct AST_Node{
    AST_NodeType type;
    AST_Node* next; 
    //info per gestione errori
    int line;
    int col;

    //payload per ogni nodo
    union{
        int num_val;
        char* var_name;
        char* string_val;
        //op. binarie
        struct{
            TokenType op;
            AST_Node* left;
            AST_Node* right;
        }binop;
        //valori numerici estesi
        struct {
            int is_float;
            union {
                int int_val;
                double float_val;
            } value;
        } num;
        //assegnaazione variabile
        struct{
            char* var_name;
            AST_Node* expr;
        }assign;
        //dichiarazione variabile
        struct { 
            char* var_name;
            int is_pointer;
            int array_size;
            DataType base_type;
            char* struct_name;
            struct AST_Node* expr;
        } var_decl;
        //istruzione return
        struct{
            AST_Node* expr; 
        }ret;
        //costrutto if/else
        struct{
            AST_Node* condition;
            AST_Node* true_branch;
            AST_Node* false_branch;
        }if_stmt;
        //costrutto while
        struct{
            AST_Node* condition;
            AST_Node* loop_body;
        }while_stmt;
        //costrutto for
        struct{
            AST_Node* init;
            AST_Node* condition;
            AST_Node* increment;
            AST_Node* loop_body;
        }for_stmt;
        //istruzione stampa
        struct{
            char* format_str;
            AST_Node* expr;
        }printf_stmt;
        //dichiarazione funzione
        struct{
            char* func_name;
            AST_Node* params;
            AST_Node* func_body;
            DataType return_type;
        }func_decl;
        //chiamata funzione
        struct{
            char* func_name;
            AST_Node* args;
        }func_call;
        //op. unarie (indirizzamento/dereferenziazione) 
        struct {
            AST_Node* expr; 
        } unary;
        //assegnazione valore puntato da puntatore
        struct{
            char* ptr_name;
            AST_Node* expr;
        }ptr_assign;
        //lettura da indice array
        struct{
            char* array_name;
            AST_Node* index_expr;
        }array_access;
        //scrittura su indice array
        struct{
            char* array_name;
            AST_Node* index_expr;
            AST_Node* value_expr;
        }array_write;
        //definzione struct
        struct {
            char* struct_name;       
            struct AST_Node* fields; 
        } struct_def;
        //accesso a campo di uan struct
        struct {
            struct AST_Node* object; 
            char* member_name;       
            int is_pointer;          
        } member_access;
        //assegnazione a campo di una struct
        struct {
            struct AST_Node* object;       
            char* member_name;      
            int is_pointer;          
            struct AST_Node* expr;   
        } member_assign;
        //cast
        struct {
            DataType target_type;
            struct AST_Node* expr; 
        } cast;
        //allocazione dinamica memoria
        struct {
            struct AST_Node* size_expr;
        } malloc_expr;
        //deallocazione dinamica memoria
        struct {
            struct AST_Node* ptr_expr;
        } free_stmt;
        //operatore per dimensioni tipi
        struct {
            DataType type_val;
            char* struct_name;
            int is_pointer;
        } sizeof_expr;
    }data;
};

void init_parser(char* source);
void error_at_token(Token t, const char* message); //gestione errori sintattici 
void semantic_error(AST_Node* node, const char* message); //gestione errori semantici
AST_Node* parse_program(); //genero albero sintatiico 
void optimize_ast(AST_Node* node);
void print_ast(AST_Node* node, int level); //stampa del AST
extern int syntax_errors; //var. globale conteggio errori 

#endif