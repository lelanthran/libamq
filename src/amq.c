#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "ds_hmap.h"
#include "ds_str.h"
#include "cmq.h"

#include "amq.h"
#include "amq_container.h"

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

amq_container_t *g_queue_container;
amq_container_t *g_worker_container;

/* ************************************************************
 * Public variables and functions
 */

bool amq_lib_init (void)
{
   if (!(g_queue_container = amq_container_new ())) {
      return false;
   }
   struct queue_t *errq = queue_new (AMQ_QUEUE_ERROR);
   if (!errq)
      return false;

   if (!(amq_container_add (g_queue_container, AMQ_QUEUE_ERROR, errq))) {
      queue_del (errq);
      return false;
   }
   return true;
}

void amq_lib_destroy (void)
{
   struct queue_t *errq = amq_container_remove (g_queue_container, AMQ_QUEUE_ERROR);
   queue_del (errq);
   amq_container_del (g_queue_container, (void (*) (void *))queue_del);
   g_queue_container = NULL;
}

void amq_post (const char *queue_name, void *buf, size_t buf_len)
{
   struct queue_t *queue = amq_container_find (g_queue_container, queue_name);
   if (!queue)
      return;

   // TODO: This really should go into a struct that we post.
   cmq_post (queue->cmq, buf, buf_len);
}

bool amq_producer_create (const char *worker_name,
                          amq_producer_func_t *worker, void *cdata)
{
   bool error = true;

   error = false;
errorexit:
   return !error;
}

bool amq_consumer_create (const char *supply_queue_name,
                          const char *worker_name,
                          amq_consumer_func_t *worker, void *cdata)
{
}

