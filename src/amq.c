#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "ds_hmap.h"
#include "ds_str.h"
#include "cmq.h"

#include "amq.h"

/* ************************************************************
 * Private variables and functions
 */

/* ************************************************************
 * Queues
 */

struct queue_t {
   char  *name;
   cmq_t *cmq;
};

static void queue_del (struct queue_t *q)
{
   if (!q)
      return;
   free (q->name);
   cmq_del (q->cmq);
   free (q);
}

static struct queue_t *queue_new (const char *name)
{
   struct queue_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   ret->name = ds_str_dup (name);
   ret->cmq = cmq_new ();
   if (!ret->name || !ret->cmq) {
      queue_del (ret);
      ret = NULL;
   }

   return ret;
}

/* ************************************************************
 * The global queue container
 */

ds_hmap_t *g_queue_container;  // struct queue_t *
pthread_mutex_t g_queue_container_rlock;
pthread_mutex_t g_queue_container_wlock;

static void queue_container_destroy (void)
{
   pthread_mutex_lock (&g_queue_container_rlock);
   pthread_mutex_lock (&g_queue_container_wlock);
   if (g_queue_container) {
      const char **names = NULL;
      size_t *namelens = NULL;
      if ((ds_hmap_keys (g_queue_container, (void ***)&names, &namelens))) {
         for (size_t i=0; names && namelens && names[i] &&namelens[i]; i++) {
            struct queue_t *queue = NULL;
            if (!(ds_hmap_get (g_queue_container, names[i], namelens[i],
                                                  (void **)&queue, NULL))) {
               continue;
            }
            queue_del (queue);
         }
         free (names);
         free (namelens);
      }
      ds_hmap_del (g_queue_container);
   }

   pthread_mutex_unlock (&g_queue_container_wlock);
   pthread_mutex_unlock (&g_queue_container_rlock);
   pthread_mutex_destroy (&g_queue_container_rlock);
   pthread_mutex_destroy (&g_queue_container_wlock);
}

static bool queue_container_init (void)
{
   if (!(g_queue_container = ds_hmap_new (256))) {
      return false;
   }
   pthread_mutex_init (&g_queue_container_rlock, NULL);
   pthread_mutex_init (&g_queue_container_wlock, NULL);
   return true;
}

static bool queue_container_add (const char *queue_name)
{
   void *exist_data = NULL;
   size_t exist_datalen = 0;

   pthread_mutex_lock (&g_queue_container_rlock);
   pthread_mutex_lock (&g_queue_container_wlock);

   // Check if this item exists - we don't allow duplicates and we
   // don't want to overwrite any existing queue that exists with this
   // name.
   if ((ds_hmap_get (g_queue_container, queue_name, strlen (queue_name) + 1,
                                         &exist_data, &exist_datalen))) {
      pthread_mutex_unlock (&g_queue_container_wlock);
      pthread_mutex_unlock (&g_queue_container_rlock);
      return false;
   }

   struct queue_t *new_item = queue_new (queue_name);
   if (!new_item)
      return false;

   if (!(ds_hmap_set (g_queue_container, queue_name, strlen (queue_name) + 1,
                                         new_item, sizeof *new_item))) {
      queue_del (new_item);
      pthread_mutex_unlock (&g_queue_container_wlock);
      pthread_mutex_unlock (&g_queue_container_rlock);
      return false;
   }

   pthread_mutex_unlock (&g_queue_container_wlock);
   pthread_mutex_unlock (&g_queue_container_rlock);
   return true;
}

static void queue_container_del (const char *name)
{
   struct queue_t *queue = NULL;

   pthread_mutex_lock (&g_queue_container_rlock);
   pthread_mutex_lock (&g_queue_container_wlock);

   if (!(ds_hmap_get (g_queue_container, name, strlen (name) + 1,
                                         (void **)&queue, NULL))) {
      pthread_mutex_unlock (&g_queue_container_wlock);
      pthread_mutex_unlock (&g_queue_container_rlock);
      return;
   }

   pthread_mutex_unlock (&g_queue_container_wlock);
   pthread_mutex_unlock (&g_queue_container_rlock);
   queue_del (queue);
}

static struct queue_t *queue_container_find (const char *name)
{
   struct queue_t *ret = NULL;
   size_t namelen = strlen (name);

   pthread_mutex_lock (&g_queue_container_rlock);
   bool rc = ds_hmap_get (g_queue_container, name, namelen, (void **)&ret, NULL);
   pthread_mutex_unlock (&g_queue_container_rlock);

   if (!rc)
      ret = NULL;

   return ret;
}

/* ************************************************************
 * Public variables and functions
 */

bool amq_lib_init (void)
{
   if (!(queue_container_init ())) {
      return false;
   }
   if (!(queue_container_add (AMQ_QUEUE_ERROR))) {
      return false;
   }
   return true;
}

void amq_lib_destroy (void)
{
   queue_container_del (AMQ_QUEUE_ERROR);
   queue_container_destroy ();
}


void amq_post (const char *queue_name, void *buf, size_t buf_len)
{
   struct queue_t *queue = queue_container_find (queue_name);
   if (!queue)
      return;

   // TODO: This really should go into a struct that we post.
   cmq_post (queue->cmq, buf, buf_len);
}
