
#include <stdio.h>
#include <stdlib.h>

#include "amq.h"
#include "ds_str.h"

int main (void)
{
   int ret = EXIT_FAILURE;

   amq_t *amq = amq_new ();
   if (!amq) {
      AMQ_PRINT ("Failed to create amq_data object\n");
      goto errorexit;
   }

   ret = EXIT_SUCCESS;

errorexit:
   amq_del (amq);

   return ret;
}
