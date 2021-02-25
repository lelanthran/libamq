#include <string.h>

#include "amq.h"
#include "amq_wgroup.h"

#include "ds_array.h"
#include "ds_str.h"

struct amq_wgroup_t {
   char *name;
   ds_array_t *workers;
};

void amq_wgroup_del (amq_wgroup_t *group)
{
   if (!group)
      return;

   free (group->name);
   size_t nworkers = ds_array_length (group->workers);
   for (size_t i=0; i<nworkers; i++) {
      char *wname = ds_array_index (group->workers, i);
      free (wname);
   }
   ds_array_del (group->workers);

   free (group);
}

amq_wgroup_t *amq_wgroup_new (const char *name)
{
   amq_wgroup_t *ret = NULL;

   if (!name || !*name)
      return NULL;

   if (!(ret = calloc (1, sizeof *ret))) {
      return NULL;
   }

   ret->name = ds_str_dup (name);
   ret->workers = ds_array_new ();

   if (!ret->name || !ret->workers) {
      amq_wgroup_del (ret);
      ret = NULL;
   }

   return ret;
}

const char *amq_wgroup_name (amq_wgroup_t *group)
{
   return group ? group->name : "";
}

bool amq_wgroup_add_worker (amq_wgroup_t *group, const char *worker_name)
{
   char *tmp = ds_str_dup (worker_name);
   if (!tmp)
      return false;

   if (!(ds_array_ins_tail (group->workers, tmp))) {
      free (tmp);
      return false;
   }

   return true;
}

bool amq_wgroup_remove_worker (amq_wgroup_t *group, const char *worker_name)
{
   size_t nworkers = ds_array_length (group->workers);

   for (size_t i=0; i<nworkers; i++) {
      char *worker = ds_array_index (group->workers, i);
      if ((strcmp (worker, worker_name))==0) {
         free (worker);
         ds_array_remove (group->workers, i);
         return true;
      }
   }

   return false;
}

void amq_wgroup_sigset (amq_wgroup_t *group, uint64_t sigmask)
{
   size_t nworkers = ds_array_length (group->workers);

   for (size_t i=0; i<nworkers; i++) {
      const char *tmp = ds_array_index (group->workers, i);
      amq_worker_sigset (tmp, sigmask);
   }
}

void amq_wgroup_sigclr (amq_wgroup_t *group, uint64_t sigmask)
{
   size_t nworkers = ds_array_length (group->workers);

   for (size_t i=0; i<nworkers; i++) {
      const char *tmp = ds_array_index (group->workers, i);
      amq_worker_sigclr (tmp, sigmask);
   }
}

void amq_wgroup_wait (amq_wgroup_t *group)
{
   size_t nworkers = ds_array_length (group->workers);

   for (size_t i=0; i<nworkers; i++) {
      const char *tmp = ds_array_index (group->workers, i);
      amq_worker_wait (tmp);
   }
}


