CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -Iinclude
TARGET = compilatore

all:
	$(CC) $(CFLAGS) src/main.c src/lexer.c src/parser.c src/codegen.c src/ir.c src/backend.c src/utils.c -o $(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe