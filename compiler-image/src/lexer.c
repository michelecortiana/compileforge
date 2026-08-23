#include "../include/lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"

//stato interno del lexer
static char* source_code;
static int current_pos = 0; 
static int current_line = 1; 
static int current_col = 1;  

//inizializza il lexer resettando i contatori
void init_lexer(char* source) {
    source_code = source;
    current_pos = 0;
    current_line = 1;
    current_col = 1;
}

//utility per creare token e calcolare l'avanzamento della colonna
static Token make_token(TokenType type, char* value, int start_line, int start_col, int start_pos) {
    Token token;
    token.type = type;
    token.value = value;
    token.line = start_line;
    token.col = start_col;
    
    //aggiorna la colonna globale in base ai caratteri consumati
    current_col = start_col + (current_pos - start_pos);
    return token;
}

Token get_next_token() {
    //salta spazi vuoti e commenti
    while (1) {
        //gestione del fine riga
        if (isspace(source_code[current_pos])) {
            if (source_code[current_pos] == '\n') {
                current_line++;
                current_col = 1;
            } else {
                current_col++;
            }
            current_pos++;
        } 
        //riconosce i commenti a singola riga
        else if (source_code[current_pos] == '/' && source_code[current_pos + 1] == '/') {
            //Ignora tutto fino a fine riga o fine file
            while (source_code[current_pos] != '\n' && source_code[current_pos] != '\0') {
                current_pos++; 
            }
        } 
        else {
            break; //trovato codice valido, esce dal loop
        }
    }
    //salva le coordinate iniziali di questo token per gli errori
    int start_col = current_col;
    int start_line = current_line;
    int start_pos = current_pos;

    //controllo di fine file
    if(source_code[current_pos] == '\0'){
        return make_token(TOKEN_EOF, NULL, start_line, start_col, start_pos); 
    }

    char c = source_code[current_pos]; //lettura carattere corrente

    //gestione operatori di confronto e assegnamento
    if (c == '=') {
        if (source_code[current_pos + 1] == '=') {
            current_pos += 2;
            return make_token(TOKEN_EQUAL, NULL, start_line, start_col, start_pos);
        }
        current_pos++;
        return make_token(TOKEN_ASSIGN, NULL, start_line, start_col, start_pos);
    }
    
    if (c == '!') {
        if (source_code[current_pos + 1] == '=') {
            current_pos += 2;
            return make_token(TOKEN_NOT_EQUAL, NULL, start_line, start_col, start_pos);
        }
    }
    
    if (c == '<') {
        if (source_code[current_pos + 1] == '=') {
            current_pos += 2;
            return make_token(TOKEN_LESS_EQUAL, NULL, start_line, start_col, start_pos);
        }
        current_pos++;
        return make_token(TOKEN_LESS, NULL, start_line, start_col, start_pos);
    }
    
    if (c == '>') {
        if (source_code[current_pos + 1] == '=') {
            current_pos += 2;
            return make_token(TOKEN_GREATER_EQUAL, NULL, start_line, start_col, start_pos);
        }
        current_pos++;
        return make_token(TOKEN_GREATER, NULL, start_line, start_col, start_pos);
    }

    //lettura stringhe letterali
    if(c == '"'){
        current_pos++; //salto virgole d'apertura
        
        int capacity = 256; //capacità inziale buffer
        char* str_val = malloc(capacity);
        int length = 0;

        //ciclo estrazione contenuto stringa
        while (source_code[current_pos] != '"' && source_code[current_pos] != '\0') {
            if (source_code[current_pos] == '\\') {
                str_val[length++] = source_code[current_pos++];
                
                //previene letture fuori limite
                if (source_code[current_pos] == '\0') break;
                str_val[length++] = source_code[current_pos];
            } else {
                str_val[length++] = source_code[current_pos];
            }
            
            //rialocazione dinamica se buffer è full
            if (length >= capacity - 2) {
                capacity *= 2;
                str_val = realloc(str_val, capacity);
            }
            current_pos++;
        }
    
        str_val[length] = '\0';  //termina stringa

        if (source_code[current_pos] == '"') {
            current_pos++;
        }
        return make_token(TOKEN_STRING, str_val, start_line, start_col, start_pos);
    }

    //gestione punteggiatura e operatori a singolo carattere
    if (c == ',') { current_pos++; return make_token(TOKEN_COMMA, my_strdup(","), start_line, start_col, start_pos); }
    if (c == ';') { current_pos++; return make_token(TOKEN_SEMICOLON, NULL, start_line, start_col, start_pos); }
    if (c == '+') { current_pos++; return make_token(TOKEN_PLUS, NULL, start_line, start_col, start_pos); }
    //gestione '-' e dell'accesso a membro freccia
    if (c == '-') { 
        if (source_code[current_pos + 1] == '>') {
            current_pos += 2;
            return make_token(TOKEN_ARROW, NULL, start_line, start_col, start_pos);
        }
        current_pos++; 
        return make_token(TOKEN_MINUS, NULL, start_line, start_col, start_pos); 
    }
    if (c == '*') { current_pos++; return make_token(TOKEN_STAR, NULL, start_line, start_col, start_pos); }
    if (c == '/') { current_pos++; return make_token(TOKEN_SLASH, NULL, start_line, start_col, start_pos); }
    if (c == '%') { current_pos++; return make_token(TOKEN_MODULUS, NULL, start_line, start_col, start_pos); }
    if (c == '(') { current_pos++; return make_token(TOKEN_LPAREN, NULL, start_line, start_col, start_pos); }
    if (c == ')') { current_pos++; return make_token(TOKEN_RPAREN, NULL, start_line, start_col, start_pos); }
    if (c == '[') { current_pos++; return make_token(TOKEN_LBRACKET, NULL, start_line, start_col, start_pos); }
    if (c == ']') { current_pos++; return make_token(TOKEN_RBRACKET, NULL, start_line, start_col, start_pos); }
    if (c == '{') { current_pos++; return make_token(TOKEN_LBRACE, NULL, start_line, start_col, start_pos); }
    if (c == '}') { current_pos++; return make_token(TOKEN_RBRACE, NULL, start_line, start_col, start_pos); }
    if (c == '.') { current_pos++; return make_token(TOKEN_DOT, NULL, start_line, start_col, start_pos); }
    
    //gestione operatori logici e AND/OR bit a bit 
    if (c == '&') {
        if (source_code[current_pos + 1] == '&') {
            current_pos += 2; 
            return make_token(TOKEN_AND, my_strdup("&&"), start_line, start_col, start_pos);
        }
        //and bit a bit 
        current_pos++;
        return make_token(TOKEN_AMPERSAND, my_strdup("&"), start_line, start_col, start_pos);
    }
    if (c == '|') {
        if (source_code[current_pos + 1] == '|') {
            current_pos += 2; 
            return make_token(TOKEN_OR, my_strdup("||"), start_line, start_col, start_pos);
        }
        current_pos++;
        return make_token(TOKEN_BITWISE_OR, my_strdup("|"), start_line, start_col, start_pos);
    }

    //lettura numeri letterali(int e decimali)
    if (isdigit(c)) {
        int length = 0;
        int has_dot = 0;  //flag per numeri float
        char buffer[128]; //buffer temporaneeo

        //max un punto decimale 
        while ((isdigit(c) || (c == '.' && has_dot == 0)) && length < 127) {
            if (c == '.') {
                has_dot = 1; 
            }
            buffer[length] = c;
            length++;
            current_pos++; 
            c = source_code[current_pos]; 
        }
        
        //salto cifre eccesso se buffer full
        while (isdigit(c) || (c == '.' && has_dot == 0)) {
            current_pos++;
            c = source_code[current_pos];
        }
        buffer[length] = '\0'; //termino stringa
        
        //classifica token finale in caso del punto decimale 
        if (has_dot) {
            return make_token(TOKEN_FLOAT_NUMBER, my_strdup(buffer), start_line, start_col, start_pos);
        } else {
            return make_token(TOKEN_NUMBER, my_strdup(buffer), start_line, start_col, start_pos); 
        }
    }

    //gestione identificatori e keyword
    if (isalpha(c) || c == '_') {
        while (isalnum(source_code[current_pos]) || source_code[current_pos] == '_') {
            current_pos++;
        }
        //estraggo stringa         
        int length = current_pos - start_pos;
        
        // 👇 FIX SICUREZZA: Limite imposto agli identificatori
        if (length > 63) {
            printf("Errore lessicale: identificatore troppo lungo (max 63 caratteri) alla riga %d, colonna %d\n", current_line, current_col);
            exit(1);
        }
        
        char* text = malloc(length + 1);
        strncpy(text, &source_code[start_pos], length);
        text[length] = '\0';

        //corrispondenza stringa -> keyword
        if (strcmp(text, "return") == 0) return make_token(TOKEN_KEYWORD_RETURN, text, start_line, start_col, start_pos);
        if (strcmp(text, "int") == 0)    return make_token(TOKEN_KEYWORD_INT, text, start_line, start_col, start_pos);
        if (strcmp(text, "char") == 0)   return make_token(TOKEN_KEYWORD_CHAR, text, start_line, start_col, start_pos);
        if (strcmp(text, "short") == 0)  return make_token(TOKEN_KEYWORD_SHORT, text, start_line, start_col, start_pos);
        if (strcmp(text, "long") == 0)   return make_token(TOKEN_KEYWORD_LONG, text, start_line, start_col, start_pos);
        if (strcmp(text, "float") == 0)  return make_token(TOKEN_KEYWORD_FLOAT, text, start_line, start_col, start_pos);
        if (strcmp(text, "double") == 0) return make_token(TOKEN_KEYWORD_DOUBLE, text, start_line, start_col, start_pos);
        if (strcmp(text, "if") == 0)     return make_token(TOKEN_KEYWORD_IF, text, start_line, start_col, start_pos);
        if (strcmp(text, "else") == 0)   return make_token(TOKEN_KEYWORD_ELSE, text, start_line, start_col, start_pos);
        if (strcmp(text, "while") == 0)  return make_token(TOKEN_KEYWORD_WHILE, text, start_line, start_col, start_pos);
        if (strcmp(text, "for") == 0)    return make_token(TOKEN_KEYWORD_FOR, text, start_line, start_col, start_pos);
        if (strcmp(text, "break") == 0)  return make_token(TOKEN_KEYWORD_BREAK, text, start_line, start_col, start_pos);    
        if (strcmp(text, "continue") == 0) return make_token(TOKEN_KEYWORD_CONTINUE, text, start_line, start_col, start_pos); 
        if (strcmp(text, "malloc") == 0) return make_token(TOKEN_KEYWORD_MALLOC, text, start_line, start_col, start_pos);
        if (strcmp(text, "free") == 0)   return make_token(TOKEN_KEYWORD_FREE, text, start_line, start_col, start_pos);
        if (strcmp(text, "sizeof") == 0) return make_token(TOKEN_KEYWORD_SIZEOF, text, start_line, start_col, start_pos);
        if (strcmp(text, "printf") == 0) return make_token(TOKEN_KEYWORD_PRINTF, text, start_line, start_col, start_pos);
        if (strcmp(text, "struct") == 0) return make_token(TOKEN_KEYWORD_STRUCT, text, start_line, start_col, start_pos);
        
        //se non è una keyword è identificatore 
        return make_token(TOKEN_IDENTIFIER, text, start_line, start_col, start_pos);
    }
    printf("Errore lessicale: carattere sconosciuto '%c' alla riga %d, colonna %d\n", c, current_line, current_col);
    exit(1); 
}