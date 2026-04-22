#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "bp_indexnode.h"
#include "bp_file.h"
#include "record.h"

// create an empty index block
int create_index_node(int file_desc, BPLUS_INFO* bplus_info){
    
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_AllocateBlock(file_desc, block));

    void* data = BF_Block_GetData(block);

    // create index block metadata
    BPLUS_INDEX_NODE index_node;
    index_node.num_keys = 0;

    int count;
    BF_GetBlockCounter(file_desc, &count);
    index_node.block_id =  count - 1; // block id (-1 because numbering starts at 0)
    index_node.parent_id = -1;
    index_node.is_data_node = 0;

    // copy metadata into block
    memcpy(data, &index_node, sizeof(BPLUS_INDEX_NODE));

    
    BF_Block_SetDirty(block); // block changed, mark it dirty
    CALL_BF(BF_UnpinBlock(block)); // no longer needed
    BF_Block_Destroy(&block); // destroy block pointer

    // update block 0 metadata
    BF_Block* block0;
    BF_Block_Init(&block0);
    CALL_BF(BF_GetBlock(file_desc, 0, block0));

    BF_Block_SetDirty(block0); 
    CALL_BF(BF_UnpinBlock(block0));
    BF_Block_Destroy(&block0);


    return index_node.block_id;
}


BPLUS_INDEX_NODE* get_metadata_indexnode(int file_desc, int block_id){
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(file_desc, block_id, block));

    void* data = BF_Block_GetData(block);

    BPLUS_INDEX_NODE* index_node;

    // copy metadata from block to index_node structure
    index_node = (BPLUS_INDEX_NODE*)(data);

    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    return index_node;
}


// Add a key-pointer pair to an index block
// Function accepts the id of the block that split (left_child_id),
// and the id of the block created by the split (right_child_id)
// key is the smallest key of the new block (right_child_id),
// and must be inserted into the index block.
void insert_key_indexnode(int fd, int id_index_node, BPLUS_INFO* bplus_info, int key, int left_child_id, int right_child_id){

    BF_Block* block;
    BF_Block_Init(&block);
    
    // load index block into pointer block
    CALL_BF(BF_GetBlock(fd, id_index_node, block));
    void* data = BF_Block_GetData(block);
    BPLUS_INDEX_NODE* metadata_index_node = get_metadata_indexnode(fd, id_index_node); // block metadata


    // if index block is empty (newly created)
    // e.g. after splitting a data block
    if(metadata_index_node->num_keys == 0){
        memcpy(data + sizeof(BPLUS_INDEX_NODE), &left_child_id, sizeof(int)); // left of key
        memcpy(data + sizeof(BPLUS_INDEX_NODE) + sizeof(int), &key, sizeof(int)); // key
        memcpy(data + sizeof(BPLUS_INDEX_NODE) + 2 * sizeof(int), &right_child_id, sizeof(int)); // right of key
    }

    else{
        // if block is not empty, find where the new key should be inserted
        int current_key;
        bool is_max = true;
        int pos = 0; // insertion position of new key

        // for i = 1 we read the first key
        // for i = 0 we have the first pointer
        for(int i = 1; i <= 2 * metadata_index_node->num_keys + 1; i+=2){

            memcpy(&current_key, data + sizeof(BPLUS_INDEX_NODE) + i * sizeof(int), sizeof(int));

            // if new key is smaller than key at position i, insert before it
            if(key < current_key){
        
                is_max = false;

                pos = i; // insertion position of new key

                break;
            }
        }

        // if it is the largest key, append at the end with right-child pointer
        if(is_max == true){

            memcpy(data + sizeof(BPLUS_INDEX_NODE) + (2 * metadata_index_node->num_keys + 1) * sizeof(int), &key, sizeof(int));
            memcpy(data + sizeof(BPLUS_INDEX_NODE) + (2 * metadata_index_node->num_keys + 2) * sizeof(int), &right_child_id, sizeof(int));
        }
        else{
            // shift (key + pointer) pairs one position right
            // so the new pair can be inserted at pos
            void* tempDest = data + sizeof(BPLUS_INDEX_NODE) + (pos + 2) * sizeof(int);
            void* tempSrc = data + sizeof(BPLUS_INDEX_NODE) + pos * sizeof(int);
            memmove(tempDest, tempSrc, (2 * metadata_index_node->num_keys + 1 - pos) * sizeof(int));
            
            // insert new key
            memcpy(tempSrc, &key, sizeof(int));

            // insert new pointer to the right of the key
            memcpy(tempSrc + sizeof(int), &right_child_id, sizeof(int));  
        }        
        
    }

    metadata_index_node->num_keys++; // increase number of keys

    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);  

}

