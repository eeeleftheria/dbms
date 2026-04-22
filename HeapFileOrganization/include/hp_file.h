#ifndef HP_FILE_H
#define HP_FILE_H
#include <record.h>

#define HP_ERROR -1 

/* The HP_info structure stores metadata related to the heap file */
typedef struct {
    int last_block;
    int number_of_blocks;
    int records_per_block;
    int first_block_with_records;
} HP_info;

/* Information stored for each block */
typedef struct {
    int number_of_records;
    int current_block_capacity;
    int next_block;
} HP_block_info;

/*
 * HP_CreateFile is used to create and properly initialize
 * an empty heap file with the given fileName.
 * On success, it returns 0, otherwise -1.
 */
int HP_CreateFile(
    char *fileName /* file name */);

/*
 * HP_OpenFile opens the file with the given filename and reads
 * the heap file metadata from the first block.
 *
 * It then initializes a structure containing all necessary
 * information about the file so that its records can be accessed.
 *
 * The variable *file_desc refers to the file descriptor returned
 * by BF_OpenFile.
 */
HP_info* HP_OpenFile(char *fileName,    /* file name */  
                     int *file_desc    /* file descriptor returned by BF_OpenFile */
                    );

/*
 * HP_CloseFile closes the file identified by file_desc.
 * On success, it returns 0, otherwise -1.
 *
 * The function is also responsible for freeing the memory
 * occupied by the header_info structure if the file is
 * successfully closed.
 */
int HP_CloseFile(int file_desc,         /* file descriptor returned by BF_OpenFile */
                 HP_info* header_info   /* file metadata structure */
                 );

/*
 * HP_InsertEntry inserts a record into the heap file.
 *
 * file_desc identifies the file,
 * header_info contains the file metadata,
 * and record is the entry to be inserted.
 *
 * On success, it returns the block ID where the record
 * was inserted. On failure, it returns -1.
 */
int HP_InsertEntry(
    int file_desc,
    HP_info* header_info, /* file header */
    Record record /* record to insert */ );

/*
 * HP_GetAllEntries prints all records in the heap file
 * whose key field (id) matches the given value.
 *
 * header_info contains the file metadata as returned by HP_OpenFile.
 *
 * For each matching record, its contents are printed,
 * including the key field.
 *
 * The function also returns the number of blocks read
 * until all matching records are found.
 *
 * On success, it returns the number of blocks read.
 * On failure, it returns -1.
 */
int HP_GetAllEntries(
    int file_desc,
    HP_info* header_info, /* file header */
    int id                /* id value to search for */);

#endif // HP_FILE_H