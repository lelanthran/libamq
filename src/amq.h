
#ifndef H_AMQ
#define H_AMQ

#define AMQ_PRINT(...)           do {\
   printf ("[%s:%i] ", __FILE__, __LINE__);\
   printf (__VA_ARGS__);\
} while (0)

typedef struct amq_t amq_t;

#ifdef __cplusplus
extern "C" {
#endif

   amq_t *amq_new (void);
   void amq_del (amq_t *amq);


#ifdef __cplusplus
};
#endif


#endif
