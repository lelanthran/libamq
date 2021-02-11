#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "ds_hmap.h"
#include "amq_container.h"

struct amq_container_t {
   ds_hmap_t *map;
   pthread_mutex_t rlock;
   pthread_mutex_t wlock;
};


amq_container_t *amq_container_new (void)
{
   amq_container_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   if (!(ret->map = ds_hmap_new (256))) {
      free (ret);
      return NULL;
   }
   pthread_mutex_init (&ret->rlock, NULL);
   pthread_mutex_init (&ret->wlock, NULL);
   return ret;
}

void amq_container_del (amq_container_t *container,  void (*item_del_fptr) (void *))
{
   if (!container)
      return;

   pthread_mutex_lock (&container->rlock);
   pthread_mutex_lock (&container->wlock);

   const char **names = NULL;
   size_t *namelens = NULL;
   if ((ds_hmap_keys (container->map, (void ***)&names, &namelens))) {
      for (size_t i=0; names && namelens && names[i] &&namelens[i]; i++) {
         void *item = NULL;
         if (!(ds_hmap_get (container->map, names[i], namelens[i],
                                            &item, NULL))) {
            continue;
         }
         item_del_fptr (item);
      }
      free (names);
      free (namelens);
   }
   ds_hmap_del (container->map);

   pthread_mutex_unlock (&container->wlock);
   pthread_mutex_unlock (&container->rlock);
   pthread_mutex_destroy (&container->rlock);
   pthread_mutex_destroy (&container->wlock);
}

bool amq_container_add (amq_container_t *container,
                        const char *name, void *element)
{
   if (!container)
      return false;

   void *exist_data = NULL;
   size_t exist_datalen = 0;

   pthread_mutex_lock (&container->rlock);
   pthread_mutex_lock (&container->wlock);


   // Check if this item exists - we don't allow duplicates and we
   // don't want to overwrite any existing queue that exists with this
   // name.
   if ((ds_hmap_get (container->map, name, strlen (name) + 1,
                                     &exist_data, &exist_datalen))) {
      pthread_mutex_unlock (&container->wlock);
      pthread_mutex_unlock (&container->rlock);
      return false;
   }

   if (!(ds_hmap_set (container->map, name, strlen (name) + 1,
                                      element, 0))) {
      pthread_mutex_unlock (&container->wlock);
      pthread_mutex_unlock (&container->rlock);
      return false;
   }

   pthread_mutex_unlock (&container->wlock);
   pthread_mutex_unlock (&container->rlock);
   return true;
}

void *amq_container_remove (amq_container_t *container, const char *name)
{
   void *ret = NULL;

   pthread_mutex_lock (&container->rlock);
   pthread_mutex_lock (&container->wlock);

   if (!(ds_hmap_get (container->map, name, strlen (name) + 1,
                                      &ret, NULL))) {
      pthread_mutex_unlock (&container->wlock);
      pthread_mutex_unlock (&container->rlock);
      return NULL;
   }

   pthread_mutex_unlock (&container->wlock);
   pthread_mutex_unlock (&container->rlock);
   return ret;
}

void *amq_container_find (amq_container_t *container, const char *name)
{
   void *ret = NULL;
   size_t namelen = strlen (name);

   pthread_mutex_lock (&container->rlock);
   bool rc = ds_hmap_get (container->map, name, namelen, ret, NULL);
   pthread_mutex_unlock (&container->rlock);

   return rc ? NULL : ret;
}


