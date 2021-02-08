#include <stdlib.h>
#include <string.h>

#include "amq.h"

struct amq_t {
   int replace_me;
};

amq_t *amq_new (void)
{
   amq_t *ret = calloc (1, sizeof *ret);

   if (!ret)
      return NULL;


   return ret;
}

void amq_del (amq_t *amq)
{
   if (!amq)
      return;

   free (amq);
}

