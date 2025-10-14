#include "BlockAccess.h"

#include <cstring>



RecId BlockAccess::linearSearch(int relId, char *attrName, Attribute attrVal, int op)
{
    // get the previous search index of the relation relId from the relation cache
    // (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId,&prevRecId);

    // let block and slot denote the record id of the record being currently checked
    int block,slot;

    // if the current search index record is invalid(i.e. both block and slot = -1)
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // (no hits from previous search; search should start from the
        // first record itself)
        RelCatEntry relCatBuf;
        // get the first record block of the relation from the relation cache
        // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
        RelCacheTable::getRelCatEntry(relId,&relCatBuf);
        
        // block = first record block of the relation
        // slot = 0
        block=relCatBuf.firstBlk;
        slot=0;
    }
    else
    {
        // (there is a hit from previous search; search should start from
        // the record next to the search index record)
        
        // block = search index's block
        // slot = search index's slot + 1
        block=prevRecId.block;
        slot=prevRecId.slot+1;
    }

    /* The following code searches for the next record in the relation
       that satisfies the given condition
       We start from the record id (block, slot) and iterate over the remaining
       records of the relation
    */
    while (block != -1)
    {
        /* create a RecBuffer object for block (use RecBuffer Constructor for
           existing block) */
        RecBuffer recBuffer(block);
        HeadInfo header;
        // get header of the block using RecBuffer::getHeader() function
        recBuffer.getHeader(&header);
        // get the record with id (block, slot) using RecBuffer::getRecord()
        Attribute record[header.numAttrs];
        recBuffer.getRecord(record,slot);
        // get slot map of the block using RecBuffer::getSlotMap() function
        unsigned char slotMap[header.numSlots];
        recBuffer.getSlotMap(slotMap);

        // If slot >= the number of slots per block(i.e. no more slots in this block)
        if(slot>=header.numSlots)
        {
             block = header.rblock;
             slot = 0;
            continue;  // continue to the beginning of this while loop
        }

        // if slot is free skip the loop
        // (i.e. check if slot'th entry in slot map of block contains SLOT_UNOCCUPIED)
        if(slotMap[slot]==SLOT_UNOCCUPIED)
        {
            slot++;// increment slot and continue to the next record slot
            continue;
        }
        AttrCatEntry attrCatBuf;
        // compare record's attribute value to the the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute
            from the attribute cache entry of the relation using
            AttrCacheTable::getAttrCatEntry()
        */
        /* use the attribute offset to get the value of the attribute from
           current record */
        int ret=AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);
        int cmpVal=compareAttrs(record[attrCatBuf.offset],attrVal,attrCatBuf.attrType);  // will store the difference between the attributes
        // set cmpVal using compareAttrs()

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if (
            (op == NE && cmpVal != 0) ||    // if op is "not equal to"
            (op == LT && cmpVal < 0) ||     // if op is "less than"
            (op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) ||    // if op is "equal to"
            (op == GT && cmpVal > 0) ||     // if op is "greater than"
            (op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
        ) {
            /*
            set the search index in the relation cache as
            the record id of the record that satisfies the given condition
            (use RelCacheTable::setSearchIndex function)
            */
           RecId searchIndex={block,slot};
           RelCacheTable::setSearchIndex(relId,&searchIndex);
            return searchIndex;
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1, -1};
}






