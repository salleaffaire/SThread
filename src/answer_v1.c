
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <sys/time.h>

/* Just for debugging */
#include <stdio.h>

#define M 4 /* Number of writer threads - feel free to change them */
#define N 2 /* Number of reader threads - feel free to change them */

#define PROCESS_TIME         3     /* In seconds */
#define BUFFER_TIMEOUT_TIME  3     /* In seconds */
#define INPUT_BUFFER_SIZE    512   /* In bytes   */ 

#define DEBUG_ON             1
#define REENTRANT_SRC_SNK    1

/************************************************************************* 
   Structures for passing arguments to both 
   reader and writer threads.
*/

typedef struct
{
   unsigned int          thread_id;
   volatile unsigned int is_running;
} thread_arguments;

volatile thread_arguments thread_arguments_writer_array[M];
volatile thread_arguments thread_arguments_reader_array[N];

/************************************************************************
  List node
  This is a simple doubly linked list node storing a data buffer and
  a buffer size. 
*/
typedef struct list_node
{
   char             *buffer;
   int               buffer_size;
   struct list_node *next_node;
   struct list_node *previous_node;
} list_node;


/************************************************************************
   Synchronization structures - Protected List
    
   Main structure is a doubly linked list. Buffers are put at the front 
   and taken from the end.
    
   All accesses to the list are done through the 2 functions -
      void put_buffer_to_list(char * const buffer, int buffer_size)
      void get_buffer_from_list(char ** const buffer, int * const buffer_size)
   
   Starting from a 3-node list (N-1), (N-2) and (N-3)
   
           (N-1) -> (N-2) -> (N-3) -> NULL
   NULL <-       <-       <- 
           ^                 ^ 
           first             current           
           node              node
           
   put_buffer_to_list a new node (N) does the following:
   
           (N) ->  (N-1) -> (N-2) -> (N-3) -> NULL
   NULL <-     <-       <-       <- 
           ^                         ^ 
           first                     current           
           node                      node

   get_buffer_from_list returns the (N-3) node and leaves the list in this
   state:   
  
           (N-1) -> (N-2) -> NULL
   NULL <-       <-        
           ^         ^ 
           first     current           
           node      node
           
   The list is not responsible to allocate the memory to hold the data
   so all buffer pointer pass to it must be preallocated.
   
   IMPORTANT
   get_buffer_from_list() is a blocking call and will return only if
   there is data avaliable in the list. Multiple threads can wait on the
   list and will all be blcok if empty. When a buffer is put in, one of the
   reader threads will be woken up to process it. 
*/
struct 
{
   sem_t             count;         /* Count */
   pthread_mutex_t   mutex_mod;     /* Protection */
   list_node        *first_node;    /* Writer put data here*/
   list_node        *current_node;  /* Eeader gets data from here*/
} protected_list;

/************************************************************************
   Initialize the protected list
*/
int init_protected_list()
{
   int rval = 0; 
   
   if (pthread_mutex_init(&(protected_list.mutex_mod),   NULL))
   {
      rval = -1;
   }
   else
   {
      if (sem_init(&(protected_list.count), 0, 0)) /* Start empty */
      {
         pthread_mutex_destroy(&(protected_list.mutex_mod));
         rval = -1;
      }
      else 
      {         
         /* List is empty when both first_node and current_node
         are NULL
         */
         protected_list.first_node   = NULL;
         protected_list.current_node = NULL;
      }
   }   
   return rval;
}

/************************************************************************
   Destroy the protected list - clean up when not used anymore
*/
int destroy_protected_list()
{
   int rval = 0;
   
   rval = pthread_mutex_destroy(&protected_list.mutex_mod); 
   rval = sem_destroy(&protected_list.count);
   
   protected_list.first_node   = NULL;
   protected_list.current_node = NULL;
   
   return rval;
}

/************************************************************************ 
   Function to pseudo-process the data
*/ 
void process_data(char *buffer, int bufferSizeInBytes)
{
   #if (1 == DEBUG_ON)
   printf("PROCESSING BUFFER ADDRESS: %.8X\n", (int)buffer); fflush(stdout);
   #endif
}

/************************************************************************ 
   Function to get data from an external entity 
   for example: device driver 
*/ 
int get_external_data(char *buffer, int bufferSizeInBytes)
{
   int rval = (int) (INPUT_BUFFER_SIZE * (rand() / (double) RAND_MAX));
   
   #if (1 == DEBUG_ON)
   printf("FILLING BUFFER ADDRESS: %.8X with %d bytes\n", (int)buffer, rval); fflush(stdout);
   #endif
   
   return rval;
}

/************************************************************************ 
   Functions to protect source and sink functions. I wasn't sure if they were
   reentrant and could be called by different threads simultaneouly. If not then
   these 2 should be called instead. If internally they are safe to call then these
   2 functions were not necessary.   
*/
#if (0 == REENTRANT_SRC_SNK)
pthread_mutex_t process_data_mutex;
void protected_process_data(char * const buffer, int bufferSizeInBytes)
{
   assert(NULL != buffer); 
   assert(bufferSizeInBytes > 0);
   
   pthread_mutex_lock(&process_data_mutex);
   process_data(buffer, bufferSizeInBytes);
   pthread_mutex_unlock(&process_data_mutex);
}