// Split an index block in two and insert the middle key into its parent.
// Parameters: file descriptor, B+ metadata, id of the block to split,
// key to insert, and child block id to insert as pointer.
int split_index_node(int fd, BPLUS_INFO* bplus_info, int index_node_id, int key_to_insert, int child_id){

    // data block pointed to by the new pointer to insert
    BF_Block* child;
    BF_Block_Init(&child);
    CALL_BF(BF_GetBlock(fd, child_id, child));
    BPLUS_DATA_NODE* metadata_child = get_metadata_datanode(fd, child_id);

    //######## original index block ##########//
    BF_Block* block1;
    BF_Block_Init(&block1);
    CALL_BF(BF_GetBlock(fd, index_node_id, block1));
    void* data1 = BF_Block_GetData(block1);
    BPLUS_INDEX_NODE* metadata1 = get_metadata_indexnode(fd, index_node_id);

    //####### new index block ##########//
    int new_index_node_id = create_index_node(fd, bplus_info);
    BF_Block* block2;
    BF_Block_Init(&block2);
    CALL_BF(BF_GetBlock(fd, new_index_node_id, block2));
    void* data2 = BF_Block_GetData(block2);
    BPLUS_INDEX_NODE* metadata2 = get_metadata_indexnode(fd, new_index_node_id);


    //######## even distribution of pointers and keys ########//
    int split_point = (metadata1->num_keys) / 2; 


    int pos = 0;
    bool is_max = true;
    int current_key;
    int keys[metadata1->num_keys + 1]; // array of keys

    int current_p;
    int pointers[metadata1->num_keys + 2]; // array of pointers

    int j = 0; // index for keys array

    // number of elements in block before split

    int count_of_elements = 2 * metadata1->num_keys + 1;

    for(int i = 1; i <= count_of_elements; i += 2){
        
        // store key
        memcpy(&current_key, data1 + sizeof(BPLUS_INDEX_NODE) + i * sizeof(int), sizeof(int));
        keys[j] = current_key;
        

        // since we skip the leftmost pointer, store it separately
        if(i == 1){ // when examining the first key
            memcpy(&current_p, data1 + sizeof(BPLUS_INDEX_NODE), sizeof(int));
            pointers[j] = current_p; 
        }

        // store pointer to the right of key
        memcpy(&current_p, data1 + sizeof(BPLUS_INDEX_NODE) + (i + 1) * sizeof(int), sizeof(int));
        // j + 1 because pointer for each key is at j + 1,
        // while position 0 stores the first pointer
        pointers[j + 1] = current_p;
        
        if(key_to_insert < current_key){
            is_max = false;
        }
        j++;
    }
    for(int i = 0; i < metadata1->num_keys; i++){
        if(key_to_insert < keys[i]){
            pos = i; // store insertion position according to array indexing
            break;
        }
    }
    
    // if key is maximum, insert at end of array
     if(is_max == true){
        pos = metadata1->num_keys; 
    }

    // update parent id of the block inserted as pointer
    // if in first half, parent remains original index node
    if(pos < split_point){
        metadata_child->parent_id = index_node_id;
    }
    // if in second half, parent becomes new index node
    else{
        metadata_child->parent_id = new_index_node_id;
    }


    // shift keys from pos one position to the right
    for(int i = metadata1->num_keys; i > pos; i--){
        keys[i] = keys[i - 1];
    }


    // insert new key at pos
    keys[pos] = key_to_insert;


    // shift pointers from pos + 1 one position to the right
    for(int i = metadata1->num_keys + 1; i > pos + 1; i--){
        pointers[i] = pointers[i - 1];
    }

    // insert new pointer at pos + 1, i.e. right of new key
    pointers[pos + 1] = child_id;

    // find middle key to be promoted
    // odd number of keys: exact middle key is promoted
    // even number of keys: upper middle key is promoted
    int key_to_move_up = keys[split_point];


    // move keys up to split point - 1 into old block
    for(int i = 0; i < split_point; i++){
        // first (leftmost) pointer must be placed before first key
        if (i == 0){
            memcpy(data1 + sizeof(BPLUS_INDEX_NODE), &pointers[i], sizeof(int));
        }
        
        memcpy(data1 + sizeof(BPLUS_INDEX_NODE) + (2 * i + 1) * sizeof(int), &keys[i], sizeof(int));
        memcpy(data1 + sizeof(BPLUS_INDEX_NODE) + (2 * i + 2) * sizeof(int), &pointers[i + 1], sizeof(int));
        
    }
    
    for(int i = split_point + 1; i <= metadata1->num_keys; i++){

        if(i == split_point + 1){
            // take pointer corresponding to promoted key
            // so it becomes first pointer in new block
            memcpy(data2 + sizeof(BPLUS_INDEX_NODE), &pointers[split_point + 1], sizeof(int));
        }
        
        memcpy(data2 + sizeof(BPLUS_INDEX_NODE) + (2 * (i - split_point - 1) + 1) * sizeof(int), &keys[i], sizeof(int));

        memcpy(data2 + sizeof(BPLUS_INDEX_NODE) + (2 * (i - split_point - 1) + 2) * sizeof(int), &pointers[i + 1], sizeof(int));

    }

    

    
    int old_num_keys = metadata1->num_keys; // initial key count in old block
    // update metadata of both index nodes
    metadata1->num_keys = split_point;
    
    // if initial number of keys is even (e.g. 4 keys => split point 2)
    // each block gets same number of keys after removing promoted key
    if((old_num_keys) % 2 == 0){
        metadata2->num_keys = split_point;
    }
    
    // if initial number is odd (e.g. 7 keys => split point 3)
    // one block gets 3 keys and the other gets 4
    else{ 
        metadata2->num_keys = split_point + 1;
    }


    update_parents(fd, bplus_info, new_index_node_id);
    
    BF_Block_SetDirty(block1);
    CALL_BF(BF_UnpinBlock(block1));
    BF_Block_Destroy(&block1);
    
    BF_Block_SetDirty(block2);
    CALL_BF(BF_UnpinBlock(block2));
    BF_Block_Destroy(&block2);
    
    BF_Block_SetDirty(child);
    CALL_BF(BF_UnpinBlock(child));
    BF_Block_Destroy(&child);

    //############ Parent handling #############//
    if(metadata1->parent_id == -1){
        int new_root_id = create_index_node(fd, bplus_info);
        
        BF_Block* new_root;
        BF_Block_Init(&new_root);

        CALL_BF(BF_GetBlock(fd, new_root_id, new_root));

        BPLUS_INDEX_NODE* new_root_data = get_metadata_indexnode(fd, new_root_id);

        insert_key_indexnode(fd, new_root_id, bplus_info, key_to_move_up, index_node_id, new_index_node_id);

        bplus_info->root_block = new_root_id;
        bplus_info->height++;

        metadata1->parent_id = new_root_id;
        metadata2->parent_id = new_root_id;


        BF_Block_SetDirty(new_root);
        CALL_BF(BF_UnpinBlock(new_root));
        BF_Block_Destroy(&new_root);

    }

    else{
        // if parent has space, insert there
        if(is_full_indexnode(fd, metadata1->parent_id, bplus_info) == false){
            insert_key_indexnode(fd, metadata1->parent_id, bplus_info, key_to_move_up, index_node_id, new_index_node_id);
            metadata2->parent_id = metadata1->parent_id;
        }
        // otherwise recursively split parent index node
        else{
            return split_index_node(fd, bplus_info, metadata1->parent_id, key_to_move_up, new_index_node_id);
        }
    }  
 
    return new_index_node_id;

}


