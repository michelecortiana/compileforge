#ifndef LEXER_H
#define LEXER_H

//enum con tutti i possibili tipi di token supportati dal compilatore
typedef enum {
    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_CHAR,
    TOKEN_KEYWORD_SHORT,
    TOKEN_KEYWORD_LONG,
    TOKEN_KEYWORD_FLOAT,
    TOKEN_KEYWORD_DOUBLE,
    TOKEN_STRING,
    TOKEN_KEYWORD_RETURN,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_WHILE,
    TOKEN_KEYWORD_FOR,
    TOKEN_KEYWORD_BREAK,     
    TOKEN_KEYWORD_CONTINUE,
    TOKEN_KEYWORD_MALLOC,    
    TOKEN_KEYWORD_FREE,      
    TOKEN_KEYWORD_SIZEOF,    
    
    TOKEN_IDENTIFIER, 
    TOKEN_NUMBER, 
    TOKEN_FLOAT_NUMBER, 
    TOKEN_KEYWORD_PRINTF,
    TOKEN_COMMA, //,
    TOKEN_KEYWORD_STRUCT,
    TOKEN_DOT, //.
    TOKEN_ARROW, //->
    
    TOKEN_ASSIGN, //=         
    TOKEN_EQUAL, //==          
    TOKEN_NOT_EQUAL, //!=      
    TOKEN_LESS, //<          
    TOKEN_LESS_EQUAL, //<=     
    TOKEN_GREATER, //>        
    TOKEN_GREATER_EQUAL, //>=  
    TOKEN_AMPERSAND, //&
    TOKEN_AND, //&&
    TOKEN_OR, //||     
    TOKEN_BITWISE_OR,
    
    TOKEN_SEMICOLON, //;     
    TOKEN_PLUS, //+          
    TOKEN_MINUS, //-          
    TOKEN_STAR, //*          
    TOKEN_SLASH, // /          
    TOKEN_MODULUS, //%        
    TOKEN_LPAREN, //(        
    TOKEN_RPAREN, //)        
    TOKEN_LBRACKET, //[
    TOKEN_RBRACKET, //]
    TOKEN_LBRACE, //{        
    TOKEN_RBRACE, //}        
    TOKEN_EOF //Fine del file (\0)            
} TokenType;

//struttura dati per un singolo token letto dal file sorgente
typedef struct {
    TokenType type; //categoria del token
    char* value; //valore testuale estratto
    int line; //riga per diagnostica errori
    int col; //colonna per diagnostica errori
} Token;

//inizializza lo stato interno del lexer caricando il codice sorgente
void init_lexer(char* source);

Token get_next_token();

#endif