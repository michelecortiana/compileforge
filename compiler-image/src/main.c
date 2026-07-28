#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/backend.h" 

int main(int argc, char** argv) {
    //controllo degli argomenti da terminale
    if (argc < 2) {
        printf("Errore, devi passare un file sorgente\n");
        printf("Esempio: %s examples/mio_test.c\n", argv[0]);
        return 1;
    }

    const char* file_input = argv[1];

    //apertura e lettura dinamica del file sorgente
    FILE* file = fopen(file_input, "rb");
    if (!file) {
        printf("Errore: impossibile aprire il file '%s'\n", file_input);
        return 1;
    }

    //calcolo della dimensione del file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    //allocazione del buffer e lettura
    char* codice_sorgente = malloc(file_size + 1);
    if (!codice_sorgente) {
        printf("Errore: Memoria insufficiente per caricare il file.\n");
        fclose(file);
        return 1;
    }
    fread(codice_sorgente, 1, file_size, file);
    codice_sorgente[file_size] = '\0'; 
    fclose(file);

    printf("--- Avvio Compilatore su: %s ---\n", file_input);

    //flusso di compilazione (Frontend e Backend)
    init_parser(codice_sorgente);
    AST_Node* ast = parse_program();

    if (syntax_errors > 0) {
        printf("\nFallimento: Trovati %d errori di sintassi.\n", syntax_errors);
        free(codice_sorgente);
        return 1; 
    }
    printf("[1/3] Parsing sintattico completato.\n");

    optimize_ast(ast);

    //generazione intermedia e assembly
    generate_ir_program(ast);
    print_ir_to_file("output.ir"); 
    printf("[2/3] Generazione IR completata (salvata in output.ir).\n");

    generate_x86_64(ir_head, "output.s"); 
    printf("[3/3] Generazione x86-64 completata (salvata in output.s).\n");

    //pulizia della stringa sorgente
    free(codice_sorgente);

    //automaizzazione di GCC
    printf("\nInvocazione di GCC per l'assemblaggio...\n");
    
    //il compilatore lancia il comando di linking per conto dell'utente
    // Aggiungi -z noexecstack
int gcc_status = system("gcc -z noexecstack -no-pie output.s -o programma_eseguibile");
    
    if (gcc_status == 0) {
        printf("\nCompilazione completata senza errori!\n");
        printf("-> Per testarlo digita: ./programma_eseguibile\n\n");
    } else {
        printf("\nErrore: Il linking con GCC è fallito.\n");
    }

    return 0;
}