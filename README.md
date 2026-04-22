# Storage Systems in C

This repository shows how data is stored and accessed on disk by implementing two basic storage structures in C.

The projects focus on handling data in blocks and managing memory efficiently.

## 🗂️ Projects Overview

### Heap-Based Storage
A simple way to store records sequentially in fixed-size blocks.

- Creates files and inserts records  
- Reads data by scanning all blocks  
- Shows how unordered data is stored  

### B+ Tree Indexing
A structure that makes searching for records faster.

- Organizes data using a tree  
- Finds records by key efficiently  
- Handles splitting nodes as data grows  

## ⚙️ Underlying Architecture

Both projects use a provided **block file layer**, which helps manage how data is stored on disk and loaded into memory.

It is responsible for:
- Creating and accessing blocks  
- Keeping some blocks in memory  
- Handling I/O to disk  

## How to run
Each project contains its own `Makefile` and `README` where you can find all the necessary information on how to run each project. 

## 👥  Team
- Stefanos Tzaferis
- Eleftheria Galiatsatou
