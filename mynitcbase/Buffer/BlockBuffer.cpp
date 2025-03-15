#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

// the declarations for these functions can be found in "BlockBuffer.h"

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType)
{
  int diff;
  (attrType == NUMBER) ? diff = attr1.nVal - attr2.nVal : diff = strcmp(attr1.sVal, attr2.sVal);
  if (diff > 0)
  {
      return 1; // attr1 > attr2
  }
  else if (diff < 0)
  {
      return -1; //attr 1 < attr2
  }
  else
  { 
      return 0;
  }
}

BlockBuffer::BlockBuffer(int blockNum) 
{
    // initialise this.blockNum with the argument
    this->blockNum=blockNum;
}
  
// calls the parent class constructor
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}

// the declarations for these functions can be found in "BlockBuffer.h"


/*
Used to get the header of the block into the location pointed to by `head`
NOTE: this function expects the caller to allocate memory for `head`
*/
int BlockBuffer::getHeader(struct HeadInfo *head)
{
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;   // return any errors that might have occured in the process
  }
  
  memcpy(&head->numSlots, bufferPtr + 24, 4);
  memcpy(&head->numEntries, bufferPtr + 16, 4);
  memcpy(&head->numAttrs, bufferPtr + 20, 4);
  memcpy(&head->lblock, bufferPtr + 8, 4);
  memcpy(&head->rblock, bufferPtr + 12, 4);

  return SUCCESS;
}

/*
Used to get the record at slot `slotNum` into the array `rec`
NOTE: this function expects the caller to allocate memory for `rec`
*/
int RecBuffer::getRecord(union Attribute *rec, int slotNum) 
{
  struct HeadInfo head;

  this->getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) 
  {
    return ret;
  }
  
  // header size -> 32
  // slotMapSize -> numSlots
  // index -> recordSize*slotNum
  int recordSize = attrCount * ATTR_SIZE;
  unsigned char* slotPointer = bufferPtr + 32 + slotCount + recordSize*slotNum;

  memcpy(rec, slotPointer, recordSize);
  return SUCCESS;
}

/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/
int RecBuffer::getSlotMap(unsigned char *slotMap)
{
  unsigned char *bufferPtr;

  // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS)
  {
    return ret;
  }

  struct HeadInfo head;
  // get the header of the block using getHeader() function
  getHeader(&head);

  int slotCount = head.numSlots;

  // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  // copy the values from `slotMapInBuffer` to `slotMap` (size is `slotCount`)
  memcpy(slotMap, slotMapInBuffer, slotCount);
  return SUCCESS;
}

/*
Used to load a block to the buffer and get a pointer to it.
NOTE: this function expects the caller to allocate memory for the argument
*/
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr)
{
  // check whether the block is already present in the buffer using StaticBuffer.getBufferNum()
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  if (bufferNum == E_BLOCKNOTINBUFFER)
  {
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    if (bufferNum == E_OUTOFBOUND)
    {
      return E_OUTOFBOUND;
    }

    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }

  // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
  *buffPtr = StaticBuffer::blocks[bufferNum];
  return SUCCESS;
}


