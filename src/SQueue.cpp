#include <time.h>

#include "SQueue.hpp"

#define SQUEUE_NULL 0
#define SQUEUE_TIMEOUT_TIME  3  // In seconds


SQueueNode::SQueueNode() :
   mNextNode(SQUEUE_NULL),
   mPreviousNode(SQUEUE_NULL)
{
}
 
SQueueNode::SQueueNode(SQueueNode *next, SQueueNode *previous, SQueueData &data) :
   mNextNode(next),
   mPreviousNode(previous)
{
   mData = data;
}

SQueueNode::~SQueueNode()
{
   // Nothing to do
}

SQueueData *SQueueNode::GetData()
{
   return &mData;
}

SQueue::SQueue()
{
   Init(0, 0);
}
SQueue::SQueue(unsigned int initCount, unsigned int bufferSize)
{
   Init(initCount, bufferSize);
}

SQueue::~SQueue()
{
   // If the list is not empty - empty is 
   // Delete all remaining nodes in the list
   Empty();
   
   pthread_mutex_destroy(&mMutexMod); 
   sem_destroy(&mCount);
}

void 
SQueue::Init(unsigned int initCount, unsigned int bufferSize)
{
   if (pthread_mutex_init(&mMutexMod, SQUEUE_NULL))
   {
      mState = SQueue::SQFailed;
   }
   else
   {
      if (sem_init(&mCount, 0, 0)) // Start empty
      {
         pthread_mutex_destroy(&mMutexMod);
         mState = SQueue::SQFailed;
      }
      else 
      {
         // List is empty when both first_node and current_node
         // are NULL      
         mFirstNode   = SQUEUE_NULL;
         mCurrentNode = SQUEUE_NULL;
         
         mState       = SQueue::SQOK;
      }
   }
   
   // Create the initial nodes
   if (SQueue::SQOK == mState)
   {
      unsigned int i;
      for (i=0;i<initCount;i++)
      {
         SQueueNode *newNode        = new SQueueNode;
         char       *newData        = new char [bufferSize];
         newNode->mData.mBuffer     = newData;
         newNode->mData.mBufferSize = bufferSize;
         
         PutBufferToQueue(newNode);
      }
   }
}

void
SQueue::Empty()
{
   
}

void  
SQueue::PutBufferToQueue(SQueueNode *node)
{     
   // Lock the protected list before modifying
   pthread_mutex_lock(&mMutexMod);
 
   node->mPreviousNode = NULL;
   
   // If empty
   if ((mFirstNode == NULL) && (mCurrentNode == NULL))
   {
      node->mNextNode     = NULL;
      mFirstNode          = node;
      mCurrentNode        = node;
   }
   else
   {    
      node->mNextNode            = mFirstNode;
      mFirstNode->mPreviousNode  = node;
      mFirstNode                 = node;
   }
   // Unlock
   pthread_mutex_unlock(&mMutexMod);
   
   // Increase the buffer count value and unlock a waiting thread if any
   sem_post(&mCount);
}

int
SQueue::GetBufferFromQueue(SQueueNode **node)
{
   int    timeout = 0;
      
#if 1
   // 3-second timeout
   struct timespec ts;

   if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
   }
   ts.tv_sec += SQUEUE_TIMEOUT_TIME;

   // Lock threads on the list if empty
   timeout = sem_timedwait(&mCount, &ts);
#endif

   
   if (!timeout)
   {
      // Lock the protected list before modifying   
      pthread_mutex_lock(&mMutexMod);
      
      *node = mCurrentNode;
      
      // If there is only one item in the list - there can't be 0 since the semaphore let 
      // me in.
         
      if (mCurrentNode == mFirstNode)
      {
         mCurrentNode = NULL;
         mFirstNode   = NULL;
      }
      else
      {
         mCurrentNode            = mCurrentNode->mPreviousNode;
         mCurrentNode->mNextNode = NULL;
      }
      // Unlock
      pthread_mutex_unlock(&mMutexMod);
   }

   return timeout;   
}

