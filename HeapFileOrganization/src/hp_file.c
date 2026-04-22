#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

// updates the metadata of a file
static int update_hp_info(int file_desc, HP_info* hp_info){
    BF_Block* block;
    BF_Block_Init(&block);

    int error = BF_GetBlock(file_desc, 0, block);
    if (error != BF_OK) {
        BF_PrintError(error);
        BF_Block_Destroy(&block);
        return HP_ERROR;
    }

    void* data = BF_Block_GetData(block);
    memcpy(data, hp_info, sizeof(HP_info));

    BF_Block_SetDirty(block);

    error = BF_UnpinBlock(block);
    if (error != BF_OK){
        BF_PrintError(error);
        BF_Block_Destroy(&block);
        return HP_ERROR;
    }

    BF_Block_Destroy(&block);
    return 0;
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

      // Create a block pointer:
      // the block is essentially just a pointer to the data of a block
      // which we use to access that data and when we no longer need it, we must destroy it
      BF_Block* block;
      
      BF_Block_Init(&block);

      // Allocate a block in the file
      error = BF_AllocateBlock(file_desc, block);
      if (error != BF_OK) {
        BF_PrintError(error);
        BF_Block_Destroy(&block);
        BF_CloseFile(file_desc);
        return -1;
      }
      
      void* data = BF_Block_GetData(block);

      HP_info hp_info;
      hp_info.last_block = -1; // the number of the last data block in the file
      hp_info.number_of_blocks = 1; // total number of blocks including the metadata block
      hp_info.first_block_with_records = -1; // the first block containing records

      // calculate how many records fit in one block, subtracting the size of HP_block_info
      // which is stored at the end of EVERY block
      hp_info.records_per_block = (BF_BLOCK_SIZE - sizeof(HP_block_info) )/ sizeof(Record);

      // copy the HP_info structure data into block 0, which stores the metadata
      memcpy(data, &hp_info , sizeof(hp_info));

      BF_Block_SetDirty(block); // since the block was modified, mark it dirty

      error = BF_UnpinBlock(block);
      if (error != BF_OK) {
        BF_PrintError(error);
        BF_Block_Destroy(&block);
        BF_CloseFile(file_desc);
        return -1;
      }

      BF_Block_Destroy(&block); // destroy the pointer

      error = BF_CloseFile(file_desc); // close the file
      if (error != BF_OK) {
        BF_PrintError(error);
        return -1;
      }

      // returns BF_OK
      return 0;
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
      BF_Block_Destroy(&block);
      BF_CloseFile(*file_desc);
      return NULL;
    }

    else{
      char* data = BF_Block_GetData(block);

      // store the metadata in the hp_info structure
      HP_info* hp_info = malloc(sizeof(HP_info));
      if (hp_info == NULL) {
        BF_UnpinBlock(block);
        BF_Block_Destroy(&block);
        BF_CloseFile(*file_desc);
        return NULL;
      }

      memcpy(hp_info, data, sizeof(HP_info));

      error = BF_UnpinBlock(block); // only unpin, without set_dirty, since nothing was modified
      if (error != BF_OK) {
        BF_PrintError(error);
        free(hp_info);
        BF_Block_Destroy(&block);
        BF_CloseFile(*file_desc);
        return NULL;
      }

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

  free(hp_info);

  return 0;
}



