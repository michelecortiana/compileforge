#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/parser.h"
#include "../include/codegen.h"
#include "../include/backend.h" 

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Errore, devi passare un file sorgente\n");
        printf("Esempio: %s examples/mio_test.c [--no-link]\n", argv[0]);
        return 1;
    }

    const char* file_input = argv[1];
    int no_link = 0;

    // Controlliamo se è stato passato il flag --no-link
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--no-link") == 0) {
            no_link = 1;
        }
    }

    FILE* file = fopen(file_input, "rb");
    if (!file) {
        printf("Errore: impossibile aprire il file '%s'\n", file_input);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

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

    init_parser(codice_sorgente);
    AST_Node* ast = parse_program();

    if (syntax_errors > 0) {
        printf("\nFallimento: Trovati %d errori di sintassi.\n", syntax_errors);
        free(codice_sorgente);
        return 1; 
    }
    printf("[1/3] Parsing sintattico completato.\n");

    optimize_ast(ast);

    // Usiamo PERCORSI RELATIVI. In Docker, WORKDIR è /app, quindi verranno 
    // scritti esattamente nel volume mappato dal worker.
    generate_ir_program(ast);
    print_ir_to_file("output.ir"); 
    printf("[2/3] Generazione IR completata (salvata in output.ir).\n");

    generate_x86_64(ir_head, "output.s"); 
    printf("[3/3] Generazione x86-64 completata (salvata in output.s).\n");

    free(codice_sorgente);

    // Fase 4: Linking Condizionale
    if (!no_link) {
        printf("[4/4] Avvio linking con GCC...\n");
        int ret = system("gcc -z noexecstack -no-pie output.s -o output.exe");
        if (ret != 0) {
            printf("Errore: Il linking con GCC è fallito.\n");
        } else {
            printf("Linking completato (eseguibile: output.exe).\n");
        }
    } else {
        printf("[4/4] Linking ignorato (--no-link attivato).\n");
    }

    printf("\nCompilazione terminata. Pronti per la restituzione.\n");

    return 0;
}