
#ifndef H_AMQ
#define H_AMQ

#include <stdbool.h>
#include <stdlib.h>

// A utility macro to make logging easier.
#define AMQ_PRINT(...)           do {\
   printf ("[%s:%i] ", __FILE__, __LINE__);\
   printf (__VA_ARGS__);\
} while (0)

// Post all errors to this queue.
#define AMQ_QUEUE_ERROR    ("AMQ:ERROR")

enum amq_worker_result_t {
   amq_worker_result_CONTINUE,
   amq_worker_result_STOP,
};

typedef enum amq_worker_result_t (amq_worker_t) (void *);

typedef struct amq_t amq_t;

#ifdef __cplusplus
extern "C" {
#endif

   // Initialise the library and destroy the library; amq_lib_init() must be the first
   // amq function to be called and must not be called from multiple threads as it is
   // not thread-safe. Before the application ends the application must call
   // amq_lib_destroy(). amq_lib_destroy() must be the last amq function call before
   // the application ends.
   //
   // amq_lib_init () and amq_lib_destroy() must not be called more than once in a
   // single application. amq_destroy() must not be called while there are still worker
   // threads running; give all workers enough time to end gracefully before calling
   // amq_lib_destroy().
   //
   bool amq_lib_init (void);
   void amq_lib_destroy (void);

   // Once the calling application has called amq_lib_init(), the following functions
   // are available.

   void amq_post (const char *queue_name, void *buf, size_t buf_len);


#ifdef __cplusplus
};
#endif


#endif
