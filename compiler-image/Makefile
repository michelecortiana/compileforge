# Compilatore da usare
CC = gcc
# Flag: mostra avvisi (-Wall -Wextra), prepara per il debug (-g) e include gli header (-Iinclude)
CFLAGS = -Wall -Wextra -g -Iinclude
# Nome del file eseguibile finale
TARGET = compilatore

# Regola principale: punta esplicitamente ai file dentro src/
all:
	$(CC) $(CFLAGS) src/main.c src/lexer.c src/parser.c src/codegen.c src/ir.c src/backend.c src/utils.c -o $(TARGET)

# Regola per pulire: quando digiti "make clean", cancella l'eseguibile vecchio
clean:
	rm -f $(TARGET) $(TARGET).exe