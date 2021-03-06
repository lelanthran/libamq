
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#include "amq.h"
#include "amq_wgroup.h"
#include "ds_str.h"

#define TEST_MSG           ("Test Message")
#define TEST_MSGQ          ("APP:TEST_MSG_QUEUE")
#define TEST_GROUPNAME     ("TEST_GROUP")

static void stats_dump (const struct amq_worker_t *w)
{
   printf ("[statistics:%s] count:%zu, min=%0.4f, max=%0.4f, avg=%0.4f, dev=%0.4f\n",
            w->worker_name,
            w->stats.count,
            w->stats.min,
            w->stats.max,
            w->stats.average,
            w->stats.deviation);
}

static enum amq_worker_result_t gen_event (const struct amq_worker_t *self,
                                           void *cdata)
{
   char *caller = cdata;

   char *msg = NULL;
   ds_str_printf (&msg, "{%s} [%i] %s", self->worker_name, rand (), caller);
   amq_post (TEST_MSGQ, msg, strlen (msg));

   struct timespec tv = { 0, 100 * 1000000 };

   nanosleep (&tv, NULL);

   return amq_worker_result_CONTINUE;
}

static enum amq_worker_result_t handle_event (const struct amq_worker_t *self,
                                              void *mesg, size_t mesg_len, void *cdata)
{
   const char *caller = cdata;
   const char *message = mesg;

   AMQ_PRINT ("{%s} Handling event [%s:%zu] from [%s]\n", self->worker_name,
                                                          caller, mesg_len, message);
   free (mesg);

   stats_dump (self);

   return amq_worker_result_CONTINUE;
}

static enum amq_worker_result_t error_logger (const struct amq_worker_t *self,
                                              void *mesg, size_t mesg_len, void *cdata)
{
   const char *caller = cdata;
   struct amq_error_t *errobj = mesg;

   // TODO: Specify the interface for the error-reporting messages.
   AMQ_PRINT ("{%s} Rxed error %i: {%s:%zu} from {%s}\n",
               self->worker_name,
               errobj->code, errobj->message, mesg_len, caller);

   amq_error_del (errobj);
   stats_dump (self);

   return amq_worker_result_CONTINUE;
}

int main (void)
{
   int ret = EXIT_FAILURE;
   amq_wgroup_t *group = NULL;

   if (!(amq_lib_init ())) {
      AMQ_PRINT ("Failed to initialise the Application Message Queue library\n");
      goto errorexit;
   }

   if (!(group = amq_wgroup_new (TEST_GROUPNAME))) {
      AMQ_PRINT ("Failed to create group [%s]\n", TEST_GROUPNAME);
      goto errorexit;
   }

   AMQ_ERROR_POST (-1, "Test of the error signalling mechanism [ret:%i]\n", ret);

   if (!(amq_message_queue_create (TEST_MSGQ))) {
      AMQ_PRINT ("Unable to create message queue [%s]\n", TEST_MSGQ);
      goto errorexit;
   }

   amq_consumer_create (AMQ_QUEUE_ERROR, "ErrorLogger", error_logger, "Created by " __FILE__);
   amq_consumer_create (TEST_MSGQ, "", handle_event, "Created by " __FILE__);
   sleep (1);
   amq_producer_create ("GenEventWorker-0", gen_event, (void *)__func__);
#if 1 // Used this when testing anonymous workers and queue performance
   amq_producer_create ("GenEventWorker-1", gen_event, (void *)__func__);
   amq_producer_create ("GenEventWorker-2", gen_event, (void *)__func__);
   amq_producer_create ("GenEventWorker-3", gen_event, (void *)__func__);
   amq_producer_create ("GenEventWorker-4", gen_event, (void *)__func__);
   amq_producer_create ("GenEventWorker-5", gen_event, (void *)__func__);
   amq_producer_create ("GenEventWorker-6", gen_event, (void *)__func__);
   amq_producer_create ("GenEventWorker-7", gen_event, (void *)__func__);
   amq_producer_create ("GenEventWorker-8", gen_event, (void *)__func__);
   amq_producer_create ("GenEventWorker-9", gen_event, (void *)__func__);
   amq_producer_create ("", gen_event, (void *)__func__);
   amq_producer_create ("", gen_event, (void *)__func__);
   amq_producer_create ("", gen_event, (void *)__func__);
   amq_producer_create (NULL, gen_event, (void *)__func__);
   amq_producer_create (NULL, gen_event, (void *)__func__);
   amq_producer_create (NULL, gen_event, (void *)__func__);

   if (!(amq_wgroup_add_worker (group, "GenEventWorker-1")) ||
       !(amq_wgroup_add_worker (group, "GenEventWorker-2")) ||
       !(amq_wgroup_add_worker (group, "GenEventWorker-3")) ||
       !(amq_wgroup_add_worker (group, "GenEventWorker-4"))) {
      AMQ_PRINT ("Failed to add workers to group\n");
      goto errorexit;
   }

   sleep (5);
#endif


#if 1 // usaed to test the supension and resumption of workers.
   sleep (1);
   for (size_t i=0; i<4; i++) {
      amq_wgroup_sigset (group, AMQ_SIGNAL_SUSPEND);
      AMQ_PRINT ("<suspended %zu: %s %016" PRIu64 "]>\n",
                  i, amq_wgroup_name (group), amq_worker_sigget ("GenEventWorker-3"));
      sleep (2);
      amq_wgroup_sigclr (group, AMQ_SIGNAL_SUSPEND);
      AMQ_PRINT ("</unsuspended %zu: %s %016" PRIu64 "]>\n",
                  i, amq_wgroup_name (group), amq_worker_sigget ("GenEventWorker-4"));
      sleep (2);
   }
#endif

   ret = EXIT_SUCCESS;

errorexit:

   amq_worker_sigset ("ErrorLogger", AMQ_SIGNAL_TERMINATE);
   amq_worker_sigset ("HandleEvent", AMQ_SIGNAL_TERMINATE);
   amq_worker_sigset ("GenEventWorker", AMQ_SIGNAL_TERMINATE);

   amq_worker_wait ("ErrorLogger");
   amq_worker_wait ("HandleEvent");
   amq_worker_wait ("GenEventWorker");

   amq_wgroup_del (group);

   amq_lib_destroy ();
   return ret;
}
