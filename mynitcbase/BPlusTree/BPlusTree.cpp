#include "BPlusTree.h"
#include <stdio.h>
#include <cstring>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op)
{
    // declare searchIndex which will be used to store search index for attrName.
    IndexId searchIndex;

    /* get the search index corresponding to attribute with name attrName
       using AttrCacheTable::getSearchIndex(). */
    AttrCacheTable::getSearchIndex(relId,attrName,&searchIndex);
    /* load the attribute cache entry into attrCatEntry using
     AttrCacheTable::getAttrCatEntry(). */
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatEntry);
    // declare variables block and index which will be used during search
    int block, index;

    if (searchIndex.block==-1 || searchIndex.index==-1)
    {
        // (search is done for the first time)
        
        // start the search from the first entry of root.
        block = attrCatEntry.rootBlock;
        index = 0;
        
        if (block==-1)
        {
            return RecId{-1, -1};
        }

    }
    else
    {
        /*a valid searchIndex points to an entry in the leaf index of the attribute's
        B+ Tree which had previously satisfied the op for the given attrVal.*/

        block = searchIndex.block;
        index = searchIndex.index + 1;  // search is resumed from the next index.

        // load block into leaf using IndLeaf::IndLeaf().
        IndLeaf leaf(block);
        
        // declare leafHead which will be used to hold the header of leaf.
        HeadInfo leafHead;
        leaf.getHeader(&leafHead);
        // load header into leafHead using BlockBuffer::getHeader().
        if (index >= leafHead.numEntries)
        {
            /* (all the entries in the block has been searched; search from the
            beginning of the next leaf index block. */
            
            // update block to rblock of current block and index to 0.
            block=leafHead.rblock;
            index=0;
            if (block == -1)
            {
                // (end of linked list reached - the search is done.)
                return RecId{-1, -1};
            }
        }
    }

    /******  Traverse through all the internal nodes according to value
             of attrVal and the operator op                             ******/

    /* (This section is only needed when
        - search restarts from the root block (when searchIndex is reset by caller)
        - root is not a leaf
        If there was a valid search index, then we are already at a leaf block
        and the test condition in the following loop will fail)
    */

    while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL)
    { 
        // load the block into internalBlk using IndInternal::IndInternal().
        IndInternal internalBlk(block);

        HeadInfo intHead;

        // load the header of internalBlk into intHead using BlockBuffer::getHeader()
        internalBlk.getHeader(&intHead);
        // declare intEntry which will be used to store an entry of internalBlk.
        InternalEntry intEntry;

        if (op==NE || op==LT || op==LE)
        {
            /*
            - NE: need to search the entire linked list of leaf indices of the B+ Tree,
            starting from the leftmost leaf index. Thus, always move to the left.
            - LT and LE: the attribute values are arranged in ascending order in the
            leaf indices of the B+ Tree. Values that satisfy these conditions, if
            any exist, will always be found in the left-most leaf index. Thus,
            always move to the left.
            */

            // load entry in the first slot of the block into intEntry
            // using IndInternal::getEntry().
            internalBlk.getEntry(&intEntry,0);
            block = intEntry.lChild;

        }
        else
        {
            /*
            - EQ, GT and GE: move to the left child of the first entry that is
            greater than (or equal to) attrVal
            (we are trying to find the first entry that satisfies the condition.
            since the values are in ascending order we move to the left child which
            might contain more entries that satisfy the condition)
            */
            int targetIndex=-1;
            /*
             traverse through all entries of internalBlk and find an entry that
             satisfies the condition.
             if op == EQ or GE, then intEntry.attrVal >= attrVal
             if op == GT, then intEntry.attrVal > attrVal
             Hint: the helper function compareAttrs() can be used for comparing
            */
            for(int i=0; i<intHead.numEntries;i++)
            {
                internalBlk.getEntry(&intEntry,i);
                int cmpVal=compareAttrs(intEntry.attrVal,attrVal,attrCatEntry.attrType);
                if(cmpVal>=0)
                {
                    targetIndex=i;
                    break;
                }
            }
            if (targetIndex!=-1)
            {
                // move to the left child of that entry
                internalBlk.getEntry(&intEntry,targetIndex);
                block=intHead.lblock; // left child of the entry

            }
            else
            {
                // move to the right child of the last entry of the block
                // i.e numEntries - 1 th entry of the block
                internalBlk.getEntry(&intEntry,intHead.numEntries-1);
                block=intHead.rblock;  // right child of last entry
            }
        }
    }

    // NOTE: `block` now has the block number of a leaf index block.

    /******  Identify the first leaf index entry from the current position
                that satisfies our condition (moving right)             ******/

    while (block != -1) 
    {
        // load the block into leafBlk using IndLeaf::IndLeaf().
        IndLeaf leafBlk(block);
        HeadInfo leafHead;

        // load the header to leafHead using BlockBuffer::getHeader().
        leafBlk.getHeader(&leafHead);
        // declare leafEntry which will be used to store an entry from leafBlk
        Index leafEntry;
        
        while (index<leafHead.numEntries)
        {

            // load entry corresponding to block and index into leafEntry
            // using IndLeaf::getEntry().
            leafBlk.getEntry(&leafEntry,index);
            int cmpVal=compareAttrs(leafEntry.attrVal,attrVal,attrCatEntry.attrType); /* comparison between leafEntry's attribute valueand input attrVal using compareAttrs()*/
            if(
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)
            )
            {
                // (entry satisfying the condition found)
                // set search index to {block, index}
                searchIndex={block,index};
                AttrCacheTable::setSearchIndex(relId,attrName,&searchIndex);
                return RecId({leafEntry.block, leafEntry.slot});
                
            }
            else if ((op == EQ || op == LE || op == LT) && cmpVal > 0)
            {
                /*future entries will not satisfy EQ, LE, LT since the values
                    are arranged in ascending order in the leaves */
                
                return RecId({-1, -1});
            }
            // search next index.
            ++index;
        }

        /*only for NE operation do we have to check the entire linked list;
        for all the other op it is guaranteed that the block being searched
        will have an entry, if it exists, satisying that op. */
        if (op != NE)
        {
            break;
        }

        // block = next block in the linked list, i.e., the rblock in leafHead.
        // update index to 0.
        block=leafHead.rblock;
        index=0;
    }
    // no entry satisying the op was found; return the recId {-1,-1}
    return RecId({-1,-1});
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE])
{

    // if relId is either RELCAT_RELID or ATTRCAT_RELID:
    //     return E_NOTPERMITTED;
    if(relId == RELCAT_RELID || relId == ATTRCAT_RELID)
    {
        return E_NOTPERMITTED;
    }

    // get the attribute catalog entry of attribute `attrName`
    // using AttrCacheTable::getAttrCatEntry()
    AttrCatEntry attrCatBuf;
    int ret=AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);
    // if getAttrCatEntry fails
    //     return the error code from getAttrCatEntry
    if(ret != SUCCESS)
    {
        return ret;
    }
    if (attrCatBuf.rootBlock != -1)
    {
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
    if (rootBlock == E_DISKFULL)
    {
        return E_DISKFULL;
    }
    attrCatBuf.rootBlock=rootBlock;
    AttrCacheTable::setAttrCatEntry(relId,attrName,&attrCatBuf);

    RelCatEntry relCatEntry;

    // load the relation catalog entry into relCatEntry
    // using RelCacheTable::getRelCatEntry().
    ret = RelCacheTable::getRelCatEntry(relId,&relCatEntry);
    if(ret != SUCCESS)
    {
        return ret;
    }
    int block = relCatEntry.firstBlk;

    /***** Traverse all the blocks in the relation and insert them one
           by one into the B+ Tree *****/
    while (block != -1)
    {

        // declare a RecBuffer object for `block` (using appropriate constructor)
        RecBuffer currentBlock(block);
        unsigned char slotMap[relCatEntry.numSlotsPerBlk];

        // load the slot map into slotMap using RecBuffer::getSlotMap().
        currentBlock.getSlotMap(slotMap);
        for(int i = 0; i < relCatEntry.numSlotsPerBlk; i++)
        {
            if(slotMap[i] == SLOT_UNOCCUPIED)
            {
                continue;
            }
            Attribute record[relCatEntry.numAttrs];
            // load the record corresponding to the slot into `record`
            // using RecBuffer::getRecord().
            currentBlock.getRecord(record,i);
            // declare recId and store the rec-id of this record in it
            // RecId recId{block, slot};
            RecId recId = {block,i};
            // insert the attribute value corresponding to attrName from the record
            // into the B+ tree using bPlusInsert.
            // (note that bPlusInsert will destroy any existing bplus tree if
            // insert fails i.e when disk is full)
            // retVal = bPlusInsert(relId, attrName, attribute value, recId);
            int retVal = BPlusTree::bPlusInsert(relId,attrName,record[attrCatBuf.offset],recId);
            // if (retVal == E_DISKFULL) {
            //     // (unable to get enough blocks to build the B+ Tree.)
            //     return E_DISKFULL;
            // }
            if(retVal == E_DISKFULL)
            {
                return E_DISKFULL;
            }
        }

        // get the header of the block using BlockBuffer::getHeader()
        HeadInfo currentHeader;
        currentBlock.getHeader(&currentHeader);
        // set block = rblock of current block (from the header)
        block = currentHeader.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum)
{
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
    {
        return E_OUTOFBOUND;
    }

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (type == IND_LEAF)
    {
        // declare an instance of IndLeaf for rootBlockNum using appropriate
        // constructor
        IndLeaf rootNode(rootBlockNum);
        // release the block using BlockBuffer::releaseBlock().
        rootNode.releaseBlock();
        return SUCCESS;
    }
    else if (type == IND_INTERNAL)
    {
        // declare an instance of IndInternal for rootBlockNum using appropriate
        // constructor
        HeadInfo rootHeader;
        IndInternal rootNode(rootBlockNum);
        // load the header of the block using BlockBuffer::getHeader().
        rootNode.getHeader(&rootHeader);
        /*iterate through all the entries of the internalBlk and destroy the lChild
        of the first entry and rChild of all entries using BPlusTree::bPlusDestroy().
        (the rchild of an entry is the same as the lchild of the next entry.
         take care not to delete overlapping children more than once ) */
        InternalEntry indEntry;
        rootNode.getEntry(&indEntry,0);
        if(indEntry.lChild != -1)
        {
            int ret = bPlusDestroy(indEntry.lChild);
            if(ret != SUCCESS)
            {
                return ret;
            }
        }
        int numEntries = rootHeader.numEntries;
        for(int i=0; i<numEntries; i++)
        {
            rootNode.getEntry(&indEntry,i);
            if(indEntry.rChild != -1)
            {
                int ret = bPlusDestroy(indEntry.rChild);
                if(ret != SUCCESS)
                {
                    return ret;
                }
            }
        }
        // release the block using BlockBuffer::releaseBlock().

        return SUCCESS;

    }
    else
    {
        // (block is not an index block.)
        return E_INVALIDBLOCK;
    }
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId)
{
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatBuf;
    int ret = AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);
    // if getAttrCatEntry() failed
    //     return the error code
    if(ret != SUCCESS)
    {
        return ret;
    }
    int blockNum = attrCatBuf.rootBlock;

    if (blockNum == -1)
    {
        return E_NOINDEX;
    }

    // find the leaf block to which insertion is to be done using the
    // findLeafToInsert() function
    int leafBlkNum = findLeafToInsert(blockNum,attrVal,attrCatBuf.attrType);

    // insert the attrVal and recId to the leaf block at blockNum using the
    // insertIntoLeaf() function.
    // declare a struct Index with attrVal = attrVal, block = recId.block and
    // slot = recId.slot to pass as argument to the function.
    // insertIntoLeaf(relId, attrName, leafBlkNum, Index entry)
    // NOTE: the insertIntoLeaf() function will propagate the insertion to the
    //       required internal nodes by calling the required helper functions
    //       like insertIntoInternal() or createNewRoot()
    Index entry;
    entry.attrVal = attrVal;
    entry.block = recId.block;
    entry.slot = recId.slot;
    int ret = insertIntoLeaf(relId,attrName,leafBlkNum,entry);
    if (ret == E_DISKFULL)
    {
        // destroy the existing B+ tree by passing the rootBlock to bPlusDestroy().
        BPlusTree::bPlusDestroy(blockNum);
        // update the rootBlock of attribute catalog cache entry to -1 using
        // AttrCacheTable::setAttrCatEntry().
        AttrCacheTable::setAttrCatEntry(relId,attrName,&attrCatBuf);
        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType)
{
  int blockNum = rootBlock;

  while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF)
  {  
     
     // declare an IndInternal object for block using appropriate constructor
     IndInternal intBlock(blockNum);
     // get header of the block using BlockBuffer::getHeader()
     HeadInfo intHeader;
     intBlock.getHeader(&intHeader);
     /* iterate through all the entries, to find the first entry whose
             attribute value >= value to be inserted.
             NOTE: the helper function compareAttrs() declared in BlockBuffer.h
                   can be used to compare two Attribute values. */
     int numEntries = intHeader.numEntries;
     InternalEntry intEntry;
     int targetIndex = -1;
     for(int i=0; i<numEntries; i++)
     {
        intBlock.getEntry(&intEntry,i);
        if(compareAttrs(intEntry.attrVal,attrVal,attrType) > 0)
        {
           targetIndex=i;
           break;
        }
     }
     if (targetIndex == -1)
     {
            // set blockNum = rChild of (nEntries-1)'th entry of the block
            // (i.e. rightmost child of the block)
            intBlock.getEntry(&intEntry,numEntries-1);
            blockNum = intEntry.rChild;

     } 
     else
     {
            // set blockNum = lChild of the entry that was found
            intBlock.getEntry(&intEntry,targetIndex);
            blockNum = intEntry.lChild;
     }
  }
  
  return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry)
{
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatBuf;
    int ret = AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatBuf);
    if(ret != SUCCESS)
    {
        return ret;
    }
    // declare an IndLeaf instance for the block using appropriate constructor
    IndLeaf leafBlock(blockNum);
    HeadInfo blockHeader;
    // store the header of the leaf index block into blockHeader
    // using BlockBuffer::getHeader()
    leafBlock.getHeader(&blockHeader);
    int numEntries = blockHeader.numEntries;
    // the following variable will be used to store a list of index entries with
    // existing indices + the new index to insert
    Index indices[blockHeader.numEntries + 1];
    int targetIndex = -1;
    /*
    Iterate through all the entries in the block and copy them to the array indices.
    Also insert `indexEntry` at appropriate position in the indices array maintaining
    the ascending order.
    - use IndLeaf::getEntry() to get the entry
    - use compareAttrs() declared in BlockBuffer.h to compare two Attribute structs
    */
    Index leafentry;
    for(int i=0; i<numEntries; i++)
    {
        leafBlock.getEntry(&leafentry,i);
        if(compareAttrs(leafentry.attrVal,indexEntry.attrVal,attrCatBuf.attrType) > 0)
        {
            targetIndex = i;
            break;
        }
    }
    for (int i = 0; i < targetIndex; i++)
    {
        leafBlock.getEntry(&indices[i], i);
    }
    indices[targetIndex] = indexEntry;
    for (int i = targetIndex; i < numEntries; i++)
    {
        leafBlock.getEntry(&indices[i+1], i);
    }
    if (numEntries != MAX_KEYS_LEAF)
    {
        // (leaf block has not reached max limit)

        // increment blockHeader.numEntries and update the header of block
        // using BlockBuffer::setHeader().
        blockHeader.numEntries++;
        leafBlock.setHeader(&blockHeader);
        // iterate through all the entries of the array `indices` and populate the
        // entries of block with them using IndLeaf::setEntry().
        for (int i = 0; i < blockHeader.numEntries; i++)
        {
            leafBlock.setEntry(&indices[i], i);
        }
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
    if(newRightBlk == E_DISKFULL)
    {
        return E_DISKFULL;
    }
    if (blockHeader.lblock != -1)
    {  // check pblock in header
        // insert the middle value from `indices` into the parent block using the
        // insertIntoInternal() function. (i.e the last value of the left block)

        // the middle value will be at index 31 (given by constant MIDDLE_INDEX_LEAF)

        // create a struct InternalEntry with attrVal = indices[MIDDLE_INDEX_LEAF].attrVal,
        // lChild = currentBlock, rChild = newRightBlk and pass it as argument to
        // the insertIntoInternalFunction as follows


        // insertIntoInternal(relId, attrName, parent of current block, new internal entry)

    } else {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.
        // To do this, call the createNewRoot() function with the following arguments

        // createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal,
        //               current block, new right block)
    }

    // if either of the above calls returned an error (E_DISKFULL), then return that
    // else return SUCCESS
}