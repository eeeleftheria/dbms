#ifndef BP_DATANODE_H
#define BP_DATANODE_H
#include <record.h>
#include <record.h>
#include <bf.h>
#include <bp_file.h>
#include <bp_indexnode.h>


#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }


typedef struct {
    int is_data_node;
    int num_records; // number of records
    int block_id;    // block id
    int next_block;  // id of next block
    int minKey;     // the min key value of the block
    int parent_id; // parent id
} BPLUS_DATA_NODE;


int create_data_node(int file_desc, BPLUS_INFO* bplus_info);
BPLUS_DATA_NODE* get_metadata_datanode(int file_desc, int block_id);
void print_data_node(int fd, int id);
void insert_rec_in_datanode(int fd, int node, BPLUS_INFO* bplus_info, Record rec);
int split_data_node(int fd, int node, BPLUS_INFO* bplus_info, Record rec);



#endif 