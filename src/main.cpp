#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

/* Just for debugging */
#include <stdio.h>

#include "SQueue.hpp"

#define M 4 /* Number of writer threads - feel free to change them */
#define N 2 /* Number of reader threads - feel free to change them */

#define DEBUG_ON             1
#define NUM_OF_INPUT_BUFFERS 0
#define INPUT_BUFFER_SIZE    512
#define PROCESS_TIME         5

// Empty queue starts with NUM_OF_INPUT_BUFFERS empty buffers
SQueue gEmptyBufferQueue(NUM_OF_INPUT_BUFFERS, INPUT_BUFFER_SIZE);

// Full queue starts empty
SQueue gFullBufferQueue(0, INPUT_BUFFER_SIZE);

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
   Function to pseudo-process the data
*/ 
void process_data(char *buffer, int bufferSizeInBytes)
{
   #if (1 == DEBUG_ON)
   printf("PROCESSING BUFFER ADDRESS: %.8X\n", (int)buffer); 
   fflush(stdout);
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
   printf("FILLING BUFFER ADDRESS: %.8X with %d bytes\n", (int)buffer, rval); 
   fflush(stdout);
   #endif
   
   return rval;
}

/************************************************************************
 * This thread is responsible for pulling data off of the shared data 
 * area and processing it using the process_data() API.
 */
void *reader_thread(void *arg)
{
   //char *buffer = NULL;
   //int   buffer_size = 0;
   
   thread_arguments *my_data;
   my_data = (thread_arguments *)arg;   
   
   SQueueNode *bufferNode;
   
   while (my_data->is_running)
   {
      // For debugging only - output the thread ID
      #if (1 == DEBUG_ON)
      printf("READER: %d \n", my_data->thread_id); 
      fflush(stdout);
      #endif
      
      // Get data from the list - this function will block if no 
      // data is available to be processed.  
      if (0 == gFullBufferQueue.GetBufferFromQueue(&bufferNode))
      {
         // Pseudo process the data
         process_data(bufferNode->GetData()->mBuffer, 
                      bufferNode->GetData()->mBufferSize);
         
         // Add data to the empty list
         gEmptyBufferQueue.PutBufferToQueue(bufferNode);
      }
      else
      {
         // We're getting out - no more data
         my_data->is_running = 0;
      }
   }
 
   #if (1 == DEBUG_ON)
   printf("READER: %d is DONE \n", my_data->thread_id); 
   fflush(stdout);
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

   //char *buffer = NULL;
   //int   buffer_size = 0;
   
   thread_arguments *my_data;
   my_data = (thread_arguments *)arg;

   SQueueNode *bufferNode;
   
   while (my_data->is_running)
   {
      // For debugging only - output the thread ID
      #if (1 == DEBUG_ON)
      printf("WRITER: %d \n", my_data->thread_id); 
      fflush(stdout);
      #endif
      
      if (0 == gEmptyBufferQueue.GetBufferFromQueue(&bufferNode))
      {
         // Call the external function to get more data 
         //   - It is assumed that this function is blocking. I didn't implement any machamism to wake up
         //      the writing threads when data becomes available and totally rely on the get_external_data
         //      function to block if nothing is available - hence suspending the writing threads.
         //
         rval = get_external_data(bufferNode->GetData()->mBuffer, 
                                  bufferNode->GetData()->mBufferSize);
         
         // If OK
         if (rval >= 0)
         {
            // Add data to the full list
            gFullBufferQueue.PutBufferToQueue(bufferNode);
         }
         else // There was an error - Put the buffer back in the empty queue
         {
            gEmptyBufferQueue.PutBufferToQueue(bufferNode);
         }
      }
	}
   
   #if (1 == DEBUG_ON)
   printf("WRITER: %d is DONE \n", my_data->thread_id); 
   fflush(stdout);
   #endif
   
	return NULL;
}


int main(int argc, char *argv[])
{
   int rval = 0;
   int i;
   
   // Writing thread structure
   pthread_t w_thread[M];
   // Reading thread structure
   pthread_t r_thread[N];
   
   // Creating the reader threads
   for(i = 0; i < N; i++) 
   {
      thread_arguments_reader_array[i].is_running = 1;
      thread_arguments_reader_array[i].thread_id  = i;
      if ((rval = pthread_create(&(r_thread[i]), NULL, reader_thread, (void *)(&thread_arguments_reader_array[i]))) != 0)
      {
         break;
      }
   }
   
   for(i = 0; i < M; i++) {
      thread_arguments_writer_array[i].is_running = 1;   
      thread_arguments_writer_array[i].thread_id  = i;
      if ((rval = pthread_create(&(w_thread[i]), NULL, writer_thread, (void *)(&thread_arguments_writer_array[i]))) != 0)
      {
         // Something went wrong
         // Delete all the created threads
         // The readers will time out and exit
         for (i=(i-1);i>=0;i--)
         {
            thread_arguments_writer_array[i].is_running = 0;
         }
         break;
      }
   }

   // Wait a bit - running a bit and then cleaning up
   sleep(PROCESS_TIME);
   
   // Terminating all writing threads
   for (i=0;i<M;i++)
   {
      thread_arguments_writer_array[i].is_running = 0;  
   }
   
   // Main block now waits for all writing threads to terminate before continuing
   //
   for (i=0;i<M;i++)
   {
      pthread_join(w_thread[i], NULL);
   }
   
   // Reading threads will eventually timeout when no data is available - so we'll wait 
   // for them to finish
   for (i=0;i<N;i++)
   {   
      pthread_join(r_thread[i], NULL);   
   }

   
   return rval;
}
