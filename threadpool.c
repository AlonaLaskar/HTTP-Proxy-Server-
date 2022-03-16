#include "threadpool.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>


threadpool* create_threadpool(int num_threads_in_pool){//This function produces a threadpool

    if(num_threads_in_pool<0 || num_threads_in_pool>MAXT_IN_POOL){//chack if num of thread is legal
        return NULL;
    }
    else {
        threadpool *thread_pool = (threadpool *) malloc(sizeof(threadpool));
        if (thread_pool == NULL) {
            perror("error: allocation failed\n");
            exit(EXIT_FAILURE);
        }
        bzero(thread_pool,sizeof (threadpool));//Resets the threadpol
        thread_pool->num_threads = num_threads_in_pool;
        thread_pool->qsize = 0;
        thread_pool->qhead = NULL;
        thread_pool->qtail = NULL;
        int cheakInit1=pthread_mutex_init(&thread_pool->qlock, NULL);//Entering a critical section so we will lock this part
        if (cheakInit1 != 0) {
            perror("error: init mutex failed>\n");
            exit(EXIT_FAILURE);
        }
        int cheakInit2=pthread_cond_init(&thread_pool->q_empty, NULL);
        if (cheakInit2 != 0) {
            perror("error: init mutex failed>\n");
            exit(EXIT_FAILURE);
        }
        int cheakInit3=pthread_cond_init(&thread_pool->q_not_empty, NULL);
        if (cheakInit3 != 0) {
            perror("error: init mutex failed>\n");
            exit(EXIT_FAILURE);
        }
        thread_pool->dont_accept = 0;//flag up Tasks can open to the line
        thread_pool->shutdown = 0;
        thread_pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_threads_in_pool);
        if(thread_pool->threads==NULL){
            perror("error: allocation failed\n");
            exit(EXIT_FAILURE);
        }
        int check;
        for (int i = 0; i < thread_pool->num_threads; i++) {//create threads
            check= pthread_create(&thread_pool->threads[i], NULL, do_work, (void *) thread_pool);
            if (check != 0) {
                perror("error: pthread create failed\n");
                exit(EXIT_FAILURE);
            }
        }
        return thread_pool;
    }

}
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    work_t* work=(work_t*) malloc(sizeof (work_t));
    if(work==NULL){
        perror("error: allocation failed\n");
        exit(EXIT_FAILURE);
    }
    work->arg=arg;
    work->next=NULL;
    work->routine=dispatch_to_here;
    if(from_me->dont_accept==1){
        free(work);
        return;
    }

    int chekLock=pthread_mutex_lock(&from_me->qlock);
    if (chekLock != 0) {
        perror("error: mutex lock if failed\n");
        exit(EXIT_FAILURE);
    }
    if(from_me->qsize<1){//the queue is empty
        from_me->qhead=work;
        from_me->qtail=work;
    }
    else{//if the queue is not empty
        from_me->qtail->next=work;
        from_me->qtail=from_me->qtail->next;

    }
    from_me->qsize++;
    int checkSignal=pthread_cond_signal(&from_me->q_not_empty);
    if (checkSignal != 0) {
        perror("error: pthread cond signal is failed\n");
        exit(EXIT_FAILURE);
    }
    int checkUnLock=pthread_mutex_unlock(&from_me->qlock);
    if (checkUnLock!= 0) {
        perror("error: mutex unlock if failed\n");
        exit(EXIT_FAILURE);
    }

}