int BlockAccess::renameRelation(char *oldName, char *newName)
{
   /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute newRelationName;    // set newRelationName with newName
    memcpy(newRelationName.sVal,newName,ATTR_SIZE);
    // search the relation catalog for an entry with "RelName" = newRelationName
    
    RecId recId=BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,newRelationName,EQ);
    // If relation with name newName already exists (result of linearSearch
    //                                               is not {-1, -1})
    //    return E_RELEXIST;
    if(recId.block!=-1 || recId.slot!=-1){
        return E_RELEXIST;
    }

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */

     RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName;    // set oldRelationName with oldName
    memcpy(oldRelationName.sVal,oldName,ATTR_SIZE);

    // search the relation catalog for an entry with "RelName" = oldRelationName
     recId=BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,oldRelationName,EQ);
    // If relation with name oldName does not exist (result of linearSearch is {-1, -1})
    //    return E_RELNOTEXIST;
    if(recId.block==-1 && recId.slot==-1){
        return E_RELNOTEXIST;
    }
    /* get the relation catalog record of the relation to rename using a RecBuffer
       on the relation catalog [RELCAT_BLOCK] and RecBuffer.getRecord function
    */
   RecBuffer recBuffer(recId.block);
   Attribute record[RELCAT_NO_ATTRS];
   recBuffer.getRecord(record,recId.slot);
   
    /* update the relation name attribute in the record with newName.
       (use RELCAT_REL_NAME_INDEX) */
       memcpy(&record[RELCAT_REL_NAME_INDEX],&newRelationName,ATTR_SIZE);
    // set back the record value using RecBuffer.setRecord
    recBuffer.setRecord(record,recId.slot);

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    /*
    update all the attribute catalog entries in the attribute catalog corresponding
    to the relation with relation name oldName to the relation name newName
    */

    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */

    //for i = 0 to numberOfAttributes :
    //    linearSearch on the attribute catalog for relName = oldRelationName
    //    get the record using RecBuffer.getRecord
    //
    //    update the relName field in the record to newName
    //    set back the record using RecBuffer.setRecord
    while(true){
        RecId attrEntryId=BlockAccess::linearSearch(ATTRCAT_RELID,(char*)ATTRCAT_ATTR_RELNAME,oldRelationName,EQ);
        if(attrEntryId.block==-1 &&attrEntryId.slot==-1){
            break;
        }
        RecBuffer attrCatRecBuffer(attrEntryId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatRecBuffer.getRecord(attrCatRecord,attrEntryId.slot);
        memcpy(&attrCatRecord[ATTRCAT_REL_NAME_INDEX],&newRelationName,ATTR_SIZE);
        attrCatRecBuffer.setRecord(attrCatRecord,attrEntryId.slot);
    }
    return SUCCESS;
}




int BlockAccess::renameAttribute(char *relName, char *oldName, char *newName)
{
     /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute relNameAttr;    // set relNameAttr to relName
    memcpy(relNameAttr.sVal,relName,ATTR_SIZE);
    // Search for the relation with name relName in relation catalog using linearSearch()
    RecId recId=BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,relNameAttr,EQ);
    // If relation with name relName does not exist (search returns {-1,-1})
    //    return E_RELNOTEXIST;
    if(recId.block==-1&&recId.slot==-1){
        return E_RELNOTEXIST;
    }

    /* reset the searchIndex of the attribute catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    /* declare variable attrToRenameRecId used to store the attr-cat recId
    of the attribute to rename */
     RecId attrToRenameRecId{-1, -1};
    /* iterate over all Attribute Catalog Entry record corresponding to the
       relation to find the required attribute */
    while (true) {

        // linear search on the attribute catalog for RelName = relNameAttr
         RecId attrRecId=BlockAccess::linearSearch(ATTRCAT_RELID,(char*)ATTRCAT_ATTR_RELNAME,relNameAttr,EQ);
        // if there are no more attributes left to check (linearSearch returned {-1,-1})
        //     break;
        if(attrRecId.block==-1&&attrRecId.slot==-1){
            break;
        }
        /* Get the record from the attribute catalog using RecBuffer.getRecord
          into attrCatEntryRecord */
        RecBuffer recBuffer(attrRecId.block);
        Attribute record[ATTRCAT_NO_ATTRS];
        recBuffer.getRecord(record,attrRecId.slot);
        // if attrCatEntryRecord.attrName = oldName
        //     attrToRenameRecId = block and slot of this record
            char attrName[ATTR_SIZE];
            memcpy(attrName,record[ATTRCAT_ATTR_NAME_INDEX].sVal,ATTR_SIZE);

            if(strcmp(attrName,oldName)==0){
                attrToRenameRecId=attrRecId;
            }
           
        // if attrCatEntryRecord.attrName = newName
        //     return E_ATTREXIST;
         if(strcmp(attrName,newName)==0){
                return E_ATTREXIST;
            }

    }//end the loop


     if (attrToRenameRecId.block==-1&&attrToRenameRecId.slot==-1){ 
         return E_ATTRNOTEXIST;
     }

    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    /*   declare a RecBuffer for attrToRenameRecId.block and get the record at
         attrToRenameRecId.slot */
    //   update the AttrName of the record with newName
    //   set back the record with RecBuffer.setRecord
        RecBuffer BuffertoRename(attrToRenameRecId.block);
        Attribute recordToRename[ATTRCAT_NO_ATTRS];
        BuffertoRename.getRecord(recordToRename,attrToRenameRecId.slot);
        memcpy(recordToRename[ATTRCAT_ATTR_NAME_INDEX].sVal,newName,ATTR_SIZE);
        BuffertoRename.setRecord(recordToRename,attrToRenameRecId.slot);

    return SUCCESS;
}



