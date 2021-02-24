
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "folder_stats.h"
#include "amq.h"

static void process_cline (int argc, char **argv)
{
   (void)argc;
   size_t i=1;
   while (argv[i]) {
      if ((memcmp (argv[i], "--", 2))!=0)
         continue;
      char *name = argv[i];
      char *value = strchr (name, '=');
      if (value) {
         *value++ = 0;
      } else {
         value = "";
      }
      // TODO: Write wrapper for this function for WINDOWS
      setenv (name, value, 1);
      i++;
   }
}

static const char *g_help_msg[] = {
   "Usage: folder_stats [options]",
   NULL,
};

static void print_long_msg (const char **msg)
{
   for (size_t i=0; msg[i]; i++) {
      printf ("%s\n", msg[i]);
   }
}

/* ********************************************************************** */

#define STRINGIFY_(x)      #x
#define STRINGIFY(x)       STRINGIFY_(x)

#define Q_OUT           "q:output"
#define Q_FOLDERS       "q:folders"

/* ********************************************************************** */

enum amq_worker_result_t errhandler (const struct amq_worker_t *self,
                                     void *mesg, size_t mesg_len,
                                     void *cdata)
{
   struct amq_error_t *error = mesg;
   (void)cdata;
   (void)self;

   fprintf (stderr, "Error %i: [%s]\n", error->code, error->message);
   amq_error_del (error);
   return amq_worker_result_CONTINUE;
}

/* ********************************************************************** */

int main (int argc, char **argv)
{
   int ret = EXIT_FAILURE;

   process_cline (argc, argv);
   printf ("Folder statistics [platform=%s] (%s)\n", STRINGIFY (PLATFORM), folder_stats_version);
   if (getenv ("--help")) {
      print_long_msg (g_help_msg);
      goto errorexit;
   }

   const char *out_fname = getenv ("--output-file") ? getenv ("--output-file") : "fstats.bin";
   const char *scan_path = getenv ("--scan-path") ? getenv ("--scan-path") : "./";

   printf ("Writing output to [%s]\n", out_fname);
   printf ("Scanning from folder [%s]\n", scan_path);

   if (!(amq_lib_init ())) {
      printf ("Failed to initialise application message queue library\n");
      goto errorexit;
   }

   if (!(amq_message_queue_create (Q_OUT))) {
      printf ("Failed to create output queue\n");
      goto errorexit;
   }

   if (!(amq_message_queue_create (Q_FOLDERS))) {
      printf ("Failed to create folder queue\n");
      goto errorexit;
   }

   ret = EXIT_SUCCESS;

errorexit:
   amq_lib_destroy ();
   return ret;

}








































