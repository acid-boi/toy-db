#include <stdio.h>
#include <stdlib.h>
#include "codec.h"
#include "tbl.h"
#include "util.h"
#include "../pflayer/pf.h"
#include "../amlayer/am.h"
#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(1);}}

#define MAX_PAGE_SIZE 4000

void
printRow(void *callbackObj, RecId rid, byte *row, int len) {
    // [Abhay] Decode the record back into CSV fields, same order/types
    // as encode() in loaddb.c, and print comma-separated like the CSV.
    Schema *schema = (Schema *) callbackObj;
    byte *cursor = row;

    for (int i = 0; i < schema->numColumns; i++) {
	if (i > 0) printf(",");
	switch (schema->columns[i]->type) {
	    case VARCHAR: {
		char str[MAX_LINE_LEN];
		int strLen = DecodeShort(cursor); // encoded string length
		DecodeCString(cursor, str, sizeof(str));
		printf("%s", str);
		cursor += 2 + strLen;
		break;
	    }
	    case INT:
		printf("%d", DecodeInt(cursor));
		cursor += 4;
		break;
	    case LONG:
		printf("%lld", DecodeLong(cursor));
		cursor += 8;
		break;
	}
    }
    printf("\n");
}

#define DB_NAME "data.db"
#define INDEX_NAME "data.db.0"

void
index_scan(Table *tbl, Schema *schema, int indexFD, int op, int value) {
    // [Abhay] Walk the index for matching keys; AM_FindNextEntry returns
    // a negative AME_ code (e.g. AME_EOF) once the scan is done.
    int sd = AM_OpenIndexScan(indexFD, 'i', sizeof(int), op, (char *) &value);
    checkerr(sd);
    RecId rid;
    while ((rid = AM_FindNextEntry(sd)) >= 0) {
	byte record[MAX_PAGE_SIZE];
	int len = Table_Get(tbl, rid, record, sizeof(record));
	printRow(schema, rid, record, len);
    }
    AM_CloseIndexScan(sd);
}

int
main(int argc, char **argv) {
    char *schemaTxt = "Country:varchar,Capital:varchar,Population:int";
    Schema *schema = parseSchema(schemaTxt);
    Table *tbl;

    // [Abhay] overwrite=false, we're reading a db already built by loaddb
    int err = Table_Open(DB_NAME, schema, false, &tbl);
    checkerr(err);

    if (argc == 2 && *(argv[1]) == 's') {
	// [Abhay] sequential scan: printRow gets called for every row in the table
	Table_Scan(tbl, schema, printRow);
    } else {
	// index scan by default
	int indexFD = PF_OpenFile(INDEX_NAME);
	checkerr(indexFD);

	// Ask for populations less than 100000, then more than 100000. Together they should
	// yield the complete database.
	index_scan(tbl, schema, indexFD, LESS_THAN_EQUAL, 100000);
	index_scan(tbl, schema, indexFD, GREATER_THAN, 100000);

	err = PF_CloseFile(indexFD);
	checkerr(err);
    }
    Table_Close(tbl);
}
