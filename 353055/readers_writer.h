#pragma once

#include <pthread.h>
#include "data_structures.h"
#include "stdbool.h"

void initRWLock(RWLock* rw_lock){
    pthread_mutex_init(&rw_lock->mutex, NULL);
    pthread_cond_init(&rw_lock->read_cond, NULL);
    pthread_cond_init(&rw_lock->write_cond, NULL);
    rw_lock->readers = 0;
    rw_lock->writing = false;
}

void readLock(RWLock* rw_lock){
    pthread_mutex_lock(&rw_lock->mutex);
    
    while (rw_lock->writing) {
        pthread_cond_wait(&rw_lock->read_cond, &rw_lock->mutex);
    }

    rw_lock -> readers++;

    pthread_mutex_unlock(&rw_lock->mutex);
}

void readUnlock(RWLock* rw_lock){
    pthread_mutex_lock(&rw_lock->mutex);
    
    rw_lock -> readers--;
    if(rw_lock -> readers == 0)
        pthread_cond_signal(&rw_lock->write_cond);
    
    pthread_mutex_unlock(&rw_lock->mutex);
}

void writeLock(RWLock* rw_lock){
    pthread_mutex_lock(&rw_lock->mutex);

    while((rw_lock->writing) || (rw_lock->readers > 0))
        pthread_cond_wait(&rw_lock->write_cond, &rw_lock->mutex);
    
    rw_lock->writing = true;

    pthread_mutex_unlock(&rw_lock->mutex);
    
}


void writeUnlock(RWLock* rw_lock) {
    pthread_mutex_lock(&rw_lock->mutex);

    rw_lock->writing = false;

    pthread_cond_broadcast(&rw_lock->read_cond);
    pthread_cond_signal(&rw_lock->write_cond);

    pthread_mutex_unlock(&rw_lock->mutex);
}

void destroyRWLock(RWLock* rwlock) {
    pthread_mutex_destroy(&rwlock->mutex);
    pthread_cond_destroy(&rwlock->read_cond);
    pthread_cond_destroy(&rwlock->write_cond);
}