
#ifndef H_AMQ_CONTAINER
#define H_AMQ_CONTAINER

#include <stdbool.h>

typedef struct amq_container_t amq_container_t;

#ifdef __cplusplus
extern "C" {
#endif

   amq_container_t *amq_container_new (void);
   void amq_container_del (amq_container_t *container, void (*item_del_fptr) (void *));

   bool amq_container_add (amq_container_t *container,
                           const char *name, void *element);

   void *amq_container_remove (amq_container_t *container,
                               const char *name);

   void *amq_container_find (amq_container_t *container, const char *name);

   size_t amq_container_names (amq_container_t *container, char ***names);

#ifdef __cplusplus
};
#endif


#endif
