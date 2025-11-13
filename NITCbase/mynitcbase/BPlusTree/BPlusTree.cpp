#include "BPlusTree.h"
#include<stdio.h>
#include <cstring>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {

  // get the last hit search index
  IndexId lastSearchIndex;
  AttrCacheTable::getSearchIndex(relId, attrName, &lastSearchIndex);
  
  AttrCatEntry attrCatEntry;
  AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
  int attrType = attrCatEntry.attrType;

  // initialise the block and index to traverse
  int block, index;

  // if {-1, -1} start from beginning
  if(lastSearchIndex.block == -1 && lastSearchIndex.index == -1) {
    block = attrCatEntry.rootBlock;
    index = 0;

    if(block == -1)
      return RecId{-1, -1};
  }

  // else start from next slot
  else {
    block = lastSearchIndex.block;
    index = lastSearchIndex.index + 1;

    IndLeaf leafBuffer(block);
    HeadInfo leafHead;
    leafBuffer.getHeader(&leafHead);

    // move to next block, if current block is over
    if(index >= leafHead.numEntries) {
      block = leafHead.rblock;
      index = 0;
      if(block == -1) 
        return RecId{-1, -1};
    }
  }

  // iterate through leaf blocks starting from root (only enters this if its not a leaf)
  while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {

    // create buffer for current internal node
    IndInternal indBlockBuffer(block);
    HeadInfo indBlockHead;
    indBlockBuffer.getHeader(&indBlockHead);

    InternalEntry intEntry;

    // for these operations, first occurence will be always at leftmost leaf, so always move left
    if(op == NE || op == LT || op == LE) {
      indBlockBuffer.getEntry(&intEntry, 0);
      block = intEntry.lChild;
    }

    // for other operations, move to left of the first entry greater than given value
    else {
      int numEntries = indBlockHead.numEntries;

      bool found = false;
      for(int i=0; i<numEntries; i++) {
        indBlockBuffer.getEntry(&intEntry, i);

        // case for >= or =
        if(op == EQ || op == GE) {
          if(compareAttrs(intEntry.attrVal, attrVal, attrType) >= 0) {
            found = true;
            block = intEntry.lChild;
            break;
          }
        }

        // case for >
        else {
          if(compareAttrs(intEntry.attrVal, attrVal, attrType) > 0) {
            found = true;
            block = intEntry.lChild;
            break;
          }
        }
      }

      // if no entry is greater, move to last entry's rightchild
      if(!found) {
        indBlockBuffer.getEntry(&intEntry, numEntries-1);
        block = intEntry.rChild;
      }
    }
    
  }

  // now we found a LEAF node
  
  // iterate through all leaf nodes starting from the one we found
  while(block != -1) {
    IndLeaf leafBlockBuffer(block);
    HeadInfo leafBlockHead;
    leafBlockBuffer.getHeader(&leafBlockHead);
    int numEntries = leafBlockHead.numEntries;

    // iterate through current leaf node
    while(index < numEntries) {
      Index leafEntry;
      leafBlockBuffer.getEntry(&leafEntry, index);

      int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrType);

      // if found, return it and change search index
      if(
        (op == EQ && cmpVal == 0) ||
        (op == LE && cmpVal <= 0) ||
        (op == LT && cmpVal < 0) ||
        (op == GT && cmpVal > 0) ||
        (op == GE && cmpVal >= 0) ||
        (op == NE && cmpVal != 0)
      ) {
        IndexId foundIndexId = {block, index};
        AttrCacheTable::setSearchIndex(relId, attrName, &foundIndexId);
        return RecId{leafEntry.block, leafEntry.slot};
      }
      // for these operations, we will never find a suitable record later on, hence return
      else if((op == EQ || op == LE || op == LT) && cmpVal > 0) {
        return RecId{-1, -1};
      }

      index += 1;
    }

    // for all operations other than ne, its guaranteed to find result in 1 lead node only
    if(op != NE)
      break;

    // next block
    block = leafBlockHead.rblock;
    index = 0;
  }

  // nothing found 
  return RecId{-1, -1};
} 







