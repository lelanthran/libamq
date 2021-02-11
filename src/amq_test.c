
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "amq.h"
#include "ds_str.h"

#define TEST_MSG           ("Test Message")
#define TEST_MSGQ          ("APP:TEST_MSG_QUEUE")

static enum amq_worker_result_t gen_event (void *cdata)
{
   char *caller = cdata;

   char *msg = NULL;
   ds_str_printf (&msg, "[%i] %s", rand (), caller);
   amq_post (TEST_MSGQ, msg, strlen (msg));

   struct timespec tv = { 0, 100 * 1000000 };

   nanosleep (&tv, NULL);

   return amq_worker_result_CONTINUE;
}

static enum amq_worker_result_t handle_event (void *mesg, size_t mesg_len, void *cdata)
{
   const char *caller = cdata;
   const char *message = mesg;

   AMQ_PRINT ("Handling event [%s:%zu] from [%s]\n", caller, mesg_len, message);
   free (mesg);

   return amq_worker_result_CONTINUE;
}

static enum amq_worker_result_t error_logger (void *mesg, size_t mesg_len, void *cdata)
{
   const char *caller = cdata;
   const char *message = mesg;

   // TODO: Specify the interface for the error-reporting messages.
   AMQ_PRINT ("Rxed error [%s:%zu] from [%s]\n", caller, mesg_len, message);

   return amq_worker_result_CONTINUE;
}

int main (void)
{
   int ret = EXIT_FAILURE;

   if (!(amq_lib_init ())) {
      AMQ_PRINT ("Failed to initialise the Application Message Queue library\n");
      goto errorexit;
   }

   amq_post (AMQ_QUEUE_ERROR, TEST_MSG, strlen (TEST_MSG));

   if (!(amq_message_queue_create (TEST_MSGQ))) {
      AMQ_PRINT ("Unable to create message queue [%s]\n", TEST_MSGQ);
      goto errorexit;
   }

   amq_consumer_create (AMQ_QUEUE_ERROR, "ErrorLogger", error_logger, "Created by " __FILE__);
   amq_consumer_create (TEST_MSGQ, "HandleEvent", handle_event, "Created by " __FILE__);
   amq_producer_create ("GenEventWorker", gen_event, __func__);

   sleep (5);

   ret = EXIT_SUCCESS;

errorexit:

   amq_worker_signal ("ErrorLogger", AMQ_SIGNAL_TERMINATE);
   amq_worker_signal ("HandleEvent", AMQ_SIGNAL_TERMINATE);
   amq_worker_signal ("GenEventWorker", AMQ_SIGNAL_TERMINATE);

   amq_worker_wait ("ErrorLogger");
   amq_worker_wait ("HandleEvent");
   amq_worker_wait ("GenEventWorker");

   amq_lib_destroy ();
   return ret;
}
