#include "BlockAccess.h"
#include <cstring>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op)
{
    // get the previous search index of the relation relId from the relation cache
    // (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);
    // let block and slot denote the record id of the record being currently checked
    int block, slot;
    // if the current search index record is invalid(i.e. both block and slot = -1)
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // (no hits from previous search; search should start from the
        // first record itself)
        RelCatEntry relCatBuf;
        // get the first record block of the relation from the relation cache
        // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
        RelCacheTable::getRelCatEntry(relId, &relCatBuf);
        // block = first record block of the relation
        // slot = 0
        block = relCatBuf.firstBlk;
        slot = 0;
    }
    else
    {
        // (there is a hit from previous search; search should start from
        // the record next to the search index record)
        // block = search index's block
        // slot = search index's slot + 1
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
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
        // get the record with id (block, slot) using RecBuffer::getRecord()
        // get header of the block using RecBuffer::getHeader() function
        // get slot map of the block using RecBuffer::getSlotMap() function
        HeadInfo header;
        recBuffer.getHeader(&header);
        
        Attribute record[header.numAttrs];
        recBuffer.getRecord(record, slot);
        
        unsigned char slotMap[header.numSlots];
        recBuffer.getSlotMap(slotMap);

        // If slot >= the number of slots per block(i.e. no more slots in this block)
        if(slot >= header.numSlots)
        {
            // update block = right block of block
            // update slot = 0
            block = header.rblock;
            slot = 0;
            continue;  // continue to the beginning of this while loop
        }

        // if slot is free skip the loop
        // (i.e. check if slot'th entry in slot map of block contains SLOT_UNOCCUPIED)
        if(slotMap[slot] == SLOT_UNOCCUPIED)
        {
            // increment slot and continue to the next record slot
            slot++;
            continue;
        }

        // compare record's attribute value to the the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute
            from the attribute cache entry of the relation using
            AttrCacheTable::getAttrCatEntry()
        */
        /* use the attribute offset to get the value of the attribute from
           current record */
        AttrCatEntry attrCatBuf;
        int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);
        
        // will store the difference between the attributes
        // set cmpVal using compareAttrs()
        int cmpVal = compareAttrs(record[attrCatBuf.offset], attrVal, attrCatBuf.attrType);
        

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
            RecId searchIndex = {block, slot};
            RelCacheTable::setSearchIndex(relId, &searchIndex);
            return searchIndex;
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId({-1, -1});
}

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE])
{
    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute newRelationName;    // set newRelationName with newName
    strcpy(newRelationName.sVal, newName);
    // search the relation catalog for an entry with "RelName" = newRelationName

    // If relation with name newName already exists (result of linearSearch
    //                                               is not {-1, -1})
    //    return E_RELEXIST;
    char relName_attr[ATTR_SIZE];
    strcpy(relName_attr, RELCAT_ATTR_RELNAME);
    RecId searchRes = BlockAccess::linearSearch(RELCAT_RELID, relName_attr, newRelationName, EQ);
    if (searchRes.block != -1 && searchRes.slot != -1) 
    {
        return E_RELEXIST; // relation with relname newName exists already
    }

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute oldRelationName;    // set oldRelationName with oldName
    strcpy(oldRelationName.sVal, oldName);
    // search the relation catalog for an entry with "RelName" = oldRelationName

    // If relation with name oldName does not exist (result of linearSearch is {-1, -1})
    //    return E_RELNOTEXIST;
    searchRes = BlockAccess::linearSearch(RELCAT_RELID, relName_attr, oldRelationName, EQ);
    if (searchRes.block == -1 && searchRes.slot == -1) 
    {
        return E_RELNOTEXIST; // relation with relname oldName does not exist
    }
    /* get the relation catalog record of the relation to rename using a RecBuffer
       on the relation catalog [RELCAT_BLOCK] and RecBuffer.getRecord function
    */
    /* update the relation name attribute in the record with newName.
       (use RELCAT_REL_NAME_INDEX) */
    // set back the record value using RecBuffer.setRecord

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
    // update relName in relation catalog
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute record[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(record, searchRes.slot);
    strcpy(record[RELCAT_REL_NAME_INDEX].sVal, newName);
    relCatBlock.setRecord(record, searchRes.slot);

    // update relName in respective entries of attribute catalog from oldName to newName
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    strcpy(relName_attr, ATTRCAT_ATTR_RELNAME);
    int numAttrs = record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    for (int i = 0; i < numAttrs; i++) 
    {
        searchRes = BlockAccess::linearSearch(ATTRCAT_RELID, relName_attr, oldRelationName, EQ);
        RecBuffer attrCatBlock(searchRes.block);
        attrCatBlock.getRecord(record, searchRes.slot);
        strcpy(record[ATTRCAT_REL_NAME_INDEX].sVal, newName);
        attrCatBlock.setRecord(record, searchRes.slot);
    }
    return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE])
{

    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;    // set relNameAttr to relName
    memcpy(relNameAttr.sVal, relName, ATTR_SIZE);

    // Search for the relation with name relName in relation catalog using linearSearch()
    // If relation with name relName does not exist (search returns {-1,-1})
    //    return E_RELNOTEXIST;
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
    if (recId.block == -1 && recId.slot == -1)
    {
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
    while (true)
    {
        // linear search on the attribute catalog for RelName = relNameAttr
        RecId attrRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

        // if there are no more attributes left to check (linearSearch returned {-1,-1})
        //     break;
        if (attrRecId.block == -1 && attrRecId.slot == -1)
        {
            break;
        }
        /* Get the record from the attribute catalog using RecBuffer.getRecord
          into attrCatEntryRecord */
        RecBuffer recBuffer(attrRecId.block);
        Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];
        recBuffer.getRecord(attrCatEntryRecord, attrRecId.slot);
        
        char attrName[ATTR_SIZE];
        memcpy(attrName,attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, ATTR_SIZE);
        // if attrCatEntryRecord.attrName = oldName
        //     attrToRenameRecId = block and slot of this record
        if (strcmp(attrName, oldName) == 0)
        {
            attrToRenameRecId = attrRecId;
        }
        // if attrCatEntryRecord.attrName = newName
        //     return E_ATTREXIST;
        if (strcmp(attrName, newName) == 0)
        {
            return E_ATTREXIST;
        }
    }

    // if attrToRenameRecId == {-1, -1}
    //     return E_ATTRNOTEXIST;
    if (attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1)
    {
       return E_ATTRNOTEXIST;
    }
    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    /*   declare a RecBuffer for attrToRenameRecId.block and get the record at
         attrToRenameRecId.slot */
    //   update the AttrName of the record with newName
    //   set back the record with RecBuffer.setRecord
    RecBuffer bufferToRename(attrToRenameRecId.block);
    Attribute recordToRename[ATTRCAT_NO_ATTRS];

    bufferToRename.getRecord(recordToRename, attrToRenameRecId.slot);

    memcpy(recordToRename[ATTRCAT_ATTR_NAME_INDEX].sVal, newName, ATTR_SIZE);

    bufferToRename.setRecord(recordToRename, attrToRenameRecId.slot);

    return SUCCESS;
}

