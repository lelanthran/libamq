#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>

#include <pthread.h>

#include "ds_hmap.h"
#include "ds_str.h"
#include "cmq.h"

#include "amq.h"
#include "amq_container.h"

/* ************************************************************
 * The global queue container
 */
amq_container_t *g_queue_container;

/* ************************************************************
 * The global worker container
 */
amq_container_t *g_worker_container;

/* ************************************************************
 * Error objects, for the error queue
 */

struct amq_error_t *amq_error_new (const char *file, int line, int code, ...)
{
   bool error = true;
   char *prefix = NULL;
   char *message = NULL;

   struct amq_error_t *ret = calloc (1, sizeof *ret);

   if (!ret) {
      AMQ_PRINT ("Out of memory error: Failed to allocate memory for error object\n");
      return NULL;
   }
   ret->code = code;

   va_list ap;
   va_start (ap, code);

   const char *fmts = va_arg (ap, const char *);
   if ((ds_str_vprintf (&message, fmts, ap))==0) {
      AMQ_PRINT ("Out of memory error: Failed to allocate memory for error message\n");
      goto errorexit;
   }

   if ((ds_str_printf (&prefix, "[%s:%i] [code:%i]", file, line, code))==0) {
      AMQ_PRINT ("Out of memory error: Failed to allocate memory for error message\n");
      goto errorexit;
   }

   if (!(ret->message = ds_str_cat (prefix, " ", message, NULL))) {
      AMQ_PRINT ("Out of memory error: Failed to generate final message\n");
      goto errorexit;
   }

   error = false;

errorexit:

   va_end (ap);

   free (message);
   free (prefix);

   if (error) {
      amq_error_del (ret);
      ret = NULL;
   }

   return ret;
}

void amq_error_del (struct amq_error_t *errobj)
{
   if (errobj)
      free (errobj->message);

   free (errobj);
}



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
   AMQ_PRINT ("Removing queue, discarding %i messages\n", cmq_count (q->cmq));
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
 * Statistics object, to track performance of queues
 */
static void amq_stats_update (struct amq_stats_t *so, float newval)
{
   if (newval < so->min)
      so->min = newval;

   if (newval > so->max)
      so->max = newval;

   so->count++;

   so->average = (so->average + newval) / so->count;

   float tmp = newval - so->average;
   if (tmp < 0)
      tmp *= -1;

   so->deviation = (so->deviation + tmp) / so->count;
}

static float timespec_conv (const struct timespec *ts)
{
   float ret = 0;
   ret = ts->tv_sec * 1000000000;
   ret += ts->tv_nsec;

   ret = ret / 1000000;

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
   // The following fields cannot be added to, removed from or swapped in order
   // as they are public to the caller.
   pthread_t             worker_id;
   char                 *worker_name;
   void                 *worker_cdata;
   uint8_t               worker_type;
   struct amq_stats_t    stats;

   // These fields are private.
   cmq_t                *listen_queue;
   union worker_func_t   worker_func;
   pthread_mutex_t       flags_lock;
   uint64_t              flags;
};

static void worker_del (struct worker_t *w)
{
   if (!w)
      return;

   amq_worker_sigset (w->worker_name, AMQ_SIGNAL_TERMINATE);
   amq_worker_wait (w->worker_name);
   free (w->worker_name);
   pthread_mutex_destroy (&w->flags_lock);
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
   pthread_mutex_init (&ret->flags_lock, &attr);
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

   ret->stats.min = 999999.9999;
   return ret;
}

static void worker_sigset (struct worker_t *worker, uint64_t signals)
{
   // TODO: Could be faster using pthread_rwlock_t instead of a mutex.
   pthread_mutex_lock (&worker->flags_lock);
   worker->flags |= signals;
   pthread_mutex_unlock (&worker->flags_lock);
}

static void worker_sigclr (struct worker_t *worker, uint64_t signals)
{
   // TODO: Could be faster using pthread_rwlock_t instead of a mutex.
   pthread_mutex_lock (&worker->flags_lock);
   worker->flags &= ~signals;
   pthread_mutex_unlock (&worker->flags_lock);
}

static uint64_t worker_sigget (struct worker_t *worker)
{
   // TODO: Could be faster using pthread_rwlock_t instead of a mutex.
   pthread_mutex_lock (&worker->flags_lock);
   uint64_t ret = worker->flags;
   pthread_mutex_unlock (&worker->flags_lock);
   return ret;
}

