#include "OpenRelTable.h"
#include<stdlib.h>
#include <cstring>
#include<stdio.h>

//static member of the class and will hence need to be explicitly declared before it can be used.
OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

AttrCacheEntry* createList(int length) {
    AttrCacheEntry* head = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
    AttrCacheEntry* tail = head;
    for (int i = 1; i < length; i++) {
        tail->next = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
        tail = tail->next;
    }
    tail->next = nullptr;
    return head;
}



void clearList(AttrCacheEntry* head){
     for (AttrCacheEntry* it = head, *next; it != nullptr; it = next) {
        next = it->next;
        free(it);
    }
}
OpenRelTable::OpenRelTable() {

  // initialize relCache and attrCache with nullptr
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    OpenRelTable::tableMetaInfo[i].free=true;
  }

  /************ Setting up Relation Cache entries ************/
  // (we need to populate relation cache with entries for the relation catalog
  //  and attribute catalog.)

  /**** setting up Relation Catalog relation in the Relation Cache Table****/
  RecBuffer relCatBlock(RELCAT_BLOCK);
  Attribute relCatRecord[RELCAT_NO_ATTRS];



  /**** setting up Attribute Catalog relation in the Relation Cache Table ****/

  // set up the relation cache entry for the attribute catalog similarly
  // from the record at RELCAT_SLOTNUM_FOR_ATTRCAT

  // set the value at RelCacheTable::relCache[ATTRCAT_RELID]
  //we use the same relcacheentry for attrcat and relcat


  for(int i=RELCAT_RELID;i<=ATTRCAT_RELID;i++){
      relCatBlock.getRecord(relCatRecord, i);

      struct RelCacheEntry relCacheEntry;
      RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
      relCacheEntry.recId.block = RELCAT_BLOCK;
      relCacheEntry.recId.slot = i;

      // allocate this on the heap because we want it to persist outside this function
      RelCacheTable::relCache[i] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
      *(RelCacheTable::relCache[i]) = relCacheEntry;

      // in the tableMetaInfo array
      //   set free = false for RELCAT_RELID and ATTRCAT_RELID
     //   set relname for RELCAT_RELID and ATTRCAT_RELID

      tableMetaInfo[i].free=false;
      memcpy(tableMetaInfo[i].relName,relCacheEntry.relCatEntry.relName,ATTR_SIZE);
  }
  

  /************ Setting up Attribute cache entries ************/
  // (we need to populate attribute cache with entries for the relation catalog
  //  and attribute catalog.)

  /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  // iterate through all the attributes of the relation catalog and create a linked
  // list of AttrCacheEntry (slots 0 to 5)
  // for each of the entries, set
  //    attrCacheEntry.recId.block = ATTRCAT_BLOCK;
  //    attrCacheEntry.recId.slot = i   (0 to 5)
  //    and attrCacheEntry.next appropriately
  // NOTE: allocate each entry dynamically using malloc

  // set the next field in the last entry to nullptr
   auto relCatListHead=createList(RELCAT_NO_ATTRS);
   auto attrCacheEntry=relCatListHead;

   for(int i=0;i<RELCAT_NO_ATTRS;i++){
    attrCatBlock.getRecord(attrCatRecord,i);
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
    (attrCacheEntry->recId).block = ATTRCAT_BLOCK;
    (attrCacheEntry->recId).slot = i;
    attrCacheEntry = attrCacheEntry->next;

   }
  AttrCacheTable::attrCache[RELCAT_RELID] = relCatListHead;

  /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/
  // set up the attributes of the attribute cache similarly.
  // read slots 6-11 from attrCatBlock and initialise recId appropriately
    auto attrCatListHead = createList(ATTRCAT_NO_ATTRS);
    attrCacheEntry = attrCatListHead;
    for(int i=RELCAT_NO_ATTRS;i<RELCAT_NO_ATTRS+ATTRCAT_NO_ATTRS;i++){
        attrCatBlock.getRecord(attrCatRecord,i);
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
        (attrCacheEntry->recId).block = ATTRCAT_BLOCK;
        (attrCacheEntry->recId).slot = i;
        attrCacheEntry = attrCacheEntry->next;
    }
    AttrCacheTable::attrCache[ATTRCAT_RELID] = attrCatListHead;
  // set the value at AttrCacheTable::attrCache[ATTRCAT_RELID]

}




