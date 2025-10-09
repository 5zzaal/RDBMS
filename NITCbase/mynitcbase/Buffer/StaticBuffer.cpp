#include "StaticBuffer.h"
#include<cstring>
// the declarations for this class can be found at "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer() {

  // copy blockAllocMap blocks from disk to buffer (using readblock() of disk)
  // blocks 0 to 3

  for(int i=0;i<4;i++){
    unsigned char buffer[BLOCK_SIZE];
    Disk::readBlock(buffer,i);
    memcpy(blockAllocMap+i*BLOCK_SIZE,buffer,BLOCK_SIZE);
  }
  // initialise all blocks as free
  for (int bufferIndex=0;bufferIndex<BUFFER_CAPACITY;bufferIndex++) {
    metainfo[bufferIndex].free = true;
    metainfo[bufferIndex].dirty=false;
    metainfo[bufferIndex].timeStamp=-1;
    metainfo[bufferIndex].blockNum=-1;
  }
}

// write back all modified blocks on system exit
StaticBuffer::~StaticBuffer() {

  // copy blockAllocMap blocks from disk to buffer (using readblock() of disk)
  // blocks 0 to 3
  
  /*iterate through all the buffer blocks,
    write back blocks with metainfo as free=false,dirty=true
    using Disk::writeBlock()
    */
   for(int i=0;i<4;i++){
    unsigned char buffer[BLOCK_SIZE];
    memcpy(buffer,blockAllocMap+i*BLOCK_SIZE,BLOCK_SIZE);
    Disk::writeBlock(buffer,i);
   }
   for(int i=0;i<BUFFER_CAPACITY;i++){
    if(!metainfo[i].free && metainfo[i].dirty){
      Disk::writeBlock(StaticBuffer::blocks[i],metainfo[i].blockNum);
    }
   }
}


int StaticBuffer::getFreeBuffer(int blockNum){
    // Check if blockNum is valid (non zero and less than DISK_BLOCKS)
    // and return E_OUTOFBOUND if not valid.
     if (blockNum < 0 || blockNum > DISK_BLOCKS) {
        return E_OUTOFBOUND;
      }

    // increase the timeStamp in metaInfo of all occupied buffers.
      for(int i=0;i<BUFFER_CAPACITY;i++){
          if(!metainfo[i].free){
            metainfo[i].timeStamp++;
          }
       }
    // let bufferNum be used to store the buffer number of the free/freed buffer.
    int  bufferNum=-1;
    // iterate through metainfo and check if there is any buffer free
       for(int i=0;i<BUFFER_CAPACITY;i++){
          if(metainfo[i].free){
               bufferNum=i;
               break;
           }
        }

    // if a free buffer is available, set bufferNum = index of that free buffer.

    // if a free buffer is not available,
    //     find the buffer with the largest timestamp
    //     IF IT IS DIRTY, write back to the disk using Disk::writeBlock()
    //     set bufferNum = index of this buffer
      if( bufferNum==-1){
        int highestTimeStamp=0;
        for(int i=0;i<BUFFER_CAPACITY;i++){
          if(metainfo[i].timeStamp>highestTimeStamp){
            highestTimeStamp=metainfo[i].timeStamp;
            bufferNum=i;
          }
        }

        if(metainfo[bufferNum].dirty){
          Disk::writeBlock(StaticBuffer::blocks[bufferNum],metainfo[bufferNum].blockNum);
        }

      }
    // update the metaInfo entry corresponding to bufferNum with
    // free:false, dirty:false, blockNum:the input block number, timeStamp:0.
      metainfo[bufferNum].dirty=false;
      metainfo[bufferNum].free=false;
      metainfo[bufferNum].timeStamp=0;
      metainfo[bufferNum].blockNum=blockNum;

    // return the bufferNum.
    return bufferNum;
}
/* Get the buffer index where a particular block is stored
   or E_BLOCKNOTINBUFFER otherwise
*/
int StaticBuffer::getBufferNum(int blockNum) {
  // Check if blockNum is valid (between zero and DISK_BLOCKS)
  // and return E_OUTOFBOUND if not valid.
  if(blockNum<0 || blockNum>DISK_BLOCKS){
    return E_OUTOFBOUND;
  }
  for(int i=0;i<BUFFER_CAPACITY;i++){
    if(metainfo[i].blockNum==blockNum){
        return i;
    }
  }

  // find and return the bufferIndex which corresponds to blockNum (check metainfo)

  // if block is not in the buffer
  return E_BLOCKNOTINBUFFER;
}

int StaticBuffer::setDirtyBit(int blockNum){
    // find the buffer index corresponding to the block using getBufferNum().
     // if blockNum is out of bound (bufferNum = E_OUTOFBOUND)
    //     return E_OUTOFBOUND
    if(blockNum<0 || blockNum>=DISK_BLOCKS){
      return E_OUTOFBOUND;
    }
    int targetBuffer=-1;

    for(int i=0;i<BUFFER_CAPACITY;i++){
      if(!metainfo[i].free && metainfo[i].blockNum==blockNum){
        targetBuffer=i;
        break;
      }
    }
   
    // if block is not present in the buffer (bufferNum = E_BLOCKNOTINBUFFER)
    //     return E_BLOCKNOTINBUFFER
    if(targetBuffer==-1){
      return E_BLOCKNOTINBUFFER;  
    }
    // else
    //     (the bufferNum is valid)
    //     set the dirty bit of that buffer to true in metainfo
    metainfo[targetBuffer].dirty=true;
    
    // return SUCCESS
    return SUCCESS;
}