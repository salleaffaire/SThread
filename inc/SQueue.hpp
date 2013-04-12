#ifndef SQUEUE_HPP___

#include <pthread.h>   // POSIX Threads - Mutex
#include <semaphore.h> // POSIX Semaphore

struct SQueueData
{
   char         *mBuffer;
   unsigned int  mBufferSize;
};

class SQueueNode
{
public:
   SQueueNode();
   SQueueNode(SQueueNode *next, SQueueNode *previous, SQueueData &data);

   virtual ~SQueueNode();
   
   SQueueData *GetData();
   
private:
   SQueueData   mData;
   SQueueNode  *mNextNode;
   SQueueNode  *mPreviousNode;
   
   friend class SQueue;
};

class SQueue
{
public:
   SQueue();
   SQueue(unsigned int initCount, unsigned int bufferSize);
   virtual ~SQueue();
   
   void  PutBufferToQueue(SQueueNode *data);
   int   GetBufferFromQueue(SQueueNode **data);   
   bool  IsEmpty();
   
   enum SQueueState
   {
      SQOK     = 0,
      SQFailed = 0xFFFF | 1
   };
   
private:
   sem_t             mCount;        // Count
   pthread_mutex_t   mMutexMod;     // Protection
   SQueueNode       *mFirstNode;    // Writer put data here
   SQueueNode       *mCurrentNode;  // Eeader gets data from here 

   SQueueState       mState;
   
   bool              mIsInit;       // Can only be initialized once
   
   void              Init(unsigned int initCount, unsigned int bufferSize);
   void              Empty();
};

#endif