int BlockAccess::insert(int relId, Attribute *record)
{
    // get the relation catalog entry from relation cache
    // ( use RelCacheTable::getRelCatEntry() of Cache Layer)
    RelCatEntry relCatBuf;
    int ret = RelCacheTable::getRelCatEntry(relId, &relCatBuf);
    if (ret != SUCCESS)
    {
       return ret;
    }
    int blockNum = relCatBuf.lastBlk;

    // rec_id will be used to store where the new record will be inserted
    RecId rec_id = {-1, -1};

    int numOfSlots = relCatBuf.numSlotsPerBlk;
    int numOfAttributes = relCatBuf.numAttrs;

    int prevBlockNum = -1;

    /*
        Traversing the linked list of existing record blocks of the relation
        until a free slot is found OR
        until the end of the list is reached
    */
    while (blockNum != -1) 
    {
        // create a RecBuffer object for blockNum (using appropriate constructor!)
        RecBuffer currentBlock(blockNum);
        // get header of block(blockNum) using RecBuffer::getHeader() function
        HeadInfo currentHeader;
        currentBlock.getHeader(&currentHeader);
        // get slot map of block(blockNum) using RecBuffer::getSlotMap() function
        unsigned char slotMap[numOfSlots];
        currentBlock.getSlotMap(slotMap);
        // search for free slot in the block 'blockNum' and store it's rec-id in rec_id
        // (Free slot can be found by iterating over the slot map of the block)
        /* slot map stores SLOT_UNOCCUPIED if slot is free and
           SLOT_OCCUPIED if slot is occupied) */
        /* if a free slot is found, set rec_id and discontinue the traversal
           of the linked list of record blocks (break from the loop) */
        int freeSlot = -1;
        for (int i = 0; i < numOfSlots; i++)
        {
            if (slotMap[i] == SLOT_UNOCCUPIED)
            {
                freeSlot = i;
                break; 
            }
        }
        /* otherwise, continue to check the next block by updating the
           block numbers as follows:
              update prevBlockNum = blockNum
              update blockNum = header.rblock (next element in the linked
                                               list of record blocks)
        */
        if(freeSlot != -1) 
        {
            rec_id.block = blockNum;
            rec_id.slot = freeSlot;
            break;
        }
        prevBlockNum = blockNum;
        blockNum = currentHeader.rblock;
    }

    //  if no free slot is found in existing record blocks (rec_id = {-1, -1})
    if (rec_id.block == -1 || rec_id.slot == -1)
    {
        // if relation is RELCAT, do not allocate any more blocks
        //     return E_MAXRELATIONS;
        if (relId == RELCAT_RELID)
        {
            return E_MAXRELATIONS;
        }
        // Otherwise,
        // get a new record block (using the appropriate RecBuffer constructor!)
        // get the block number of the newly allocated block
        // (use BlockBuffer::getBlockNum() function)
        // let ret be the return value of getBlockNum() function call
        RecBuffer newBlock;

        int newBlockNum = newBlock.getBlockNum();
        if (newBlockNum == E_DISKFULL) 
        {
            return E_DISKFULL;
        }

        // Assign rec_id.block = new block number(i.e. ret) and rec_id.slot = 0
        rec_id.block = newBlockNum;
        rec_id.slot = 0;
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
        HeadInfo newBlockHeader;
        newBlock.getHeader(&newBlockHeader);
        newBlockHeader.lblock = prevBlockNum;
        newBlockHeader.numAttrs = numOfAttributes;
        newBlockHeader.numSlots = numOfSlots;
        newBlock.setHeader(&newBlockHeader);

        /*
            set block's slot map with all slots marked as free
            (i.e. store SLOT_UNOCCUPIED for all the entries)
            (use RecBuffer::setSlotMap() function)
        */
        unsigned char newBlockSlotMap[numOfSlots];
        newBlock.getSlotMap(newBlockSlotMap);
        for (int i = 0; i < numOfSlots; i++)
        {
           newBlockSlotMap[i] = SLOT_UNOCCUPIED;
        }
        newBlock.setSlotMap(newBlockSlotMap);
        // if prevBlockNum != -1
        if(prevBlockNum != -1)
        {
            // create a RecBuffer object for prevBlockNum
            // get the header of the block prevBlockNum and
            // update the rblock field of the header to the new block
            // number i.e. rec_id.block
            // (use BlockBuffer::setHeader() function)
            RecBuffer prevBlock(prevBlockNum);
            HeadInfo prevBlockHeader;
            prevBlock.getHeader(&prevBlockHeader);
            prevBlockHeader.rblock = rec_id.block;
            prevBlock.setHeader(&prevBlockHeader);
        }
        else
        {
            // update first block field in the relation catalog entry to the
            // new block (using RelCacheTable::setRelCatEntry() function)
            relCatBuf.firstBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatBuf);
        }
        relCatBuf.lastBlk = rec_id.block;
        RelCacheTable::setRelCatEntry(relId, &relCatBuf);
    }

    // create a RecBuffer object for rec_id.block
    // insert the record into rec_id'th slot using RecBuffer.setRecord())
    RecBuffer blockToInsert(rec_id.block);
    blockToInsert.setRecord(record, rec_id.slot);
    /* update the slot map of the block by marking entry of the slot to
       which record was inserted as occupied) */
    // (ie store SLOT_OCCUPIED in free_slot'th entry of slot map)
    // (use RecBuffer::getSlotMap() and RecBuffer::setSlotMap() functions)
    unsigned char slotMapToInsert[numOfSlots];
    blockToInsert.getSlotMap(slotMapToInsert);
    slotMapToInsert[rec_id.slot] = SLOT_OCCUPIED;
    blockToInsert.setSlotMap(slotMapToInsert);
    // increment the numEntries field in the header of the block to
    // which record was inserted
    // (use BlockBuffer::getHeader() and BlockBuffer::setHeader() functions)
    HeadInfo headerToInsert;
    blockToInsert.getHeader(&headerToInsert);
    headerToInsert.numEntries++;
    blockToInsert.setHeader(&headerToInsert);
    // Increment the number of records field in the relation cache entry for
    // the relation. (use RelCacheTable::setRelCatEntry function)
    relCatBuf.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatBuf);
    int flag = SUCCESS;
    // Iterate over all the attributes of the relation
    for(int attrOffset=0; attrOffset<numOfAttributes; attrOffset++)
    {
        // get the attribute catalog entry for the attribute from the attribute cache
        // (use AttrCacheTable::getAttrCatEntry() with args relId and attrOffset)
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId,attrOffset,&attrCatEntry);
        // get the root block field from the attribute catalog entry
       
        if(attrCatEntry.rootBlock == -1)
        {
            continue;
        }
        /* insert the new record into the attribute's bplus tree usingBPlusTree::bPlusInsert()*/
        int retVal = BPlusTree::bPlusInsert(relId, attrCatEntry.attrName,record[attrOffset], rec_id);
        if (retVal == E_DISKFULL) 
        {
                 //(index for this attribute has been destroyed)
                 flag = E_INDEX_BLOCKS_RELEASED;
        }
    }

    return flag;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) 
{
    // Declare a variable called recid to store the searched record
    RecId recId;
   
    /* get the attribute catalog entry from the attribute cache corresponding
    to the relation with Id=relid and with attribute_name=attrName  */
    AttrCatEntry attrCatBuf;
    int ret = AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);
    // if this call returns an error, return the appropriate error code
    if(ret != SUCCESS)
    {
        return ret;
    }
    // get rootBlock from the attribute catalog entry
    int rootBlock = attrCatBuf.rootBlock;
    if(rootBlock == -1)
    {

        /* search for the record id (recid) corresponding to the attribute with
           attribute name attrName, with value attrval and satisfying the
           condition op using linearSearch()
        */
        recId = BlockAccess::linearSearch(relId,attrName,attrVal,op);
    }
    else
    {
        // (index exists for the attribute)
        recId = BPlusTree::bPlusSearch(relId,attrName,attrVal,op);
        /* search for the record id (recid) correspoding to the attribute with
        attribute name attrName and with value attrval and satisfying the
        condition op using BPlusTree::bPlusSearch() */
    }
    // if there's no record satisfying the given condition (recId = {-1, -1})
    //     return E_NOTFOUND;
    if(recId.block == -1 || recId.slot == -1)
    {
        return E_NOTFOUND;
    }
    /* Copy the record with record id (recId) to the record buffer (record).
       For this, instantiate a RecBuffer class object by passing the recId and
       call the appropriate method to fetch the record
    */
    RecBuffer recBuffer(recId.block);
    ret=recBuffer.getRecord(record,recId.slot);
    return ret;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE])
{
    // if the relation to delete is either Relation Catalog or Attribute Catalog,
    //     return E_NOTPERMITTED
        // (check if the relation names are either "RELATIONCAT" and "ATTRIBUTECAT".
        // you may use the following constants: RELCAT_NAME and ATTRCAT_NAME)
    if (strcmp(relName, (char*)RELCAT_RELNAME) == 0 || strcmp(relName, (char*)ATTRCAT_RELNAME) == 0) 
    {
        return E_NOTPERMITTED;
    }
    /* reset the searchIndex of the relation catalog using
       RelCacheTable::resetSearchIndex() */
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttribute; // (stores relName as type union Attribute)
    // assign relNameAttr.sVal = relName
    strcpy(relNameAttribute.sVal, relName);
    //  linearSearch on the relation catalog for RelName = relNameAttr
    RecId recId = linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttribute, EQ);
    // if the relation does not exist (linearSearch returned {-1, -1})
    //     return E_RELNOTEXIST
    if (recId.block == -1 || recId.slot == -1)
    {
        return E_RELNOTEXIST;
    }

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    /* store the relation catalog record corresponding to the relation in
       relCatEntryRecord using RecBuffer.getRecord */
    RecBuffer recBuffer(recId.block);
    recBuffer.getRecord(relCatEntryRecord, recId.slot);
    /* get the first record block of the relation (firstBlock) using the
       relation catalog entry record */
    /* get the number of attributes corresponding to the relation (numAttrs)
       using the relation catalog entry record */
    int currentBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    /*
     Delete all the record blocks of the relation
    */
    // for each record block of the relation:
    //     get block header using BlockBuffer.getHeader
    //     get the next block from the header (rblock)
    //     release the block using BlockBuffer.releaseBlock
    //
    //     Hint: to know if we reached the end, check if nextBlock = -1
    while(currentBlock != -1)
    {
        RecBuffer currentBlockBuffer(currentBlock);
        HeadInfo currentBlockHeader;
        currentBlockBuffer.getHeader(&currentBlockHeader);

        int nextBlock = currentBlockHeader.rblock;

        currentBlockBuffer.releaseBlock();
        currentBlock = nextBlock;
    }
    /***
        Deleting attribute catalog entries corresponding the relation and index
        blocks corresponding to the relation with relName on its attributes
    ***/

    // reset the searchIndex of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numberOfAttributesDeleted = 0;

    while(true)
    {
        RecId attrCatRecId;
        // attrCatRecId = linearSearch on attribute catalog for RelName = relNameAttr
        attrCatRecId = linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttribute, EQ);
        // if no more attributes to iterate over (attrCatRecId == {-1, -1})
        //     break;
        if (attrCatRecId.slot == -1 || attrCatRecId.block == -1)
        {
            break;
        }
        numberOfAttributesDeleted++;

        // create a RecBuffer for attrCatRecId.block
        // get the header of the block
        // get the record corresponding to attrCatRecId.slot
        RecBuffer currentBlock(attrCatRecId.block);
        HeadInfo currentBlockHeader;
        currentBlock.getHeader(&currentBlockHeader);
        Attribute record[ATTRCAT_NO_ATTRS];
        currentBlock.getRecord(record, attrCatRecId.slot);
        // declare variable rootBlock which will be used to store the root
        // block field from the attribute catalog record.
        int rootBlock = record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
        // (This will be used later to delete any indexes if it exists)

        // Update the Slotmap for the block by setting the slot as SLOT_UNOCCUPIED
        // Hint: use RecBuffer.getSlotMap and RecBuffer.setSlotMap
        unsigned char slotMap[currentBlockHeader.numSlots];
        currentBlock.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        currentBlock.setSlotMap(slotMap);
        /* Decrement the numEntries in the header of the block corresponding to
           the attribute catalog entry and then set back the header
           using RecBuffer.setHeader */
        currentBlockHeader.numEntries--;
        currentBlock.setHeader(&currentBlockHeader);
        /* If number of entries become 0, releaseBlock is called after fixing
           the linked list.
        */
        if (currentBlockHeader.numEntries == 0)
        {
            /* Standard Linked List Delete for a Block
               Get the header of the left block and set it's rblock to this
               block's rblock
            */
            int leftBlock = currentBlockHeader.lblock;
            int rightBlock = currentBlockHeader.rblock;
            // create a RecBuffer for lblock and call appropriate methods
            
            if (leftBlock != -1) 
            {
                RecBuffer prevBlock(leftBlock);
                HeadInfo prevBlockHeader;

                prevBlock.getHeader(&prevBlockHeader);
                prevBlockHeader.rblock = rightBlock;
                prevBlock.setHeader(&prevBlockHeader);
            }

            if (rightBlock != -1) 
            {
                RecBuffer nextBlock(rightBlock);
                HeadInfo nextBlockHeader;

                nextBlock.getHeader(&nextBlockHeader);
                nextBlockHeader.lblock = leftBlock;
                nextBlock.setHeader(&nextBlockHeader);
            }
            // (Since the attribute catalog will never be empty(why?), we do not
            //  need to handle the case of the linked list becoming empty - i.e
            //  every block of the attribute catalog gets released.)
            // call releaseBlock()
            currentBlock.releaseBlock();
        }
        // (the following part is only relevant once indexing has been implemented)
        // if index exists for the attribute (rootBlock != -1), call bplus destroy
        if (rootBlock != -1) 
        {
            BPlusTree::bPlusDestroy(rootBlock);
        }
    }

    /*** Delete the entry corresponding to the relation from relation catalog ***/
    // Fetch the header of Relcat block
    HeadInfo relCatHeader;
    recBuffer.getHeader(&relCatHeader);
    /* Get the slotmap in relation catalog, update it by marking the slot as
       free(SLOT_UNOCCUPIED) and set it back. */
    unsigned char recSlotMap[relCatHeader.numSlots];
    recBuffer.getSlotMap(recSlotMap);
    recSlotMap[recId.slot] = SLOT_UNOCCUPIED;
    recBuffer.setSlotMap(recSlotMap);
    /* Decrement the numEntries in the header of the block corresponding to the
       relation catalog entry and set it back */
    relCatHeader.numEntries--;
    recBuffer.setHeader(&relCatHeader);
   
    /*** Updating the Relation Cache Table ***/
    /** Update relation catalog record entry (number of records in relation
        catalog is decreased by 1) **/
    // Get the entry corresponding to relation catalog from the relation
    // cache and update the number of records and set it back
    // (using RelCacheTable::setRelCatEntry() function)
    RelCatEntry relCatBuf;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatBuf);
    relCatBuf.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatBuf);
    /** Update attribute catalog entry (number of records in attribute catalog
        is decreased by numberOfAttributesDeleted) **/
    // i.e., #Records = #Records - numberOfAttributesDeleted
    // Get the entry corresponding to attribute catalog from the relation
    // cache and update the number of records and set it back
    // (using RelCacheTable::setRelCatEntry() function)
    RelCatEntry attrCatBuf;
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &attrCatBuf);
    attrCatBuf.numRecs -= numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &attrCatBuf);

    return SUCCESS;
}