int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {

    // if relId is either RELCAT_RELID or ATTRCAT_RELID:
    //     return E_NOTPERMITTED;
    if(relId==RELCAT_RELID||relId==ATTRCAT_RELID){
        return E_NOTPERMITTED;
    }
    AttrCatEntry attrCatBuf;
    int ret=AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);
    if(ret!=SUCCESS){
        return ret;
    }
    // get the attribute catalog entry of attribute `attrName`
    // using AttrCacheTable::getAttrCatEntry()

    // if getAttrCatEntry fails
    //     return the error code from getAttrCatEntry
   
    if (attrCatBuf.rootBlock!=-1) {
        return SUCCESS;
    }

    /******Creating a new B+ Tree ******/

    // get a free leaf block using constructor 1 to allocate a new block
    IndLeaf rootBlockBuf;

    // (if the block could not be allocated, the appropriate error code
    //  will be stored in the blockNum member field of the object)

    // declare rootBlock to store the blockNumber of the new leaf block
    int rootBlock = rootBlockBuf.getBlockNum();

    // if there is no more disk space for creating an index
    if (rootBlock == E_DISKFULL) {
        return E_DISKFULL;
    }
    attrCatBuf.rootBlock=rootBlock;
    AttrCacheTable::setAttrCatEntry(relId,attrName,&attrCatBuf);

    RelCatEntry relCatEntry;
    ret=RelCacheTable::getRelCatEntry(relId,&relCatEntry);
    if(ret!=SUCCESS){
        return ret;
    }
    // load the relation catalog entry into relCatEntry
    // using RelCacheTable::getRelCatEntry().

    int block = relCatEntry.firstBlk;

    /***** Traverse all the blocks in the relation and insert them one
           by one into the B+ Tree *****/
    while (block != -1) {

        // declare a RecBuffer object for `block` (using appropriate constructor)

        RecBuffer currentblock(block);


        unsigned char slotMap[relCatEntry.numSlotsPerBlk];
        currentblock.getSlotMap(slotMap);
        // load the slot map into slotMap using RecBuffer::getSlotMap().

        for(int i=0;i<relCatEntry.numSlotsPerBlk;i++)
        // for every occupied slot of the block
        {
            if(slotMap[i]==SLOT_UNOCCUPIED){
                continue;
            }
            Attribute record[relCatEntry.numAttrs];
            currentblock.getRecord(record,i);
            // load the record corresponding to the slot into `record`
            // using RecBuffer::getRecord().

            // declare recId and store the rec-id of this record in it
            // RecId recId{block, slot};
            RecId recId={block,i};
            int ret=BPlusTree::bPlusInsert(relId,attrName,record[attrCatBuf.offset],recId);
            if(ret==E_DISKFULL){
                return E_DISKFULL;
            }
            // insert the attribute value corresponding to attrName from the record
            // into the B+ tree using bPlusInsert.
            // (note that bPlusInsert will destroy any existing bplus tree if
            // insert fails i.e when disk is full)
            // retVal = bPlusInsert(relId, attrName, attribute value, recId);

            // if (retVal == E_DISKFULL) {
            //     // (unable to get enough blocks to build the B+ Tree.)
            //     return E_DISKFULL;
            // }
        }

        HeadInfo currentheader;
        currentblock.getHeader(&currentheader);
        block=currentheader.rblock;
        // get the header of the block using BlockBuffer::getHeader()

        // set block = rblock of current block (from the header)
    }

    return SUCCESS;
}



