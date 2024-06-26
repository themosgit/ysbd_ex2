#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "bf.h"
#include "hash_file.h"
#include <math.h>
#define MAX_OPEN_FILES 20

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }


int array_open_file[20];

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

HT_ErrorCode HT_Init() {
  
  for(int i = 0; i < 20 ; i++){
   	 array_open_file[i] =- 1;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HT_ErrorCode HT_CreateIndex(const char *filename, int depth) {
   HT_INFO *info;
  int fd1;
  

  BF_Block *block;
  BF_Block_Init(&block);


  CALL_OR_DIE(BF_CreateFile(filename));
  
  CALL_OR_DIE(BF_OpenFile(filename, &fd1));
   
  CALL_OR_DIE(BF_AllocateBlock(fd1, block));

  void* data;
  data = BF_Block_GetData(block);
  info = data; //pame to info sto block
  info->global_depth = depth;
  info->bucket_size = floor((BF_BLOCK_SIZE-sizeof(HT_INFO))/sizeof(Record)) - 1;
  info->last_block = 0;
  
  
  int *hash_table = data + sizeof(HT_INFO);

  for(int i = 0; i < pow(2, depth); i++){
    hash_table[i] = -1;
  }
  BF_Block_SetDirty(block);
  BF_UnpinBlock(block);
  BF_CloseFile(fd1);
  return HT_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc){
  int y = -1; 
  int fd;
 
  do{
    y++;
  }while(array_open_file[y] != -1 && y < 20);
  
  if(y == 20) return HT_ERROR;

  BF_OpenFile(fileName, &fd);
  array_open_file[y] = fd;
  *indexDesc = y;
  return HT_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HT_ErrorCode HT_CloseFile(int indexDesc) {
  
  BF_CloseFile(array_open_file[indexDesc]);
  array_open_file[indexDesc] = -1;

  return HT_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HT_ErrorCode HT_InsertEntry(int indexDesc, Record record) {
    
  HT_INFO* info; 
  void* data;
  int fd = array_open_file[indexDesc];
  bucket_info* blinfo;
  BF_Block *block;
  BF_Block *htblock;
  BF_Block_Init(&htblock);
  BF_Block_Init(&block);
  BF_GetBlock(fd, 0, htblock);
  data = BF_Block_GetData(htblock);
  info = data;
  int pos = 0;
  char *hash = hashing(record.id, info->bucket_size);
  int* hash_table = data + sizeof(HT_INFO);
  /*for(int i = 0; i < pow(2, info->global_depth); i++){
        printf("hash_table[%d]:%d\n", i, hash_table[i]);
      }*/
  for(int i = info->global_depth - 1, j = 0; i >= 0 && j < info->global_depth; i--, j++){
    if(hash[j] == '1') pos += pow(2, i);
  }



  if(hash_table[pos] == -1){
    info->last_block++;
    BF_AllocateBlock(fd, block);
    data = BF_Block_GetData(block);
    blinfo = data;
    blinfo->full = 0;
    Record *rec = data + sizeof(bucket_info);
    memcpy(&rec[blinfo->full], &record, sizeof(record));
    blinfo->full++;
    blinfo->local_depth = 1;
    BF_Block_SetDirty(block);
    BF_UnpinBlock(block);
    hash_table[pos] = info->last_block;
     BF_Block_SetDirty(htblock);
      BF_UnpinBlock(htblock);
    //printf("record id:%d\n",record.id);
    //printf("BLOCK :%d\n",info->last_block);
    //printf("pos: %d\n",pos);
    return HT_OK;
  }



  int previous = hash_table[pos];
  BF_GetBlock(fd, hash_table[pos], block);
  data = BF_Block_GetData(block);
  blinfo = data;
  Record *rec = data + sizeof(bucket_info);
 


  if(blinfo->full == info->bucket_size){
    if(blinfo->local_depth == info->global_depth){
      info->global_depth++;
      for(int i = pow(2, info->global_depth - 1) - 1; i >= 0; i--){
        hash_table[2 * i + 1] = hash_table[i];
        hash_table[2 * i] = hash_table[i];                      //η τελευταια θεση θα ειναι η 2^γδ-1 , παμε προς τα πισω, διπλασιαζεται ο πινακας αρα κανουμε για δυο θεσεις αυτο
        }
      } 
      pos = 0;
      for(int i = info->global_depth - 1, j = 0; i >= 0 && j < info->global_depth; i--, j++){
        if(hash[j] == '1') pos += pow(2, i);
      }
      blinfo->local_depth++;
      int num_positions = pow(2, info->global_depth - blinfo->local_depth);
      int top = pos + 1;
      int bottom = pos - 1;
      int top_count = 0;
      int bottom_count = 0;
      while(hash_table[pos] == hash_table[top]){
        top++;
        top_count++;
      }  
      while(hash_table[pos] == hash_table[bottom]){
        bottom--;
        bottom_count++;
      }  
      //printf("top: %d bottom: %d\n", top_count, bottom_count);
      bucket_info* blinfo_new;
      BF_Block* block_new;
      BF_Block_Init(&block_new);
      BF_AllocateBlock(fd, block_new);
      data = BF_Block_GetData(block_new);
      blinfo_new = data;
      blinfo_new->full = 0;
      blinfo_new->local_depth = blinfo->local_depth;
      info->last_block++;
      hash_table[pos] = info->last_block;
      Record *rec_new = data + sizeof(bucket_info);
      for(int i = 0; i < blinfo->full; i++){
         pos = 0;
        char *hash = hashing(rec[i].id, info->bucket_size);
        for(int i = info->global_depth - 1, j = 0; i >= 0 && j < info->global_depth; i--, j++){
          if(hash[j] == '1') pos += pow(2, i);
        }
        if (hash_table[pos] != previous && hash_table[pos] == info->last_block ){                                 //τοποθετηση των παλιων και αδειασμα μπλοκ
          memcpy(&rec_new[blinfo_new->full], &rec[i], sizeof(Record));
          blinfo_new->full++;
          for(int j = i + 1; j < blinfo->full; j++){
            rec[j - 1] = rec[j];
          }
          blinfo->full--;
        }
      }
      BF_Block_SetDirty(htblock);
      BF_UnpinBlock(htblock);
      BF_Block_SetDirty(block);
      BF_UnpinBlock(block);
      BF_Block_SetDirty(block_new);
      BF_UnpinBlock(block_new);
      HT_InsertEntry(indexDesc, record);
    }else{
      
      memcpy(&rec[blinfo->full], &record, sizeof(Record));
      blinfo->full++;
      BF_Block_SetDirty(block);
      BF_UnpinBlock(block);
      BF_Block_SetDirty(htblock);
      BF_UnpinBlock(htblock);
    }
    //printf("BLOCK :%d\n",info->last_block);
    //printf("pos: %d\n",pos);
  return HT_OK;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {
  HT_INFO* info;
  int fd = array_open_file[indexDesc];
  BF_Block* block;
  void* data;
  bucket_info* blinfo;
  BF_Block_Init(&block);
  CALL_OR_DIE(BF_GetBlock(fd, 0, block));
  data = BF_Block_GetData(block);
  info = data;
  int *hash_table = data + sizeof(HT_INFO);
  if(id == NULL){
    for(int i = 1; i <= info->last_block; i++){
      CALL_OR_DIE(BF_GetBlock(fd, i, block));
      data = BF_Block_GetData(block);
      Record* rec = data + sizeof(bucket_info);
      blinfo = data;
      //printf("\n");
      for(int j = 0; j <= blinfo->full; j++){
          printf("%d %s %s %s \n", rec[j].id, rec[j].name, rec[j].surname, rec[j].city);
          BF_UnpinBlock(block);
      }
    }
    }
    else{
    char *hash = hashing(*id, info->bucket_size);
    int pos = 0;
    for(int i = info->global_depth - 1, j = 0; i >= 0 && j < info->global_depth; i--, j++){
    if(hash[j] == '1') pos += pow(2, i);
    }
    CALL_OR_DIE(BF_GetBlock(fd, hash_table[pos], block));
    data = BF_Block_GetData(block);
    blinfo = data;
    Record* rec = data + sizeof(bucket_info);
    for(int i = 0; i < blinfo->full; i++){
      if(rec[i].id == *id){
      printf("%d %s %s %s \n", rec[i].id, rec[i].name, rec[i].surname, rec[i].city);
      }
    }
    BF_UnpinBlock(block);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char *hashing(int rec_id, double bucket_size){
  int num_ob = ceil(RECORDS_NUM / bucket_size); //number of buckets that we need
  int gd = 1;
  

  
  if (num_ob > 2){
    for(int i = 2; i <= num_ob ; i *= 2){
      gd++; // to global depth einai o ekueths
    };
  }
  //printf("gd is:%d  num_ob is: %d\n", gd, num_ob);
  char *hash = malloc(sizeof(char) * gd);
  char *final_hash = malloc(sizeof(char) * gd);
  for(int i = 0; i < gd; i++){
    hash[i] = '0';
  }
  int i = 0;
  int position = floor(rec_id / (bucket_size));
  int k = gd - 1;
 bool is_odd = position % 2;
  while(gd >= 0){
    if(position / pow(2, gd - 1) >= 1) {    //gd-1 because we need the 2^0
      if(gd>1 || is_odd) hash[i] = '1';  //convert the number to binary //perittos
      position -= pow(2, gd-1);
    }
  i++;
  gd--;
  }

  int l = 0;
  for(int j = k; j >= 0; j--){
    final_hash[l] = hash[j];
    l++;
  }
  free(hash);

  return final_hash;
  
}

HT_ErrorCode hash_statitics(const char* filename){
  int fd;
  HT_INFO* info;
  void* data;
  bucket_info *binfo;
  printf("Hash Statistics Running...\n");
  BF_Block *block;

  BF_Block_Init(&block);
  BF_GetBlock(fd, 0, block); //για να παρουμε το info
  data = BF_Block_GetData(block);
  info = data;
  
  double sum_of_blocks = info->last_block;
  int* hash_table = data + sizeof(HT_INFO);
  int min = 10000000;
  int max = -1;
  int sum = 0;
  for(int i = 1; i <= info->last_block; i++){
    
    BF_GetBlock(fd,  i, block);
    data = BF_Block_GetData(block);
    binfo = data;
    if(binfo->full < min) min = binfo->full;
    if(binfo->full > max) max = binfo->full;
    sum += binfo->full;
    
  }
  double av = sum / sum_of_blocks;
  printf("Minimum Number Of Records In A Bucket is: %d\n", min);
  printf("Maximum Number Of Records In A Bucket is: %d\n", max);
  printf("Average Number Of Records In All Buckets is: %f\n", av);
}

 /*int Hash(unsigned char *key, unsigned int hashmask)
{

    int n = key;
    int count = 0; 
    if (n == 0){
        count = 1;
    }           
    while (n != 0) { 
        n = n / 10; 
        ++count; 
    } 
    int bytesNeeded = sizeof(count);
    *key = malloc(sizeof(unsigned char)*bytesNeeded);
    for (int i = 0; i < bytesNeeded; i++)
    {
        key[i] = (count >> (8* i) & 0xff);
    }
    unsigned int newSize = 0;
    for (int i = 0; i < bytesNeeded; i++)
    {
        newSize += (key[i] << (8 * i));
    }
    printf("Newsize is: %d\n", newSize);

    int i;
    unsigned int h = 5381;

    printf("ITS NOT RIGHT %d\n", count);
    for (i = 0; i < count; ++i) {
        h += (h << 5) + ((const char *)key)[i];
    }
    printf("BUT ITS OKAY\n");
    return h & hashmask;
}*/
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////