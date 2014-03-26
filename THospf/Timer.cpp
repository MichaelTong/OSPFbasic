#include "include.h"
#include "system.h"
#include <stdlib.h>

extern SPFtime sys_etime;

//得到随机的周期
int Timer::random_period(int p)
{
    float f=p;
    return ((int)((f*rand())/(RAND_MAX+1.0)));
}

//停止计时器
void Timer::stop()
{
    if(!is_running())
        return;
    active=false;
    timerq.del(this);
}

Timer::~Timer()
{
    stop();
}

//开启计时器
void Timer::start(int ms,bool ran)
{
    SPFtime interval;
    SPFtime etime;

    if(is_running())
        return;
    period=ms;
    active=true;

    if(ran&&ms>=1000)
        ms+=random_period(1000)-500;

    interval.sec=ms/SECOND;
    interval.ms=ms%SECOND;
    time_add(sys_etime,interval,&etime);
    cost0=etime.sec;
    cost1=etime.ms;
    timerq.add(this);
}

//计时器重新启动
void Timer::restart(int ms)
{
    if(!is_running())
        return;
    stop();
    if(ms==0)
        ms=period;
    start(ms);
}

//间隔计时器开始计时
void ITimer::start(int ms,bool ran)
{
    SPFtime etime;

    if(is_running())
        return;

    period=ms;
    active=true;

    if(ran)
        ms=random_period(period)+1;

    time_add(sys_etime,ms,&etime);
    cost0=etime.sec;
    cost1=etime.ms;
    timerq.add(this);
}

//计时器到时间
void Timer::fire()
{
    active=false;
    action();
}

//间隔计时器到时间，产生相应动作并重新计时
void ITimer::fire()

{
    SPFtime firetime;
    SPFtime etime;

    firetime.sec = cost0;
    firetime.ms = cost1;
    time_add(firetime, period, &etime);
    cost0 = etime.sec;
    cost1 = etime.ms;
    timerq.add(this);

    action();
}

//比较时间
bool time_less(SPFtime &a, SPFtime &b)

{
    return ((a.sec < b.sec) ||
            (a.sec == b.sec && a.ms < b.ms));
}

//时间相加
void time_add(SPFtime &a, SPFtime &b, SPFtime *result)

{
    result->sec = a.sec + b.sec;
    result->ms = a.ms + b.ms;
    result->sec += result->ms/Timer::SECOND;
    result->ms = result->ms % Timer::SECOND;
}

void time_add(SPFtime &a, int milliseconds, SPFtime *result)

{
    result->sec = a.sec;
    result->ms = a.ms + milliseconds;
    result->sec += result->ms/Timer::SECOND;
    result->ms = result->ms % Timer::SECOND;
}

bool time_equal(SPFtime &a, SPFtime &b)

{
    return (a.sec == b.sec && a.ms == b.ms);
}

int time_diff(SPFtime &a, SPFtime &b)

{
    int diff;

    diff = (a.sec - b.sec)*1000;
    diff += (a.ms - b.ms);
    return(diff);
}
