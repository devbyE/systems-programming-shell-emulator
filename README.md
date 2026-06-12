# Shell Emulator

A Unix-style shell emulator written in C for a systems programming course. The project implements command execution, input/output redirection, pipes, command chaining, and basic job control.

## Overview

This shell provides a command-line interface that can run external programs and handle common shell behavior. It was built to practice low-level systems programming concepts such as process creation, file descriptors, signals, and inter-process communication.

## Features

- Execute external commands
- Support input and output redirection
- Support command piping
- Support command chaining
- Support background jobs
- Basic job control behavior
- Modular C source structure with separate files for shell logic, piping, redirection, and job control

## Project Structure

```text
include/       Header files
src/           Source files
documents/     Project notes and documentation
Makefile       Build instructions
README.md      Project overview