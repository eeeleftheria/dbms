#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "bp_file.h"
#include "bp_datanode.h"
#include "bp_indexnode.h"

//#############################################################//
//   MAIN.C  - SMALLER CAPACITY FOR KEYS AND RECORDS          //
// ########################################################### //

#define RECORDS_NUM 1950 // you can change it if you want
#define FILE_NAME_5 "data5.db"

// In this test, we create a B+ tree with smaller capacity for keys and records
// to demonstrate correct B+ tree behavior even when more splits occur
// in index nodes and data nodes, increasing the tree height.


#define MAX_KEYS_PER_INDEX 6
#define MAX_RECORDS_PER_BLOCK 4




#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
}

void insertEntries();



int main(){
 
  insertEntries();
}

void insertEntries(){

    FILE* file = fopen("output.txt", "w");
    fclose(file);

    BF_Init(LRU);
    BP_CreateFile(FILE_NAME_5);
    int file_desc;
    BPLUS_INFO* info = BP_OpenFile(FILE_NAME_5, &file_desc);



    BP_SetInfo(file_desc, info, MAX_RECORDS_PER_BLOCK, MAX_KEYS_PER_INDEX);

    Record record;
    
    for (int i = 0; i < RECORDS_NUM; i++){
        record = randomRecord();
        BP_InsertEntry(file_desc, info, record);
        
        // printf("inserting reec %d\n", record.id);

        // Record tmpRec;  // Instead of malloc
        // Record* result=&tmpRec;
        // int x = BP_GetEntry(file_desc, info, record.id, &result);
        // if(x == -1){
        //   printf("Record with id %d not found\n", record.id);
        // }    
    }
    BP_PrintTree(file_desc, info);

    BP_CloseFile(file_desc,info);
    BF_Close();
}


