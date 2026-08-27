
#include "tbl.h"
#include "../pflayer/pf.h"
#include "codec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOT_COUNT_OFFSET 2
#define checkerr(err)                                                          \
  {                                                                            \
    if (err < 0) {                                                             \
      PF_PrintError();                                                         \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  }

int getLen(int slot, byte *pageBuf);
int getNumSlots(byte *pageBuf);
void setNumSlots(byte *pageBuf, int nslots);
int getNthSlotOffset(int slot, char *pageBuf);
int getFreeSpaceOffset(byte *pageBuf); // [Yash] Declared two helper functions
                                       // for getting free spcae offset.
void setFreeSpaceOffset(int offset, byte *pageBuf);
int getLenRecordPossible(byte *pagebuf);

int getNumSlots(byte *pageBuf) { //[Yash] Wrote helper functions because
                                 // couldn't find a definition.
  byte *pointer = pageBuf + SLOT_COUNT_OFFSET;
  short answer = DecodeShort(pointer);
  return answer;
}

void setNumSlots(byte *pageBuf,
                 int nslots) { //[Yash] Wrote helper functions because couldn't
                               // find a definition.
  byte *pointer = pageBuf + SLOT_COUNT_OFFSET;
  EncodeShort((short)nslots, pointer);
}

int getLen(int slot, byte *pageBuf) {
  byte *pointer = pageBuf + SLOT_COUNT_OFFSET + 2 + slot * 4 + 2;
  return DecodeShort(pointer);
}
int getNthSlotOffset(int slot,
                     byte *pageBuf) { //[Yash] Wrote helper functions because
                                      // couldn't find a definition.
  byte *pointer = pageBuf + SLOT_COUNT_OFFSET + 2 + slot * 4;
  return DecodeShort(pointer);
}

int getFreeSpaceOffset(byte *pageBuf) {
  return DecodeShort(pageBuf);
} // [Yash] Declared two helper functions for getting free spcae offset.

void setFreeSpaceOffset(int offset, byte *pageBuf) {
  EncodeShort((short)offset, pageBuf);
} // [Yash] Declared two helper functions for getting free spcae offset.

int getLenRecordPossible(
    byte *pagebuf) { // [Yash] Helper function for table_insert
  int freeSpaceOffset = getFreeSpaceOffset(pagebuf);
  int numberOfSlots = getNumSlots(pagebuf);
  int spaceAvailable = freeSpaceOffset - (4 + (numberOfSlots + 1) * 4);
  return spaceAvailable;
}

/**
   Opens a paged file, creating one if it doesn't exist, and optionally
   overwriting it.
   Returns 0 on success and a negative error code otherwise.
   If successful, it returns an initialized Table*.
 */
int Table_Open(char *dbname, Schema *schema, bool overwrite, Table **ptable) {
  // Initialize PF, create PF file,
  // allocate Table structure  and initialize and return via ptable
  // The Table structure only stores the schema. The current functionality
  // does not really need the schema, because we are only concentrating
  // on record storage.
  int err = PF_CreateFile(dbname); //[Yash] Table open is creating a file if it
                                   // doesn't exist already
  if (err != PFE_OK &&
      overwrite == true) { //[Yash] opening the file and passing it's fd and
                           // schema to table.
    int error = PF_DestroyFile(dbname);
    checkerr(error);
    error = PF_CreateFile(dbname);
    checkerr(error);
  }
  int fd = PF_OpenFile(dbname);
  checkerr(fd);
  Table *tbl = malloc(sizeof(Table));
  tbl->schema = schema;
  tbl->pf = fd;
  tbl->lastPageWritten = -1;
  *ptable = tbl;
  return 0;
}

void Table_Close(Table *tbl) {
  // Unfix any dirty pages, close file.
  checkerr(PF_CloseFile(tbl->pf));
  free(tbl);
}

int Table_Insert(Table *tbl, byte *record, int len, RecId *rid) {
  // Allocate a fresh page if len is not enough for remaining space
  // Get the next free slot on page, and copy record in the free
  // space
  // Update slot and free space index information on top of page.
  byte *pagebuf;
  int pagenum;
  if (tbl->lastPageWritten == -1) {
    int err = PF_AllocPage(tbl->pf, &pagenum, &pagebuf);
    checkerr(err);
  } else {
    pagenum = tbl->lastPageWritten;
    int err = PF_GetThisPage(tbl->pf, pagenum, &pagebuf);
    checkerr(err);
    int length = getLenRecordPossible(pagebuf);
    if (len < length) {
      // [Yash] Logic for pasting the record into the buffer and adding a slot
      // entry and updating the offset variable.
    } else {
      //[Yash] Logic for creating a new page and then adding in it.
    }
  }
}

#define checkerr(err)                                                          \
  {                                                                            \
    if (err < 0) {                                                             \
      PF_PrintError();                                                         \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  }

/*
  Given an rid, fill in the record (but at most maxlen bytes).
  Returns the number of bytes copied.
 */
int Table_Get(Table *tbl, RecId rid, byte *record, int maxlen) {
  int slot = rid & 0xFFFF;
  int pageNum = rid >> 16;

  UNIMPLEMENTED;
  // PF_GetThisPage(pageNum)
  // In the page get the slot offset of the record, and
  // memcpy bytes into the record supplied.
  // Unfix the page
  return len; // return size of record
}

void Table_Scan(Table *tbl, void *callbackObj, ReadFunc callbackfn) {

  UNIMPLEMENTED;

  // For each page obtained using PF_GetFirstPage and PF_GetNextPage
  //    for each record in that page,
  //          callbackfn(callbackObj, rid, record, recordLen)
}