int BPlusTree::bPlusDestroy(int rootBlockNum) {
    if (rootBlockNum<0 || rootBlockNum>=DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (type == IND_LEAF) {
        // declare an instance of IndLeaf for rootBlockNum using appropriate
        // constructor
        IndLeaf rootnode(rootBlockNum);
        rootnode.releaseBlock();

        // release the block using BlockBuffer::releaseBlock().

        return SUCCESS;

    } else if (type == IND_INTERNAL) {
        // declare an instance of IndInternal for rootBlockNum using appropriate
        // constructor
        IndInternal rootnode(rootBlockNum);
        HeadInfo rootheader;
        rootnode.getHeader(&rootheader);
        
        InternalEntry indEntry;
        rootnode.getEntry(&indEntry,0);
        if(indEntry.lChild!=-1){
            int ret=bPlusDestroy(indEntry.lChild);
            if(ret!=SUCCESS){
                return ret;
            }
        }
        // load the header of the block using BlockBuffer::getHeader().
        
        /*iterate through all the entries of the internalBlk and destroy the lChild
        of the first entry and rChild of all entries using BPlusTree::bPlusDestroy().
        (the rchild of an entry is the same as the lchild of the next entry.
         take care not to delete overlapping children more than once ) */

        // release the block using BlockBuffer::releaseBlock().
        int numentries=rootheader.numEntries;
        for(int i=0;i<numentries;i++){

            rootnode.getEntry(&indEntry,i);
            if(indEntry.rChild!=-1){
                int ret=bPlusDestroy(indEntry.rChild);
                if(ret!=SUCCESS){   
                    return ret;
                }
            }
        }
        rootnode.releaseBlock();
        return SUCCESS;

    } else {
        // (block is not an index block.)
        return E_INVALIDBLOCK;
    }
}



int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatBuf;
    int ret=AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);
    if(ret!=SUCCESS){
        return ret;
    }

    // if getAttrCatEntry() failed
    //     return the error code

    int blockNum = attrCatBuf.rootBlock;

    if (blockNum==-1) {
        return E_NOINDEX;
    }

    // find the leaf block to which insertion is to be done using the
    // findLeafToInsert() function
    

    int leafBlkNum = findLeafToInsert(blockNum,attrVal, attrCatBuf.attrType);

    Index entry;
    entry.attrVal=attrVal;
    entry.block=recId.block;
    entry.slot=recId.slot;

    ret=insertIntoLeaf(relId,attrName,leafBlkNum,entry);
    
    // insert the attrVal and recId to the leaf block at blockNum using the
    // insertIntoLeaf() function.
    // declare a struct Index with attrVal = attrVal, block = recId.block and
    // slot = recId.slot to pass as argument to the function.
    // insertIntoLeaf(relId, attrName, leafBlkNum, Index entry)
    // NOTE: the insertIntoLeaf() function will propagate the insertion to the
    //       required internal nodes by calling the required helper functions
    //       like insertIntoInternal() or createNewRoot()

    if (ret==E_DISKFULL) {
        // destroy the existing B+ tree by passing the rootBlock to bPlusDestroy().
        BPlusTree::bPlusDestroy(blockNum);
        attrCatBuf.rootBlock=-1;
        AttrCacheTable::setAttrCatEntry(relId,attrName,&attrCatBuf);

        // update the rootBlock of attribute catalog cache entry to -1 using
        // AttrCacheTable::setAttrCatEntry().

        return E_DISKFULL;
    }

    return SUCCESS;
}



int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    while (StaticBuffer::getStaticBlockType(blockNum)!=IND_LEAF) {  // use StaticBuffer::getStaticBlockType()

        IndInternal intblock(blockNum);
        HeadInfo inthead;
        intblock.getHeader(&inthead);
        int numentry=inthead.numEntries;
        InternalEntry intentry;
        int targetindex=-1;
        for(int i=0;i<numentry;i++){
            intblock.getEntry(&intentry,i);
            if(compareAttrs(intentry.attrVal,attrVal,attrType)>0){
                targetindex=i;
                break;
            }
        }

        // declare an IndInternal object for block using appropriate constructor

        // get header of the block using BlockBuffer::getHeader()

        /* iterate through all the entries, to find the first entry whose
             attribute value >= value to be inserted.
             NOTE: the helper function compareAttrs() declared in BlockBuffer.h
                   can be used to compare two Attribute values. */

        if (targetindex==-1) {
            // set blockNum = rChild of (nEntries-1)'th entry of the block
            // (i.e. rightmost child of the block)
            intblock.getEntry(&intentry,numentry-1);
            blockNum=intentry.rChild;


        } else {
            // set blockNum = lChild of the entry that was found
            intblock.getEntry(&intentry,targetindex);
            blockNum=intentry.lChild;
        }
    }

    return blockNum;
}


