Team Members    : Efrem Wilkerson, Zinian Wang
Contributors    : Efrem Wilkerson, Zinian Wang
Institution     : Georgetown University
Platform        : Linux / UNIX / MSYS2
Files           : 1. src/main.c
                  2. src/shell.c
                  3. include/shell.h
                  4. Makefile
                  5. bin/myshell
                  6. build_obj/main.o
                  7. build_obj/shell.o
                  8. documents/README.TXT

Version         : 1.0 (Phase I)

Description
This project is a basic Unix shell implemented in C for the Systems Programming final project.
Phase I supports basic command execution, built-in commands, and simple error handling.

Features
- Displays a shell prompt
- Reads user input
- Parses command-line arguments
- Executes external commands
- Supports built-in commands:
  - exit
  - cd
  - pwd
- Handles invalid commands with an error message

Compilation
- Open terminal in the project root directory
- Run: make

Execution
- Run: ./bin/myshell

Testing
Example commands tested:
- ls
- pwd
- cd /
- echo hello
- nonexistent_command
- exit

Result
- Project compiles successfully with no warnings
- Phase I shell executes required commands correctly