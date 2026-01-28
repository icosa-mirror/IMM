//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <pthread.h>
#include <stdlib.h>
#include "../piMutex.h"

namespace ImmCore {

bool piMutex::Init(void)
{
    pthread_mutex_t *mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (!mutex)
        return false;
    if (pthread_mutex_init(mutex, 0) != 0)
    {
        free(mutex);
        return false;
    }
    p = (void*)mutex;
    return true;
}

void piMutex::End(void)
{
    pthread_mutex_t *mutex = (pthread_mutex_t*)p;
    if (!mutex)
        return;
    pthread_mutex_destroy(mutex);
    free(mutex);
    p = 0;
}

void piMutex::Lock(void)
{
    pthread_mutex_t *mutex = (pthread_mutex_t*)p;
    if (!mutex)
        return;
    pthread_mutex_lock(mutex);
}

void piMutex::UnLock(void)
{
    pthread_mutex_t *mutex = (pthread_mutex_t*)p;
    if (!mutex)
        return;
    pthread_mutex_unlock(mutex);
}

}
