
#ifndef H_AMQ_WGROUP
#define H_AMQ_WGROUP

#include <stdbool.h>
#include <stdint.h>

typedef struct amq_wgroup_t amq_wgroup_t;

#ifdef __cplusplus
extern "C" {
#endif

   amq_wgroup_t *amq_wgroup_new (const char *name);
   void amq_wgroup_del (amq_wgroup_t *group);

   const char *amq_wgroup_name (amq_wgroup_t *group);

   bool amq_wgroup_add_worker (amq_wgroup_t *group, const char *worker_name);
   bool amq_wgroup_remove_worker (amq_wgroup_t *group, const char *worker_name);

   void amq_wgroup_sigset (amq_wgroup_t *group, uint64_t sigmask);
   void amq_wgroup_sigclr (amq_wgroup_t *group, uint64_t sigmask);
   void amq_wgroup_wait (amq_wgroup_t *group);

#ifdef __cplusplus
};
#endif

#endif
