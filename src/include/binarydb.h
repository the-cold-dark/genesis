/*
// Full copyright information is available in the file ../doc/CREDITS
*/

#ifndef cdc_binarydb_h
#define cdc_binarydb_h

#ifdef S_IRUSR
#define READ_WRITE              (S_IRUSR | S_IWUSR)
#define READ_WRITE_EXECUTE      (S_IRUSR | S_IWUSR | S_IXUSR)
#else
#define READ_WRITE 0600
#define READ_WRITE_EXECUTE 0700
#endif

#define DUMP_BLOCK_SIZE      256
#define DUMP_NOT_IN_PROGRESS -2
#define DUMP_FAILED_TO_CLOSE -1
#define DUMP_FINISHED        1
#define DUMP_DUMPED_BLOCKS   0

void   init_binary_db(void);
void   init_new_db(void);
void   init_core_objects(void);
Int    simble_get(Obj * object, cObjnum objnum, Long *obj_size);
Int    simble_put(Obj * object, cObjnum objnum, Long *obj_size);
Int    simble_check(cObjnum objnum);
Int    simble_del(cObjnum objnum);
void   simble_close(void);
void   simble_flush(void);
Float  simble_fragmentation(void);
Int    simble_dump_start(char *dump_objects_filename);
Int    simble_dump_some_blocks (Int maxblocks);
void   simble_dump_finish(void);

/* global primarily so we can know if we are dumping */
extern FILE *dump_db_file;

#endif

