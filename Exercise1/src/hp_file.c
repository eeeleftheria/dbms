#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

int HP_CreateFile(char *fileName){
    int error = BF_CreateFile(fileName);
   
    // for example, if the file already exists
    if(error != BF_OK){
      BF_PrintError(error);
      return -1;
    }

    else{
      int file_desc;
      int error = BF_OpenFile(fileName, &file_desc);

      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }

      // Create a block pointer
      // the block is essentially just a pointer to the data of a block
      // which we use to access that data
      // when we no longer need it, we must destroy it
      BF_Block* block;
      
      BF_Block_Init(&block);

      // Allocate a block in the file
      BF_AllocateBlock(file_desc, block);
      
      void* data = BF_Block_GetData(block);

      HP_info hp_info;
      hp_info.last_block = 0; // the number of the last block in the file
      hp_info.number_of_blocks = 0; // total number of blocks
      hp_info.first_block_with_records = NULL; // the first block containing records

      // calculate how many records fit in one block, subtracting the size of HP_block_info
      // which is stored at the end of EVERY block
      hp_info.records_per_block = (BF_BLOCK_SIZE - sizeof(HP_block_info) )/ sizeof(Record);

      // copy the HP_info structure data into block 0, which stores the metadata
      memcpy(data, &hp_info , sizeof(hp_info));

      BF_Block_SetDirty(block); // since the block was modified, mark it dirty
      BF_UnpinBlock(block); // we no longer need it
      BF_Block_Destroy(&block); // destroy the pointer

      BF_CloseFile(file_desc); // close the file

      // returns BF_OK
      return error;
    }
}


HP_info* HP_OpenFile(char *fileName, int *file_desc){
    int error = BF_OpenFile(fileName, file_desc);
    if(error != BF_OK){
      BF_PrintError(error);
      return NULL;
    }

    BF_Block* block;
    BF_Block_Init(&block);

    error = BF_GetBlock(*file_desc, 0, block);  
    if(error != BF_OK){
      BF_PrintError(error);
      BF_UnpinBlock(block);
      return NULL;
    }

    else{
      char* data = BF_Block_GetData(block);

      // store the metadata in the hp_info structure
      HP_info* hp_info = (HP_info*) data;

      BF_UnpinBlock(block); // only unpin, without set_dirty, since nothing was modified
      BF_Block_Destroy(&block);
      return hp_info;
    }

}


int HP_CloseFile(int file_desc,HP_info* hp_info ){
  int error;
  BF_Block* block;
  // there is no need to unpin the blocks here because each operation,
  // for example insert, will unpin when it finishes
  // (in our own function implementations we handle the unpin ourselves)
  BF_Block_Init(&block);
  BF_Block_Destroy(&block);
  
  error = BF_CloseFile(file_desc);
  if(error != BF_OK){
      BF_PrintError(error);
      return -1;
  }

  return 0;
}



int HP_InsertEntry(int file_desc, HP_info* hp_info, Record record){

    // no need to open the file here; it is done in main

    BF_Block* new_block;
    BF_Block_Init(&new_block);

    int error;
    int block_id = hp_info->last_block;

    // if we only have one block, the metadata block
    if(block_id == 0){

      // create a new block in the file
      error = BF_AllocateBlock(file_desc, new_block);
      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }

      // copy the record to the beginning of the new block
      memcpy(BF_Block_GetData(new_block), &record, sizeof(Record));

      // update hp_info
      hp_info->number_of_blocks ++;
      hp_info->last_block = 1;
      hp_info->first_block_with_records = new_block;

      // update block information
      HP_block_info block_info;
      block_info.number_of_records = 1;
      block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(Record) - sizeof(HP_block_info);
      block_info.next_block = NULL;

      // get the address of the block data, move to the end,
      // and place the HP_block_info structure in the last
      // sizeof(HP_block_info) bytes
      memcpy(BF_Block_GetData(new_block) + BF_BLOCK_SIZE - sizeof(block_info), &block_info, sizeof(HP_block_info));

      // after insertion is complete, we no longer need the block
      BF_Block_SetDirty(new_block);
      BF_UnpinBlock(new_block);
      BF_Block_Destroy(&new_block);

      return hp_info->last_block;

    }

    // if this is not the first inserted block, we must also access the previous block
    // so that we can determine whether there is space for a new record
    else{
      BF_Block* last_block;
      BF_Block_Init(&last_block);


      error = BF_GetBlock(file_desc, block_id, last_block);
      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }

      // the data of the last block
      void* data = BF_Block_GetData(last_block);

      // find the struct containing the block information to check
      // whether there is space for a new record
      HP_block_info* last_block_info;
      last_block_info = (HP_block_info*) (data + BF_BLOCK_SIZE - sizeof(HP_block_info));


      // if another record does not fit in the block
      if(last_block_info->current_block_capacity < sizeof(Record)){
        
        // create a new block in the file
        error = BF_AllocateBlock(file_desc, new_block);
        if(error != BF_OK){
          BF_PrintError(error);
          return -1;
        }

        // copy the record to the beginning of the new block
        memcpy(BF_Block_GetData(new_block), &record, sizeof(Record));
        hp_info->number_of_blocks ++;

        int id;
        error = BF_GetBlockCounter(file_desc, &id);
        if(error != BF_OK){
          BF_PrintError(error);
          return -1;
        }

        // store the number of the last block
        // (total blocks - 1, since numbering starts from 0)
        hp_info->last_block = id - 1;

        HP_block_info block_info;
        block_info.number_of_records = 1;
        block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(Record) - sizeof(HP_block_info);
        block_info.next_block = NULL;

        // update next block of the previous one
        last_block_info->next_block = new_block;

        // get the address of the block data, move to the end,
        // and place the HP_block_info structure in the last
        // sizeof(HP_block_info) bytes
        memcpy(BF_Block_GetData(new_block) + BF_BLOCK_SIZE - sizeof(block_info), &block_info, sizeof(HP_block_info));

        // after insertion is complete, we no longer need the blocks
        BF_Block_SetDirty(last_block);
        BF_Block_SetDirty(new_block);
        BF_UnpinBlock(last_block);
        BF_UnpinBlock(new_block);

      }

      // if we have space in the current block, add the record
      else{
        // copy the record after the last existing record
        memcpy(data + ((last_block_info->number_of_records) * sizeof(Record) ), &record, sizeof(Record));
        last_block_info->number_of_records ++;
        last_block_info->current_block_capacity -= sizeof(Record);

        BF_Block_SetDirty(last_block); // since we modified the last block
        BF_UnpinBlock(last_block); 
      }

      BF_Block_Destroy(&new_block);
      BF_Block_Destroy(&last_block);
    }

    return hp_info->last_block;
}


int HP_GetAllEntries(int file_desc,HP_info* hp_info, int value){    

    BF_Block* block;
    BF_Block_Init(&block);

    for(int i = 1; i < hp_info->number_of_blocks; i++){

      int error = BF_GetBlock(file_desc, i, block);
      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }

      else{
        void* data = BF_Block_GetData(block);
        HP_block_info* block_info = (HP_block_info*) (data + BF_BLOCK_SIZE - sizeof(HP_block_info));
        int num = block_info->number_of_records;

        for(int j = 0; j < num; j++){
          Record* record = (Record*) (data + j * sizeof(Record));
          
          if(record->id == value){
            printRecord(*record);
          }
        }
      }

      BF_UnpinBlock(block);
    }
    BF_Block_Destroy(&block);
    
    return hp_info->number_of_blocks;
}