int BlockAccess::insert(int relId, Attribute *record) {
    // get the relation catalog entry from relation cache
    // ( use RelCacheTable::getRelCatEntry() of Cache Layer)
    RelCatEntry RelCatBuf;
    int ret=RelCacheTable::getRelCatEntry(relId,&RelCatBuf);
    if(ret!=SUCCESS){
        return ret;
    }

    int blockNum = RelCatBuf.firstBlk;    

    // rec_id will be used to store where the new record will be inserted
    RecId rec_id = {-1, -1};

    int numOfSlots = RelCatBuf.numSlotsPerBlk;
    int numOfAttributes = RelCatBuf.numAttrs;

    int prevBlockNum = -1;      /* block number of the last element in the linked list = -1 */

    /*
        Traversing the linked list of existing record blocks of the relation
        until a free slot is found OR
        until the end of the list is reached
    */
    while (blockNum != -1) {
        // create a RecBuffer object for blockNum (using appropriate constructor!)
            RecBuffer currentblock(blockNum);
        // get header of block(blockNum) using RecBuffer::getHeader() function
            HeadInfo head;
            currentblock.getHeader(&head);
        // get slot map of block(blockNum) using RecBuffer::getSlotMap() function
            unsigned char slotMap[numOfSlots];
            currentblock.getSlotMap(slotMap);
        // search for free slot in the block 'blockNum' and store it's rec-id in rec_id
        // (Free slot can be found by iterating over the slot map of the block)
        int freeslot=-1;
        for(int i=0;i<numOfSlots;i++){
            if(slotMap[i]==SLOT_UNOCCUPIED){
                freeslot=i;
                break;
            }
        }
        /* slot map stores SLOT_UNOCCUPIED if slot is free and
           SLOT_OCCUPIED if slot is occupied) */
        if(freeslot!=-1){

            rec_id.block=blockNum;
            rec_id.slot=freeslot;
            break;
        }
        /* if a free slot is found, set rec_id and discontinue the traversal
           of the linked list of record blocks (break from the loop) */

        /* otherwise, continue to check the next block by updating the
           block numbers as follows:
              update prevBlockNum = blockNum
              update blockNum = header.rblock (next element in the linked
                                               list of record blocks)
        */

        prevBlockNum=blockNum;
        blockNum=head.rblock;
    }

    //  if no free slot is found in existing record blocks (rec_id = {-1, -1})
    if(rec_id.block==-1||rec_id.slot==-1)
    {
        // if relation is RELCAT, do not allocate any more blocks
        //     return E_MAXRELATIONS;
        if(relId==RELCAT_RELID){
            return E_MAXRELATIONS;
        }
        RecBuffer newBlock;
        int newblocknum=newBlock.getBlockNum();
        // Otherwise,
        // get a new record block (using the appropriate RecBuffer constructor!)
        // get the block number of the newly allocated block
        // (use BlockBuffer::getBlockNum() function)
        // let ret be the return value of getBlockNum() function call
        if (newblocknum == E_DISKFULL) {
            return E_DISKFULL;
        }

        // Assign rec_id.block = new block number(i.e. ret) and rec_id.slot = 0
        rec_id.block=newblocknum;
        rec_id.slot=0;
        /*
            set the header of the new record block such that it links with
            existing record blocks of the relation
            set the block's header as follows:
            blockType: REC, pblock: -1
            lblock
                  = -1 (if linked list of existing record blocks was empty
                         i.e this is the first insertion into the relation)
                  = prevBlockNum (otherwise),
            rblock: -1, numEntries: 0,
            numSlots: numOfSlots, numAttrs: numOfAttributes
            (use BlockBuffer::setHeader() function)
        */
        HeadInfo newhead;
        newBlock.getHeader(&newhead);
        newhead.lblock=prevBlockNum;
        newhead.numAttrs=numOfAttributes;
        newhead.numSlots=numOfSlots;
        newBlock.setHeader(&newhead);
        /*
            set block's slot map with all slots marked as free
            (i.e. store SLOT_UNOCCUPIED for all the entries)
            (use RecBuffer::setSlotMap() function)
        */
       unsigned char newblockslotmap[numOfSlots];
       newBlock.getSlotMap(newblockslotmap);
       for(int i=0;i<numOfSlots;i++){
        newblockslotmap[i]=SLOT_UNOCCUPIED;
       }
       newBlock.setSlotMap(newblockslotmap);

        // if prevBlockNum != -1
        if(prevBlockNum!=-1)
        {
            // create a RecBuffer object for prevBlockNum
            // get the header of the block prevBlockNum and
            RecBuffer prevblock(prevBlockNum);
            HeadInfo prevheader;
            prevblock.getHeader(&prevheader);
            prevheader.rblock=rec_id.block;
            // update the rblock field of the header to the new block
            // number i.e. rec_id.block
            // (use BlockBuffer::setHeader() function)
            prevblock.setHeader(&prevheader);
        }
        // else
        else
        {
            // update first block field in the relation catalog entry to the
            // new block (using RelCacheTable::setRelCatEntry() function)
            RelCatBuf.firstBlk=rec_id.block;
            RelCatBuf.lastBlk=rec_id.block;
            RelCacheTable::setRelCatEntry(relId,&RelCatBuf);
        }

        // update last block field in the relation catalog entry to the
        // new block (using RelCacheTable::setRelCatEntry() function)
    }

    // create a RecBuffer object for rec_id.block
    // insert the record into rec_id'th slot using RecBuffer.setRecord())
    RecBuffer blocktoinsert(rec_id.block);
    blocktoinsert.setRecord(record,rec_id.slot);
    /* update the slot map of the block by marking entry of the slot to
       which record was inserted as occupied) */
       unsigned char slotpmaptoinsert[numOfSlots];
       blocktoinsert.getSlotMap(slotpmaptoinsert);
    // (ie store SLOT_OCCUPIED in free_slot'th entry of slot map)
    // (use RecBuffer::getSlotMap() and RecBuffer::setSlotMap() functions)
    slotpmaptoinsert[rec_id.slot]=SLOT_OCCUPIED;
    blocktoinsert.setSlotMap(slotpmaptoinsert);

    // increment the numEntries field in the header of the block to
    // which record was inserted
    // (use BlockBuffer::getHeader() and BlockBuffer::setHeader() functions)
    HeadInfo headtoinsert;
    blocktoinsert.getHeader(&headtoinsert);
    headtoinsert.numEntries++;
    blocktoinsert.setHeader(&headtoinsert);
    // Increment the number of records field in the relation cache entry for
    // the relation. (use RelCacheTable::setRelCatEntry function)
    RelCatBuf.numRecs++;
    RelCacheTable::setRelCatEntry(relId,&RelCatBuf);

    return SUCCESS;
}