int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatBuf;
    AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);


    IndLeaf leafblock(blockNum);
    // declare an IndLeaf instance for the block using appropriate constructor

    HeadInfo leafheader;
    leafblock.getHeader(&leafheader);
    int numentry=leafheader.numEntries;
    // store the header of the leaf index block into leafheader
    // using BlockBuffer::getHeader()

    // the following variable will be used to store a list of index entries with
    // existing indices + the new index to insert
    Index indices[numentry + 1];
    int targetindex=numentry;
    Index leafentry;
    for(int i=0;i<numentry;i++){
        leafblock.getEntry(&leafentry,i);
        if(compareAttrs(leafentry.attrVal,indexEntry.attrVal,attrCatBuf.attrType)>0){
            targetindex=i;
            break;
        }
    }

    for(int i=0;i<targetindex;i++){
        leafblock.getEntry(&indices[i],i);
    }
    indices[targetindex]=indexEntry;

    for(int i=targetindex;i<numentry;i++){
        leafblock.getEntry(&indices[i+1],i);
    }
    /*
    Iterate through all the entries in the block and copy them to the array indices.
    Also insert `indexEntry` at appropriate position in the indices array maintaining
    the ascending order.
    - use IndLeaf::getEntry() to get the entry
    - use compareAttrs() declared in BlockBuffer.h to compare two Attribute structs
    */

    if (numentry != MAX_KEYS_LEAF) {
        // (leaf block has not reached max limit)
        leafheader.numEntries++;
        leafblock.setHeader(&leafheader);
        // increment blockHeader.numEntries and update the header of block
        // using BlockBuffer::setHeader().
        for(int i=0;i<leafheader.numEntries;i++){
            leafblock.setEntry(&indices[i],i);
        }
        // iterate through all the entries of the array `indices` and populate the
        // entries of block with them using IndLeaf::setEntry().

        return SUCCESS;
    }

    // If we reached here, the `indices` array has more than entries than can fit
    // in a single leaf index block. Therefore, we will need to split the entries
    // in `indices` between two leaf blocks. We do this using the splitLeaf() function.
    // This function will return the blockNum of the newly allocated block or
    // E_DISKFULL if there are no more blocks to be allocated.

    int newRightBlk = splitLeaf(blockNum, indices);

    // if splitLeaf() returned E_DISKFULL
    //     return E_DISKFULL
    if(newRightBlk==E_DISKFULL){
        return newRightBlk;
    }

    if (leafheader.pblock!=-1) {  // check pblock in header
        InternalEntry intentry;
        intentry.attrVal=indices[MIDDLE_INDEX_LEAF].attrVal;
        intentry.lChild=blockNum;
        intentry.rChild=newRightBlk;
        // insert the middle value from `indices` into the parent block using the
        // insertIntoInternal() function. (i.e the last value of the left block)

        // the middle value will be at index 31 (given by constant MIDDLE_INDEX_LEAF)

        // create a struct InternalEntry with attrVal = indices[MIDDLE_INDEX_LEAF].attrVal,
        // lChild = currentBlock, rChild = newRightBlk and pass it as argument to
        // the insertIntoInternalFunction as follows


        // insertIntoInternal(relId, attrName, parent of current block, new internal entry)
        return  insertIntoInternal(relId, attrName,leafheader.pblock, intentry);

    } else {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.
        // To do this, call the createNewRoot() function with the following arguments

        // createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal,
        //               current block, new right block)
        return createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);
    }

    // if either of the above calls returned an error (E_DISKFULL), then return that
    // else return SUCCESS
    return SUCCESS;
}




