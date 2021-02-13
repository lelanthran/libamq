#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "ds_hmap.h"
#include "ds_str.h"
#include "amq_container.h"

struct amq_container_t {
   ds_hmap_t         *map;
   pthread_rwlock_t   lock;
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

   pthread_rwlock_init (&ret->lock, NULL);

   return ret;
}

void amq_container_del (amq_container_t *container,  void (*item_del_fptr) (void *))
{
   if (!container)
      return;

   pthread_rwlock_wrlock (&container->lock);

   if (item_del_fptr) {
      const char **names = NULL;
      size_t *namelens = NULL;
      if ((ds_hmap_keys (container->map, (void ***)&names, &namelens))) {
         for (size_t i=0; names && namelens && names[i] && namelens[i]; i++) {
            void *item = NULL;
            if (!(ds_hmap_get (container->map, names[i], namelens[i],
                                               &item, NULL))) {
               continue;
            }
            item_del_fptr (item);
         }
      }
      free (names);
      free (namelens);
   }

   ds_hmap_del (container->map);

   pthread_rwlock_unlock (&container->lock);
   pthread_rwlock_destroy (&container->lock);

   free (container);
}

bool amq_container_add (amq_container_t *container,
                        const char *name, void *element)
{
   if (!container)
      return false;

   void *exist_data = NULL;
   size_t exist_datalen = 0;

   pthread_rwlock_wrlock (&container->lock);

   // Check if this item exists - we don't allow duplicates and we
   // don't want to overwrite any existing queue that exists with this
   // name.
   if ((ds_hmap_get (container->map, name, strlen (name) + 1,
                                     &exist_data, &exist_datalen))) {
      pthread_rwlock_unlock (&container->lock);
      return false;
   }

   if (!(ds_hmap_set (container->map, name, strlen (name) + 1,
                                      element, 0))) {
      pthread_rwlock_unlock (&container->lock);
      return false;
   }

   pthread_rwlock_unlock (&container->lock);
   return true;
}

void *amq_container_remove (amq_container_t *container, const char *name)
{
   void *ret = NULL;

   if (!container)
      return NULL;

   pthread_rwlock_wrlock (&container->lock);

   if (!(ds_hmap_get (container->map, name, strlen (name) + 1,
                                      &ret, NULL))) {
      pthread_rwlock_unlock (&container->lock);
      return NULL;
   }

   ds_hmap_remove (container->map, name, strlen (name) + 1);
   pthread_rwlock_unlock (&container->lock);
   return ret;
}

void *amq_container_find (amq_container_t *container, const char *name)
{
   if (!container)
      return NULL;

   void *ret = NULL;
   size_t namelen = name ? strlen (name) + 1 : 0;

   pthread_rwlock_rdlock (&container->lock);
   bool rc = ds_hmap_get (container->map, name, namelen, &ret, NULL);
   pthread_rwlock_unlock (&container->lock);

   return rc ? ret : NULL;
}

size_t amq_container_names (amq_container_t *container, char ***names)
{
   if (!container)
      return 0;

   char **retvals = NULL;
   char **tmp = NULL;
   size_t ret = 0;
   pthread_rwlock_rdlock (&container->lock);
   ret = ds_hmap_keys (container->map, (void ***)&retvals, NULL);
   if (!ret) {
      pthread_rwlock_unlock (&container->lock);
      free (retvals);
      *names = NULL;
      return 0;
   }
   if (!(tmp = calloc (ret + 1, sizeof *tmp))) {
      pthread_rwlock_unlock (&container->lock);
      free (retvals);
      return 0;
   }
   for (size_t i=0; i<ret; i++) {
      if (!(tmp[i] = ds_str_dup (retvals[i]))) {
         pthread_rwlock_unlock (&container->lock);
         free (retvals);
         for (size_t j=0; tmp[j]; j++) {
            free (tmp[j]);
         }
         free (tmp);
         *names = NULL;
         return 0;
      }
   }

   pthread_rwlock_unlock (&container->lock);

   *names = tmp;
   free (retvals);

   return ret;
}

