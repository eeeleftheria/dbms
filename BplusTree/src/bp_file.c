#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "bp_file.h"
#include "record.h"
#include <bp_datanode.h>
#include <stdbool.h>

#define MAX_OPEN_FILES 20

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }
int openFiles[MAX_OPEN_FILES];


int BP_CreateFile(char *fileName){
  
  CALL_BF(BF_CreateFile(fileName));

  int file_desc;
  CALL_BF(BF_OpenFile(fileName, &file_desc));

    // Create a block pointer
    // A block is just a pointer to a block's data
    // used to access that data
    // when we no longer need it, we destroy it
    BF_Block* block;

    BF_Block_Init(&block);

    // Allocate a block in the file
    BF_AllocateBlock(file_desc, block);

    // block data
    void* data = BF_Block_GetData(block);

    // initialize B+ tree metadata
    BPLUS_INFO bplus_info;
    bplus_info.file_desc = file_desc;
    bplus_info.root_block = 0;
    bplus_info.height = 0;
    bplus_info.record_size = sizeof(Record);
    bplus_info.key_size = sizeof(int);
    bplus_info.max_records_per_block = (BF_BLOCK_SIZE - sizeof(BPLUS_DATA_NODE)) / sizeof(Record);

    int total_elements = (BF_BLOCK_SIZE - sizeof(BPLUS_INDEX_NODE)) / sizeof(int);
    int max_keys = (total_elements - 1) / 2;
    bplus_info.max_keys_per_index = max_keys;

    memcpy(data, &bplus_info, sizeof(BPLUS_INFO));


    BF_Block_SetDirty(block); // block was modified, mark it dirty
    CALL_BF(BF_UnpinBlock(block)); // unpin block since it is no longer needed
    BF_Block_Destroy(&block); // destroy the pointer

    CALL_BF(BF_CloseFile(file_desc)); // close file

  return 0;
}


BPLUS_INFO* BP_OpenFile(char *fileName, int *file_desc){
  CALL_BF(BF_OpenFile(fileName, file_desc));

  // store the open file in the table
  openFiles[*file_desc] = 1;

  BF_Block* block;
  BF_Block_Init(&block);

  // get block 0 with metadata into variable block
  CALL_BF(BF_GetBlock(*file_desc, 0, block));

  void* data = BF_Block_GetData(block);

  // store metadata into bplus_info structure
  BPLUS_INFO* bplus_info = (BPLUS_INFO*)data;

  // only unpin, no set_dirty since nothing changed
  CALL_BF(BF_UnpinBlock(block));
  
  BF_Block_Destroy(&block);

  return bplus_info;
}

int BP_CloseFile(int file_desc, BPLUS_INFO* info){

  // blocks are already unpinned in the other functions
  // so it is enough to close the file

  // file is now closed
  openFiles[file_desc] = 0;
  CALL_BF(BF_CloseFile(file_desc));
  
  return 0;
}


int BP_InsertEntry(int fd,BPLUS_INFO *bplus_info, Record record){

  // return block id where insertion happened
  int return_value;

  // If the B+ tree is empty, create a single new data block
  // which also becomes the root of the tree
  if(bplus_info->height == 0){
    int id_datanode = create_data_node(fd, bplus_info);
    insert_rec_in_datanode(fd, id_datanode, bplus_info, record);

    bplus_info->root_block = id_datanode; // root is currently a data block
    bplus_info->height++;

    return id_datanode; // return block id where insertion happened
  }

  // if a record with the same id already exists
  Record tmpRec;  // instead of malloc
  Record* result = &tmpRec;
  if(BP_GetEntry(fd, bplus_info, record.id, &result) == 0){
    return -1;
  }

  
  // If the B+ tree is not empty
  int root = bplus_info->root_block;
  int height_of_current_root = bplus_info->height;

  // Find the data block where insertion should happen
  int data_block_to_insert = BP_FindDataBlockToInsert(fd, record.id, root, height_of_current_root);

  BF_Block* block;
  BF_Block_Init(&block);
  // load target block for insertion into pointer block
  CALL_BF(BF_GetBlock(fd, data_block_to_insert, block));

  BPLUS_DATA_NODE* metadata_datanode = get_metadata_datanode(fd, data_block_to_insert);

  // if there is space in the data block, insert record there
  if(metadata_datanode->num_records < bplus_info->max_records_per_block){
    
    insert_rec_in_datanode(fd, data_block_to_insert, bplus_info, record);
    
    return_value = data_block_to_insert;

    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    return return_value;
  }

  // if there is no space, we must split it
  else{  

    int parent_id;
    // if there is no parent index node, create one
    // this only applies when we have a single data block
    // which is also the root of the tree
    if(metadata_datanode->parent_id == -1){

      // create first index node, i.e. the new root
      parent_id = create_index_node(fd, bplus_info);
      bplus_info->height++;
      bplus_info->root_block = parent_id;

      metadata_datanode->parent_id = parent_id;
    }

    else{
      parent_id = metadata_datanode->parent_id;

    }

    // id of new data block created after split
    int new_data_node_id = split_data_node(fd, data_block_to_insert, bplus_info, record);

    // store the smallest key of the new data block,
    // which must be inserted into an index node
    int key_to_move_up = get_metadata_datanode(fd, new_data_node_id)->minKey;

    // set up the new data block created by the split
    BF_Block* new_data_node;
    BF_Block_Init(&new_data_node);
    CALL_BF(BF_GetBlock(fd, new_data_node_id, new_data_node));
    BPLUS_DATA_NODE* new_metadata_datanode = get_metadata_datanode(fd, new_data_node_id);

    // if parent index node has space, add the key there
    if(is_full_indexnode(fd, parent_id, bplus_info) == false){

      new_metadata_datanode->parent_id = parent_id;
      insert_key_indexnode(fd, parent_id, bplus_info, key_to_move_up, data_block_to_insert, new_data_node_id);
    }

    // if it has no space, split the index node
    else{
      int new_index_node = split_index_node(fd, bplus_info, parent_id, key_to_move_up, new_data_node_id);
      
    }


    BF_Block_SetDirty(new_data_node);
    CALL_BF(BF_UnpinBlock(new_data_node));
    BF_Block_Destroy(&new_data_node);

    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    return_value = BP_FindDataBlockToInsert(fd, record.id, bplus_info->root_block, bplus_info->height);

    return return_value;
  }
  

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return -1;
}