int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    // declare rightBlk, an instance of IndLeaf using constructor 1 to obtain new
    // leaf index block that will be used as the right block in the splitting
    IndLeaf rightblock;
    IndLeaf leftblock(leafBlockNum);

    // declare leftBlk, an instance of IndLeaf using constructor 2 to read from
    // the existing leaf block

    int rightBlkNum =rightblock.getBlockNum();
    int leftBlkNum = leafBlockNum;

    if (rightBlkNum==E_DISKFULL) {
        //(failed to obtain a new leaf index block because the disk is full)
        return E_DISKFULL;
    }

    HeadInfo leftBlkHeader, rightBlkHeader;
    // get the headers of left block and right block using BlockBuffer::getHeader()
    rightblock.getHeader(&rightBlkHeader);
    leftblock.getHeader(&leftBlkHeader);


    rightBlkHeader.numEntries=(MAX_KEYS_LEAF+1)/2;
    rightBlkHeader.pblock=leftBlkHeader.pblock;
    rightBlkHeader.lblock=leftBlkNum;
    rightBlkHeader.rblock=leftBlkHeader.rblock;
    rightblock.setHeader(&rightBlkHeader);


    // set rightBlkHeader with the following values
    // - number of entries = (MAX_KEYS_LEAF+1)/2 = 32,
    // - pblock = pblock of leftBlk
    // - lblock = leftBlkNum
    // - rblock = rblock of leftBlk
    // and update the header of rightBlk using BlockBuffer::setHeader()


    leftBlkHeader.numEntries=(MAX_KEYS_LEAF+1)/2;
    leftBlkHeader.rblock=rightBlkNum;
    leftblock.setHeader(&leftBlkHeader);

    // set leftBlkHeader with the following values
    // - number of entries = (MAX_KEYS_LEAF+1)/2 = 32
    // - rblock = rightBlkNum
    // and update the header of leftBlk using BlockBuffer::setHeader() */

    for(int i=0;i<32;i++){
        leftblock.setEntry(&indices[i],i);
    }

    for(int i=32;i<64;i++){
        rightblock.setEntry(&indices[i],i-32);
    }
    // set the first 32 entries of leftBlk = the first 32 entries of indices array
    // and set the first 32 entries of newRightBlk = the next 32 entries of
    // indices array using IndLeaf::setEntry().

    return rightBlkNum;
}



