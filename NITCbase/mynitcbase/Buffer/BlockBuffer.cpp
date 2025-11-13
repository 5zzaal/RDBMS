#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>
#include <stdio.h>

int compareAttrs(Attribute attr1, Attribute attr2, int attrType) {
  double diff;
  if(attrType == STRING)
    diff = strcmp(attr1.sVal, attr2.sVal);
  else
    diff = attr1.nVal - attr2.nVal;

  if(diff < 0) return -1;
  else if(diff == 0) return 0;
  else return 1;
}



BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blockType) {
    int blockTypeNum;
    if (blockType == 'R')
        blockTypeNum = REC;
    else if (blockType == 'I')
        blockTypeNum = IND_INTERNAL;
    else if (blockType == 'L')
        blockTypeNum = IND_LEAF;
    else 
        blockTypeNum = UNUSED_BLK;
    int blockNum = getFreeBlock(blockTypeNum);

    this->blockNum = blockNum;
    if (blockNum < 0 || blockNum >= DISK_BLOCKS)
        return;
}


RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

RecBuffer::RecBuffer() : BlockBuffer::BlockBuffer('R') {}

int BlockBuffer::getHeader(struct HeadInfo* head) {
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);    

    if (ret != SUCCESS)
        return ret;

    HeadInfo* header = (HeadInfo*) bufferPtr;

    head->numSlots = header->numSlots;
    head->numEntries = header->numEntries;
    head->numAttrs = header->numAttrs;
    head->lblock = header->lblock;
    head->rblock = header->rblock;

    return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo* head) {
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    HeadInfo* header = (HeadInfo*) bufferPtr;

    header->numSlots = head->numSlots;
    header->numEntries = head->numEntries;
    header->numAttrs = head->numAttrs;
    header->lblock = head->lblock;
    header->rblock = head->rblock;

    return StaticBuffer::setDirtyBit(this->blockNum);
}


int RecBuffer::getRecord(union Attribute* rec, int slotNum) {
    struct HeadInfo head;

    this->getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
        return ret;

    // header size -> 32
    // slotMapSize -> numSlots
    // index -> recordSize*slotNum
    int recordSize = attrCount * ATTR_SIZE;
    unsigned char* slotPointer = bufferPtr + 32 + slotCount + recordSize*slotNum;
    memcpy(rec, slotPointer, recordSize);
    return SUCCESS;
}

int RecBuffer::setRecord(union Attribute* rec, int slotNum) {
    unsigned char* bufferPtr;

    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    HeadInfo header;
    this->getHeader(&header);

    int numAttrs = header.numAttrs;
    int numSlots = header.numSlots;

    if (slotNum < 0 || slotNum >= numSlots)
        return E_OUTOFBOUND;
    
    int recordSize = numAttrs*ATTR_SIZE;
    unsigned char* recordPtr = bufferPtr + HEADER_SIZE + numSlots + slotNum*recordSize;

    memcpy(recordPtr, rec, recordSize);
    StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char* slotMap) {
    unsigned char* bufferPtr;

    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    struct HeadInfo head;
    getHeader(&head);

    int slotCount = head.numSlots;

    unsigned char* slotMapInBuffer = bufferPtr + HEADER_SIZE;

    memcpy(slotMap, slotMapInBuffer, slotCount);

    return SUCCESS;

}

int RecBuffer::setSlotMap(unsigned char* slotMap) {
    unsigned char* bufferPtr;

    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    struct HeadInfo head;
    getHeader(&head);

    int slotCount = head.numSlots;

    unsigned char* slotMapInBuffer = bufferPtr + HEADER_SIZE;

    memcpy(slotMapInBuffer, slotMap, slotCount);

    return StaticBuffer::setDirtyBit(this->blockNum);

}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char** bufferPtr) {
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER) {
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if (bufferNum == E_OUTOFBOUND)
            return E_OUTOFBOUND;

        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }
    else {
        for (int i = 0; i < BUFFER_CAPACITY; i++) {
            if (!StaticBuffer::metainfo[i].free)
                StaticBuffer::metainfo[i].timeStamp++;
        }

        StaticBuffer::metainfo[bufferNum].timeStamp = 0;
    }

    *bufferPtr = StaticBuffer::blocks[bufferNum];

    return SUCCESS;
}

int BlockBuffer::getBlockNum() {
    return this->blockNum;  
}

int BlockBuffer::setBlockType(int blockType) {
    unsigned char* bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    *((int32_t*)bufferPtr) = blockType;

    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    return StaticBuffer::setDirtyBit(this->blockNum);
}

int BlockBuffer::getFreeBlock(int blockType) {
    int freeBlock = -1;
    for (int i = 0; i < DISK_BLOCKS; i++) {
        if (StaticBuffer::blockAllocMap[i] == UNUSED_BLK) {
            freeBlock = i;
            break;
        }
    }

    if (freeBlock == -1)
        return E_DISKFULL;

    this->blockNum = freeBlock;

    int freeBuffer = StaticBuffer::getFreeBuffer(freeBlock);

    HeadInfo header;
    header.pblock = -1;
    header.lblock = -1;
    header.rblock = -1;
    header.numEntries = 0;
    header.numAttrs = 0;
    header.numSlots = 0;

    this->setHeader(&header);
    this->setBlockType(blockType);


    return freeBlock;
}

