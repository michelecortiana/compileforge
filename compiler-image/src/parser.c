#include "../include/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

int syntax_errors = 0; //contatore globale errori sintattici
//stato interno del parser
static Token current_token;
static Token previous_token;
static char* parser_source;

//token successivo
void advance(){
    previous_token = current_token;
    current_token = get_next_token();
}
//verifica se token corrisponde a tipo di dato
int is_type_keyword(TokenType type) {
    return (type == TOKEN_KEYWORD_INT || type == TOKEN_KEYWORD_CHAR || 
            type == TOKEN_KEYWORD_SHORT || type == TOKEN_KEYWORD_LONG ||
            type == TOKEN_KEYWORD_FLOAT || type == TOKEN_KEYWORD_DOUBLE || type == TOKEN_KEYWORD_STRUCT); 
}

//allocazione e inizializzazione nuovo tipo di dato
AST_Node* create_node(AST_NodeType type){
    AST_Node* node = calloc(1, sizeof(AST_Node));
    node->type = type;
    node->line = previous_token.line;
    node->col = previous_token.col;
    
    return node;
}

//inizializzione parser e lexer
void init_parser(char* source) {
    parser_source = source;
    init_lexer(source);
    previous_token.line = 1;
    previous_token.col = 1;
    
    advance(); 
}

//sincronizzazione stato per recupero errori(continua ad analizzare il codice e fa un resoconto totale degli errori invce di bloccarsi al primo)
void synchronize() {
    advance();
    
    while (current_token.type != TOKEN_EOF) {
        if (previous_token.type == TOKEN_SEMICOLON) return;
        switch (current_token.type) {
            case TOKEN_KEYWORD_INT:
            case TOKEN_KEYWORD_CHAR:
            case TOKEN_KEYWORD_SHORT:
            case TOKEN_KEYWORD_LONG:
            case TOKEN_KEYWORD_FLOAT:
            case TOKEN_KEYWORD_DOUBLE:
            case TOKEN_KEYWORD_STRUCT:
            case TOKEN_KEYWORD_IF:
            case TOKEN_KEYWORD_WHILE:
            case TOKEN_KEYWORD_BREAK:    
            case TOKEN_KEYWORD_CONTINUE:
            case TOKEN_KEYWORD_FOR:
            case TOKEN_KEYWORD_RETURN:
            case TOKEN_KEYWORD_PRINTF:
            case TOKEN_KEYWORD_FREE:
            case TOKEN_RBRACE:
                return; 
            default:
                break;
        }
        advance();
    }
}
//gestione e stampa errori sintattici
void error_at_token(Token t, const char* message) {
    printf("\n[ERRORE DI SINTASSI] %s (Riga %d, Colonna %d)\n", message, t.line, t.col);
    
    //ricerca inzio riga contenente l'errore
    char* line_start = parser_source;
    int current_line = 1;
    while (current_line < t.line && *line_start != '\0') {
        if (*line_start == '\n') {
            current_line++;
        }
        line_start++;
    }
    //ricerca fine righe
    char* line_end = line_start;
    while (*line_end != '\n' && *line_end != '\0') {
        line_end++;
    }
    
    //stampo riga di codice con errore + cursore
    int line_length = line_end - line_start;
    printf("\n    %.*s\n", line_length, line_start);
    printf("    "); 
    for (int i = 1; i < t.col; i++) {
        printf(" ");
    }
    printf("\033[32m^\033[0m\n\n");
    //ripresa analisi per altri errori
    syntax_errors++; 
    synchronize();   
}
//gestione e stampa errori semantici
void semantic_error(AST_Node* node, const char* message) {
    printf("\n[ERRORE SEMANTICO] %s (Riga %d, Colonna %d)\n", message, node->line, node->col);

    char* line_start = parser_source;
    int current_line = 1;
    while (current_line < node->line && *line_start != '\0') {
        if (*line_start == '\n') current_line++;
        line_start++;
    }
    
    char* line_end = line_start;
    while (*line_end != '\n' && *line_end != '\0') {
        line_end++;
    }
    
    int line_length = line_end - line_start;
    printf("\n    %.*s\n", line_length, line_start);
    printf("    "); 
    for (int i = 1; i < node->col; i++) {
        printf(" ");
    }
    printf("\033[31m^\033[0m\n\n");
    
    exit(1);
}
//traduzione token in stringhe per i messaggi errore
const char* get_token_name(TokenType type) {
    switch (type) {
        case TOKEN_KEYWORD_INT: return "la parola chiave 'int'";
        case TOKEN_KEYWORD_CHAR: return "la parola chiave 'char'";
        case TOKEN_KEYWORD_SHORT: return "la parola chiave 'short'";
        case TOKEN_KEYWORD_LONG: return "la parola chiave 'long'";
        case TOKEN_KEYWORD_FLOAT: return "la parola chiave 'float'";
        case TOKEN_KEYWORD_DOUBLE: return "la parola chiave 'double'";
        case TOKEN_KEYWORD_STRUCT: return "la parola chiave 'struct'";
        case TOKEN_KEYWORD_IF: return "la parola chiave 'if'";
        case TOKEN_KEYWORD_ELSE: return "la parola chiave 'else'";
        case TOKEN_KEYWORD_WHILE: return "la parola chiave 'while'";
        case TOKEN_KEYWORD_FOR: return "la parola chiave 'for'";
        case TOKEN_KEYWORD_BREAK: return "la parola chiave 'break'";       
        case TOKEN_KEYWORD_CONTINUE: return "la parola chiave 'continue'";
        case TOKEN_KEYWORD_RETURN: return "la parola chiave 'return'";
        case TOKEN_KEYWORD_PRINTF: return "la chiamata 'printf'";
        case TOKEN_IDENTIFIER: return "un identificatore (variabile o funzione)";
        case TOKEN_NUMBER: return "un numero intero";
        case TOKEN_FLOAT_NUMBER: return "un numero decimale";
        case TOKEN_STRING: return "una stringa testuale";
        case TOKEN_SEMICOLON: return "il punto e virgola ';'";
        case TOKEN_COMMA: return "la virgola ','";
        case TOKEN_LPAREN: return "la parentesi aperta '('";
        case TOKEN_RPAREN: return "la parentesi chiusa ')'";
        case TOKEN_LBRACE: return "la parentesi graffa aperta '{'";
        case TOKEN_RBRACE: return "la parentesi graffa chiusa '}'";
        case TOKEN_LBRACKET: return "la parentesi quadra aperta '['";
        case TOKEN_RBRACKET: return "la parentesi quadra chiusa ']'";
        case TOKEN_ASSIGN: return "l'uguale '='";
        case TOKEN_PLUS: return "il più '+'";
        case TOKEN_MINUS: return "il meno '-'";
        case TOKEN_STAR: return "l'asterisco '*'";
        case TOKEN_SLASH: return "la barra '/'";
        case TOKEN_EOF: return "la fine del file";
        case TOKEN_KEYWORD_MALLOC: return "la parola chiave 'malloc'";
        case TOKEN_KEYWORD_FREE: return "la parola chiave 'free'";
        case TOKEN_KEYWORD_SIZEOF: return "la parola chiave 'sizeof'";
        default: return "un simbolo non riconosciuto";
    }
}
//verifica token atteso sennò genera errore
void expect(TokenType type) {
    if (current_token.type == type) {
        advance();
    } else {
        char err_msg[256];
        sprintf(err_msg, "Mi aspettavo %s, ma ho trovato %s ('%s')", 
                get_token_name(type), 
                get_token_name(current_token.type), 
                current_token.value ? current_token.value : "Simbolo");
                
        error_at_token(current_token, err_msg);
    }
}
//dichiarazione anticipate 
AST_Node* parse_expression();
AST_Node* parse_relational();   
AST_Node* parse_logical_and();  
AST_Node* parse_comparison();
AST_Node* parse_block();

