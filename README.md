# Goat Memory Allocator

## Overview

This project implements the Goat memory allocator, a user-space memory management system similar to `malloc` in the C standard library. The allocator provides functions for initialization, memory allocation, freeing memory, and destruction.

## Project Structure

- `goatmalloc.h`: Header file defining the interface and function prototypes.
- `goatmalloc.c`: Implementation of the Goat memory allocator.
- `test_goatmalloc.c`: Suite of test cases to verify the functionality of the allocator.
- `Makefile`: Makefile for building the project.
- `outputref.txt`: Reference output for the provided test cases.

## Getting Started

### Prerequisites

- Ensure you have a C compiler installed (e.g., GCC).

### Building and Running

1. Clone the repository:

    ```bash
    git clone https://github.com/argrabowski/goat-memory-allocator.git
    cd goat-memory-allocator
    ```

2. Build the project using the Makefile:

    ```bash
    make
    ```

3. Run the test cases:

    ```bash
    ./test_goatmalloc
    ```

### Usage

- Include `goatmalloc.h` in your application code.
- Use `init`, `walloc`, `wfree`, and `destroy` functions as described in the header file.

## Project Details

### Initialization and Destruction

- The `init` function initializes the memory allocator, requesting an arena from the OS.
- The `destroy` function releases the arena back to the OS.

### Basic Allocation

- The `walloc` function allocates memory within the arena.
- The `wfree` function frees allocated memory.

### Free Chunk Splitting

- Handles splitting of free chunks when necessary.

### Placement of Allocations

- Maintains a logically ordered chunk list.

### Free Chunk Coalescing

- Coalesces adjacent free chunks when freeing memory.