int BP_GetEntry(int file_desc, BPLUS_INFO *bplus_info, int value, Record** record){
  // find which data block the key should be in
  int height = bplus_info->height;
  int root = bplus_info->root_block;
  int block_with_rec = BP_FindDataBlockToInsert(file_desc, value, root, height);

  BF_Block* block;
  BF_Block_Init(&block);

  // printf("HEIGHT before %d\n", bplus_info->height);

  CALL_BF(BF_GetBlock(file_desc, block_with_rec, block));
  
//  printf("HEIGHT after %d\n", bplus_info->height);



  BPLUS_DATA_NODE* metadata_datanode = get_metadata_datanode(file_desc, block_with_rec);
  char* data = BF_Block_GetData(block);

  int num_of_recs = metadata_datanode->num_records;

  for(int i = 0; i < num_of_recs; i++){
    
    Record* rec = (Record*)(data + sizeof(BPLUS_DATA_NODE) + i * sizeof(Record));
    // if record was found, return 0 and make record point to it
    if(rec->id == value){
      memcpy(*record, rec, sizeof(Record));

      BF_UnpinBlock(block);
      BF_Block_Destroy(&block);
      return 0;
    }
  }

  // if no such record was found
  *record = NULL;

  BF_UnpinBlock(block);
  BF_Block_Destroy(&block);

  return -1;
}


int BP_FindDataBlockToInsert(int fd, int key, int root, int height_of_current_root){

  // we are at a data block (recursion end case)
  if(height_of_current_root == 1){
    return root;
  }


  BF_Block* block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(fd, root, block));


  void* data = BF_Block_GetData(block);
  int num_of_keys = get_metadata_indexnode(fd, root)->num_keys;

  int child_id = 0;
  int current_key;
  
  bool is_max = true;

  // iterate key by key
  for(int i = 1; i < (2 * num_of_keys + 1) ; i+= 2){
 
    memcpy(&current_key, data + sizeof(BPLUS_INDEX_NODE) + i * sizeof(int), sizeof(int));
    
    if(key < current_key){
  
      is_max = false;

      memcpy(&child_id, data + sizeof(BPLUS_INDEX_NODE) + (i - 1) * sizeof(int), sizeof(int)); 
      // child block to follow in the traversal
  
      break;
    }

  }

    // if we reached the last key and found no larger one
  if(is_max == true){

    int total_elements = 2 * num_of_keys + 1;

    memcpy(&child_id, data + sizeof(BPLUS_INDEX_NODE) + (total_elements - 1) * sizeof(int), sizeof(int)); 

  } 



  // recursive call
  height_of_current_root--;
  BF_UnpinBlock(block);
  BF_Block_Destroy(&block);

  return BP_FindDataBlockToInsert(fd, key,  child_id, height_of_current_root);
    
}


void BP_PrintBlock(int fd, int block_id, BPLUS_INFO* bplus_info){


  BF_Block* block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(fd, block_id, block));

  char* data = BF_Block_GetData(block);

  int* is_datanode;
  is_datanode = (int*)(data);

  if(*is_datanode == 1){
    print_data_node(fd, block_id);
  }

  else{
    print_index_node(fd, block_id);
  }

  BF_UnpinBlock(block);
  BF_Block_Destroy(&block);
}


void BP_PrintTree(int fd, BPLUS_INFO* bplus_info){

  FILE* file = fopen("output.txt", "a");

  fprintf(file, "\nPrinting B+ Tree\n\n");
  fprintf(file, "METADATA\n");

  fprintf(file, "---Root block: %d\n", bplus_info->root_block); 
  fprintf(file, "---Height: %d\n", bplus_info->height);
  fprintf(file, "---parent of root: %d\n", get_metadata_indexnode(fd, bplus_info->root_block)->parent_id);
  int count;
  BF_GetBlockCounter(fd, &count);
  fprintf(file, "---Number of blocks: %d\n\n-------------------\n", count);
  fflush(file);
  
  for(int i = 1; i < count; i++){
    BP_PrintBlock(fd, i, bplus_info);
  }

  fclose(file);
}


void BP_SetInfo(int fd, BPLUS_INFO* bplus_info, int max_records_per_data, int max_keys_per_index){
  
  BF_Block* block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(fd, 0, block));

  void* data = BF_Block_GetData(block);

  bplus_info = (BPLUS_INFO*)data;
  bplus_info->max_records_per_block = max_records_per_data;
  bplus_info->max_keys_per_index = max_keys_per_index;

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

}