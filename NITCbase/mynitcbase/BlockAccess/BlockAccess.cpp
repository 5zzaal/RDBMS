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