//parsing fatori base 
AST_Node* parse_factor(){
    Token token = current_token;
    //gestione operatore indirizzo
    if(token.type == TOKEN_AMPERSAND){
        advance();
        AST_Node* node = create_node(AST_ADDR);
        node->data.unary.expr = parse_factor(); 
        return node;
    }
    //gestione operatore dereferenziazione
    if (token.type == TOKEN_STAR) {
        advance(); 
        AST_Node* node = create_node(AST_DEREF);
        node->data.unary.expr = parse_factor();
        return node;
    }
    //parsing chiamata a malloc
    if (token.type == TOKEN_KEYWORD_MALLOC) {
        advance();
        AST_Node* node = create_node(AST_MALLOC);
        expect(TOKEN_LPAREN);
        node->data.malloc_expr.size_expr = parse_comparison(); 
        expect(TOKEN_RPAREN);
        return node;
    }
    //parsing sizeof
    if (token.type == TOKEN_KEYWORD_SIZEOF) {
        advance();
        AST_Node* node = create_node(AST_SIZEOF);
        expect(TOKEN_LPAREN);
        
        if (is_type_keyword(current_token.type)) {
            //gestione grandezza struct
            if (current_token.type == TOKEN_KEYWORD_STRUCT) {
                node->data.sizeof_expr.type_val = TYPE_STRUCT;
                advance(); 
                if (current_token.type != TOKEN_IDENTIFIER) {
                    error_at_token(current_token, "Mi aspettavo il nome della struct dentro sizeof");
                }
                node->data.sizeof_expr.struct_name = my_strdup(current_token.value);
                advance(); 
            } else {
                //mappatura tipi primitivi
                if(current_token.type==TOKEN_KEYWORD_INT) node->data.sizeof_expr.type_val=TYPE_INT;
                else if(current_token.type==TOKEN_KEYWORD_CHAR) node->data.sizeof_expr.type_val=TYPE_CHAR;
                else if(current_token.type==TOKEN_KEYWORD_SHORT) node->data.sizeof_expr.type_val=TYPE_SHORT;
                else if(current_token.type==TOKEN_KEYWORD_LONG) node->data.sizeof_expr.type_val=TYPE_LONG;
                else if(current_token.type==TOKEN_KEYWORD_FLOAT) node->data.sizeof_expr.type_val=TYPE_FLOAT; 
                else if(current_token.type==TOKEN_KEYWORD_DOUBLE) node->data.sizeof_expr.type_val=TYPE_DOUBLE;
                advance(); 
                node->data.sizeof_expr.struct_name = NULL;
            }
            
            //verifica se tipo indicato è puntatore
            node->data.sizeof_expr.is_pointer = 0;
            if (current_token.type == TOKEN_STAR) {
                node->data.sizeof_expr.is_pointer = 1;
                advance(); 
            }
        } else {
            error_at_token(current_token, "L'operatore 'sizeof' richiede un tipo di dato valido (es. int, struct Punto)");
        }
        expect(TOKEN_RPAREN);
        return node;
    }
    //gestione interi
    if(token.type == TOKEN_NUMBER){
        advance();
        AST_Node* node = create_node(AST_NUM);
        node->data.num.is_float = 0; 
        node->data.num.value.int_val = atoi(token.value);
        return node;
    }
    //gestione decimali
    else if(token.type == TOKEN_FLOAT_NUMBER){
        advance();
        AST_Node* node = create_node(AST_FLOAT);
        node->data.num.is_float = 1; 
        node->data.num.value.float_val = atof(token.value);
        return node;
    }
    //parsing identificatori
    else if(token.type == TOKEN_IDENTIFIER){ 
        char* name = my_strdup(token.value);
        advance();
        //verifica accesso array
        if(current_token.type == TOKEN_LBRACKET){
            advance();
            AST_Node* index = parse_comparison();
            expect(TOKEN_RBRACKET); 
            AST_Node* node = create_node(AST_ARRAY_ACCESS);
            node->data.array_access.array_name=name;
            node->data.array_access.index_expr=index;
            return node;
        }
        //verifico chiamata a funzione
        if(current_token.type == TOKEN_LPAREN){
            advance(); 
            AST_Node* node = create_node(AST_FUNC_CALL);
            node->data.func_call.func_name = name;
            node->data.func_call.args = NULL;
            //parsing argomenti funzione
            if(current_token.type != TOKEN_RPAREN){
                node->data.func_call.args = parse_comparison();
                AST_Node* current_arg = node->data.func_call.args;

                //parsing altri argomenti
                while(current_token.type == TOKEN_COMMA){
                    advance(); 
                    current_arg->next = parse_comparison();
                    current_arg = current_arg->next;
                }
            }
            expect(TOKEN_RPAREN); 
            return node;
        } else { 
            //nodo variabile
            AST_Node* node = create_node(AST_VAR);
            node->data.var_name = name;
            //gestione accesso ai membri struct tramite ->
            while (current_token.type == TOKEN_DOT || current_token.type == TOKEN_ARROW) {
                int is_ptr_access = (current_token.type == TOKEN_ARROW) ? 1 : 0;
                advance(); 
                
                if (current_token.type != TOKEN_IDENTIFIER) {
                    error_at_token(current_token, "Mi aspettavo il nome del membro della struct dopo il punto o la freccia");
                }
                char* member = my_strdup(current_token.value);
                advance(); 

                AST_Node* access_node = create_node(AST_MEMBER_ACCESS);
                access_node->data.member_access.object = node;
                access_node->data.member_access.member_name = member;
                access_node->data.member_access.is_pointer = is_ptr_access;
                node = access_node; 
            }
            return node;
        }
    }
    //gestione espressioni tra parentesi
    else if(token.type == TOKEN_LPAREN){
        advance();
        //verifica dereferenziazione esplicita 
        int is_paren_deref = 0;
        if (token.type == TOKEN_LPAREN && current_token.type == TOKEN_STAR) {
            is_paren_deref = 1;
            advance(); 
        }

        AST_Node* node;
        if (is_paren_deref) {
            //valutazione dereferenziazione interna
            AST_Node* inner = parse_factor();
            node = create_node(AST_DEREF);
            node->data.unary.expr = inner;
        } else {
            node = parse_comparison(); 
        }
        expect(TOKEN_RPAREN); 

        //accesso membri su espressioni tra parentesi
        while (current_token.type == TOKEN_DOT || current_token.type == TOKEN_ARROW) {
            int is_ptr_access = (current_token.type == TOKEN_ARROW) ? 1 : 0;
            advance(); 
            
            if (current_token.type != TOKEN_IDENTIFIER) {
                error_at_token(current_token, "Mi aspettavo il nome del membro della struct dopo la parentesi");
            }
            char* member = my_strdup(current_token.value);
            advance();
            
            AST_Node* access_node = create_node(AST_MEMBER_ACCESS);
            access_node->data.member_access.object = node;
            access_node->data.member_access.member_name = member;
            access_node->data.member_access.is_pointer = is_ptr_access;
            node = access_node;
        }
        return node;
    }
    //gestione stringhe
    else if(token.type==TOKEN_STRING){
        advance();
        AST_Node* node = create_node(AST_STRING);
        node->data.string_val=my_strdup(token.value);
        return node;
    }

    error_at_token(token, "Fattore imprevisto (mi aspettavo un numero, una variabile o un'espressione)");
    return NULL; 
}

