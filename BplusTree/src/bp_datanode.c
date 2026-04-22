#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "bp_file.h"
#include "record.h"
#include "bp_datanode.h"

// Creates a new data block and writes its metadata
// Returns the id of the new block
int create_data_node(int file_desc, BPLUS_INFO* bplus_info){
    
    BF_Block* block;
    BF_Block_Init(&block);

    CALL_BF(BF_AllocateBlock(file_desc, block)); 

    void* data = BF_Block_GetData(block);

    BPLUS_DATA_NODE data_node; // create metadata for the data block
    data_node.num_records = 0; // initially it contains no records
   
    int count;
    BF_GetBlockCounter(file_desc, &count);
    data_node.block_id =  count - 1; // block id (-1 because numbering starts at 0)
   
    data_node.next_block = -1; // initially there is no next block
    data_node.parent_id = -1;
    data_node.is_data_node = 1; // this is a data block
    data_node.minKey = -1; // initialize with a non-existing value

    memcpy(data, &data_node, sizeof(BPLUS_DATA_NODE)); // copy metadata into the block

    BF_Block_SetDirty(block); // mark block dirty after modification
    CALL_BF(BF_UnpinBlock(block)); // no longer needed
    BF_Block_Destroy(&block); // destroy the block pointer

    BF_Block* block0;
    BF_Block_Init(&block0);
    CALL_BF(BF_GetBlock(file_desc, 0, block0));

    BF_Block_SetDirty(block0); 
    CALL_BF(BF_UnpinBlock(block0));
    BF_Block_Destroy(&block0);


    return data_node.block_id; 
}


// Returns metadata of a data block
BPLUS_DATA_NODE* get_metadata_datanode(int file_desc, int block_id){
    
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(file_desc, block_id, block));

    void* data = BF_Block_GetData(block);

    // copy metadata from the block to the data_node structure
    BPLUS_DATA_NODE* data_node = (BPLUS_DATA_NODE*)(data);

    
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    return data_node;
}




// Adds a new record to a data block that has free space
void insert_rec_in_datanode(int fd, int node, BPLUS_INFO* bplus_info, Record new_rec){
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(fd, node, block));

    void* data = BF_Block_GetData(block);
    BPLUS_DATA_NODE* metadata = (BPLUS_DATA_NODE*)data; // block metadata

    int key = new_rec.id;
 
    // if there are no records, place the record right after metadata
    if(metadata->num_records == 0){
        metadata->minKey = key;
        memcpy(data + sizeof(BPLUS_DATA_NODE), &new_rec, sizeof(Record));
    }

    // if records exist, insert before the first record with a larger key
    else{
        
        int previousMinKey = metadata->minKey; // previous minimum key of the block
        if(key < previousMinKey){
            metadata->minKey = key;
        }

        bool is_max = true; // whether the new record key is the largest

        for(int i = 0; i < metadata->num_records; i++){
        
            Record* rec = (Record*) (data + sizeof(BPLUS_DATA_NODE) + i * sizeof(Record));

            // the new record must be inserted before this one
            if(rec->id > key){
                
                is_max = false; // found a larger key

                // shift records one position to the right to make room
                memmove(data + sizeof(BPLUS_DATA_NODE) + (i + 1) * sizeof(Record), data + sizeof(BPLUS_DATA_NODE) + i * sizeof(Record), 
                    (metadata->num_records - i) * sizeof(Record));
                
                // insert the new record in the created gap
                memcpy(data + sizeof(BPLUS_DATA_NODE) + i * sizeof(Record), &new_rec, sizeof(Record));
                
                break;
            }   
        }
        
        // if the new record has the largest key, append it at the end
        if(is_max == true){
                memcpy(data + sizeof(BPLUS_DATA_NODE) + metadata->num_records * sizeof(Record), &new_rec, sizeof(Record));
        }

    }

    metadata->num_records++; // increase number of records in the block

    BF_Block_SetDirty(block); 
    CALL_BF( BF_UnpinBlock(block));   

    BF_Block_Destroy(&block);
}