void update_parents(int fd, BPLUS_INFO* bplus_info, int parent_id){
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(fd, parent_id, block));
    void* data = BF_Block_GetData(block);
    BPLUS_INDEX_NODE* metadata = get_metadata_indexnode(fd, parent_id);

    int num = metadata->num_keys;
    for(int i = 0; i <= 2*num + 1; i+=2){
        int child_id;
        memcpy(&child_id, data + sizeof(BPLUS_INDEX_NODE) + i * sizeof(int), sizeof(int));
        
        BF_Block* block1;
        BF_Block_Init(&block1);
        CALL_BF(BF_GetBlock(fd, child_id, block1));
        char* data = BF_Block_GetData(block1);

        int* is_datanode;
        is_datanode = (int*)(data);

        if(*is_datanode == 1){
            BPLUS_DATA_NODE* metadata_child = get_metadata_datanode(fd, child_id);
            metadata_child->parent_id = parent_id;

            BF_Block_SetDirty(block1);
            CALL_BF(BF_UnpinBlock(block1));
            BF_Block_Destroy(&block1);
            
        }

        else{
            BPLUS_INDEX_NODE* metadata_child = get_metadata_indexnode(fd, child_id);
            metadata_child->parent_id = parent_id;
            BF_Block_SetDirty(block1);
            CALL_BF(BF_UnpinBlock(block1));
            BF_Block_Destroy(&block1);

        }

    }


    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

}