//parsing molt div e modulo
AST_Node* parse_term(){
    AST_Node* node = parse_factor();
    //gestione operatori con stessa precedenza e associtività a sinistra
    while (current_token.type == TOKEN_STAR || current_token.type == TOKEN_SLASH || current_token.type==TOKEN_MODULUS) {
        TokenType op = current_token.type;
        advance();
        //parsing fattore destro dell'operazione
        AST_Node* right = parse_factor();
        //creo nodo per operazione binaria
        AST_Node* binop_node = create_node(AST_BINOP);
        binop_node->data.binop.op = op;
        binop_node->data.binop.left = node;
        binop_node->data.binop.right = right;
        
        node = binop_node; 
    }
    return node;
}

//parsing add e sub
AST_Node* parse_expression() {
    AST_Node* node = parse_term();

    //gestione operatori add e sub
    while (current_token.type == TOKEN_PLUS || current_token.type == TOKEN_MINUS) {
        TokenType op = current_token.type;
        advance();
        AST_Node* right = parse_term();
        
        AST_Node* binop_node = create_node(AST_BINOP);
        binop_node->data.binop.op = op;
        binop_node->data.binop.left = node;
        binop_node->data.binop.right = right;
        
        node = binop_node;
    }
    return node;
}

//parsing epressioni relazionali e uguaglianza
AST_Node* parse_relational() {
    AST_Node* node = parse_expression(); 
    //gestione operatori confronto
    while (current_token.type == TOKEN_EQUAL || current_token.type == TOKEN_NOT_EQUAL || 
           current_token.type == TOKEN_LESS || current_token.type == TOKEN_LESS_EQUAL || 
           current_token.type == TOKEN_GREATER || current_token.type == TOKEN_GREATER_EQUAL) { 
        
        TokenType op = current_token.type;
        advance();
        AST_Node* right = parse_expression();
        
        AST_Node* binop_node = create_node(AST_BINOP);
        binop_node->data.binop.op = op;
        binop_node->data.binop.left = node;
        binop_node->data.binop.right = right;

        node = binop_node;
    }
    return node;
}

//parsing op.logiche AND
AST_Node* parse_logical_and() {
    AST_Node* node = parse_relational(); 
    while (current_token.type == TOKEN_AND) {
        TokenType op = current_token.type;
        advance();
        AST_Node* right = parse_relational();

        AST_Node* binop_node = create_node(AST_BINOP);
        binop_node->data.binop.op = op;
        binop_node->data.binop.left = node;
        binop_node->data.binop.right = right;

        node = binop_node;
    }
    return node;
}

//parsing op.logiche OR
AST_Node* parse_comparison() {
    AST_Node* node = parse_logical_and(); 
    while (current_token.type == TOKEN_OR) {
        TokenType op = current_token.type;
        advance();
        AST_Node* right = parse_logical_and();

        AST_Node* binop_node = create_node(AST_BINOP);
        binop_node->data.binop.op = op;
        binop_node->data.binop.left = node;
        binop_node->data.binop.right = right;

        node = binop_node;
    }
    return node;
}

