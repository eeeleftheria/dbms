#ifndef BF_H
#define BF_H

#ifdef __cplusplus
extern "C" {
#endif

#define BF_BLOCK_SIZE 512      /* The size of a block in bytes */
#define BF_BUFFER_SIZE 100     /* Max number of blocks in memory */
#define BF_MAX_OPEN_FILES 100  /* Max number of open files */

typedef enum BF_ErrorCode {
  BF_OK,
  BF_OPEN_FILES_LIMIT_ERROR,     /* BF_MAX_OPEN_FILES files already open */
  BF_INVALID_FILE_ERROR,         /* File descriptor does not correspond to any open file */
  BF_ACTIVE_ERROR,               /* BF layer is already initialized and cannot be initialized again */
  BF_FILE_ALREADY_EXISTS,        /* File cannot be created because it already exists */
  BF_FULL_MEMORY_ERROR,          /* Memory is full with active blocks */
  BF_INVALID_BLOCK_NUMBER_ERROR, /* Requested block does not exist in the file */
  BF_AVAILABLE_PIN_BLOCKS_ERROR, /* File cannot be closed because there are pinned blocks in memory */
  BF_ERROR
} BF_ErrorCode;

typedef enum ReplacementAlgorithm {
  LRU,
  MRU
} ReplacementAlgorithm;

// Block structure
typedef struct BF_Block BF_Block;

/*
 * BF_Block_Init initializes and allocates the appropriate memory
 * for the BF_Block structure.
 */
void BF_Block_Init(BF_Block **block);

/*
 * BF_Block_Destroy releases the memory occupied
 * by the BF_Block structure.
 */
void BF_Block_Destroy(BF_Block **block);

/*
 * BF_Block_SetDirty marks the block as dirty.
 * This means that the block's data has been modified and the BF layer
 * will write it back to disk when necessary.
 * If we only read the data without modifying it, there is no need
 * to call this function.
 */
void BF_Block_SetDirty(BF_Block *block);

/*
 * BF_Block_GetData returns a pointer to the block's data.
 * If the data is modified, the block must be marked as dirty
 * using BF_Block_SetDirty.
 */
char* BF_Block_GetData(const BF_Block *block);

/*
 * BF_Init initializes the BF layer.
 * You can choose between two block replacement policies:
 * LRU and MRU.
 */
BF_ErrorCode BF_Init(const ReplacementAlgorithm repl_alg);

/*
 * BF_CreateFile creates a file with the given filename,
 * consisting of blocks. If the file already exists, an error
 * code is returned.
 * On success, BF_OK is returned. On failure, an error code is returned.
 * To see the error details, call BF_PrintError.
 */
BF_ErrorCode BF_CreateFile(const char* filename);

/*
 * BF_OpenFile opens an existing block file with the given filename
 * and returns its file descriptor in file_desc.
 * On success, BF_OK is returned. On failure, an error code is returned.
 * To see the error details, call BF_PrintError.
 */
BF_ErrorCode BF_OpenFile(const char* filename, int *file_desc);

/*
 * BF_CloseFile closes the open file identified by file_desc.
 * On success, BF_OK is returned. On failure, an error code is returned.
 * To see the error details, call BF_PrintError.
 */
BF_ErrorCode BF_CloseFile(const int file_desc);

/*
 * BF_GetBlockCounter returns the number of blocks in an open file
 * identified by file_desc, storing the result in blocks_num.
 * On success, BF_OK is returned. On failure, an error code is returned.
 * To see the error details, call BF_PrintError.
 */
BF_ErrorCode BF_GetBlockCounter(const int file_desc, int *blocks_num);

/*
 * BF_AllocateBlock allocates a new block for the file identified
 * by file_desc. The new block is always placed at the end of the file,
 * so its number is BF_GetBlockCounter(file_desc) - 1.
 * The allocated block is pinned in memory and returned in block.
 * When it is no longer needed, BF_UnpinBlock must be called.
 * On success, BF_OK is returned. On failure, an error code is returned.
 * To see the error details, call BF_PrintError.
 */
BF_ErrorCode BF_AllocateBlock(const int file_desc, BF_Block *block);

/*
 * BF_GetBlock retrieves the block with number block_num from the
 * open file file_desc and returns it in block.
 * The block is pinned in memory. When it is no longer needed,
 * BF_UnpinBlock must be called.
 * On success, BF_OK is returned. On failure, an error code is returned.
 * To see the error details, call BF_PrintError.
 */
BF_ErrorCode BF_GetBlock(const int file_desc,
                         const int block_num,
                         BF_Block *block);

/*
 * BF_UnpinBlock releases (unpins) the block from the BF layer.
 * The block may later be written back to disk if needed.
 * On success, BF_OK is returned. On failure, an error code is returned.
 * To see the error details, call BF_PrintError.
 */
BF_ErrorCode BF_UnpinBlock(BF_Block *block);

/*
 * BF_PrintError prints error messages produced by BF layer functions.
 * A description of the error is printed to stderr.
 */
void BF_PrintError(BF_ErrorCode err);

/*
 * BF_Close shuts down the BF layer, writing any remaining blocks
 * in memory back to disk.
 */
BF_ErrorCode BF_Close();

#ifdef __cplusplus
}
#endif
#endif // BF_H