// prints metadata of an index node and its pointers/keys
void print_index_node(int fd, int id){
    // Open file in append mode
    FILE* file = fopen("output.txt", "a");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Get block
    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(fd, id, block));

    // Get data
    void* data = BF_Block_GetData(block);
    BPLUS_INDEX_NODE* metadata = get_metadata_indexnode(fd, id);

    // Print metadata
    fprintf(file, "Block id: %d\n", metadata->block_id);
    fprintf(file, "Number of keys: %d\n", metadata->num_keys);
    fprintf(file, "Parent id: %d\n", metadata->parent_id);

    // Print keys and pointers
    for (int i = 1; i < 2 * metadata->num_keys + 1; i += 2) {
        int key;
        int child_id;

        // First pointer (left)
        if (i == 1) {
            memcpy(&child_id, data + sizeof(BPLUS_INDEX_NODE), sizeof(int));
            fprintf(file, "%d | ", child_id);
        }

        // Key and pointer
        memcpy(&key, data + sizeof(BPLUS_INDEX_NODE) + i * sizeof(int), sizeof(int));
        memcpy(&child_id, data + sizeof(BPLUS_INDEX_NODE) + (i + 1) * sizeof(int), sizeof(int));
        fprintf(file, "Key: %d | %d | ", key, child_id);
    }

    fprintf(file, "\n\n");

    // Release resources
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    // Close file
    fclose(file);
}



// returns true if index node with id is full, false otherwise
bool is_full_indexnode(int fd, int id, BPLUS_INFO* bplus_info){
    
    bool is_full = false;

    BF_Block* block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(fd, id, block));

    void* data = BF_Block_GetData(block);
    BPLUS_INDEX_NODE* metadata = get_metadata_indexnode(fd, id);

    int max_keys = bplus_info->max_keys_per_index;

    if(metadata->num_keys == max_keys){
        is_full = true;
    }
    

    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    
    return is_full;
}