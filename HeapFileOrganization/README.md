# Heap File Implementation

A lightweight implementation of a heap-based storage system in C, built on top of a provided block file (BF) layer.

The system stores records sequentially in fixed-size blocks and demonstrates how low-level storage mechanisms work, 
including block management, record organization, and interaction with a buffered storage layer.

## Features

- Create and manage heap files
- Insert records dynamically
- Retrieve records by key
- Maintain file metadata across blocks
- Integrate with a block-level storage layer

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


## Build & Run

To compile and run the example program that demonstrates the BF library:
```bash
make bf
./build/bf_main
```

To compile and run the example program implemented with HP
```bash
make hp
./build/hp_main
```
