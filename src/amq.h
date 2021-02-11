
#ifndef H_AMQ
#define H_AMQ

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

// A utility macro to make logging easier.
#define AMQ_PRINT(...)           do {\
   printf ("[%s:%i] ", __FILE__, __LINE__);\
   printf (__VA_ARGS__);\
} while (0)

// Signals that can be sent to workers. This is not the same as asynchronous signals from
// `man signal`.
#define AMQ_SIGNAL_TERMINATE        (1 << 0)
#define AMQ_SIGNAL_RFU1             (1 << 1)
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

// Post all errors to this queue.
#define AMQ_QUEUE_ERROR    ("AMQ:ERROR")

enum amq_worker_result_t {
   amq_worker_result_CONTINUE,
   amq_worker_result_STOP,
};

typedef enum amq_worker_result_t (amq_producer_func_t) (void *cdata);
typedef enum amq_worker_result_t (amq_consumer_func_t) (void *mesg, size_t mesg_len,
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

   // Send a signal (see #defines at the top of this file) to a worker.
   void amq_worker_signal (const char *worker_name, uint64_t signals);

   // Wait for a worker to finish: this function will only return when a worker returns!
   // If a worker never returns, then waiting for that worker will wait indefinitely.
   void amq_worker_wait (const char *worker_name);

#ifdef __cplusplus
};
#endif


#endif
