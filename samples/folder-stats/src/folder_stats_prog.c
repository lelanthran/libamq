
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static char *lstrdup (const char *src)
{
   char *ret = malloc (strlen (src) + 1);
   if (ret)
      strcpy (ret, src);
   return ret;
}

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

/* ********************************************************************** */
static volatile sig_atomic_t g_endflag = 0;

static void sigh (int n)
{
   if (n==SIGINT)
      g_endflag = 1;
}

enum amq_worker_result_t errhandler (const struct amq_worker_t *self,
                                     void *mesg, size_t mesg_len,
                                     void *cdata)
{
   struct amq_error_t *error = mesg;
   (void)self;
   (void)mesg_len;
   (void)cdata;

   fprintf (stderr, "Error %i: [%s]\n", error->code, error->message);
   if (error->code == INT_MAX) {
      g_endflag = 1;
   }
   amq_error_del (error);
   return amq_worker_result_CONTINUE;
}

enum amq_worker_result_t output_writer (const struct amq_worker_t *self,
                                        void *mesg, size_t mesg_len,
                                        void *cdata)
{
   static const char *paddles = "-\\|/";
   static const size_t npaddles = 4;
   static int paddles_index = 0;

   struct folder_stats_entry_t *fentry = mesg;
   FILE *fout = cdata;

   (void)self;
   (void)mesg_len;

   // printf ("\r%s                                  ", folder_stats_entry_name (fentry));
   printf ("\r   %c", paddles[paddles_index++ % npaddles]);
   folder_stats_entry_write (fentry, fout);
   folder_stats_entry_del (fentry);

   return amq_worker_result_CONTINUE;
}

enum amq_worker_result_t wfpath_open (const struct amq_worker_t *self,
                                      void *mesg, size_t mesg_len,
                                      void *cdata)
{
   char *pathname = mesg;
   (void)self;
   (void)mesg_len;
   (void)cdata;

   if (!pathname) {
      AMQ_ERROR_POST (-2, "NULL pathname received, ignoring\n");
      return amq_worker_result_CONTINUE;
   }

   folder_stats_entry_t *entry = folder_stats_entry_new (pathname);
   if (!entry) {
      free (pathname);
      return amq_worker_result_CONTINUE;
   }

   amq_post (Q_OUTPUT, entry, 0);
   free (pathname);

   return amq_worker_result_CONTINUE;
}

/* ********************************************************************** */

int main (int argc, char **argv)
{
   int ret = EXIT_FAILURE;
   FILE *outfile = NULL;

   process_cline (argc, argv);
   printf ("Folder statistics [platform=%s] (%s)\n", STRINGIFY (PLATFORM), folder_stats_version);
   if (getenv ("--help")) {
      print_long_msg (g_help_msg);
      goto errorexit;
   }

   const char *out_fname = getenv ("--output-file") ? getenv ("--output-file") : "fstats.csv";
   const char *scan_path = getenv ("--scan-path") ? getenv ("--scan-path") : "./";

   printf ("Writing output to [%s]\n", out_fname);
   printf ("Scanning from folder [%s]\n", scan_path);

   if (!(amq_lib_init ())) {
      printf ("Failed to initialise application message queue library\n");
      goto errorexit;
   }

   // This could run a long time on large filesystems, the user must be able to
   // abort at any time.
   signal (SIGINT, sigh);

   // The output file, if specified, otherwise we use the default .csv filename.
   // If the user specified --stdout we ignore the specified filename and use
   // NULL which causes the worker to send the output to stdout.
   if (out_fname && !(getenv ("--stdout"))) {
      if (!(outfile = fopen (out_fname, "wt"))) {
         printf ("Failed to open [%s] for writing: %m\n", out_fname);
         goto errorexit;
      }
   }
   fprintf (outfile, "%s,%s,%s,%s,%s\n", "Name",
                                         "Extension",
                                         "Size",
                                         "Type",
                                         "Modified");

   // A queue just to write the output to a file
   if (!(amq_message_queue_create (Q_OUTPUT))) {
      printf ("Failed to create output queue\n");
      goto errorexit;
   }

   // A queue to interrogate a single pathname that creates the fentry object
   if (!(amq_message_queue_create (Q_PATHNAMES))) {
      printf ("Failed to create folder queue\n");
      goto errorexit;
   }

   // A consumer of the error queue
   if (!(amq_consumer_create (AMQ_QUEUE_ERROR, W_ERRHANDLER, errhandler, NULL))) {
      printf ("Failed to create worker to handle errors\n");
      goto errorexit;
   }

   // A consumer of the results-output queue
   if (!(amq_consumer_create (Q_OUTPUT, W_OUTPUT, output_writer, outfile))) {
      printf ("Failed to create worker to handle errors\n");
      goto errorexit;
   }

   // Multiple consumers for the path interrogation queue
   for (size_t i=0; i<6; i++) {
      char snum[25];
      snprintf (snum, sizeof snum, "%s-%zu", W_PATHNAMES, i);
      if (!(amq_consumer_create (Q_PATHNAMES, snum, wfpath_open, NULL))) {
         printf ("Failed to create worker to handle errors\n");
         goto errorexit;
      }
   }

   amq_post (Q_PATHNAMES, lstrdup (scan_path), 0);

   AMQ_ERROR_POST (0, "Successfully initialised");

   size_t retries = 0;
   while (g_endflag == 0) {
      if ((amq_count (Q_PATHNAMES))==0) {
         retries++;
      } else {
         retries = 0;
      }
      if (retries > 2) {
         printf ("\nNo new folders specified in the two seconds, ending...\n");
         g_endflag = 1;
      } else {
         sleep (1);
      }
   }

   ret = EXIT_SUCCESS;

errorexit:

   if (outfile)
      fclose (outfile);

   amq_worker_sigset (W_ERRHANDLER, AMQ_SIGNAL_TERMINATE);
   amq_worker_wait (W_ERRHANDLER);
   amq_lib_destroy ();
   return ret;

}








































