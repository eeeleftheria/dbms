# B+ Tree Implementation

Implementation of a B+ tree on top of the BF (Block File) layer.

## Build And Run

Use the provided Makefile targets:

- `make`: build all binaries (`bplus_main`, `bp_test1`, `bp_test2`, `bp_test3`)
- `make bp_main`: run the main demo
- `make test1`: run test 1
- `make test2`: run test 2
- `make test3`: run test 3
- `make clean`: remove generated binaries and `.db` files

## Project Architecture

The tree is stored in fixed-size blocks managed by BF.

- Block `0`: file-level metadata (`BPLUS_INFO`)
- Data blocks: metadata (`BPLUS_DATA_NODE`) + sorted `Record` values
- Index blocks: metadata (`BPLUS_INDEX_NODE`) + interleaved pointers/keys

Both block types start with `is_data_node` as the first field:

- `1` => data block
- `0` => index block

This lets the implementation detect block type by reading the first `int` of a block.

## Core Data Structures

### Record

```c
typedef struct Record {
    int id;
    char name[15];
    char surname[20];
    char city[20];
} Record;
```

### File Metadata (`BPLUS_INFO`)

```c
typedef struct {
    int file_desc;
    int root_block;
    int height;                 // height=1 means data-block level
    int record_size;
    int key_size;
    int max_records_per_block;
    int max_keys_per_index;
} BPLUS_INFO;
```

### Data Block Metadata (`BPLUS_DATA_NODE`)

```c
typedef struct {
    int is_data_node;
    int num_records;
    int block_id;
    int next_block;
    int minKey;
    int parent_id;
} BPLUS_DATA_NODE;
```

### Index Block Metadata (`BPLUS_INDEX_NODE`)

```c
typedef struct {
    int is_data_node;
    int num_keys;
    int block_id;
    int parent_id;
} BPLUS_INDEX_NODE;
```

## Block Layout

### Data Block

```text
+-------------------------+
| BPLUS_DATA_NODE         |
+-------------------------+
| Record[0]               |
| Record[1]               |
| ...                     |
+-------------------------+
```

Records inside a data block are kept sorted by key (`Record.id`).

### Index Block

```text
+-------------------------+
| BPLUS_INDEX_NODE        |
+-------------------------+
| ptr0 | key0 | ptr1 | ...|
+-------------------------+
```

Pointers and keys are stored in interleaved form after metadata.

## API Summary

### File-Level Operations

- `BP_CreateFile`: creates and initializes a new B+ tree file
- `BP_OpenFile`: opens file and returns metadata handle
- `BP_CloseFile`: closes file
- `BP_SetInfo`: overrides capacity parameters used for testing

### Tree Operations

- `BP_InsertEntry`: inserts a new record into the tree
- `BP_GetEntry`: finds a record by id
- `BP_FindDataBlockToInsert`: traverses tree to locate target data block

### Data-Node Helpers

- `create_data_node`: allocates a new data node block and initializes metadata
- `get_metadata_datanode`: accesses metadata of a data block
- `insert_rec_in_datanode`: inserts a record in sorted order within a data block
- `split_data_node`: splits a full data block and redistributes records
- `print_data_node`: prints data-block metadata and records

### Index-Node Helpers

- `create_index_node`: allocates a new index node block and initializes metadata
- `get_metadata_indexnode`: accesses metadata of an index block
- `insert_key_indexnode`: inserts a key and corresponding pointer into an index block
- `split_index_node`: splits full index node and promotes middle key
- `update_parents`: updates parent ids after index split
- `is_full_indexnode`: checks if an index node is full
- `print_index_node`: prints index-node metadata and key-pointer sequence