/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/
int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {

 for(int i=0;i<MAX_OPEN;i++){
     if(!tableMetaInfo[i].free&&strcmp(relName,tableMetaInfo[i].relName)==0 ){
        return i;
      } 
  }

  return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry() {

  /* traverse through the tableMetaInfo array,
    find a free entry in the Open Relation Table.*/
  for(int i=2;i<MAX_OPEN;i++){
    if(tableMetaInfo[i].free)
        return i;
  }

  // if found return the relation id, else return E_CACHEFULL.
  return E_CACHEFULL;
}



int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
  int alreadyexist=getRelId(relName);// (checked using OpenRelTable::getRelId())
  if(alreadyexist>=0){
      // return that relation id;
     return alreadyexist;
  }
  /* find a free slot in the Open Relation Table
     using OpenRelTable::getFreeOpenRelTableEntry(). */
    int freeslot=OpenRelTable::getFreeOpenRelTableEntry();

  if (freeslot==E_CACHEFULL){
    return E_CACHEFULL;
  }

  // let relId be used to store the free slot.
  int relId=freeslot;

  /****** Setting up Relation Cache entry for the relation ******/
  Attribute relNameAttribute;
  memcpy(relNameAttribute.sVal,relName,ATTR_SIZE);

  /* search for the entry with relation name, relName, in the Relation Catalog using
      BlockAccess::linearSearch().
      Care should be taken to reset the searchIndex of the relation RELCAT_RELID
      before calling linearSearch().*/
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
  // relcatRecId stores the rec-id of the relation `relName` in the Relation Catalog.
    RecId relcatRecId=BlockAccess::linearSearch(RELCAT_RELID,(char*)RELCAT_ATTR_RELNAME,relNameAttribute,EQ);

  if ( relcatRecId.block == -1 && relcatRecId.slot==-1) {
    // (the relation is not found in the Relation Catalog.)
      return E_RELNOTEXIST;
  }

  /* read the record entry corresponding to relcatRecId and create a relCacheEntry
      on it using RecBuffer::getRecord() and RelCacheTable::recordToRelCatEntry().
      update the recId field of this Relation Cache entry to relcatRecId.
      use the Relation Cache entry to set the relId-th entry of the RelCacheTable.
    NOTE: make sure to allocate memory for the RelCacheEntry using malloc()
  */
    RecBuffer recBuffer(relcatRecId.block);
    Attribute record[RELCAT_NO_ATTRS];
    recBuffer.getRecord(record,relcatRecId.slot);
    RelCatEntry relCatEntry;
    RelCacheTable::recordToRelCatEntry(record,&relCatEntry);

    RelCacheTable::relCache[relId]=(RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    RelCacheTable::relCache[relId]->recId=relcatRecId;
    RelCacheTable::relCache[relId]->relCatEntry=relCatEntry;

  /****** Setting up Attribute Cache entry for the relation ******/
    int numAttrs=relCatEntry.numAttrs;
  // let listHead be used to hold the head of the linked list of attrCache entries.
  AttrCacheEntry* listHead=createList(numAttrs);
  AttrCacheEntry*node=listHead;

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
  /*iterate over all the entries in the Attribute Catalog corresponding to each
  attribute of the relation relName by multiple calls of BlockAccess::linearSearch()
  care should be taken to reset the searchIndex of the relation, ATTRCAT_RELID,
  corresponding to Attribute Catalog before the first call to linearSearch().*/
while(true)
  {
      /* let attrcatRecId store a valid record id an entry of the relation, relName,
      in the Attribute Catalog.*/
      RecId attrcatRecId=BlockAccess::linearSearch(ATTRCAT_RELID,(char*)ATTRCAT_ATTR_RELNAME,relNameAttribute,EQ);
      if(attrcatRecId.block!=-1 && attrcatRecId.slot!=-1){
         RecBuffer recBuffer(attrcatRecId.block);
         Attribute attrcatrecord[ATTRCAT_NO_ATTRS];
         recBuffer.getRecord(attrcatrecord,attrcatRecId.slot);
         AttrCatEntry attrcatentry;
         AttrCacheTable::recordToAttrCatEntry(attrcatrecord,&attrcatentry);

         node->attrCatEntry=attrcatentry;
         node->recId=attrcatRecId;
         node=node->next;
      }
      else{
        break;
      }
      /* read the record entry corresponding to attrcatRecId and create an
      Attribute Cache entry on it using RecBuffer::getRecord() and
      AttrCacheTable::recordToAttrCatEntry().
      update the recId field of this Attribute Cache entry to attrcatRecId.
      add the Attribute Cache entry to the linked list of listHead .*/
      // NOTE: make sure to allocate memory for the AttrCacheEntry using malloc()
  }

  // set the relIdth entry of the AttrCacheTable to listHead.
  AttrCacheTable::attrCache[relId]=listHead;
  /****** Setting up metadata in the Open Relation Table for the relation******/
  // update the relIdth entry of the tableMetaInfo with free as false and
  // relName as the input.
  OpenRelTable::tableMetaInfo[relId].free=false;
  memcpy(OpenRelTable::tableMetaInfo[relId].relName,relCatEntry.relName,ATTR_SIZE);

  return relId;
}


int OpenRelTable::closeRel(int relId) {
  if (relId==RELCAT_RELID || relId==ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  if (relId<0 || relId>=MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (OpenRelTable::tableMetaInfo[relId].free) {
    return E_RELNOTOPEN;
  }

  // free the memory allocated in the relation and attribute caches which was
  // allocated in the OpenRelTable::openRel() function

  // update `tableMetaInfo` to set `relId` as a free slot
  // update `relCache` and `attrCache` to set the entry at `relId` to nullptr
  OpenRelTable::tableMetaInfo[relId].free=true;
  
  free(RelCacheTable::relCache[relId]);
  clearList(AttrCacheTable::attrCache[relId]);

  RelCacheTable::relCache[relId]=nullptr;
  AttrCacheTable::attrCache[relId]=nullptr;

  return SUCCESS;
}



OpenRelTable::~OpenRelTable() {
  // close all open relations (from rel-id = 2 onwards.
   for(int i=2;i<MAX_OPEN;i++){
    if(!tableMetaInfo[i].free){
      OpenRelTable::closeRel(i);
    }
   }
   // free the memory allocated for rel-id 0 to maxopen
   for(int i=0;i<MAX_OPEN;i++){
    free(RelCacheTable::relCache[i]);
    clearList(AttrCacheTable::attrCache[i]);

    RelCacheTable::relCache[i]=nullptr;
    AttrCacheTable::attrCache[i]=nullptr;
   }
}


