#include "OpenRelTable.h"
#include <cstring>
#include <stdlib.h>

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

AttrCacheEntry* createList(int length) 
{
    AttrCacheEntry* head = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
    AttrCacheEntry* tail = head;
    for (int i = 1; i < length; i++) 
    {
        tail->next = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
        tail = tail->next;
    }
    tail->next = nullptr;
    return head;
}

void clearList(AttrCacheEntry* head) 
{
    for (AttrCacheEntry* it = head, *next; it != nullptr; it = next) 
    {
        next = it->next;
        free(it);
    }
}

OpenRelTable::OpenRelTable() 
{
    // initialise all values in relCache and attrCache to be nullptr and all entries
    // in tableMetaInfo to be free
    for (int i = 0; i < MAX_OPEN; ++i) 
    {
      RelCacheTable::relCache[i] = nullptr;
      AttrCacheTable::attrCache[i] = nullptr;
      OpenRelTable::tableMetaInfo[i].free = true;
    }
  
    /************ Setting up Relation Cache entries ************/
    // (we need to populate relation cache with entries for the relation catalog
    //  and attribute catalog.)
  
    /**** setting up Relation Catalog relation in the Relation Cache Table****/
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    
    for (int i = RELCAT_RELID; i <= ATTRCAT_RELID; i++) // 0->relCat, 1 -> attrCat
    { 
        relCatBlock.getRecord(relCatRecord, i);

        struct RelCacheEntry relCacheEntry;
        RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
        relCacheEntry.recId.block = RELCAT_BLOCK;
        relCacheEntry.recId.slot = i;

        RelCacheTable::relCache[i] = (struct RelCacheEntry*) malloc(sizeof(RelCacheEntry));
        *(RelCacheTable::relCache[i]) = relCacheEntry;
        /************ Setting up tableMetaInfo entries ************/

        // in the tableMetaInfo array
        //   set free = false for RELCAT_RELID and ATTRCAT_RELID
        //   set relname for RELCAT_RELID and ATTRCAT_RELID
        tableMetaInfo[i].free = false;
        memcpy(tableMetaInfo[i].relName, relCacheEntry.relCatEntry.relName, ATTR_SIZE);
    }

    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    auto relCatListHead = createList(RELCAT_NO_ATTRS);
    auto attrCacheEntry = relCatListHead;
    
    for (int i = 0; i < RELCAT_NO_ATTRS; i++) 
    {
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
        (attrCacheEntry->recId).block = ATTRCAT_BLOCK;
        (attrCacheEntry->recId).slot = i;
        
        attrCacheEntry = attrCacheEntry->next;
    }
    AttrCacheTable::attrCache[RELCAT_RELID] = relCatListHead;

    auto attrCatListHead = createList(ATTRCAT_NO_ATTRS);
    attrCacheEntry = attrCatListHead;
    for (int i = RELCAT_NO_ATTRS; i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS; i++)
    {
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
        (attrCacheEntry->recId).block = ATTRCAT_BLOCK;
        (attrCacheEntry->recId).slot = i;

        attrCacheEntry = attrCacheEntry->next;
    }
    AttrCacheTable::attrCache[ATTRCAT_RELID] = attrCatListHead;
}