/*
NOTE: the caller is expected to allocate space for the argument `record` based
      on the size of the relation. This function will only copy the result of
      the projection onto the array pointed to by the argument.
*/
int BlockAccess::project(int relId, Attribute *record)
{
    // get the previous search index of the relation relId from the relation
    // cache (use RelCacheTable::getSearchIndex() function)
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);
    // declare block and slot which will be used to store the record id of the
    // slot we need to check.
    int block, slot;

    /* if the current search index record is invalid(i.e. = {-1, -1})
       (this only happens when the caller reset the search index)
    */
    if(prevRecId.block == -1 && prevRecId.slot == -1)
    {
        // (new project operation. start from beginning)
        RelCatEntry relCatEntry;
        // get the first record block of the relation from the relation cache
        // (use RelCacheTable::getRelCatEntry() function of Cache Layer)
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);
        // block = first record block of the relation
        // slot = 0
        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else
    {
        // (a project/search operation is already in progress)
        // block = previous search index's block
        // slot = previous search index's slot + 1
        block = prevRecId.block;
        slot = prevRecId.slot+1;
    }


    // The following code finds the next record of the relation
    /* Start from the record id (block, slot) and iterate over the remaining
       records of the relation */
    while (block != -1)
    {
        // create a RecBuffer object for block (using appropriate constructor!)
        RecBuffer currentBlock(block);
        // get header of the block using RecBuffer::getHeader() function
        // get slot map of the block using RecBuffer::getSlotMap() function
        HeadInfo currentHeader;
        currentBlock.getHeader(&currentHeader);
        unsigned char currentSlotMap[currentHeader.numSlots];
        currentBlock.getSlotMap(currentSlotMap);
        if(slot >= currentHeader.numSlots)
        {
            // (no more slots in this block)
            // update block = right block of block
            // update slot = 0
            // (NOTE: if this is the last block, rblock would be -1. this would
            //        set block = -1 and fail the loop condition )
            block = currentHeader.rblock;
            slot = 0;
        }
        // (i.e slot-th entry in slotMap contains SLOT_UNOCCUPIED)
        else if (currentSlotMap[slot] == SLOT_UNOCCUPIED)
        { 

            // increment slot
            slot++;
        }
        else 
        {
            // (the next occupied slot / record has been found)
            break;
        }
    }

    if (block == -1)
    {
        // (a record was not found. all records exhausted)
        return E_NOTFOUND;
    }

    // declare nextRecId to store the RecId of the record found
    RecId nextRecId{block, slot};

    // set the search index to nextRecId using RelCacheTable::setSearchIndex
    RelCacheTable::setSearchIndex(relId, &nextRecId);
    /* Copy the record with record id (nextRecId) to the record buffer (record)
       For this Instantiate a RecBuffer class object by passing the recId and
       call the appropriate method to fetch the record
    */
    RecBuffer targetBlock(block);
    targetBlock.getRecord(record, slot);

    return SUCCESS;
}

