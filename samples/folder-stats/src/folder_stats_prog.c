
#include <stdio.h>
#include <stdlib.h>

#define STRINGIFY(x)       #x

int main (int arhc, char **argv)
{
   printf ("Folder statistics for %s (%s)\n", STRINGIFY (PLATFORM), folder_stats_version);
   return 0;
}