void* do_work(void* p){//This function activates the threads they perform what is happening within this function
    threadpool* newThreadPool=(threadpool*)p;
    while (1) {
      int checkLock=pthread_mutex_lock(&newThreadPool->qlock);
        if (checkLock != 0) {
            perror("error: mutex lock if failed\n");
            exit(EXIT_FAILURE);
        }
        if (newThreadPool->shutdown == 1) {
            int checkUnLock=pthread_mutex_unlock(&newThreadPool->qlock);
            if (checkUnLock != 0) {
                perror("error: mutex unlock if failed\n");
                exit(EXIT_FAILURE);
            }

            return NULL;
        }
        if (newThreadPool->qsize == 0) {
            if (newThreadPool->shutdown == 1) {
                int checkUnLock1=pthread_mutex_unlock(&newThreadPool->qlock);
                if (checkUnLock1 != 0) {
                    perror("error: mutex unlock if failed\n");
                    exit(EXIT_FAILURE);
                }

                return NULL;
            }

            int checkWait=pthread_cond_wait(&newThreadPool->q_not_empty, &newThreadPool->qlock);
            if (checkWait != 0) {
                perror("error: pthread cond wait failed\n");
                exit(EXIT_FAILURE);
            }

            if (newThreadPool->shutdown==1) {
                int checkUnLock2=pthread_mutex_unlock(&newThreadPool->qlock);
                if (checkUnLock2 != 0) {
                    perror("error: mutex unlock if failed\n");
                    exit(EXIT_FAILURE);
                }
                    return NULL;
                }
              int checkUnLock3=pthread_mutex_unlock(&newThreadPool->qlock);
               if (checkUnLock3 != 0) {
                   perror("error: mutex unlock if failed\n");
                     exit(EXIT_FAILURE);
                }

                continue;
        }
        else if (newThreadPool->qsize > 0) {//there is more than one work in queue
            work_t *work3 = newThreadPool->qhead;
            newThreadPool->qhead = newThreadPool->qhead->next;
            newThreadPool->qsize--;
            if(newThreadPool->qsize==0)
                pthread_cond_signal(&newThreadPool->q_empty);
           int checkUnLock4= pthread_mutex_unlock(&newThreadPool->qlock);
            if (checkUnLock4 != 0) {
                perror("error: mutex unlock if failed\n");
                exit(EXIT_FAILURE);
            }
            work3->routine(work3->arg);
            free(work3);
            work3 = NULL;

        }else{
            if(pthread_mutex_unlock(&newThreadPool->qlock)!=0){
               int checkUnLock5=pthread_mutex_unlock(&newThreadPool->qlock);
                if (checkUnLock5 != 0) {
                    perror("error: mutex unlock if failed\n");
                    exit(EXIT_FAILURE);
                }
                return NULL;
            }
        }
    }

}
void destroy_threadpool(threadpool* destroyme){//This function destroys the thradpool and releases assignments
   int checkUnLock= pthread_mutex_lock(&destroyme->qlock);
    if (checkUnLock != 0) {
        perror("error: mutex unlock if failed\n");
        exit(EXIT_FAILURE);
    }
    destroyme->dont_accept=1;//flag up

   if(destroyme->qsize>0)
       pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock);
    destroyme->shutdown=1;
    int checkUnLock1=pthread_mutex_unlock(&destroyme->qlock);
    if (checkUnLock1 != 0) {
        perror("error: mutex unlock if failed\n");
        exit(EXIT_FAILURE);
    }

    int checkBroad=pthread_cond_broadcast(&destroyme->q_not_empty);
    if (checkBroad != 0) {
        perror("error: pthread cond broadcast is failed\n");
        exit(EXIT_FAILURE);
    }
    int checJoin;
    for (int i = 0; i < destroyme->num_threads; i++) {
         checJoin=pthread_join(destroyme->threads[i],NULL);
        if (checJoin != 0) {
            perror("error: pthread join is failed\n");
            exit(EXIT_FAILURE);
        }
    }

    int checkDestroy=pthread_cond_destroy(&destroyme->q_not_empty);
    if (checkDestroy != 0) {
        perror("error: pthread cond destroy is failed\n");
        exit(EXIT_FAILURE);
    }
    int checkDestroy1=pthread_cond_destroy(&destroyme->q_empty);
    if (checkDestroy1 != 0) {
        perror("error: pthread cond destroy is failed\n");
        exit(EXIT_FAILURE);
    }
    int checkDestroyMutex=pthread_mutex_destroy(&destroyme->qlock);
    if (checkDestroyMutex != 0) {
        perror("error: pthread mutex destroy is failed\n");
        exit(EXIT_FAILURE);
    }
    free(destroyme->threads);
    free(destroyme);

}