pthread_mutex_t get_external_data_mutex;
int protected_get_external_data(char * const buffer, int bufferSizeInBytes)
{
   int rval = 0;
   
   assert(NULL != buffer); 
   assert(bufferSizeInBytes > 0);
   pthread_mutex_lock(&get_external_data_mutex);
   rval = get_external_data(buffer, bufferSizeInBytes);
   pthread_mutex_unlock(&get_external_data_mutex);
   return rval;
}
#endif

/************************************************************************
 * This function creates a new list node, attach a buffer to it and
   add it to the protected list.
   \param[in] buffer Data buffer to be added to the list
   \param[in] buffer_size Size in bytes 
 */
void put_buffer_to_list(char * const buffer, int buffer_size)
{
   assert(NULL != buffer); 
   assert(buffer_size > 0);
   
   /* Dynamically allocating a new node
      This could be done in a better way by having a preallocated list of nodes
      although with no restriction on the amount of memory it wouldn't help that much.
   */
   list_node *new_node = (list_node *)malloc(sizeof(list_node));

   new_node->buffer      = buffer;
   new_node->buffer_size = buffer_size;
   
   /* Lock the protected list before modifying */ 
   pthread_mutex_lock(&protected_list.mutex_mod);
 
   new_node->previous_node     = NULL;
   
   /* If empty */
   if ((protected_list.first_node == NULL) && (protected_list.current_node == NULL))
   {
      new_node->next_node         = NULL;
      protected_list.first_node   = new_node;
      protected_list.current_node = new_node;
   }
   else
   {    
      new_node->next_node                      = protected_list.first_node;
      protected_list.first_node->previous_node = new_node;
      protected_list.first_node                = new_node;
   }
   /* Unlock */
   pthread_mutex_unlock(&(protected_list.mutex_mod));
   
   /* Increase the buffer count value and unlock a waiting thread if any */
   sem_post(&(protected_list.count));
}

/************************************************************************
 * This function detaches a node from the protected list and returns the
   pointer to the buffer content.
   Finally, it deletes the node.
   \param[out] buffer Pointer to a data buffer that stores the data
   \param[out] buffer_size Size of the data buffer in bytes
   
   \retval 0 if a buffer was acquired
   \retval non-zero if no buffer could be acquired
 */
int get_buffer_from_list(char ** const buffer, int * const buffer_size)
{
   int             timeout = 0;
   list_node      *current_node = NULL;
   struct timespec ts;
   
   assert(NULL != buffer_size);
   assert(NULL != buffer);
   
   /* 3-second timeout */
   //if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
   //}
   //ts.tv_sec += BUFFER_TIMEOUT_TIME;

   /* Lock threads on the list if empty */
   timeout = sem_timedwait(&(protected_list.count), &ts);
   
   if (!timeout)
   {
      /* Lock the protected list before modifying */   
      pthread_mutex_lock(&(protected_list.mutex_mod));
      
      current_node = protected_list.current_node;
      
      /* If there is only one item in the list - there can't be 0 since the semaphore let 
         me in.
      */   
      if (protected_list.current_node == protected_list.first_node)
      {
         protected_list.current_node = NULL;
         protected_list.first_node   = NULL;
      }
      else
      {
         protected_list.current_node            = current_node->previous_node;
         protected_list.current_node->next_node = NULL;
      }
      /* Unlock */
      pthread_mutex_unlock(&(protected_list.mutex_mod));

      *buffer      = current_node->buffer;
      *buffer_size = current_node->buffer_size;
      
      /* Free the node */
      free(current_node);
   }

   return timeout;   
}

/************************************************************************
 * This thread is responsible for pulling data off of the shared data 
 * area and processing it using the process_data() API.
 */
void *reader_thread(void *arg)
{
   char *buffer = NULL;
   int   buffer_size = 0;
   
   thread_arguments *my_data;
   my_data = (thread_arguments *)arg;   
   
   while (my_data->is_running)
   {
      /* For debugging only - output the thread ID */
      #if (1 == DEBUG_ON)
      printf("READER: %d \n", my_data->thread_id); fflush(stdout);
      #endif
      
      /* Get data from the list - this function will block if no 
         data is available to be processed.  
      */
      if (!get_buffer_from_list(&buffer, &buffer_size))
      {
         /* Pseudo process the data */
         #if (0 == REENTRANT_SRC_SNK)
         protected_process_data(buffer, buffer_size);
         #else
         process_data(buffer, buffer_size);      
         #endif         
      }
      else
      {
         /* We're getting out - no more data*/
         my_data->is_running = 0;
      }
   }
 
   #if (1 == DEBUG_ON)
   printf("READER: %d is DONE \n", my_data->thread_id); fflush(stdout);
   #endif 
   
	return NULL;
}