//parsing istruzioni
AST_Node* parse_statement() {
    //istruzione return
    if (current_token.type == TOKEN_KEYWORD_RETURN) {
        advance();
        AST_Node* node = create_node(AST_RETURN);
        node->data.ret.expr = parse_comparison();
        expect(TOKEN_SEMICOLON); 
        return node;
    }
    //istruzione break
    else if (current_token.type == TOKEN_KEYWORD_BREAK) {
        advance();
        AST_Node* node = create_node(AST_BREAK);
        expect(TOKEN_SEMICOLON); 
        return node;
    }
    //istruzione continue
    else if (current_token.type == TOKEN_KEYWORD_CONTINUE) {
        advance();
        AST_Node* node = create_node(AST_CONTINUE);
        expect(TOKEN_SEMICOLON); 
        return node;
    }
    //gestione istruzione free per deallocazione memoria
    else if (current_token.type == TOKEN_KEYWORD_FREE) {
        advance();
        AST_Node* node = create_node(AST_FREE);
        expect(TOKEN_LPAREN);
        node->data.free_stmt.ptr_expr = parse_comparison(); 
        expect(TOKEN_RPAREN);
        expect(TOKEN_SEMICOLON); 
        return node;
    }
    //gestione dichiarazione variabili e struct
    else if (is_type_keyword(current_token.type)) {
        DataType current_base_type;
        char* custom_struct_name = NULL; 
        //caso struct
        if (current_token.type == TOKEN_KEYWORD_STRUCT) {
            current_base_type = TYPE_STRUCT;
            advance(); 
            
            if (current_token.type != TOKEN_IDENTIFIER) {
                error_at_token(current_token, "Mi aspettavo il nome della struct");
            }
            custom_struct_name = my_strdup(current_token.value);
            advance(); 
            //corpo struct
            if (current_token.type == TOKEN_LBRACE) {
                AST_Node* def_node = create_node(AST_STRUCT_DEF);
                def_node->data.struct_def.struct_name = custom_struct_name;
                def_node->data.struct_def.fields = parse_block(); 
                expect(TOKEN_SEMICOLON); 
                return def_node;
            }
        } 
        else { 
            //gestione tipi primitivi
            if(current_token.type==TOKEN_KEYWORD_INT) current_base_type=TYPE_INT;
            else if(current_token.type==TOKEN_KEYWORD_CHAR) current_base_type=TYPE_CHAR;
            else if(current_token.type==TOKEN_KEYWORD_SHORT) current_base_type=TYPE_SHORT;
            else if(current_token.type==TOKEN_KEYWORD_LONG) current_base_type=TYPE_LONG;
            else if(current_token.type==TOKEN_KEYWORD_FLOAT) current_base_type=TYPE_FLOAT; 
            else if(current_token.type==TOKEN_KEYWORD_DOUBLE) current_base_type=TYPE_DOUBLE; 
            
            advance(); 
        } 
        
        int pointer_flag = 0;
        //verifico se tipo è puntatore
        if (current_token.type == TOKEN_STAR) {
            advance();
            pointer_flag = 1;
        }
        if (current_token.type != TOKEN_IDENTIFIER) {
            error_at_token(current_token, "Mi aspettavo il nome della variabile o della funzione dopo la dichiarazione del tipo");
        }
        
        char* name = my_strdup(current_token.value);
        advance();
        //verifica dichiarazione funzione
        if (current_token.type == TOKEN_LPAREN) {
            advance();
            AST_Node* node = create_node(AST_FUNC_DECL);
            node->data.func_decl.func_name = name;
            node->data.func_decl.params = NULL;
            node->data.func_decl.return_type = current_base_type;
            //parsing parametri funzione
            if (current_token.type != TOKEN_RPAREN) {
                if (!is_type_keyword(current_token.type)) {
                    error_at_token(current_token, "Mi aspettavo un tipo di dato valido per il parametro della funzione");
                }
                
                DataType param_base_type = TYPE_INT;
                char* param_struct_name = NULL;
                //identificazione tipo di dato del primo parametro
                if(current_token.type==TOKEN_KEYWORD_INT) {
                    param_base_type=TYPE_INT;
                    advance();
                }
                else if(current_token.type==TOKEN_KEYWORD_CHAR) {
                    param_base_type=TYPE_CHAR;
                    advance();
                }
                else if(current_token.type==TOKEN_KEYWORD_SHORT) {
                    param_base_type=TYPE_SHORT;
                    advance();
                }
                else if(current_token.type==TOKEN_KEYWORD_LONG) {
                    param_base_type=TYPE_LONG;
                    advance();
                }
                else if(current_token.type==TOKEN_KEYWORD_FLOAT) {
                    param_base_type=TYPE_FLOAT;
                    advance();
                }   
                else if(current_token.type==TOKEN_KEYWORD_DOUBLE) {
                    param_base_type=TYPE_DOUBLE;
                    advance();
                } 
                else if(current_token.type==TOKEN_KEYWORD_STRUCT) {
                    param_base_type = TYPE_STRUCT;
                    advance(); 
                    if (current_token.type != TOKEN_IDENTIFIER) {
                        error_at_token(current_token, "Mi aspettavo il nome della struct nel parametro della funzione");
                    }
                    param_struct_name = my_strdup(current_token.value);
                    advance(); 
                } else {
                    advance(); 
                }
                //verifico se primo parametro puntatore
                int param_pointer_flag = 0;
                if (current_token.type == TOKEN_STAR) {
                    advance();
                    param_pointer_flag = 1;
                }

                AST_Node* param = create_node(AST_VAR_DECL);
                param->data.var_decl.var_name = my_strdup(current_token.value);
                param->data.var_decl.is_pointer = param_pointer_flag;
                param->data.var_decl.expr = NULL;
                param->data.var_decl.base_type = param_base_type; 
                param->data.var_decl.struct_name = param_struct_name; 
                advance();
                //assegno parametro alla funzione
                node->data.func_decl.params = param;
                AST_Node* curr_param = param;
                //gestione parametri multipli
                while (current_token.type == TOKEN_COMMA) {
                    advance();
                    if (!is_type_keyword(current_token.type)) {
                        error_at_token(current_token, "Mi aspettavo un tipo di dato dopo la virgola nei parametri della funzione");
                    }
                    
                    char* next_param_struct_name = NULL;
                    //identificazione tipo di dato del parametro successivo
                    if(current_token.type==TOKEN_KEYWORD_INT) {
                        param_base_type=TYPE_INT;
                        advance();
                    }
                    else if(current_token.type==TOKEN_KEYWORD_CHAR) {
                        param_base_type=TYPE_CHAR;
                        advance();
                    }
                    else if(current_token.type==TOKEN_KEYWORD_SHORT) {
                        param_base_type=TYPE_SHORT;
                        advance();
                    }
                    else if(current_token.type==TOKEN_KEYWORD_LONG) {
                        param_base_type=TYPE_LONG;
                        advance();
                    }
                    else if(current_token.type==TOKEN_KEYWORD_FLOAT) {
                        param_base_type=TYPE_FLOAT;
                        advance();
                    }   
                    else if(current_token.type==TOKEN_KEYWORD_DOUBLE) {
                        param_base_type=TYPE_DOUBLE;
                        advance();
                    } 
                    else if(current_token.type==TOKEN_KEYWORD_STRUCT) {
                        param_base_type = TYPE_STRUCT;
                        advance(); 
                        if (current_token.type != TOKEN_IDENTIFIER) {
                            error_at_token(current_token, "Mi aspettavo il nome della struct nel parametro");
                        }
                        next_param_struct_name = my_strdup(current_token.value);
                        advance(); 
                    } else {
                        advance(); 
                    }
                    //verifico se parametro successivo è puntatore   
                    param_pointer_flag = 0;
                    if (current_token.type == TOKEN_STAR) {
                        advance();
                        param_pointer_flag = 1;
                    }
                    //creo collegamento del nodo per il nuovo parametro
                    curr_param->next = create_node(AST_VAR_DECL);
                    curr_param->next->data.var_decl.var_name = my_strdup(current_token.value);
                    curr_param->next->data.var_decl.is_pointer = param_pointer_flag;
                    curr_param->next->data.var_decl.expr = NULL;
                    curr_param->next->data.var_decl.base_type = param_base_type; 
                    curr_param->next->data.var_decl.struct_name = next_param_struct_name; 
                        
                    advance();
                    curr_param = curr_param->next;
                }
            }
            //chiusura lista parametri e parsing corpo funzione
            expect(TOKEN_RPAREN); 
            node->data.func_decl.func_body = parse_block(); 
            return node; 
        }
        //gestione dichiarazione di variabili o array
        else {
            //nodo per variabile
            AST_Node* node = create_node(AST_VAR_DECL);
            node->data.var_decl.var_name = name;
            node->data.var_decl.is_pointer = pointer_flag;
            node->data.var_decl.base_type = current_base_type;
            node->data.var_decl.struct_name = custom_struct_name; 
            //controllo se è array e parsing dimensione
            if(current_token.type == TOKEN_LBRACKET){
                advance();
                if (current_token.type != TOKEN_NUMBER) {
                    error_at_token(current_token, "La dimensione dell'array deve essere un numero intero");
                }
                int size = atoi(current_token.value);
                advance(); 
                
                expect(TOKEN_RBRACKET); 
                node->data.var_decl.array_size = size;
            } else {
                node->data.var_decl.array_size = 0;
            }
            //gestione dell'inizializzaione della variabile
            if (current_token.type == TOKEN_ASSIGN) {
                advance();
                node->data.var_decl.expr = parse_comparison();
            } else {
                node->data.var_decl.expr = NULL;
            }
            expect(TOKEN_SEMICOLON);
            return node;
        }
    }
    //gestione identificatori per assegnazioni a struct array o variabili
    else if (current_token.type == TOKEN_IDENTIFIER) {
        char* var_name = my_strdup(current_token.value);
        advance();
        //gestione accesso ai membri di struct con punto o freccia
        if (current_token.type == TOKEN_DOT || current_token.type == TOKEN_ARROW) {
            AST_Node* base_var = create_node(AST_VAR);
            base_var->data.var_name = var_name;
            
            AST_Node* current_obj = base_var;
            char* final_member = NULL;
            int final_is_ptr = 0;
            //gestione accessi concatenati ai membri della struct
            while (current_token.type == TOKEN_DOT || current_token.type == TOKEN_ARROW) {
                int is_ptr = (current_token.type == TOKEN_ARROW) ? 1 : 0;
                advance(); 
                
                if (current_token.type != TOKEN_IDENTIFIER) {
                    error_at_token(current_token, "Mi aspettavo il nome di un membro della struct nell'assegnazione");
                }
                char* current_member = my_strdup(current_token.value);
                advance(); 
                //membro d'assegnare
                if (current_token.type == TOKEN_ASSIGN) {
                    final_member = current_member;
                    final_is_ptr = is_ptr;
                    break;
                } else {
                    AST_Node* intermediate_access = create_node(AST_MEMBER_ACCESS);
                    intermediate_access->data.member_access.object = current_obj;
                    intermediate_access->data.member_access.member_name = current_member;
                    intermediate_access->data.member_access.is_pointer = is_ptr;
                    
                    current_obj = intermediate_access; 
                }
            }
            expect(TOKEN_ASSIGN); 
            //creazione nodo finale
            AST_Node* node = create_node(AST_MEMBER_ASSIGN);
            node->data.member_assign.object = current_obj;  
            node->data.member_assign.member_name = final_member;
            node->data.member_assign.is_pointer = final_is_ptr;
            node->data.member_assign.expr = parse_comparison(); 
            
            expect(TOKEN_SEMICOLON); 
            return node;
        }
        //gestione dell'assegnazione agli elementi di un array
        else if(current_token.type == TOKEN_LBRACKET){
            AST_Node* node = create_node(AST_ARRAY_ASSIGN);
            node->data.array_write.array_name = var_name;
            advance(); 
            //parsing indice array
            node->data.array_write.index_expr = parse_comparison();
            
            expect(TOKEN_RBRACKET); 
            expect(TOKEN_ASSIGN);   
            //parsing valore da assegnare all'array
            node->data.array_write.value_expr = parse_comparison(); 
            
            expect(TOKEN_SEMICOLON); 
            return node;
        }
        //gestione assegnazione semplice a variabile
        else {
            expect(TOKEN_ASSIGN);
            
            AST_Node* node = create_node(AST_ASSIGN);
            node->data.assign.var_name = var_name;
            node->data.assign.expr = parse_comparison();
            
            expect(TOKEN_SEMICOLON);
            return node;
        }
    }
    //gestione assegnazioni con dereferenziazioni o parentesi
    else if (current_token.type == TOKEN_STAR || current_token.type == TOKEN_LPAREN) {
        //parsing valoer a sx dell'uguale
        AST_Node* lvalue_expr = parse_factor(); 
        //gestione accessi ai membri dopo dereferenziazione
        while (current_token.type == TOKEN_DOT || current_token.type == TOKEN_ARROW) {
            int is_ptr_access = (current_token.type == TOKEN_ARROW) ? 1 : 0;
            advance();
            if (current_token.type != TOKEN_IDENTIFIER) {
                error_at_token(current_token, "Mi aspettavo il nome del membro della struct");
            }
            char* member = my_strdup(current_token.value);
            advance();
            
            AST_Node* access_node = create_node(AST_MEMBER_ACCESS);
            access_node->data.member_access.object = lvalue_expr;
            access_node->data.member_access.member_name = member;
            access_node->data.member_access.is_pointer = is_ptr_access;
            lvalue_expr = access_node;
        }
        expect(TOKEN_ASSIGN);
        AST_Node* right_expr = parse_comparison();
        expect(TOKEN_SEMICOLON);
        //risoluzione del tipo di assegnazione
        if (lvalue_expr->type == AST_MEMBER_ACCESS) {
            AST_Node* node = create_node(AST_MEMBER_ASSIGN);
            node->data.member_assign.object = lvalue_expr->data.member_access.object;
            node->data.member_assign.member_name = lvalue_expr->data.member_access.member_name;
            node->data.member_assign.is_pointer = lvalue_expr->data.member_access.is_pointer;
            node->data.member_assign.expr = right_expr;
            return node;
        } else if (lvalue_expr->type == AST_DEREF) {
            AST_Node* node = create_node(AST_PTR_ASSIGN);
            //soluzione temporanea nel caso in cui il puntatore potrebbe necessitare risoluzione dinamica
            node->data.ptr_assign.ptr_name = my_strdup("ptr_deref"); 
            node->data.ptr_assign.expr = right_expr;
            return node;
        } else {
            error_at_token(current_token, "L-value non valido a sinistra dell'uguale");
            return NULL;
        }
    }
    //gestione istruzioni condizionali
    else if(current_token.type == TOKEN_KEYWORD_IF){
        advance();
        AST_Node* node = create_node(AST_IF);
        //if
        expect(TOKEN_LPAREN);
        node->data.if_stmt.condition = parse_comparison();
        expect(TOKEN_RPAREN);
        //ramo true
        node->data.if_stmt.true_branch=parse_block();
        //parsing opzionale ramo else
        if(current_token.type == TOKEN_KEYWORD_ELSE){
            advance();
            node->data.if_stmt.false_branch = parse_block();
        }else{
            node->data.if_stmt.false_branch=NULL; 
        }
        return node;
    }
    //gesione while
    else if(current_token.type == TOKEN_KEYWORD_WHILE){
        advance();
        AST_Node* node = create_node(AST_WHILE);
        //parsing condizione d'uscita
        expect(TOKEN_LPAREN);
        node->data.while_stmt.condition = parse_comparison();
        expect(TOKEN_RPAREN);
        //parsing corpo ciclo
        node->data.while_stmt.loop_body=parse_block();
        return node;
    }
    //gestione for
    else if(current_token.type == TOKEN_KEYWORD_FOR){
        advance();
        AST_Node* node = create_node(AST_FOR);
        expect(TOKEN_LPAREN); 
        //parsing istruzione inizializzazione
        node->data.for_stmt.init = parse_statement(); 
        //parsing condizione di permanenza nel ciclo
        node->data.for_stmt.condition = parse_comparison();
        expect(TOKEN_SEMICOLON); 
        //parsing istruzione d'incrmeneto
        if (current_token.type == TOKEN_IDENTIFIER) {
            char* var_name = my_strdup(current_token.value);
            advance();
            expect(TOKEN_ASSIGN);
            AST_Node* inc_node = create_node(AST_ASSIGN);
            inc_node->data.assign.var_name = var_name;
            inc_node->data.assign.expr = parse_comparison();
            node->data.for_stmt.increment = inc_node;
        } else {
            error_at_token(current_token, "Mi aspettavo l'assegnazione per l'incremento del ciclo FOR");
        }
        expect(TOKEN_RPAREN); 
        //parsing corpo ciclo
        node->data.for_stmt.loop_body = parse_block(); 
        
        return node;
    }
    //gestione funzione stampa
    else if(current_token.type == TOKEN_KEYWORD_PRINTF){
        advance();
        AST_Node* node = create_node(AST_PRINTF);

        expect(TOKEN_LPAREN); 
        //primo argomento deve essere stringa di formattazione
        if (current_token.type != TOKEN_STRING) {
            error_at_token(current_token, "La funzione 'printf' richiede una stringa letterale come primo argomento");
        }
        node->data.printf_stmt.format_str = my_strdup(current_token.value);
        advance(); 
        //parsing argomenti opzionali
        if (current_token.type == TOKEN_COMMA) {
            advance(); 
            node->data.printf_stmt.expr = parse_comparison(); 
            AST_Node* current_arg = node->data.printf_stmt.expr;
            while (current_token.type == TOKEN_COMMA) {
                advance(); 
                current_arg->next = parse_comparison();
                current_arg = current_arg->next;
            }
        } else {
            node->data.printf_stmt.expr = NULL; 
        }

        expect(TOKEN_RPAREN); 
        expect(TOKEN_SEMICOLON); 
        return node;
    }

    error_at_token(current_token, "Istruzione non valida o simbolo imprevisto (forse manca un punto e virgola sopra?)");
    return NULL;
}
//parsing blocco di codice racchiuso tra graffe
AST_Node* parse_block(){
    AST_Node* head = NULL;
    AST_Node* tail = NULL;
    expect(TOKEN_LBRACE);
    //cicla fino a } oppure fine file
    while(current_token.type != TOKEN_RBRACE && current_token.type != TOKEN_EOF){
        AST_Node* stmt = parse_statement();
        //aggiunge nuova istruzione alla fine della lista
        if (stmt != NULL) {
            if (head == NULL) {
                head = stmt;
                tail = stmt;
            } else {
                tail->next = stmt;
                tail = stmt;
            }
        }
    }
    expect(TOKEN_RBRACE);
    return head;
}
//ingresso parser costruisce albero sintattico
AST_Node* parse_program() {
    AST_Node* head = NULL;
    AST_Node* tail = NULL;
    //contiuna a parsare istruzioni globali fino alla fine file
    while (current_token.type != TOKEN_EOF) {
        AST_Node* stmt = parse_statement();
        if (stmt != NULL) {
            if (head == NULL) {
                head = stmt;
                tail = stmt;
            } else {
                tail->next = stmt;
                tail = stmt;
            }
        }
    }
    return head;
}
//converte l'enum del tipo di dato in stringa 
const char* get_type_name(DataType type) {
    switch(type) {
        case TYPE_CHAR: return "char";
        case TYPE_SHORT: return "short";
        case TYPE_INT: return "int";
        case TYPE_LONG: return "long";
        case TYPE_FLOAT: return "float";   
        case TYPE_DOUBLE: return "double"; 
        case TYPE_STRUCT: return "struct"; 
        default: return "unknown";
    }
}
//ottimizzazione albero sintattico
void optimize_ast(AST_Node* node) {
    //caso base ricorsione nodo
    if (!node) return;
    //attraversamento ricorsivo di tutti i figli del nodo corrente
    switch (node->type) {
        case AST_BINOP:
            optimize_ast(node->data.binop.left);
            optimize_ast(node->data.binop.right);
            break;
        case AST_VAR_DECL:
            if (node->data.var_decl.expr) optimize_ast(node->data.var_decl.expr);
            break;
        case AST_ASSIGN:
            optimize_ast(node->data.assign.expr);
            break;
        case AST_RETURN:
            if (node->data.ret.expr) optimize_ast(node->data.ret.expr);
            break;
        case AST_IF:
            //otimizza prima l'espressione della condizione
            optimize_ast(node->data.if_stmt.condition);
            //dead code elimination se condizione è valore noto
            if (node->data.if_stmt.condition != NULL && node->data.if_stmt.condition->type == AST_NUM) {
                int cond_val = node->data.if_stmt.condition->data.num.value.int_val;
                
                if (cond_val != 0) {
                    //condizione true
                    printf("[Ottimizzazione] Dead Code Elimination: Condizione IF sempre VERA. Tengo solo il ramo True.\n");
                    optimize_ast(node->data.if_stmt.true_branch);
                    
                    AST_Node* true_branch = node->data.if_stmt.true_branch;
                    node->type = true_branch->type;
                    node->data = true_branch->data;
                    //ricolegga resto del programma alla fine del ramo true
                    AST_Node* old_next = node->next;
                    node->next = true_branch->next;
                    AST_Node* tail = node;
                    while (tail->next != NULL) {
                        tail = tail->next;
                    }
                    tail->next = old_next;
                    
                } else {
                    //condizione false
                    printf("[Ottimizzazione] Dead Code Elimination: Condizione IF sempre FALSA. Rimuovo il ramo True.\n");
                    if (node->data.if_stmt.false_branch != NULL) {
                        optimize_ast(node->data.if_stmt.false_branch);
                        AST_Node* false_branch = node->data.if_stmt.false_branch;
                        node->type = false_branch->type;
                        node->data = false_branch->data;
                        
                        AST_Node* old_next = node->next;
                        node->next = false_branch->next;
                        
                        AST_Node* tail = node;
                        while (tail->next != NULL) {
                            tail = tail->next;
                        }
                        tail->next = old_next;
                    } 
                }
            } else {
                //se condzione non è costante otimizza entrambi i rami
                optimize_ast(node->data.if_stmt.true_branch);
                optimize_ast(node->data.if_stmt.false_branch);
            }
            break;
        case AST_WHILE:
            optimize_ast(node->data.while_stmt.condition);
            optimize_ast(node->data.while_stmt.loop_body);
            break;
        case AST_FOR:
            if (node->data.for_stmt.init) optimize_ast(node->data.for_stmt.init);
            if (node->data.for_stmt.condition) optimize_ast(node->data.for_stmt.condition);
            if (node->data.for_stmt.increment) optimize_ast(node->data.for_stmt.increment);
            if (node->data.for_stmt.loop_body) optimize_ast(node->data.for_stmt.loop_body);
            break;
        case AST_FUNC_DECL:
            optimize_ast(node->data.func_decl.func_body);
            break;
        case AST_PRINTF: {
            //attraversa la lista concatenata degli argomenti del printf
            AST_Node* arg = node->data.printf_stmt.expr;
            while (arg != NULL) {
                AST_Node* next_arg = arg->next;
                arg->next = NULL; //scollega per evitare ricorsioni infinite
                optimize_ast(arg);
                arg->next = next_arg;
                arg = next_arg;
            }
            break;
        }
        case AST_FUNC_CALL: {
            //attraversa lista concatenata degli argomenti passati alla funzione
            AST_Node* arg = node->data.func_call.args;
            while (arg != NULL) {
                AST_Node* next_arg = arg->next;
                arg->next = NULL;
                optimize_ast(arg);
                arg->next = next_arg;
                arg = next_arg;
            }
            break;
        }
        case AST_ARRAY_ASSIGN:
            optimize_ast(node->data.array_write.index_expr);
            optimize_ast(node->data.array_write.value_expr);
            break;
        case AST_ARRAY_ACCESS:
            optimize_ast(node->data.array_access.index_expr);
            break;
        case AST_PTR_ASSIGN:
            if (node->data.ptr_assign.expr) optimize_ast(node->data.ptr_assign.expr);
            break;
        case AST_ADDR:
        case AST_DEREF:
            if (node->data.unary.expr) optimize_ast(node->data.unary.expr);
            break;
        case AST_MEMBER_ACCESS:
            if (node->data.member_access.object) optimize_ast(node->data.member_access.object);
            break;
        case AST_CAST:
            if (node->data.cast.expr) optimize_ast(node->data.cast.expr);
            break;
        case AST_MALLOC:
            if (node->data.malloc_expr.size_expr) optimize_ast(node->data.malloc_expr.size_expr);
            break;
        case AST_FREE:
            if (node->data.free_stmt.ptr_expr) optimize_ast(node->data.free_stmt.ptr_expr);
            break;
        case AST_MEMBER_ASSIGN:
            if (node->data.member_assign.object) optimize_ast(node->data.member_assign.object);
            if (node->data.member_assign.expr) optimize_ast(node->data.member_assign.expr);
            break;
        default:
            break;
    }
    //fase costant folding se il nodo è un operazione binaria tra costanti
    if (node->type == AST_BINOP && 
        node->data.binop.left != NULL && node->data.binop.left->type == AST_NUM && 
        node->data.binop.right != NULL && node->data.binop.right->type == AST_NUM) {
        //estrae valori interi 
        int val_left = node->data.binop.left->data.num.value.int_val;
        int val_right = node->data.binop.right->data.num.value.int_val;
        int result = 0;
        //esegue calcolo a compile time
        switch (node->data.binop.op) {
            case TOKEN_PLUS:          result = val_left + val_right; break;
            case TOKEN_MINUS:         result = val_left - val_right; break;
            case TOKEN_STAR:          result = val_left * val_right; break;
            case TOKEN_SLASH: 
                //previene divisione per zero durante compile
                if (val_right != 0) result = val_left / val_right;
                else return;
                break;
            case TOKEN_MODULUS: 
                //previene modulo per zero
                if (val_right != 0) result = val_left % val_right;
                else return;
                break;
            case TOKEN_EQUAL:         result = (val_left == val_right); break;
            case TOKEN_NOT_EQUAL:     result = (val_left != val_right); break;
            case TOKEN_LESS:          result = (val_left < val_right);  break;
            case TOKEN_LESS_EQUAL:    result = (val_left <= val_right); break;
            case TOKEN_GREATER:       result = (val_left > val_right);  break;
            case TOKEN_GREATER_EQUAL: result = (val_left >= val_right); break;
            case TOKEN_AND:           result = (val_left != 0 && val_right != 0); break;
            case TOKEN_OR:            result = (val_left != 0 || val_right != 0); break;
            default: return;
        }
        //sostituisce l'op binaria con il risultato calcolato
        node->type = AST_NUM;
        node->data.num.value.int_val = result;
        printf("[Ottimizzazione] Constant folding: %d calcolato.\n", result);
    }
    //passa alla prosiima otimizzazione istruzione
    if (node->next != NULL) optimize_ast(node->next);
}
//funzione ricorsiva per stampare l'AST 
void print_ast(AST_Node* node, int level) {
    if (!node) return;
    //cicla su tutti i nodi
    while(node != NULL) {
        for (int i = 0; i < level; i++) printf("  ");
        //riconosce il tipo di nodo e stampa le proprietà 
        switch (node->type) {
            case AST_NUM:
                printf("└─[Numero: %d]\n", node->data.num.value.int_val);
                break;
            case AST_FLOAT:
                printf("└─[Numero Float: %f]\n", node->data.num.value.float_val);
                break;
            case AST_VAR:
                printf("└─[Variabile: %s]\n", node->data.var_name);
                break;
            case AST_STRING:
                printf("└─[Stringa: \"%s\"]\n", node->data.string_val);
                break;
            case AST_BINOP:
                printf("└─[Operazione: ");
                if (node->data.binop.op == TOKEN_PLUS) printf("+");
                if (node->data.binop.op == TOKEN_MINUS) printf("-");
                if (node->data.binop.op == TOKEN_STAR) printf("*");
                if (node->data.binop.op == TOKEN_SLASH) printf("/");
                if (node->data.binop.op == TOKEN_EQUAL) printf("==");
                if (node->data.binop.op == TOKEN_NOT_EQUAL) printf("!=");
                if (node->data.binop.op == TOKEN_LESS) printf("<");
                if (node->data.binop.op == TOKEN_LESS_EQUAL) printf("<=");
                if (node->data.binop.op == TOKEN_GREATER) printf(">");
                if (node->data.binop.op == TOKEN_GREATER_EQUAL) printf(">=");

                printf("]\n");
                print_ast(node->data.binop.left, level + 1);
                print_ast(node->data.binop.right, level + 1);
                break;

            case AST_VAR_DECL:
                if (node->data.var_decl.base_type == TYPE_STRUCT) {
                    printf("└─[Dichiara struct %s: %s]\n", node->data.var_decl.struct_name, node->data.var_decl.var_name);
                } else {
                    printf("└─[Dichiara %s: %s]\n", get_type_name(node->data.var_decl.base_type), node->data.var_decl.var_name);
                }
                if (node->data.var_decl.expr) print_ast(node->data.var_decl.expr, level + 1);
                break;
                    
            case AST_ASSIGN:
                printf("└─[Assegnazione a: %s]\n", node->data.assign.var_name);
                print_ast(node->data.assign.expr, level + 1);
                break;
            case AST_RETURN:
                printf("└─[Return]\n");
                print_ast(node->data.ret.expr, level + 1);
                break;

           case AST_IF:
                printf("└─[IF Conditional]\n");
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("├─[Condizione]:\n");
                print_ast(node->data.if_stmt.condition, level + 2);
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("├─[Ramo True]:\n");
                print_ast(node->data.if_stmt.true_branch, level + 2);
                if (node->data.if_stmt.false_branch != NULL) {
                    for (int j = 0; j < level + 1; j++) printf("  ");
                    printf("└─[Ramo False (Else)]:\n");
                    print_ast(node->data.if_stmt.false_branch, level + 2);
                }
                break;
            case AST_WHILE:
                printf("└─[WHILE Loop]\n");
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("├─[Condizione]:\n");
                print_ast(node->data.while_stmt.condition, level + 2);
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("└─[Corpo del ciclo]:\n");
                print_ast(node->data.while_stmt.loop_body, level + 2);
                break;
                
            case AST_FOR:
                printf("└─[FOR Loop]\n");
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("├─[Inizializzazione]:\n");
                print_ast(node->data.for_stmt.init, level + 2);
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("├─[Condizione]:\n");
                print_ast(node->data.for_stmt.condition, level + 2);
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("├─[Incremento]:\n");
                print_ast(node->data.for_stmt.increment, level + 2);
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("└─[Corpo del ciclo]:\n");
                print_ast(node->data.for_stmt.loop_body, level + 2);
                break;
            case AST_PRINTF:
                printf("└─[Chiamata a funzione: printf]\n");
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("├─[Formato]: \"%s\"\n", node->data.printf_stmt.format_str);
                if (node->data.printf_stmt.expr != NULL) {
                    for (int j = 0; j < level + 1; j++) printf("  ");
                    printf("└─[Argomenti]:\n");
                    
                    AST_Node* printf_arg = node->data.printf_stmt.expr;
                    while (printf_arg != NULL) {
                        AST_Node* next_arg = printf_arg->next;
                        printf_arg->next = NULL; 
                        print_ast(printf_arg, level + 2);
                        printf_arg->next = next_arg; 
                        
                        printf_arg = next_arg;
                    }
                }
                break;
            case AST_FUNC_DECL:
            {
                const char* ret_name = get_type_name(node->data.func_decl.return_type);
                printf("└─[Dichiarazione Funzione: %s %s()]\n", ret_name, node->data.func_decl.func_name);
                
                AST_Node* param = node->data.func_decl.params;
                if (param != NULL) {
                    for (int j = 0; j < level + 1; j++) printf("  ");
                    printf("├─[Parametri in ingresso]:\n");
                    while (param != NULL) {
                        for (int j = 0; j < level + 2; j++) printf("  ");
                        
                        const char* p_name = get_type_name(param->data.var_decl.base_type);
                        if (param->data.var_decl.is_pointer) {
                            printf("└─ %s* %s\n", p_name, param->data.var_decl.var_name);
                        } else {
                            printf("└─ %s %s\n", p_name, param->data.var_decl.var_name);
                        }
                        param = param->next;
                    }
                }
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("└─[Corpo della Funzione]:\n");
                print_ast(node->data.func_decl.func_body, level + 2);
                break;
            }

            case AST_FUNC_CALL:
                printf("└─[Chiamata a Funzione: %s()]\n", node->data.func_call.func_name);
                
                AST_Node* arg = node->data.func_call.args;
                if (arg != NULL) {
                    for (int j = 0; j < level + 1; j++) printf("  ");
                    printf("└─[Argomenti passati]:\n");
                    while (arg != NULL) {
                        AST_Node* next_arg = arg->next;
                        arg->next = NULL;
                        print_ast(arg, level + 2);
                        arg->next = next_arg;
                        arg = next_arg;
                    }
                }
                break;
            case AST_ADDR:
                printf("└─[Indirizzo di memoria: &]\n");
                print_ast(node->data.unary.expr, level + 1);
                break;

            case AST_DEREF:
                printf("└─[Dereferenziazione: *]\n");
                print_ast(node->data.unary.expr, level + 1);
                break;

            case AST_PTR_ASSIGN:
                printf("└─[Scrittura in Puntatore: *%s]\n", node->data.ptr_assign.ptr_name);
                print_ast(node->data.ptr_assign.expr, level + 1);
                break;
            case AST_ARRAY_ACCESS:
                printf("└─[Lettura da Array: %s[...]]\n", node->data.array_access.array_name);
                
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("└─[Indice]:\n");
                print_ast(node->data.array_access.index_expr, level + 2);
                break;

            case AST_ARRAY_ASSIGN:
                printf("└─[Scrittura in Array: %s[...]]\n", node->data.array_write.array_name);
                
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("├─[Indice]:\n");
                print_ast(node->data.array_write.index_expr, level + 2);
                
                for (int j = 0; j < level + 1; j++) printf("  ");
                printf("└─[Valore da inserire]:\n");
                print_ast(node->data.array_write.value_expr, level + 2);
                break;
            case AST_STRUCT_DEF:
                printf("└─[Definizione Struct: %s]\n", node->data.struct_def.struct_name);
                print_ast(node->data.struct_def.fields, level + 1);
                break;
            case AST_MEMBER_ACCESS:
                printf("└─[Accesso Membro: %s%s%s]\n", 
                        node->data.member_access.is_pointer ? "->" : ".", 
                        node->data.member_access.member_name, "");
                print_ast(node->data.member_access.object, level + 1);
                break;
            case AST_CAST:
                printf("└─[Cast implicito a: %s]\n", get_type_name(node->data.cast.target_type));
                print_ast(node->data.cast.expr, level + 1);
                break;
            
            case AST_BREAK:
                printf("└─[Break]\n");
                break;
            case AST_CONTINUE:
                printf("└─[Continue]\n");
                break;
            case AST_MALLOC:
                printf("└─[Allocazione Heap: malloc]\n");
                print_ast(node->data.malloc_expr.size_expr, level + 1);
                break;
            case AST_FREE:
                printf("└─[Deallocazione Heap: free]\n");
                print_ast(node->data.free_stmt.ptr_expr, level + 1);
                break;
            case AST_SIZEOF:
                if (node->data.sizeof_expr.type_val == TYPE_STRUCT) {
                    printf("└─[Sizeof: struct %s%s]\n", node->data.sizeof_expr.struct_name, node->data.sizeof_expr.is_pointer ? "*" : "");
                } else {
                    printf("└─[Sizeof: %s%s]\n", get_type_name(node->data.sizeof_expr.type_val), node->data.sizeof_expr.is_pointer ? "*" : "");
                }
                break;
            case AST_MEMBER_ASSIGN:
                printf("└─[Scrittura in Membro: %s%s]\n", 
                        node->data.member_assign.is_pointer ? "->" : ".", 
                        node->data.member_assign.member_name);
                print_ast(node->data.member_assign.object, level + 1); 
                print_ast(node->data.member_assign.expr, level + 1);
                break;
        }
        node = node->next;
    }
}