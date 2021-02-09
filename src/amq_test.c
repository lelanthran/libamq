
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "amq.h"
#include "ds_str.h"

#define STARTED_MSG        ("Application started\n")

int main (void)
{
   int ret = EXIT_FAILURE;

   if (!(amq_lib_init ())) {
      AMQ_PRINT ("Failed to initialise the Application Message Queue library\n");
      goto errorexit;
   }

   amq_post (AMQ_QUEUE_ERROR, STARTED_MSG, strlen (STARTED_MSG));

   ret = EXIT_SUCCESS;

errorexit:

   amq_lib_destroy ();

   return ret;
}