/************************************************************************
 * This thread is responsible for pulling data from a device using
 * the get_external_data() API and placing it into a shared area
 * for later processing by one of the reader threads.
 */
 
void *writer_thread(void *arg)
{
   int rval = 0;

   char *buffer = NULL;
   int   buffer_size = 0;
   
   thread_arguments *my_data;
   my_data = (thread_arguments *)arg;

   while (my_data->is_running)
   {
      /* For debugging only - output the thread ID */
      #if (1 == DEBUG_ON)
      printf("WRITER: %d \n", my_data->thread_id); fflush(stdout);
      #endif
      
      /* Allocating the memory to place the data into
         - If get_external_data() doesn't completely fill buffer (write INPUT_BUFFER_SIZE bytes)
           some ammount of memory will be unused - and therefore wasted. An alternate
           solution would have been to allocate a big buffer and keep incrementing a pointer,
           therefore not wasting memory space. This was simpler and still efficient if 
           get_external_data fills INPUT_BUFFER_SIZE bytes most of the time.
         - Without restricting the ammount of data available to fill - this system will most 
           likely exhaust all the memory and crash. I think that the assignment says that 
           I can assume infinite amount of memory so ... I did.
      */
      buffer      = malloc(INPUT_BUFFER_SIZE);
      buffer_size = INPUT_BUFFER_SIZE;
      
      /* Call the external function to get more data 
         - It is assumed that this function is blocking. I didn't implement any machamism to wake up
            the writing threads when data becomes available and totally rely on the get_external_data
            function to block if nothing is available - hence suspending the writing threads.
      */
      #if (0 == REENTRANT_SRC_SNK)
      rval = protected_get_external_data(buffer, buffer_size);
      #else
      rval = get_external_data(buffer, buffer_size);
      #endif
      
      /* If OK */
      if (rval >= 0)
      {
         /* Add the data to the protected list to be processed */
         put_buffer_to_list(buffer, buffer_size);
      }
      else /* There was an error - free the allocated buffer */
      {
         free(buffer);
      }
	}
   
   #if (1 == DEBUG_ON)
   printf("WRITER: %d is DONE \n", my_data->thread_id); fflush(stdout);
   #endif
   
	return NULL;
}

int main(int argc, char **argv) 
{
	int rval = 0;
   
   int  i;
   
   /* Writing thread structure */
   pthread_t w_thread[M];
   /* Reading thread structure */
   pthread_t r_thread[N];
   
   /*Initialization of the protected list */
   rval = init_protected_list();
   
   /* If the list was properly created */
   if (!rval)
   {
      #if (0 == REENTRANT_SRC_SNK)
      rval = pthread_mutex_init(&process_data_mutex, NULL);
      if (!rval)
      {
         rval = pthread_mutex_init(&get_external_data_mutex, NULL);
      }
      else
      {
         pthread_mutex_destroy(&process_data_mutex);
      }
      #endif
      
      /* If both mutexes were created properly */
      if (!rval)
      {      
         /* Creating the reader threads */
         for(i = 0; i < N; i++) {
            thread_arguments_reader_array[i].is_running = 1;
            thread_arguments_reader_array[i].thread_id  = i;
            if ((rval = pthread_create(&(r_thread[i]), NULL, reader_thread, (void *)(&thread_arguments_reader_array[i]))) != 0)
            {
               break;
            }
         }

         /* Creating the writer threads */
         if (!rval)
         {
            for(i = 0; i < M; i++) {
               thread_arguments_writer_array[i].is_running = 1;   
               thread_arguments_writer_array[i].thread_id  = i;
               if ((rval = pthread_create(&(w_thread[i]), NULL, writer_thread, (void *)(&thread_arguments_writer_array[i]))) != 0)
               {
                  /* Something went wrong */
                  /* Delete all the created threads */
                  for (i=(i-1);i>=0;i--)
                  {
                     thread_arguments_writer_array[i].is_running = 0;
                  }
                  break;
               }
            }
         }
         
         /* All was OK */         
         if (!rval)
         {
            /* Wait a bit to simulate running a bit and then cleaning up- */
            // sleep(PROCESS_TIME);
            
            /* Terminating all writing threads */
            for (i=0;i<M;i++)
            {
               thread_arguments_writer_array[i].is_running = 0;  
            }
            
            /* Main block now waits for all writing threads to terminate before continuing
            */ 
            for (i=0;i<M;i++)
            {
               pthread_join(w_thread[i], NULL);
            }
            
            /* Reading threads will eventually timeout when no data is available - so we'll wait 
               for them to finish
            */
            for (i=0;i<N;i++)
            {   
               pthread_join(r_thread[i], NULL);   
            }
         }
         
         #if (0 == REENTRANT_SRC_SNK)
         pthread_mutex_destroy(&process_data_mutex);
         pthread_mutex_destroy(&get_external_data_mutex);  
         #endif
      }
      /* Some clean up */
      destroy_protected_list();   
   }
  
	return rval;
}