static void *worker_run (void *worker)
{
   struct worker_t *w = worker;
   enum amq_worker_result_t worker_result = amq_worker_result_CONTINUE;
   uint64_t flags = 0;

   AMQ_PRINT ("Thread started [%s]\n", w->worker_name);
   while ((worker_result != amq_worker_result_STOP)) {

      if ((pthread_mutex_trylock (&w->flags_lock))==0) {
         flags = w->flags;
         pthread_mutex_unlock (&w->flags_lock);
         if ((flags & AMQ_SIGNAL_TERMINATE))
            break;
         if ((flags & AMQ_SIGNAL_SUSPEND)) {
            sleep (1);
            continue;
         }
      }

      worker_result = amq_worker_result_STOP;

      if (w->worker_type == WORKER_PRODUCER) {
         worker_result = w->worker_func.producer_func ((struct amq_worker_t *)w,
                                                        w->worker_cdata);
      }
      if (w->worker_type == WORKER_CONSUMER) {
         void *mesg = NULL;
         size_t mesg_len = 0;
         worker_result = amq_worker_result_CONTINUE;

         struct timespec ts;
         if (!(cmq_wait (w->listen_queue, &mesg, &mesg_len, 1000, &ts)))
            continue;

         amq_stats_update (&w->stats, timespec_conv (&ts));

         worker_result = w->worker_func.consumer_func ((struct amq_worker_t *)w,
                                                        mesg, mesg_len, w->worker_cdata);
      }
   }

   AMQ_PRINT ("Ending thread [%s][%i:%" PRIx64 "]\n", w->worker_name, worker_result, flags);
   if (!(amq_container_remove (g_worker_container, w->worker_name))) {
      AMQ_PRINT ("Could not remove [%s] from container - double-free()?\n", w->worker_name);
   }
   worker_del (w);

   return NULL;
}


/* ************************************************************
 * Internal utility functions.
 */
static char *gen_random_string (size_t nbytes)
{
   static int seed = 0;
   if (!seed) {
      seed = (int)time (NULL);
      srand (seed);
   }

   char *ret = malloc ((nbytes * 2) + 1);
   if (!ret)
      return ds_str_dup ("");

   for (size_t i=0; i<nbytes; i++) {
      uint8_t r = rand () & 0xff;
      snprintf (&ret[i*2], 3, "%02x", r);
   }
   return ret;
}

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

   if (!(amq_message_queue_create (AMQ_QUEUE_ERROR)))
      goto errorexit;

   error = false;

errorexit:
   if (error) {
      amq_lib_destroy ();
   }
   return !error;
}

void amq_lib_destroy (void)
{
#if 1
   char **worker_names = NULL;

   if ((amq_container_names (g_worker_container, &worker_names))!=0 && worker_names) {
      for (size_t i=0; worker_names[i]; i++) {
         amq_worker_sigset (worker_names[i], AMQ_SIGNAL_TERMINATE);
         amq_worker_wait (worker_names[i]);
         free (worker_names[i]);
      }
      free (worker_names);
   }
#endif

   amq_container_del (g_worker_container, NULL);
   g_worker_container = NULL;

   amq_container_del (g_queue_container, (void (*) (void *))queue_del);
   g_queue_container = NULL;

}

bool amq_message_queue_create (const char *name)
{
   struct queue_t *newq = queue_new (name);
   if (!newq) {
      return false;
   }

   if (!(amq_container_add (g_queue_container, name, newq))) {
      queue_del (newq);
      return false;
   }

   return true;
}

void amq_post (const char *queue_name, void *buf, size_t buf_len)
{
   struct queue_t *queue = amq_container_find (g_queue_container, queue_name);
   if (!queue)
      return;

   cmq_post (queue->cmq, buf, buf_len);
}

size_t amq_count (const char *queue_name)
{
   struct queue_t *queue = amq_container_find (g_queue_container, queue_name);
   if (!queue)
      return 0;

   int actual = cmq_count (queue->cmq);
   if (actual < 0)
      return 0;

   return actual;
}

static bool worker_create (const char *worker_name, cmq_t *listen_queue, uint8_t type,
                           void *worker_func, void *cdata)
{
   bool error = true;
   char *actual_name = NULL;

   if (!worker_name || !worker_name[0]) {
      actual_name = gen_random_string (8);
   } else {
      actual_name = ds_str_dup (worker_name);
   }

   struct worker_t *worker = worker_new (actual_name, listen_queue, type,
                                         worker_func, cdata);

   if (!worker)
      goto errorexit;

   if (!(amq_container_add (g_worker_container, actual_name, worker))) {
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
      amq_container_remove (g_worker_container, actual_name);
      worker_del (worker);
   }
   free (actual_name);

   return !error;
}

bool amq_producer_create (const char *worker_name,
                          amq_producer_func_t *worker_func, void *cdata)
{
   return worker_create (worker_name, NULL, WORKER_PRODUCER, worker_func, cdata);
}

bool amq_consumer_create (const char *supply_queue_name,
                          const char *worker_name,
                          amq_consumer_func_t *worker_func, void *cdata)
{
   struct queue_t *queue = amq_container_find (g_queue_container, supply_queue_name);
   cmq_t *supply_queue = queue->cmq;
   return worker_create (worker_name, supply_queue, WORKER_CONSUMER, worker_func, cdata);
}

void amq_worker_sigset (const char *worker_name, uint64_t signals)
{
   struct worker_t *worker = amq_container_find (g_worker_container, worker_name);
   if (!worker)
      return;

   worker_sigset (worker, signals);
}

void amq_worker_sigclr (const char *worker_name, uint64_t signals)
{
   struct worker_t *worker = amq_container_find (g_worker_container, worker_name);
   if (!worker)
      return;

   worker_sigclr (worker, signals);
}

uint64_t amq_worker_sigget (const char *worker_name)
{
   struct worker_t *worker = amq_container_find (g_worker_container, worker_name);
   if (!worker)
      return 0;

   return worker_sigget (worker);
}

void amq_worker_wait (const char *worker_name)
{
   struct worker_t *worker = amq_container_find (g_worker_container, worker_name);
   if (!worker)
      return;

   pthread_join (worker->worker_id, NULL);
}