int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatBuf;
    int ret=AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);
    if(ret!=SUCCESS){
        return ret;
    }
    IndInternal intblock(intBlockNum);


    // declare intBlk, an instance of IndInternal using constructor 2 for the block
    // corresponding to intBlockNum

    HeadInfo intheader;
    intblock.getHeader(&intheader);
    // load blockHeader with header of intBlk using BlockBuffer::getHeader().


    // declare internalEntries to store all existing entries + the new entry
    InternalEntry internalEntries[intheader.numEntries + 1];

    int targetindex=intheader.numEntries;
    InternalEntry intbuf;
    for(int i=0;i<intheader.numEntries;i++){
        intblock.getEntry(&intbuf,i);
        if(compareAttrs(intbuf.attrVal,intEntry.attrVal,attrCatBuf.attrType)>0){
            targetindex=i;
            break;
        }
    }


    for(int i=0;i<targetindex;i++){
        intblock.getEntry(&internalEntries[i],i);
    }
    internalEntries[targetindex]=intEntry;
    for(int i=targetindex;i<intheader.numEntries;i++){
        intblock.getEntry(&internalEntries[i+1],i);
    }

    if(targetindex<intheader.numEntries){
        internalEntries[targetindex+1].lChild=internalEntries[targetindex].rChild;
    }
    /*
    Iterate through all the entries in the block and copy them to the array
    `internalEntries`. Insert `indexEntry` at appropriate position in the
    array maintaining the ascending order.
        - use IndInternal::getEntry() to get the entry
        - use compareAttrs() to compare two structs of type Attribute

    Update the lChild of the internalEntry immediately following the newly added
    entry to the rChild of the newly added entry.
    */

    if (intheader.numEntries != MAX_KEYS_INTERNAL) {
        // (internal index block has not reached max limit)
        intheader.numEntries++;
        intblock.setHeader(&intheader);
        // increment blockheader.numEntries and update the header of intBlk
        // using BlockBuffer::setHeader().

        for(int i=0;i<intheader.numEntries;i++){
            intblock.setEntry(&internalEntries[i],i);
        }
        // iterate through all entries in internalEntries array and populate the
        // entries of intBlk with them using IndInternal::setEntry().

        return SUCCESS;
    }

    // If we reached here, the `internalEntries` array has more than entries than
    // can fit in a single internal index block. Therefore, we will need to split
    // the entries in `internalEntries` between two internal index blocks. We do
    // this using the splitInternal() function.
    // This function will return the blockNum of the newly allocated block or
    // E_DISKFULL if there are no more blocks to be allocated.

    int newRightBlk = splitInternal(intBlockNum, internalEntries);

    if (newRightBlk==E_DISKFULL) {

        // Using bPlusDestroy(), destroy the right subtree, rooted at intEntry.rChild.
        // This corresponds to the tree built up till now that has not yet been
        // connected to the existing B+ Tree

        return E_DISKFULL;
    }

    if (intheader.pblock!=-1) {  // (check pblock in header)
        // insert the middle value from `internalEntries` into the parent block
        // using the insertIntoInternal() function (recursively).
        InternalEntry entryinparent;
        entryinparent.attrVal=internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;
        entryinparent.lChild=intBlockNum;
        entryinparent.rChild=newRightBlk;

        // the middle value will be at index 50 (given by constant MIDDLE_INDEX_INTERNAL)

        // create a struct InternalEntry with lChild = current block, rChild = newRightBlk
        // and attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal
        // and pass it as argument to the insertIntoInternalFunction as follows

        // insertIntoInternal(relId, attrName, parent of current block, new internal entry)
        return insertIntoInternal(relId, attrName,intheader.pblock, entryinparent);

    } else {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.
        // To do this, call the createNewRoot() function with the following arguments

        // createNewRoot(relId, attrName,
        //               internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
        //               current block, new right block)

        return createNewRoot(relId, attrName,internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,intBlockNum,newRightBlk);

    }
    // if either of the above calls returned an error (E_DISKFULL), then return that
    // else return SUCCESS
    return SUCCESS;
}



