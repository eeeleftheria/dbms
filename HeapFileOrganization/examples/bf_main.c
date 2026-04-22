#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "record.h"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

int main() {
  int fd1;
  BF_Block *block;
  BF_Block_Init(&block);



  /* First Part: use of the block library to create a file
   * with 10 blocks, load each block into the buffer
   * and write two records at the beginning of each block. In order
   * for the changes to be written to disk, the block must be marked
   * as dirty and unpinned so that the buffer does not fill up. We can
   * open the file block_example.db to inspect its contents.
   */
  CALL_OR_DIE(BF_Init(LRU));
  CALL_OR_DIE(BF_CreateFile("block_example.db"))
  CALL_OR_DIE(BF_OpenFile("block_example.db", &fd1));

  void* data;
  for (int i = 0; i < 100; ++i) {
    CALL_OR_DIE(BF_AllocateBlock(fd1, block));  // Allocate a new block
    data = BF_Block_GetData(block);             // The contents of the block in the buffer
    Record* rec = data;                         // Pointer rec points to the beginning of memory area data
    rec[0] = randomRecord();
    rec[1] = randomRecord();
    BF_Block_SetDirty(block);
    CALL_OR_DIE(BF_UnpinBlock(block));
  }
  CALL_OR_DIE(BF_CloseFile(fd1));               // Close file and release memory
  CALL_OR_DIE(BF_Close());




  /* Second Part: use of the block library to read
   * the first two records from each block.
   */

  CALL_OR_DIE(BF_Init(LRU));
  CALL_OR_DIE(BF_OpenFile("block_example.db", &fd1));
  int blocks_num;
  CALL_OR_DIE(BF_GetBlockCounter(fd1, &blocks_num));

  for (int i = 0; i < blocks_num; ++i) {
    printf("Contents of Block %d\n\t",i);
    CALL_OR_DIE(BF_GetBlock(fd1, i, block));
    data = BF_Block_GetData(block);
    Record* rec= data;
    printRecord(rec[0]);
    printf("\t");
    printRecord(rec[1]);
    CALL_OR_DIE(BF_UnpinBlock(block));
  }

  BF_Block_Destroy(&block);
  CALL_OR_DIE(BF_CloseFile(fd1));
  CALL_OR_DIE(BF_Close());

}