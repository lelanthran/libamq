
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
   amq_post (AMQ_QUEUE_ERROR, caller, strlen (caller) + 1);
   sleep (1);
   return amq_worker_result_CONTINUE;
}

static enum amq_worker_result_t handle_event (void *mesg, size_t mesg_len, void *cdata)
{
   const char *caller = cdata;
   const char *message = mesg;

   AMQ_PRINT ("rxed event [%s:%zu] from [%s]\n", caller, mesg_len, message);

   return amq_worker_result_CONTINUE;
}

static enum amq_worker_result_t error_logger (void *mesg, size_t mesg_len, void *cdata)
{
   const char *caller = cdata;
   const char *message = mesg;

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

   amq_consumer_create (AMQ_QUEUE_ERROR, "ErrorLogger", error_logger, "Created by " __FILE__);
   amq_producer_create ("GenEventWorker", gen_event, __func__);

   sleep (10);

   ret = EXIT_SUCCESS;

errorexit:

   sleep (1);

   amq_lib_destroy ();

   sleep (5);

   return ret;
}