// Splits a data block into two, moves half the records to the new block,
// and inserts the new record into the appropriate block.
// Finally, returns the id of the new block created.
int split_data_node(int fd, int id, BPLUS_INFO* bplus_info, Record new_rec){

    //######### Original block ##########//
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(fd, id, block));

    void* data = BF_Block_GetData(block); // block data
    BPLUS_DATA_NODE* metadata = (BPLUS_DATA_NODE*)data; // block metadata
    
    // existing records plus the new one must be split evenly
    int split_point = (metadata->num_records + 1) / 2; // +1 due to new record


    //############# setup of new block ##########//
    BF_Block* new_block;
    BF_Block_Init(&new_block);

    int new_id = create_data_node(fd, bplus_info); // create new block

    CALL_BF(BF_GetBlock(fd, new_id, new_block)); // load new block into new_block pointer
    
    void* new_data = BF_Block_GetData(new_block); // new block data
    BPLUS_DATA_NODE* new_metadata = get_metadata_datanode(fd, new_id); // new block metadata

    new_metadata->next_block = metadata->next_block; // new block points to old next block
    metadata->next_block = new_id; // old block now points to new block


    // find the position where the new record should be inserted
    int pos = 0; // position of the new record
    bool is_max = true; // whether the new record key is the largest
    for(int i = 0; i < metadata->num_records; i++){ 
        
        Record* rec = (Record*) (data + sizeof(BPLUS_DATA_NODE) + i * sizeof(Record)); // examine record i
        
        if(new_rec.id < rec->id){ // if new key is smaller than key of record i
            is_max = false;
            pos = i; // insert before record i
            break;
        }
    }

    // if the new record has the maximum key, append at the end
    if(is_max == true){
        pos = metadata->num_records;
    }
    
    int old_num_records = metadata->num_records; // initial number of records in old block
    // update number of records in the old block
    if((old_num_records + 1) % 2 == 0){ // if old records + new one split exactly in half
        metadata->num_records = split_point;
        new_metadata->num_records = split_point;
    }
    else{
        metadata->num_records = old_num_records - split_point + 1;
        new_metadata->num_records = old_num_records - split_point ;// one fewer record
    }


    // if the new record should be inserted in the old block
    if(pos < metadata->num_records){

        // move records from the middle onwards
        void* tempDest = new_data + sizeof(BPLUS_DATA_NODE); // move them to the right block
        
        // which records? those from the middle and after (new record not inserted yet, hence -1)
        // metadata->num_records now stores half, so tempSrc points from the middle record onward
        void* tempSrc = data + sizeof(BPLUS_DATA_NODE) + ( (metadata->num_records - 1) * sizeof(Record) ); //
        memmove(tempDest, tempSrc, new_metadata->num_records  * sizeof(Record));

        metadata->num_records--; // record not inserted yet; count is updated inside function
        insert_rec_in_datanode(fd, id, bplus_info, new_rec); // insert new record in old block
    
    }

    // if the new record should be inserted in the new block
    else{

        void* tempDest = new_data + sizeof(BPLUS_DATA_NODE); 
        void* tempSrc = data + sizeof(BPLUS_DATA_NODE) + metadata->num_records * sizeof(Record); // from the middle onward
        memmove(tempDest, tempSrc, (new_metadata->num_records - 1) * sizeof(Record));

        new_metadata->num_records--;
        insert_rec_in_datanode(fd, new_id, bplus_info, new_rec); // insert new record in new block
    }
    
    // Update min keys of both blocks
    Record* firstRec1 = (Record*) (data + sizeof(BPLUS_DATA_NODE)); // first record of old block
    Record* firstRec2 = (Record*) (new_data + sizeof(BPLUS_DATA_NODE)); // first record of new block
    
    metadata->minKey = firstRec1->id; // old block minKey becomes first record key
    new_metadata->minKey = firstRec2->id; // new block minKey becomes first record key

    BF_Block_SetDirty(block);
    BF_Block_SetDirty(new_block);
    BF_UnpinBlock(block);
    BF_UnpinBlock(new_block);
    BF_Block_Destroy(&block);
    BF_Block_Destroy(&new_block);

    return new_id;
}


// Prints the records of a data block
void print_data_node(int fd, int id){
    // Open output file in append mode
    FILE* file = fopen("output.txt", "a");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Get block
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(fd, id, block));

    // Get block data
    void* data = BF_Block_GetData(block);
    BPLUS_DATA_NODE* metadata = (BPLUS_DATA_NODE*)data;

    // Print metadata
    fprintf(file, "Block id: %d\n", metadata->block_id);
    fprintf(file, "Number of records: %d\n", metadata->num_records);
    fprintf(file, "Next block: %d\n", metadata->next_block);
    fprintf(file, "Parent id: %d\n", metadata->parent_id);


    // Print records
    for (int i = 0; i < metadata->num_records; i++) {
        Record* rec = (Record*)(data + sizeof(BPLUS_DATA_NODE) + i * sizeof(Record));
        fprintf(file, "(%d, %s, %s, %s)\n", rec->id, rec->name, rec->surname, rec->city);
    }
    fprintf(file, "\n");

    // Release resources
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    // Close file
    fclose(file);
}