void BlockBuffer::releaseBlock() {
    if (blockNum < 0 || blockNum >= DISK_BLOCKS || StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
        return;


    int bufferNum = StaticBuffer::getBufferNum(blockNum);

    if (bufferNum == E_BLOCKNOTINBUFFER)
        return;

    StaticBuffer::metainfo[bufferNum].free = true;

    StaticBuffer::blockAllocMap[blockNum] = UNUSED_BLK;

    this->blockNum = INVALID_BLOCKNUM;
}

IndBuffer::IndBuffer(char blockType):BlockBuffer(blockType){}
IndBuffer::IndBuffer(int blockNum):BlockBuffer(blockNum){}

IndInternal::IndInternal():IndBuffer('I'){}
IndInternal::IndInternal(int blockNum):IndBuffer(blockNum){}

IndLeaf::IndLeaf():IndBuffer('L'){}
IndLeaf::IndLeaf(int blockNum):IndBuffer(blockNum){}




int IndInternal::getEntry(void *ptr, int indexNum) {
     if (indexNum<0 || indexNum>=MAX_KEYS_INTERNAL){
        return E_OUTOFBOUND;
     }

    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block*/

    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }
  
    struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;
    /*
    - copy the entries from the indexNum`th entry to *internalEntry
    - make sure that each field is copied individually as in the following code
    - the lChild and rChild fields of InternalEntry are of type int32_t
    - int32_t is a type of int that is guaranteed to be 4 bytes across every
      C++ implementation. sizeof(int32_t) = 4
    */

    /* the indexNum'th entry will begin at an offset of
       HEADER_SIZE + (indexNum * (sizeof(int) + ATTR_SIZE) )         [why?]
       from bufferPtr */

    unsigned char*entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

    memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
    memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
    memcpy(&(internalEntry->rChild), entryPtr + 20, sizeof(int32_t));

     return SUCCESS;
}


int IndInternal::setEntry(void *ptr, int indexNum) {
    // if the indexNum is not in the valid range of [0, MAX_KEYS_INTERNAL-1]
    //     return E_OUTOFBOUND.
    if(indexNum<0 || indexNum>=MAX_KEYS_INTERNAL){
        return E_OUTOFBOUND;
    }

    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */

    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    //     return the value returned by the call.
    if(ret!=SUCCESS){
        return ret;
    }

    // typecast the void pointer to an internal entry pointer
    struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;


    /*
    - copy the entries from *internalEntry to the indexNum`th entry
    - make sure that each field is copied individually as in the following code
    - the lChild and rChild fields of InternalEntry are of type int32_t
    - int32_t is a type of int that is guaranteed to be 4 bytes across every
      C++ implementation. sizeof(int32_t) = 4
    */

    /* the indexNum'th entry will begin at an offset of
       HEADER_SIZE + (indexNum * (sizeof(int) + ATTR_SIZE) )         [why?]
       from bufferPtr */

    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

    memcpy(entryPtr, &(internalEntry->lChild), 4);
    memcpy(entryPtr + 4, &(internalEntry->attrVal), ATTR_SIZE);
    memcpy(entryPtr + 20, &(internalEntry->rChild), 4);


    // update dirty bit using setDirtyBit()
    // if setDirtyBit failed, return the value returned by the call

  int dirtyRes = StaticBuffer::setDirtyBit(this->blockNum);
  if(dirtyRes != SUCCESS)
    return dirtyRes;
  
  return SUCCESS;
    // return SUCCESS
}



int IndLeaf::getEntry(void *ptr, int indexNum) {

     if (indexNum<0 ||indexNum >=MAX_KEYS_LEAF)
         return E_OUTOFBOUND;

    unsigned char *bufferPtr;
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret!=SUCCESS){
        return ret;
    }
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */

    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    //     return the value returned by the call.

    // copy the indexNum'th Index entry in buffer to memory ptr using memcpy

    /* the indexNum'th entry will begin at an offset of
       HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE)  from bufferPtr */


    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
    memcpy((struct Index *)ptr, entryPtr, LEAF_ENTRY_SIZE);

     return SUCCESS;
}


int IndLeaf::setEntry(void *ptr, int indexNum) {

    // if the indexNum is not in the valid range of [0, MAX_KEYS_LEAF-1]
    //     return E_OUTOFBOUND.
    if(indexNum<0 ||indexNum>= MAX_KEYS_LEAF){
        return E_OUTOFBOUND;
    }
    unsigned char *bufferPtr;
    /* get the starting address of the buffer containing the block
       using loadBlockAndGetBufferPtr(&bufferPtr). */
    int ret=loadBlockAndGetBufferPtr(&bufferPtr);
    // if loadBlockAndGetBufferPtr(&bufferPtr) != SUCCESS
    //     return the value returned by the call.
    if(ret!=SUCCESS){
        return ret;
    }


     Index *index=(Index*)ptr;
    // copy the Index at ptr to indexNum'th entry in the buffer using memcpy
   

    /* the indexNum'th entry will begin at an offset of
       HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE)  from bufferPtr */
    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
    memcpy(entryPtr, index, LEAF_ENTRY_SIZE);

    // update dirty bit using setDirtyBit()
    // if setDirtyBit failed, return the value returned by the call

    
  int dirtyRes = StaticBuffer::setDirtyBit(this->blockNum);
  if(dirtyRes != SUCCESS)
    return dirtyRes;
  
  return SUCCESS;
    //return SUCCESS
}



