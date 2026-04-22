#ifndef BP_FILE_H
#define BP_FILE_H
#include <record.h>
#include <stdbool.h>



typedef struct {
    int file_desc;              // file descriptor 
    int root_block;             // root block
    int height;                 // B+ tree height
    int record_size;            // size of one record
    int key_size;               // key size
    int max_records_per_block;  // maximum number of records per block
    int max_keys_per_index;     // maximum number of keys per index block
} BPLUS_INFO;


/** BP_CreateFile: This function is used to create and initialize
 * an empty B+ tree file with the name fileName. During file
 * creation, BP_info metadata is also stored in the first block
 * of the file. On success it returns 0, otherwise it returns -1.**/
int BP_CreateFile(char *fileName);

/** BP_OpenFile: This function opens the B+ tree file with the name
 * fileName and retrieves its metadata. BP_info is the returned
 * structure and contains the B+ tree metadata. The parameter
 * *file_desc is the file descriptor obtained from BF_OpenFile.**/
BPLUS_INFO* BP_OpenFile(char *fileName, int *file_desc);

/** BP_CloseFile: This function closes the B+ tree file identified
 * by file_desc. On success it returns 0, otherwise it returns -1.
 * On a successful close, it also frees the memory used by the
 * BP_info structure, which stores the B+ tree format in memory.**/
int BP_CloseFile(int file_desc,BPLUS_INFO* info);

/** BP_InsertEntry: This function inserts a new record into the
 * B+ tree structure. The file is identified by file_desc, the
 * B+ tree metadata is in BP_info, and the record to insert is
 * provided by the Record structure. The function locates the
 * appropriate data node for the record key and inserts it,
 * applying required changes to keep the tree balanced. If the
 * insertion succeeds, it returns the blockId where the record
 * was inserted; otherwise it returns -1.**/
int BP_InsertEntry(int file_desc,BPLUS_INFO* bplus_info, Record record);

/** BP_GetEntry: This function searches for a record in a B+ tree
 * with a key that matches the specified id. Starting from the
 * root, it traverses the B+ tree to locate the relevant leaf
 * where the key should be stored. If a matching record is found,
 * the function sets result to point to that record and returns 0
 * to indicate success. If no such record exists, the function
 * sets result to nullptr and returns -1 to indicate failure.**/
int BP_GetEntry(int file_desc, BPLUS_INFO* header_info, int id, Record** result);

// Returns the block id where the key should be inserted
int BP_FindDataBlockToInsert(int fd, int key, int root, int height_of_current_root);

void BP_PrintBlock(int fd, int block_id, BPLUS_INFO* bplus_info);

void BP_PrintTree(int fd, BPLUS_INFO* bplus_info);

void BP_SetInfo(int fd, BPLUS_INFO* bplus_info, int max_records_per_data, int max_keys_per_index);





#endif 
