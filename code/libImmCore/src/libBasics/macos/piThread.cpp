//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <mach/mach.h>
#include "../piThread.h"

namespace ImmCore {

struct piThreadImpl
{
    pthread_t thread;
    piThreadDoworFunc func;
    void *data;
};

static void *iThreadEntry(void *user)
{
    piThreadImpl *me = (piThreadImpl*)user;
    if (me && me->func)
        me->func(me->data);
    return 0;
}

piThread piThread_Init(piThreadDoworFunc func, void *data)
{
    piThreadImpl *me = (piThreadImpl*)malloc(sizeof(piThreadImpl));
    if (!me)
        return 0;
    me->func = func;
    me->data = data;
    if (pthread_create(&me->thread, 0, iThreadEntry, me) != 0)
    {
        free(me);
        return 0;
    }
    return (piThread)me;
}

void piThread_End(piThread vme)
{
    piThreadImpl *me = (piThreadImpl*)vme;
    if (!me)
        return;
    pthread_join(me->thread, 0);
    free(me);
}

void *piThread_GetOSID(void)
{
    mach_port_t tid = pthread_mach_thread_np(pthread_self());
    return (void*)(uintptr_t)tid;
}

}
