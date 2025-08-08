#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <iostream>

/*
void copyArray(unsigned char* dest, unsigned char* src, int len) {
  for (int i = 0; i < len; i++)
    dest[i] = (int)src[i];
}



  

int main(int argc, char *argv[]) {
  Disk disk_run;

 
  unsigned char BMP[5*BLOCK_SIZE];
    for(int i=0;i<4;i++){
      unsigned char buffer[BLOCK_SIZE];
       Disk::readBlock(buffer, i);
       copyArray(BMP+i*BLOCK_SIZE, buffer, BLOCK_SIZE);
    }
  for(int i=0;i<100;i++){
  std::cout << (int)BMP[i];
  }

  return 0;
}

*/


void q1(){
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);

  for(int i=0;i<relCatHeader.numEntries;i++){

    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(relCatRecord, i);
    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);
    printf("no of attributes: %f\n", relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal);
    printf("no of records: %f\n", relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal);
    printf("no of slot: %f\n", relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal);

     int attrCatBlockNumber = ATTRCAT_BLOCK;
     /*
     while(attrCatBlockNumber!=-1){
      RecBuffer attrCatBuffer(attrCatBlockNumber);
      HeadInfo attrCatHeader;
      attrCatBuffer.getHeader(&attrCatHeader);
      attrCatBlockNumber=attrCatHeader.rblock;
      
      for(int j=0;j<attrCatHeader.numEntries;j++){
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord,j);
        if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,relCatRecord[RELCAT_REL_NAME_INDEX].sVal)==0){
          const char *attrType = (attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER )? "NUM" : "STR";
          printf(" %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
        }
      }
     }  */
     printf("\n");
  }
}






int main(int argc, char *argv[]) {
  Disk disk_run;

 
  q1();

  return 0;
}











































/*
void q2(){
  const char required[]="Students";
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);

    for(int i=0;i<relCatHeader.numEntries;i++){
      Attribute relCatRecord[RELCAT_NO_ATTRS];
      relCatBuffer.getRecord(relCatRecord, i);
      int attrCatBlockNumber = ATTRCAT_BLOCK;
      while(attrCatBlockNumber!=-1){
        RecBuffer attrCatBuffer(attrCatBlockNumber);
        HeadInfo attrCatHeader;
        attrCatBuffer.getHeader(&attrCatHeader);
        attrCatBlockNumber=attrCatHeader.rblock;
        for(int j=0;j<attrCatHeader.numEntries;j++){
          Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
          attrCatBuffer.getRecord(attrCatRecord,j);
          if(strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,required)==0){
            if(strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,"Class")==0){
              unsigned char buffer[BLOCK_SIZE];
              Disk::readBlock(buffer, ATTRCAT_BLOCK);
              memcpy(buffer+52+96*j+16,"Batch",ATTR_SIZE);
              Disk::writeBlock(buffer,ATTRCAT_BLOCK);
            }
          }
        }
      }
    }
}*/






