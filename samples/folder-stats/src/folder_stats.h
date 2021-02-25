
#ifndef H_FOLDER_STATS
#define H_FOLDER_STATS

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define Q_OUTPUT        "q:output"
#define Q_PATHNAMES     "q:folders"
#define W_ERRHANDLER    "w:err-handler"
#define W_OUTPUT        "w:output"
#define W_PATHNAMES     "w:pathnames"

typedef struct folder_stats_entry_t folder_stats_entry_t;

#ifdef __cplusplus
extern "C" {
#endif

   folder_stats_entry_t *folder_stats_entry_new (const char *name);
   void folder_stats_entry_del (folder_stats_entry_t *fs);

   bool folder_stats_entry_write (folder_stats_entry_t *fs, FILE *fout);
   const char *folder_stats_entry_name (folder_stats_entry_t *fs);


#ifdef __cplusplus
};
#endif

#endif
