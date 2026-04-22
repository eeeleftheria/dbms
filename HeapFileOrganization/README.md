# Heap File Implementation

A lightweight implementation of a heap-based storage system in C, built on top of a provided block file (BF) layer.

The system stores records sequentially in fixed-size blocks and demonstrates how low-level storage mechanisms work, 
including block management, record organization, and interaction with a buffered storage layer.

## Quick Start

Build and run the BF demo:
```bash
make bf
./build/bf_main
```

Build and run the heap-file demo:
```bash
make hp
./build/hp_main
```
Clean all build and database files:
```bash
make clean
```

## Features

- Create and manage heap files
- Insert records dynamically
- Retrieve records by key
- Maintain file metadata across blocks
- Integrate with a block-level storage layer

## Project Layout

```text
HeapFileOrganization/
    include/
        bf_file.h      # Public block-file API
        hp_file.h      # Public heap-file API
        record.h       # Record definition
    src/
        hp_file.c      # Heap-file implementation
        record.c       # Record utilities
    examples/
        hp_main.c      # Heap-file demo
        bf_main.c      # BF layer demo
    build/           # Generated binaries
```

## Underlying System

This project builds on top of a block file (BF) layer, which acts as a memory manager and serves as a cache between disk and main memory.

It provides:
- Block allocation and access
- Buffered I/O
- Replacement strategies (LRU / MRU)

The heap file functionality is implemented on top of this abstraction.


## Data Model
The heap file management is implemented through the use of functions with the prefix `HP_`. 

A heap file stores records of format:
```c
typedef struct {
    int id;
    char name[20];
    char surname[20];
    char city[20];
} Record;
```

## API Overview

Main functions (declared in `include/hp_file.h`):

- `HP_CreateFile`: Creates and initializes a new heap file
- `HP_OpenFile`: Opens a heap file and loads metadata
- `HP_CloseFile`: Closes the heap file and releases resources
- `HP_InsertEntry`: Inserts a record and returns the block id
- `HP_GetAllEntries`: Prints matching records by `id` and returns blocks read

Return convention:
- `0` on success for create/close operations
- `-1` on failure (`HP_ERROR`)
- Block id or blocks-read count where applicable