int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
    // declare rightBlk, an instance of IndInternal using constructor 1 to obtain new
    // internal index block that will be used as the right block in the splitting
    IndInternal rightblock;
    IndInternal leftblock(intBlockNum);

    // declare leftBlk, an instance of IndInternal using constructor 2 to read from
    // the existing internal index block

    int rightBlkNum = rightblock.getBlockNum();
    int leftBlkNum = intBlockNum;

    if (rightBlkNum== E_DISKFULL) {
        //(failed to obtain a new internal index block because the disk is full)
        return E_DISKFULL;
    }

    HeadInfo leftBlkHeader, rightBlkHeader;
    // get the headers of left block and right block using BlockBuffer::getHeader()
    leftblock.getHeader(&leftBlkHeader);
    rightblock.getHeader(&rightBlkHeader);


    rightBlkHeader.numEntries=(MAX_KEYS_INTERNAL)/2;
    rightBlkHeader.pblock=leftBlkHeader.pblock;
    rightblock.setHeader(&rightBlkHeader);

    // set rightBlkHeader with the following values
    // - number of entries = (MAX_KEYS_INTERNAL)/2 = 50
    // - pblock = pblock of leftBlk
    // and update the header of rightBlk using BlockBuffer::setHeader()

    leftBlkHeader.numEntries=(MAX_KEYS_INTERNAL)/2;
    leftblock.setHeader(&leftBlkHeader);

    // set leftBlkHeader with the following values
    // - number of entries = (MAX_KEYS_INTERNAL)/2 = 50
    // and update the header using BlockBuffer::setHeader()
    for(int i=0;i<MIDDLE_INDEX_INTERNAL;i++){
        leftblock.setEntry(&internalEntries[i],i);
    }
    for(int i=MIDDLE_INDEX_INTERNAL+1;i<=100;i++){
        rightblock.setEntry(&internalEntries[i],i-MIDDLE_INDEX_INTERNAL-1);
    }
    /*
    - set the first 50 entries of leftBlk = index 0 to 49 of internalEntries
      array
    - set the first 50 entries of newRightBlk = entries from index 51 to 100
      of internalEntries array using IndInternal::setEntry().
      (index 50 will be moving to the parent internal index block)
    */

    InternalEntry entrybuf;
    rightblock.getEntry(&entrybuf,0);
    BlockBuffer childbuffer(entrybuf.lChild);
    HeadInfo childheader;
    childbuffer.getHeader(&childheader);
    childheader.pblock=rightBlkNum;
    childbuffer.setHeader(&childheader);


    //int type = /* block type of a child of any entry of the internalEntries array */;
    //            (use StaticBuffer::getStaticBlockType())

    for (int i=0;i<rightBlkHeader.numEntries;i++) {
        // declare an instance of BlockBuffer to access the child block using
        // constructor 2
        rightblock.getEntry(&entrybuf,i);
        BlockBuffer childbuffer(entrybuf.rChild);

        HeadInfo childheader;
        childbuffer.getHeader(&childheader);
        childheader.pblock=rightBlkNum;
        childbuffer.setHeader(&childheader);

        // update pblock of the block to rightBlkNum using BlockBuffer::getHeader()
        // and BlockBuffer::setHeader().
    }

    return rightBlkNum;
}



int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatBuf;
     AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);


    // declare newRootBlk, an instance of IndInternal using appropriate constructor
    // to allocate a new internal index block on the disk
    IndInternal newrootblock;
    int newRootBlkNum = newrootblock.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) {
        // (failed to obtain an empty internal index block because the disk is full)
        BPlusTree::bPlusDestroy(lChild);
        BPlusTree::bPlusDestroy(rChild);
        // Using bPlusDestroy(), destroy the right subtree, rooted at rChild.
        // This corresponds to the tree built up till now that has not yet been
        // connected to the existing B+ Tree

        return E_DISKFULL;
    }


    HeadInfo newrootheader;
    newrootblock.getHeader(&newrootheader);
    newrootheader.numEntries=1;
    newrootblock.setHeader(&newrootheader);

    InternalEntry intentry;
    intentry.attrVal=attrVal;
    intentry.lChild=lChild;
    intentry.rChild=rChild;

    newrootblock.setEntry(&intentry,0);

    // update the header of the new block with numEntries = 1 using
    // BlockBuffer::getHeader() and BlockBuffer::setHeader()

    // create a struct InternalEntry with lChild, attrVal and rChild from the
    // arguments and set it as the first entry in newRootBlk using IndInternal::setEntry()

    BlockBuffer leftchild(lChild),rightchild(rChild);
    HeadInfo leftheader,rightheader;
    leftchild.getHeader(&leftheader);
    leftheader.pblock=newRootBlkNum;
    leftchild.setHeader(&leftheader);

    rightchild.getHeader(&rightheader);
    rightheader.pblock=newRootBlkNum;
    rightchild.setHeader(&rightheader);
    // declare BlockBuffer instances for the `lChild` and `rChild` blocks using
    // appropriate constructor and update the pblock of those blocks to `newRootBlkNum`
    // using BlockBuffer::getHeader() and BlockBuffer::setHeader()

    attrCatBuf.rootBlock=newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId,attrName,&attrCatBuf);
    // update rootBlock = newRootBlkNum for the entry corresponding to `attrName`
    // in the attribute cache using AttrCacheTable::setAttrCatEntry().

    return SUCCESS;
}