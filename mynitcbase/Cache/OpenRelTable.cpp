#include "OpenRelTable.h"
#include <cstring>
#include <stdlib.h>

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

    // initialize relCache and attrCache with nullptr
    for (int i = 0; i < MAX_OPEN; ++i) 
    {
      RelCacheTable::relCache[i] = nullptr;
      AttrCacheTable::attrCache[i] = nullptr;
    }
  
    /************ Setting up Relation Cache entries ************/
    // (we need to populate relation cache with entries for the relation catalog
    //  and attribute catalog.)
  
    /**** setting up Relation Catalog relation in the Relation Cache Table****/
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    
    for (int i = 0; i <= 2; i++) // 0->relCat, 1 -> attrCat
    { 
        relCatBlock.getRecord(relCatRecord, i);

        struct RelCacheEntry relCacheEntry;
        RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
        relCacheEntry.recId.block = RELCAT_BLOCK;
        relCacheEntry.recId.slot = i;

        RelCacheTable::relCache[i] = (struct RelCacheEntry*) malloc(sizeof(RelCacheEntry));
        *(RelCacheTable::relCache[i]) = relCacheEntry;
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
    for (int i = 6; i < 6 + ATTRCAT_NO_ATTRS; i++)
    {
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
        (attrCacheEntry->recId).block = ATTRCAT_BLOCK;
        (attrCacheEntry->recId).slot = i;

        attrCacheEntry = attrCacheEntry->next;
    }
    AttrCacheTable::attrCache[ATTRCAT_RELID] = attrCatListHead;

    // exercise
    auto studentsListHead = createList(4);
    attrCacheEntry = studentsListHead;
    for (int i = 12; i < 16; i++) {
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
        (attrCacheEntry->recId).block = ATTRCAT_BLOCK;
        (attrCacheEntry->recId).slot = i;

        attrCacheEntry = attrCacheEntry->next;
    }
    AttrCacheTable::attrCache[2] = studentsListHead;
}

/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/
int OpenRelTable::getRelId(char relName[ATTR_SIZE])
{

    // if relname is RELCAT_RELNAME, return RELCAT_RELID
    // if relname is ATTRCAT_RELNAME, return ATTRCAT_RELID
    if (strcmp(relName, RELCAT_RELNAME) == 0)
    {
       return RELCAT_RELID;
    }
    if (strcmp(relName, ATTRCAT_RELNAME) == 0)
    {
        return ATTRCAT_RELID;
    }
    if (strcmp(relName, "Students") == 0)
    {
        return 2;
    }
    return E_RELNOTOPEN;
}

OpenRelTable::~OpenRelTable() 
{
    // free all the memory that you allocated in the constructor
    for (int i = 0; i < MAX_OPEN; i++)
    {
        free(RelCacheTable::relCache[i]);
    }
    for (int i = 0; i < MAX_OPEN; i++)
    {
        clearList(AttrCacheTable::attrCache[i]);
    }
}