
#ifndef H_AMQ
#define H_AMQ

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <pthread.h>

// A utility macro to make logging easier.
#define AMQ_PRINT(...)           do {\
   printf ("[%s:%i] ", __FILE__, __LINE__);\
   printf (__VA_ARGS__);\
} while (0)

// Signals that can be sent to workers. This is not the same as asynchronous signals from
// `man signal`.
#define AMQ_SIGNAL_TERMINATE        (1 << 0)
#define AMQ_SIGNAL_SUSPEND          (1 << 1)
#define AMQ_SIGNAL_RFU2             (1 << 2)
#define AMQ_SIGNAL_RFU3             (1 << 3)
#define AMQ_SIGNAL_RFU4             (1 << 4)
#define AMQ_SIGNAL_RFU5             (1 << 5)
#define AMQ_SIGNAL_RFU6             (1 << 6)
#define AMQ_SIGNAL_RFU7             (1 << 7)
#define AMQ_SIGNAL_RFU8             (1 << 8)
#define AMQ_SIGNAL_RFU9             (1 << 9)
#define AMQ_SIGNAL_RFU10            (1 << 10)
#define AMQ_SIGNAL_RFU11            (1 << 11)
#define AMQ_SIGNAL_RFU12            (1 << 12)
#define AMQ_SIGNAL_RFU13            (1 << 13)
#define AMQ_SIGNAL_RFU14            (1 << 14)
#define AMQ_SIGNAL_RFU15            (1 << 15)

/* ************************************************
 * All errors will be posted to AMQ_QUEUE_ERROR queue that is created on
 * library initialisation. Errors are posted as a (struct amq_error_t *).
 *
 * The caller must have a consumer worker retrieve the error off the queue.
 * If errors are not retrieved the queue will gradually fill up until it
 * consumes all memory.
 *
 * More than one consumer can listen on the AMQ_QUEUE_ERROR queue. The consumer
 * must free the error using amq_error_del(). Any worker may post to this queue
 * using the wrapper function amq_error_new().
 */
#define AMQ_ERROR_POST(code,...)    do {\
   struct amq_error_t *errobj = amq_error_new (__FILE__, __LINE__, code, __VA_ARGS__);\
   if (errobj) {\
      amq_post (AMQ_QUEUE_ERROR, errobj, 0);\
   } else {\
      AMQ_PRINT ("Failed to create error object\n" __VA_ARGS__);\
   }\
} while (0)

#define AMQ_QUEUE_ERROR    ("AMQ:ERROR")
struct amq_error_t {
   int   code;
   char *message;
};
#ifdef __cplusplus
extern "C" {
#endif
struct amq_error_t *amq_error_new (const char *file, int line, int code, ...);
void amq_error_del (struct amq_error_t *errobj);
#ifdef __cplusplus
};
#endif

struct amq_stats_t {
   size_t   count;
   float    min;
   float    max;
   float    average;
   float    deviation;
};

struct amq_worker_t {
   pthread_t             worker_id;
   char                 *worker_name;
   void                 *worker_cdata;
   uint8_t               worker_type;
   struct amq_stats_t    stats;
};

enum amq_worker_result_t {
   amq_worker_result_CONTINUE,
   amq_worker_result_STOP,
};

typedef enum amq_worker_result_t (amq_producer_func_t) (const struct amq_worker_t *self,
                                                        void *cdata);
typedef enum amq_worker_result_t (amq_consumer_func_t) (const struct amq_worker_t *self,
                                                        void *mesg, size_t mesg_len,
                                                        void *cdata);

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

   // Create a new message queue that workers can post to or consume. Returns true on
   // success and false on error. Error messages will be posted to the AMQ_QUEUE_ERROR
   // message queue.
   bool amq_message_queue_create (const char *name);

   // Post a message to a message queue
   void amq_post (const char *queue_name, void *buf, size_t buf_len);

   // Returns the number of elements in the specified queue.
   size_t amq_count (const char *queue_name);

   // Create a new producer thread, with an optional name. Name can be specified as NULL
   // or an empty string. The cdata will be passed unchanged to the worker.
   //
   // Returns true if the producer was created, false otherwise. All errors are posted
   // to the AMQ_QUEUE_ERROR message queue.
   bool amq_producer_create (const char *worker_name,
                             amq_producer_func_t *worker_func, void *cdata);

   // Create a new consumer thread, with an optional name. Name can be specified as NULL
   // or an empty string. The cdata will be passed unchanged to the worker. The worker
   // will be passed messages from the queue supply_queue.
   //
   // Returns true if the consumer  was created, false otherwise. All errors are posted
   // to the AMQ_QUEUE_ERROR message queue.
   bool amq_consumer_create (const char *supply_queue_name,
                             const char *worker_name,
                             amq_consumer_func_t *worker_func, void *cdata);

   // Set and clear specific signals for a worker. See the #defines for values that
   // can be bitwise-ORed into sigmask.
   void amq_worker_sigset (const char *worker_name, uint64_t sigmask);
   void amq_worker_sigclr (const char *worker_name, uint64_t sigmask);

   // Get the current sigmask for a worker.
   uint64_t amq_worker_sigget (const char *worker_name);

   // Wait for a worker to finish: this function will only return when a worker returns!
   // If a worker never returns, then waiting for that worker will wait indefinitely.
   void amq_worker_wait (const char *worker_name);

#ifdef __cplusplus
};
#endif


#endif