/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/
int OpenRelTable::getRelId(char relName[ATTR_SIZE]) 
{

    /* traverse through the tableMetaInfo array,
      find the entry in the Open Relation Table corresponding to relName.*/
    // if found return the relation id, else indicate that the relation do not
    // have an entry in the Open Relation Table.
    for (int i = 0; i < MAX_OPEN; i++)
    {
        if (!tableMetaInfo[i].free && strcmp(relName, tableMetaInfo[i].relName) == 0)
        {
            return i;
        }
    }
    return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry() 
{

    /* traverse through the tableMetaInfo array,
      find a free entry in the Open Relation Table.*/
    // if found return the relation id, else return E_CACHEFULL.
    for (int i = 2; i < MAX_OPEN; i++)
    {
        if (tableMetaInfo[i].free)
        {
            return i;
        }
    }
    return E_CACHEFULL;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE])
{
    int alreadyExists = OpenRelTable::getRelId(relName);

    if(alreadyExists>=0/* the relation `relName` already has an entry in the Open Relation Table */)
    {
      // (checked using OpenRelTable::getRelId())
      // return that relation id;
      return alreadyExists;
    }
  
    /* find a free slot in the Open Relation Table
       using OpenRelTable::getFreeOpenRelTableEntry(). */
    int freeSlot = OpenRelTable::getFreeOpenRelTableEntry();

    if (freeSlot==E_CACHEFULL)
    {
      return E_CACHEFULL;
    }
  
    /****** Setting up Relation Cache entry for the relation ******/
  
    /* search for the entry with relation name, relName, in the Relation Catalog using
        BlockAccess::linearSearch().
        Care should be taken to reset the searchIndex of the relation RELCAT_RELID
        before calling linearSearch().*/
    Attribute relNameAttribute;
    memcpy(relNameAttribute.sVal, relName, ATTR_SIZE);
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    // relcatRecId stores the rec-id of the relation `relName` in the Relation Catalog.
    RecId relcatRecId=BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttribute, EQ);
  
    if (relcatRecId.block == -1 && relcatRecId.slot == -1) 
    {
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
   recBuffer.getRecord(record, relcatRecId.slot);

   RelCatEntry relCatEntry;

   RelCacheTable::recordToRelCatEntry(record, &relCatEntry);

   RelCacheTable::relCache[freeSlot] = (RelCacheEntry*) malloc(sizeof(RelCacheEntry));

   RelCacheTable::relCache[freeSlot]->recId = relcatRecId;
   RelCacheTable::relCache[freeSlot]->relCatEntry = relCatEntry;

   /****** Setting up Attribute Cache entry for the relation ******/
  
    // let listHead be used to hold the head of the linked list of attrCache entries.
    int numAttrs = relCatEntry.numAttrs;
    AttrCacheEntry* listHead = createList(numAttrs);
    AttrCacheEntry* node = listHead;
  
    /*iterate over all the entries in the Attribute Catalog corresponding to each
    attribute of the relation relName by multiple calls of BlockAccess::linearSearch()
    care should be taken to reset the searchIndex of the relation, ATTRCAT_RELID,
    corresponding to Attribute Catalog before the first call to linearSearch().*/
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    while(true)
    {
        /* let attrcatRecId store a valid record id an entry of the relation, relName,
        in the Attribute Catalog.*/
        RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttribute, EQ);
        /* read the record entry corresponding to attrcatRecId and create an
        Attribute Cache entry on it using RecBuffer::getRecord() and
        AttrCacheTable::recordToAttrCatEntry().
        update the recId field of this Attribute Cache entry to attrcatRecId.
        add the Attribute Cache entry to the linked list of listHead .*/
        // NOTE: make sure to allocate memory for the AttrCacheEntry using malloc()
        if (attrcatRecId.block != -1 && attrcatRecId.slot != -1) {
            Attribute attrcatRecord[ATTRCAT_NO_ATTRS];

            RecBuffer attrRecBuffer(attrcatRecId.block);

            attrRecBuffer.getRecord(attrcatRecord, attrcatRecId.slot);

            AttrCatEntry attrCatEntry;
            AttrCacheTable::recordToAttrCatEntry(attrcatRecord, &attrCatEntry);

            node->recId = attrcatRecId;
            node->attrCatEntry = attrCatEntry;
            node = node->next;
        }
        else
        {
            break;
        }

    }
  
    // set the relIdth entry of the AttrCacheTable to listHead.
    /****** Setting up metadata in the Open Relation Table for the relation******/
    // update the relIdth entry of the tableMetaInfo with free as false and
    // relName as the input.
    AttrCacheTable::attrCache[freeSlot] = listHead;
    OpenRelTable::tableMetaInfo[freeSlot].free = false;
    memcpy(OpenRelTable::tableMetaInfo[freeSlot].relName, relCatEntry.relName, ATTR_SIZE);

    return freeSlot;
}


int OpenRelTable::closeRel(int relId)
{
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) 
    {
      return E_NOTPERMITTED;
    }
  
    if (relId < 0 || relId >= MAX_OPEN)
    {
      return E_OUTOFBOUND;
    }
  
    if (OpenRelTable::tableMetaInfo[relId].free)
    {
      return E_RELNOTOPEN;
    }
  
    // free the memory allocated in the relation and attribute caches which was
    // allocated in the OpenRelTable::openRel() function
    // update `tableMetaInfo` to set `relId` as a free slot
    // update `relCache` and `attrCache` to set the entry at `relId` to nullptr
    OpenRelTable::tableMetaInfo[relId].free = true;
    free(RelCacheTable::relCache[relId]);
    clearList(AttrCacheTable::attrCache[relId]);
    RelCacheTable::relCache[relId] = nullptr;
    AttrCacheTable::attrCache[relId] = nullptr;
  
    return SUCCESS;
  }
  

OpenRelTable::~OpenRelTable() 
{
  // close all open relations (from rel-id = 2 onwards. Why?)
  for (int i = 2; i < MAX_OPEN; ++i) 
  {
    if (!tableMetaInfo[i].free) 
    {
      OpenRelTable::closeRel(i); // we will implement this function later
    }
  }
  // free the memory allocated for rel-id 0 and 1 in the caches
  for (int i = 0; i < MAX_OPEN; i++)
  {
    free(RelCacheTable::relCache[i]);
    clearList(AttrCacheTable::attrCache[i]);

    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
  }
}