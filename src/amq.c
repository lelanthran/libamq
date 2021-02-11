#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <pthread.h>

#include "ds_hmap.h"
#include "ds_str.h"
#include "cmq.h"

#include "amq.h"
#include "amq_container.h"

/* ************************************************************
 * Private variables and functions
 */
#define WORKER_FLAG_STOP         (1 << 0)

/* ************************************************************
 * Queue objects, so we can keep track of queues
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
 * Worker objects, so we can keep track of workers
 */

#define WORKER_ERROR          (0)
#define WORKER_PRODUCER       (1)
#define WORKER_CONSUMER       (2)
union worker_func_t {
   amq_producer_func_t *producer_func;
   amq_consumer_func_t *consumer_func;
};

struct worker_t {
   pthread_t   worker_id;
   char       *worker_name;
   void       *worker_cdata;
   uint8_t     worker_type;
   union worker_func_t worker_func;
   cmq_t      *listen_queue;

   pthread_mutex_t lock;
   uint64_t    flags;
};

static void worker_del (struct worker_t *w)
{
   if (!w)
      return;
   free (w->worker_name);
   memset (w, 0, sizeof *w);
   free (w);
}

static struct worker_t *worker_new (const char *name, cmq_t *listen_queue, uint8_t type,
                                    void *worker_func, void *cdata)
{
   struct worker_t *ret = calloc (1, sizeof *ret);
   if (!ret)
      return NULL;

   pthread_mutexattr_t attr;
   pthread_mutexattr_init (&attr);
   pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);

   ret->listen_queue = listen_queue;
   ret->worker_type = type;
   ret->worker_cdata = cdata;
   pthread_mutex_init (&ret->lock, &attr);
   pthread_mutexattr_destroy (&attr);

   ret->worker_name = ds_str_dup (name);

   if (type==WORKER_PRODUCER)
      ret->worker_func.producer_func = worker_func;

   if (type==WORKER_CONSUMER)
      ret->worker_func.consumer_func = worker_func;

   if (!ret->worker_name               ||
       !ret->worker_func.consumer_func ||
       !ret->worker_func.producer_func) {
      worker_del (ret);
      ret = NULL;
   }

   return ret;
}

static void *worker_run (void *worker)
{
   struct worker_t *w = worker;
   enum amq_worker_result_t worker_result = amq_worker_result_CONTINUE;
   uint64_t flags = 0;

   AMQ_PRINT ("Thread started\n");
   while ((worker_result != amq_worker_result_STOP) && (~(flags & WORKER_FLAG_STOP))) {

      worker_result = amq_worker_result_STOP;

      if ((pthread_mutex_trylock (&w->lock))==0) {
         flags = w->flags;
         pthread_mutex_unlock (&w->lock);
      }

      if (w->worker_type == WORKER_PRODUCER) {
         worker_result = w->worker_func.producer_func (w->worker_cdata);
      }
      if (w->worker_type == WORKER_CONSUMER) {
         void *mesg = NULL;
         size_t mesg_len = 0;
         if (!(cmq_wait (w->listen_queue, &mesg, &mesg_len, 1000)))
            continue;

         worker_result = w->worker_func.consumer_func (mesg, mesg_len, w->worker_cdata);
      }
   }

   return NULL;
}

/* ************************************************************
 * The global queue container
 */
amq_container_t *g_queue_container;

/* ************************************************************
 * The global worker container
 */
amq_container_t *g_worker_container;

/* ************************************************************
 * Public variables and functions
 */

bool amq_lib_init (void)
{
   bool error = true;

   if (!(g_queue_container = amq_container_new ()) ||
       !(g_worker_container = amq_container_new ())) {
      goto errorexit;
   }

   struct queue_t *errq = queue_new (AMQ_QUEUE_ERROR);
   if (!errq) {
      goto errorexit;
   }

   if (!(amq_container_add (g_queue_container, AMQ_QUEUE_ERROR, errq))) {
      goto errorexit;
   }

   error = false;

errorexit:
   if (error) {
      amq_lib_destroy ();
   }
   return !error;
}

void amq_lib_destroy (void)
{
   struct queue_t *errq = amq_container_remove (g_queue_container, AMQ_QUEUE_ERROR);

   amq_container_del (g_queue_container, (void (*) (void *))queue_del);
   g_queue_container = NULL;

   amq_container_del (g_worker_container, (void (*) (void *))worker_del);
   g_worker_container = NULL;
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
                          amq_producer_func_t *worker_func, void *cdata)
{
   bool error = true;
   struct worker_t *worker = worker_new (worker_name, NULL, WORKER_PRODUCER,
                                         worker_func, cdata);

   if (!worker)
      goto errorexit;

   if (!(amq_container_add (g_worker_container, worker_name, worker))) {
      // TODO: Post an error to the AMQ_QUEUE_ERROR queue
      AMQ_PRINT ("Failed to create thread: %m\n");
      goto errorexit;
   }

   if ((pthread_create (&worker->worker_id, NULL, worker_run, worker))!=0) {
      // TODO: Post an error to the AMQ_QUEUE_ERROR queue
      AMQ_PRINT ("Failed to create thread: %m\n");
      goto errorexit;
   }

   error = false;

errorexit:
   if (error) {
      amq_container_remove (g_worker_container, worker_name);
      worker_del (worker);
   }
   return !error;
}

bool amq_consumer_create (const char *supply_queue_name,
                          const char *worker_name,
                          amq_consumer_func_t *worker, void *cdata)
{
}

