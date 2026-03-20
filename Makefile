CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude
SRC = src/main.c src/shell.c src/redirect.c
OBJ = build_obj/main.o build_obj/shell.o build_obj/redirect.o
TARGET = bin/myshell

all: $(TARGET)

$(TARGET): $(OBJ) | build_obj bin
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

build_obj:
	mkdir -p build_obj

bin:
	mkdir -p bin

build_obj/main.o: src/main.c include/shell.h | build_obj
	$(CC) $(CFLAGS) -c src/main.c -o build_obj/main.o

build_obj/shell.o: src/shell.c include/shell.h include/redirect.h | build_obj
	$(CC) $(CFLAGS) -c src/shell.c -o build_obj/shell.o

build_obj/redirect.o: src/redirect.c include/redirect.h include/shell.h | build_obj
	$(CC) $(CFLAGS) -c src/redirect.c -o build_obj/redirect.o

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f build_obj/*.o bin/myshell
