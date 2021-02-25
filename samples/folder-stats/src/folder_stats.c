#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>


#include "amq.h"

#include "folder_stats.h"

char **trap = NULL;

static char *lstrdup (const char *src)
{
   if (!src)
      src = "";

   char *ret = malloc (strlen (src) + 1);
   if (!ret) {
      AMQ_PRINT ("Out of memory error\n");
      return *trap; // Hopefully this crashes
   }

   strcpy (ret, src);
   return ret;
}

static char *lstrcat (const char *s1, const char *s2, const char *s3)
{
   size_t s1_len = strlen (s1);
   size_t s2_len = strlen (s2);
   size_t s3_len = strlen (s3);

   char *ret = malloc (s1_len + s2_len + s3_len + 1);
   if (!ret) {
      return NULL;
   }

   memcpy (ret, s1, s1_len);
   memcpy (&ret[s1_len], s2, s2_len);
   memcpy (&ret[s1_len + s2_len], s3, s3_len + 1);
   return ret;
}



struct folder_stats_entry_t {
   char        *f_name;
   char        *f_ext;
   size_t       f_size;
   char         f_type;
   uint64_t     f_mtime;
};

static void post_direntries (const char *dirname)
{
   char *tmp_fname = NULL;
   DIR *dirp = NULL;

   if (!(dirp = opendir (dirname))) {
      AMQ_ERROR_POST (errno, "Failed to read directory entries in [%s]: %m\n", dirname);
      goto errorexit;
   }

   struct dirent *de = NULL;

   while ((de = readdir (dirp)) != NULL) {
      if ((de->d_name[0] == '.') || (memcmp (de->d_name, "..", 2))==0)
         continue;

      if (!(tmp_fname = lstrcat (dirname, "/", de->d_name))) {
         AMQ_ERROR_POST (errno, "Out of memory error\n");
         goto errorexit;
      }

      amq_post (Q_PATHNAMES, tmp_fname, 0);
   }

errorexit:
   if (dirp)
      closedir (dirp);
}

folder_stats_entry_t *folder_stats_entry_new (const char *name)
{
   struct stat statbuf;

   folder_stats_entry_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   if ((stat (name, &statbuf)) != 0) {
      AMQ_ERROR_POST (errno, "Failed to stat [%s]: %m", name);
      free (ret);
      return NULL;
   }

   ret->f_name = lstrdup (name);
   ret->f_ext = lstrdup (strrchr (name, '.'));

   if (!ret->f_name || !ret->f_ext) {
      AMQ_ERROR_POST (errno, "Out of memory error\n");
      folder_stats_entry_del (ret);
      return NULL;
   }

   ret->f_size = statbuf.st_size;
   ret->f_type = '?';

   if ((S_ISREG (statbuf.st_mode)))
      ret->f_type = 'f';

   if ((S_ISDIR (statbuf.st_mode)))
      ret->f_type = 'd';

   if ((S_ISCHR (statbuf.st_mode)))
      ret->f_type = 'c';

   if ((S_ISBLK (statbuf.st_mode)))
      ret->f_type = 'b';

   if ((S_ISFIFO (statbuf.st_mode)))
      ret->f_type = '|';

#ifdef PLATFORM_POSIX
   if ((S_ISLNK (statbuf.st_mode)))
      ret->f_type = 'l';

   if ((S_ISSOCK (statbuf.st_mode)))
      ret->f_type = 's';
#endif

#ifdef PLATFORM_POSIX
   ret->f_mtime = statbuf.st_mtim.tv_sec;
#endif
#ifdef PLATFORM_WINDOWS
   ret->f_mtime = statbuf.st_mtime.tv_sec;
#endif

   if ((S_ISDIR (statbuf.st_mode))) {
      post_direntries (name);
   }

   return ret;
}

void folder_stats_entry_del (folder_stats_entry_t *fs)
{
   if (!fs)
      return;

   free (fs->f_name);
   free (fs->f_ext);
   free (fs);
}


bool folder_stats_entry_write (folder_stats_entry_t *fs, FILE *fout)
{
   if (!fout)
      fout = stdout;

   if (!fs) {
      fprintf (fout, "no-name,no-ext,no-size,no-type,no-mtime\n");
      return true;
   }

   fprintf (fout, "%s,%s,%zu,%c,%" PRIu64 "\n", fs->f_name,
                                                fs->f_ext,
                                                fs->f_size,
                                                fs->f_type,
                                                fs->f_mtime);
   return true;
}

const char *folder_stats_entry_name (folder_stats_entry_t *fs)
{
   return fs ? fs->f_name : "Invalid object";
}