int HP_InsertEntry(int file_desc, HP_info* hp_info, Record record){

    // no need to open the file here; it is done in main

    BF_Block* new_block;
    BF_Block_Init(&new_block);

    int error;
    int block_id = hp_info->last_block;

    // if we only have one block, the metadata block
    if(block_id == -1){

      // create the first data block in the file
      error = BF_AllocateBlock(file_desc, new_block);
      if(error != BF_OK){
        BF_PrintError(error);
        BF_Block_Destroy(&new_block);
        return -1;
      }

      // copy the record to the beginning of the new block
      memcpy(BF_Block_GetData(new_block), &record, sizeof(Record));

      // update hp_info
      hp_info->last_block = hp_info->number_of_blocks;
      hp_info->first_block_with_records = hp_info->last_block;
      hp_info->number_of_blocks ++;

      // update block information
      HP_block_info block_info;
      block_info.number_of_records = 1;
      block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(Record) - sizeof(HP_block_info);
      block_info.next_block = -1;

      // get the address of the block data, move to the end,
      // and place the HP_block_info structure in the last
      // sizeof(HP_block_info) bytes
      memcpy((char*)BF_Block_GetData(new_block) + BF_BLOCK_SIZE - sizeof(block_info), &block_info, sizeof(HP_block_info));

      // after insertion is complete, we no longer need the block
      BF_Block_SetDirty(new_block);

      error = BF_UnpinBlock(new_block);
      if (error != BF_OK) {
        BF_PrintError(error);
        BF_Block_Destroy(&new_block);
        return -1;
      }

      BF_Block_Destroy(&new_block);

      if (update_hp_info(file_desc, hp_info) != 0) {
        return -1;
      }

      return hp_info->last_block;

    }

    // if this is not the first data block, we must also access the previous block
    // so that we can determine whether there is space for a new record
    else{
      BF_Block* last_block;
      BF_Block_Init(&last_block);

      error = BF_GetBlock(file_desc, block_id, last_block);
      if(error != BF_OK){
        BF_PrintError(error);
        BF_Block_Destroy(&new_block);
        BF_Block_Destroy(&last_block);
        return -1;
      }

      // the data of the last block
      void* data = BF_Block_GetData(last_block);

      // find the struct containing the block information to check
      // whether there is space for a new record
      HP_block_info* last_block_info;
      last_block_info = (HP_block_info*)((char*)data + BF_BLOCK_SIZE - sizeof(HP_block_info));


      // if there is no space for a new record in the block
      if(last_block_info->current_block_capacity < (int)sizeof(Record)){
        
        // create a new block in the file
        error = BF_AllocateBlock(file_desc, new_block);
        if(error != BF_OK){
          BF_PrintError(error);
          BF_UnpinBlock(last_block);
          BF_Block_Destroy(&new_block);
          BF_Block_Destroy(&last_block);
          return -1;
        }

        // copy the record to the beginning of the new block
        memcpy(BF_Block_GetData(new_block), &record, sizeof(Record));

        int id;
        error = BF_GetBlockCounter(file_desc, &id);
        if(error != BF_OK){
          BF_PrintError(error);
          BF_UnpinBlock(last_block);
          BF_UnpinBlock(new_block);
          BF_Block_Destroy(&new_block);
          BF_Block_Destroy(&last_block);
          return -1;
        }

        // store the number of the last block
        // (total blocks - 1, since numbering starts from 0)
        hp_info->last_block = id - 1;
        hp_info->number_of_blocks = id;

        HP_block_info block_info;
        block_info.number_of_records = 1;
        block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(Record) - sizeof(HP_block_info);
        block_info.next_block = -1;

        // update next block of the previous one
        last_block_info->next_block = hp_info->last_block;

        // get the address of the block data, move to the end,
        // and place the HP_block_info structure in the last
        // sizeof(HP_block_info) bytes
        memcpy((char*)BF_Block_GetData(new_block) + BF_BLOCK_SIZE - sizeof(block_info), &block_info, sizeof(HP_block_info));

        // after insertion is complete, we no longer need the blocks
        BF_Block_SetDirty(last_block);
        BF_Block_SetDirty(new_block);

        error = BF_UnpinBlock(last_block);
        if (error != BF_OK) {
          BF_PrintError(error);
          BF_UnpinBlock(new_block);
          BF_Block_Destroy(&new_block);
          BF_Block_Destroy(&last_block);
          return -1;
        }

        error = BF_UnpinBlock(new_block);
        if (error != BF_OK) {
          BF_PrintError(error);
          BF_Block_Destroy(&new_block);
          BF_Block_Destroy(&last_block);
          return -1;
        }

        if (update_hp_info(file_desc, hp_info) != 0) {
          return -1;
        }

      }

      // if we have space in the current block, add the record
      else{
        // copy the record after the last existing record
        memcpy((char*)data + ((last_block_info->number_of_records) * sizeof(Record) ), &record, sizeof(Record));
        last_block_info->number_of_records ++;
        last_block_info->current_block_capacity -= sizeof(Record);

        BF_Block_SetDirty(last_block); // since we modified the last block

        error = BF_UnpinBlock(last_block);
        if (error != BF_OK) {
          BF_PrintError(error);
          BF_Block_Destroy(&new_block);
          BF_Block_Destroy(&last_block);
          return -1;
        }
      }

      BF_Block_Destroy(&new_block);
      BF_Block_Destroy(&last_block);
    }

    return hp_info->last_block;
}


int HP_GetAllEntries(int file_desc,HP_info* hp_info, int value){    

    BF_Block* block;
    BF_Block_Init(&block);

    int blocks_read = 0;

    for(int i = 1; i < hp_info->number_of_blocks; i++){

      int error = BF_GetBlock(file_desc, i, block);
      if(error != BF_OK){
        BF_PrintError(error);
        BF_Block_Destroy(&block);
        return -1;
      }

      else{
        void* data = BF_Block_GetData(block);
        HP_block_info* block_info = (HP_block_info*) ((char*)data + BF_BLOCK_SIZE - sizeof(HP_block_info));
        int num = block_info->number_of_records;

        for(int j = 0; j < num; j++){
          Record* record = (Record*) ((char*)data + j * sizeof(Record));
          
          if(record->id == value){
            printRecord(*record);
          }
        }
      }

      blocks_read++;
      error = BF_UnpinBlock(block);
      if (error != BF_OK) {
        BF_PrintError(error);
        BF_Block_Destroy(&block);
        return -1;
      }
    }

    BF_Block_Destroy(&block);
    
    return blocks_read;
}