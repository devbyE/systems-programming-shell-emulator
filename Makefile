CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude

SRC = src/main.c src/shell.c src/redirect.c src/shell_pipe.c src/job_control.c
OBJ = build_obj/main.o build_obj/shell.o build_obj/redirect.o build_obj/shell_pipe.o build_obj/job_control.o
TARGET = bin/myshell
ROOT_LINK = myshell

all: $(TARGET) link

$(TARGET): $(OBJ)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(ROOT_LINK): $(TARGET)
	ln -sf $(TARGET) $(ROOT_LINK)

link: $(TARGET)
	ln -sf $(TARGET) $(ROOT_LINK)

build_obj/main.o: src/main.c include/shell.h include/shell_pipe.h
	mkdir -p build_obj
	$(CC) $(CFLAGS) -c src/main.c -o build_obj/main.o

build_obj/shell.o: src/shell.c include/shell.h include/redirect.h
	mkdir -p build_obj
	$(CC) $(CFLAGS) -c src/shell.c -o build_obj/shell.o

build_obj/redirect.o: src/redirect.c include/redirect.h include/shell.h
	mkdir -p build_obj
	$(CC) $(CFLAGS) -c src/redirect.c -o build_obj/redirect.o

build_obj/shell_pipe.o: src/shell_pipe.c include/shell_pipe.h include/shell.h
	mkdir -p build_obj
	$(CC) $(CFLAGS) -c src/shell_pipe.c -o build_obj/shell_pipe.o

build_obj/job_control.o: src/job_control.c include/job_control.h include/shell.h
	mkdir -p build_obj
	$(CC) $(CFLAGS) -c src/job_control.c -o build_obj/job_control.o

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f build_obj/*.o bin/myshell myshell