/*
NOTE: This function will copy the result of the search to the `record` argument.
      The caller should ensure that space is allocated for `record` array
      based on the number of attributes in the relation.
*/
int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    // Declare a variable called recid to store the searched record
    RecId recId;
    recId=linearSearch(relId,attrName,attrVal,op);
    /* search for the record id (recid) corresponding to the attribute with
    attribute name attrName, with value attrval and satisfying the condition op
    using linearSearch() */

    // if there's no record satisfying the given condition (recId = {-1, -1})
    //    return E_NOTFOUND;
    if(recId.block==-1||recId.slot==-1){
        return E_NOTFOUND;
    }
    RecBuffer recBuffer(recId.block);
    
    /* Copy the record with record id (recId) to the record buffer (record)
       For this Instantiate a RecBuffer class object using recId and
       call the appropriate method to fetch the record
    */
   recBuffer.getRecord(record,recId.slot);

    return SUCCESS;
}






int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {
    // if the relation to delete is either Relation Catalog or Attribute Catalog,
    //     return E_NOTPERMITTED
        // (check if the relation names are either "RELATIONCAT" and "ATTRIBUTECAT".
        // you may use the following constants: RELCAT_NAME and ATTRCAT_NAME)
    if(strcmp(relName,(char*)RELCAT_RELNAME)==0||strcmp(relName,(char*)ATTRCAT_RELNAME)==0){
        return E_NOTPERMITTED;
    }
    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */

       RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr; // (stores relName as type union Attribute)
    // assign relNameAttr.sVal = relName
    strcpy(relNameAttr.sVal,relName);


    //  linearSearch on the relation catalog for RelName = relNameAttr
    RecId recId=linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,relNameAttr,EQ);

    // if the relation does not exist (linearSearch returned {-1, -1})
    //     return E_RELNOTEXIST
    if(recId.block==-1||recId.slot==-1){
        return E_RELNOTEXIST;
    }

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    /* store the relation catalog record corresponding to the relation in
       relCatEntryRecord using RecBuffer.getRecord */
       RecBuffer recBuffer(recId.block);
       recBuffer.getRecord(relCatEntryRecord,recId.slot);

    /* get the first record block of the relation (firstBlock) using the
       relation catalog entry record */
       int currentblock=relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    /* get the number of attributes corresponding to the relation (numAttrs)
       using the relation catalog entry record */

       /*
     Delete all the record blocks of the relation
    */
    // for each record block of the relation:
    //     get block header using BlockBuffer.getHeader
    //     get the next block from the header (rblock)
    //     release the block using BlockBuffer.releaseBlock
    //
    //     Hint: to know if we reached the end, check if nextBlock = -1


        while(currentblock!=-1){
            RecBuffer currentblockBuffer(currentblock);
            HeadInfo currentblockheader;
            currentblockBuffer.getHeader(&currentblockheader);
            int nextblock=currentblockheader.rblock;
            currentblockBuffer.releaseBlock();
            currentblock=nextblock;

        }
    






    /***
        Deleting attribute catalog entries corresponding the relation and index
        blocks corresponding to the relation with relName on its attributes
    ***/

    // reset the searchIndex of the attribute catalog

    int numberOfAttributesDeleted = 0;
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    while(true) {
        RecId attrCatRecId;
        // attrCatRecId = linearSearch on attribute catalog for RelName = relNameAttr
        attrCatRecId=linearSearch(ATTRCAT_RELID,(char*)ATTRCAT_ATTR_RELNAME,relNameAttr,EQ);

        // if no more attributes to iterate over (attrCatRecId == {-1, -1})
        //     break;
        if(attrCatRecId.slot==-1||attrCatRecId.block==-1){
            break;
        }

        numberOfAttributesDeleted++;

        // create a RecBuffer for attrCatRecId.block
        // get the header of the block
        // get the record corresponding to attrCatRecId.slot
        RecBuffer currentblock(attrCatRecId.block);
        HeadInfo currentblockheader;
        currentblock.getHeader(&currentblockheader);
        Attribute record[ATTRCAT_NO_ATTRS];
        currentblock.getRecord(record,attrCatRecId.slot);

        // declare variable rootBlock which will be used to store the root
        // block field from the attribute catalog record.
        int rootBlock =record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
        // (This will be used later to delete any indexes if it exists)

        // Update the Slotmap for the block by setting the slot as SLOT_UNOCCUPIED
        // Hint: use RecBuffer.getSlotMap and RecBuffer.setSlotMap
        unsigned char slotMap[currentblockheader.numSlots];
        currentblock.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot]=SLOT_UNOCCUPIED;
        currentblock.setSlotMap(slotMap);

        /* Decrement the numEntries in the header of the block corresponding to
           the attribute catalog entry and then set back the header
           using RecBuffer.setHeader */
           currentblockheader.numEntries--;
           currentblock.setHeader(&currentblockheader);

        /* If number of entries become 0, releaseBlock is called after fixing
           the linked list.
        */
        if (currentblockheader.numEntries==0) {
            /* Standard Linked List Delete for a Block
               Get the header of the left block and set it's rblock to this
               block's rblock
            */
            int leftblock=currentblockheader.lblock;
            int rightblock=currentblockheader.rblock;


            // create a RecBuffer for lblock and call appropriate methods

            if (leftblock==-1) {
                /* Get the header of the right block and set it's lblock to
                   this block's lblock */
                // create a RecBuffer for rblock and call appropriate methods
                RecBuffer prevBlock(leftblock);
                HeadInfo prevblockheader;
                prevBlock.getHeader(&prevblockheader);
                prevblockheader.rblock=rightblock;
                prevBlock.setHeader(&prevblockheader);

            } 
            if(rightblock!=-1) {
                // (the block being released is the "Last Block" of the relation.)
                /* update the Relation Catalog entry's LastBlock field for this
                   relation with the block number of the previous block. */
                   RecBuffer nextblock(rightblock);
                   HeadInfo nextblockheader;
                   nextblock.getHeader(&nextblockheader);
                   nextblockheader.lblock=leftblock;
                   nextblock.setHeader(&nextblockheader);
            }

            // (Since the attribute catalog will never be empty(why?), we do not
            //  need to handle the case of the linked list becoming empty - i.e
            //  every block of the attribute catalog gets released.)

            // call releaseBlock()
            currentblock.releaseBlock();
        }


        // (the following part is only relevant once indexing has been implemented)
        // if index exists for the attribute (rootBlock != -1), call bplus destroy
        if (rootBlock != -1) {
            // delete the bplus tree rooted at rootBlock using BPlusTree::bPlusDestroy()
        }
    }



    /*** Delete the entry corresponding to the relation from relation catalog ***/
    // Fetch the header of Relcat block
    HeadInfo relCatHeader;
    recBuffer.getHeader(&relCatHeader);
    unsigned char recSlotMap[relCatHeader.numSlots];
    recBuffer.getSlotMap(recSlotMap);
    recSlotMap[recId.slot]=SLOT_UNOCCUPIED;
    recBuffer.setSlotMap(recSlotMap);
    /* Decrement the numEntries in the header of the block corresponding to the
       relation catalog entry and set it back */
    relCatHeader.numEntries--;
    recBuffer.setHeader(&relCatHeader);
    /* Get the slotmap in relation catalog, update it by marking the slot as
       free(SLOT_UNOCCUPIED) and set it back. */

    /*** Updating the Relation Cache Table ***/
    /** Update relation catalog record entry (number of records in relation
        catalog is decreased by 1) **/
    // Get the entry corresponding to relation catalog from the relation
    // cache and update the number of records and set it back
    // (using RelCacheTable::setRelCatEntry() function)
    RelCatEntry relCatBuf;
    RelCacheTable::getRelCatEntry(RELCAT_RELID,&relCatBuf);
    relCatBuf.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID,&relCatBuf);
    /** Update attribute catalog entry (number of records in attribute catalog
        is decreased by numberOfAttributesDeleted) **/
    // i.e., #Records = #Records - numberOfAttributesDeleted
    RelCatEntry attrCatBuf;
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID,&attrCatBuf);
    attrCatBuf.numRecs-=numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID,&attrCatBuf);

    // Get the entry corresponding to attribute catalog from the relation
    // cache and update the number of records and set it back
    // (using RelCacheTable::setRelCatEntry() function)

    return SUCCESS;
}