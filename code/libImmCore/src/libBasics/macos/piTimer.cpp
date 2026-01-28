//
// Branched off piLibs (Copyright © 2015 Inigo Quilez, The MIT License), in 2015. See THIRD_PARTY_LICENSES.txt
//
#include <mach/mach_time.h>
#include <unistd.h>
#include <stdlib.h>
#include "../piTimer.h"

namespace ImmCore {

struct iTimer
{
    mach_timebase_info_data_t timebase;
    uint64_t start;
};

piTimer::piTimer()
{
    mImplementation = 0;
}

piTimer::~piTimer()
{
}

piTimer::Alarm piTimer::CreateAlarm(DoAlarmnCallback func, void *data, int deltamiliseconds)
{
    (void)func;
    (void)data;
    (void)deltamiliseconds;
    return 0;
}

void piTimer::DestroyAlarm(Alarm vme)
{
    (void)vme;
}

bool piTimer::Init(void)
{
    iTimer *me = (iTimer*)malloc(sizeof(iTimer));
    if (!me)
        return false;
    if (mach_timebase_info(&me->timebase) != KERN_SUCCESS)
    {
        free(me);
        return false;
    }
    me->start = mach_absolute_time();
    mImplementation = me;
    return true;
}

void piTimer::End(void)
{
    iTimer *me = (iTimer*)mImplementation;
    if (!me)
        return;
    free(me);
    mImplementation = 0;
}

static uint64_t iNowNanos(const iTimer *me)
{
    uint64_t now = mach_absolute_time();
    uint64_t diff = now - me->start;
    return (diff * me->timebase.numer) / me->timebase.denom;
}

double piTimer::GetTime(void)
{
    iTimer *me = (iTimer*)mImplementation;
    if (!me)
        return 0.0;
    const uint64_t ns = iNowNanos(me);
    return (double)ns / 1000000000.0;
}

uint64_t piTimer::GetTimeMs(void)
{
    iTimer *me = (iTimer*)mImplementation;
    if (!me)
        return 0;
    const uint64_t ns = iNowNanos(me);
    return ns / 1000000;
}

uint64_t piTimer::GetTimeMicroseconds(void)
{
    iTimer *me = (iTimer*)mImplementation;
    if (!me)
        return 0;
    const uint64_t ns = iNowNanos(me);
    return ns / 1000;
}

uint64_t piTimer::GetTimeTicks(void)
{
    iTimer *me = (iTimer*)mImplementation;
    if (!me)
        return 0;
    const uint64_t ns = iNowNanos(me);
    return (ns * 12600) / 1000000000;
}

void piTimer::Sleep(int miliseconds)
{
    if (miliseconds <= 0)
        return;
    usleep((useconds_t)(miliseconds * 1000));
}

}
