
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
#define min(a, b) ((a) > (b) ? (b) : (a))
int getLen(int slot, byte *pageBuf);
int getNumSlots(byte *pageBuf);
void setNumSlots(byte *pageBuf, int nslots);
int getNthSlotOffset(int slot, char *pageBuf);
int getFreeSpaceOffset(byte *pageBuf); // [Yash] Declared two helper functions
                                       // for getting free spcae offset.
void setFreeSpaceOffset(int offset, byte *pageBuf);
int getLenRecordPossible(byte *pagebuf); //[Yash] Declared a helper function to
                                         //make implementation check easier

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

int Table_Insert(Table *tbl, byte *record, int len,
                 RecId *rid) { //[Yash] Implemented this function
  // Allocate a fresh page if len is not enough for remaining space
  // Get the next free slot on page, and copy record in the free
  // space
  // Update slot and free space index information on top of page.
  byte *pagebuf;
  int pagenum;
  if (tbl->lastPageWritten == -1) {
    int err = PF_AllocPage(tbl->pf, &pagenum, &pagebuf);
    checkerr(err);
    setFreeSpaceOffset(4096, pagebuf);
    setNumSlots(pagebuf, 0);
  } else {
    pagenum = tbl->lastPageWritten;
    int err = PF_GetThisPage(tbl->pf, pagenum, &pagebuf);
    checkerr(err);
    int length = getLenRecordPossible(pagebuf);
    if (len > length) {
      err = PF_UnfixPage(tbl->pf, pagenum, FALSE);
      checkerr(err);
      err = PF_AllocPage(tbl->pf, &pagenum, &pagebuf);
      checkerr(err);
      setFreeSpaceOffset(4096, pagebuf);
      setNumSlots(pagebuf, 0);
    }
  }

  int freeOffset = getFreeSpaceOffset(pagebuf);
  int numSlots = getNumSlots(pagebuf);

  memcpy(pagebuf + freeOffset - len, record, len);

  setFreeSpaceOffset(freeOffset - len, pagebuf);
  EncodeShort((short)(freeOffset - len), pagebuf + 4 + 4 * numSlots);
  EncodeShort((short)len, pagebuf + 4 + 4 * numSlots + 2);
  setNumSlots(pagebuf, numSlots + 1);

  *rid = (pagenum << 16) | numSlots;
  tbl->lastPageWritten = pagenum;

  int err = PF_UnfixPage(tbl->pf, pagenum, TRUE);
  checkerr(err);

  return 0;
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
int Table_Get(Table *tbl, RecId rid, byte *record,
              int maxlen) { //[Yash] Implemented this function
  int slot = rid & 0xFFFF;
  int pageNum = rid >> 16;
  // PF_GetThisPage(pageNum)
  // In the page get the slot offset of the record, and
  // memcpy bytes into the record supplied.
  // Unfix the page
  byte *pagebuf;
  int err = PF_GetThisPage(tbl->pf, pageNum, &pagebuf);
  checkerr(err);
  int offset = getNthSlotOffset(slot, pagebuf);
  int recordLength = getLen(slot, pagebuf);
  memcpy(record, pagebuf + offset, min(maxlen, recordLength));
  err = PF_UnfixPage(tbl->pf, pageNum, 0);
  checkerr(err);
  return min(maxlen, recordLength); // return size of record
}

void Table_Scan(Table *tbl, void *callbackObj,
                ReadFunc callbackfn) { //[Yash] Implemented this function
  // For each page obtained using PF_GetFirstPage and PF_GetNextPage
  //    for each record in that page,
  //          callbackfn(callbackObj, rid, record, recordLen)
  int pagenum;
  byte *pagebuf;
  int err = PF_GetFirstPage(tbl->pf, &pagenum, &pagebuf);
  while (err == PFE_OK) {
    int numSlots = getNumSlots(pagebuf);
    for (int slot = 0; slot < numSlots; slot++) {
      int offset = getNthSlotOffset(slot, pagebuf);
      int len = getLen(slot, pagebuf);
      RecId rid = (pagenum << 16) | slot;
      callbackfn(callbackObj, rid, pagebuf + offset, len);
    }
    PF_UnfixPage(tbl->pf, pagenum, FALSE);
    err = PF_GetNextPage(tbl->pf, &pagenum, &pagebuf);
  }
}
