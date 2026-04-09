CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude

SRC = src/main.c src/shell.c src/redirect.c src/shell_pipe.c
OBJ = build_obj/main.o build_obj/shell.o build_obj/redirect.o build_obj/shell_pipe.o
TARGET = bin/myshell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

build_obj/main.o: src/main.c include/shell.h include/shell_pipe.h
	$(CC) $(CFLAGS) -c src/main.c -o build_obj/main.o

build_obj/shell.o: src/shell.c include/shell.h include/redirect.h
	$(CC) $(CFLAGS) -c src/shell.c -o build_obj/shell.o

build_obj/redirect.o: src/redirect.c include/redirect.h include/shell.h
	$(CC) $(CFLAGS) -c src/redirect.c -o build_obj/redirect.o

build_obj/shell_pipe.o: src/shell_pipe.c include/shell_pipe.h include/shell.h
	$(CC) $(CFLAGS) -c src/shell_pipe.c -o build_obj/shell_pipe.o

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f build_obj/*.o bin/myshell