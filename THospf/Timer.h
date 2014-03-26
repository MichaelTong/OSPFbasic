#ifndef TIMER_H
#define TIMER_H


class Timer : public Q_Elt
{
protected:
    int active:1;
    uns32 period;
public:
    enum {SECOND=1000};
    Timer()
    {
        active=false;
        period=0;
    }

    static int random_period(int p);
    void stop();
    void restart(int ms=0);
    inline uns32 interval();
    inline int is_running();
    virtual void start(int ms,bool ran=true);
    virtual void fire();
    virtual void action()=0;
    virtual ~Timer();

};

inline int Timer::is_running()
{
    return active;
}

inline uns32 Timer::interval()
{
    return(active ? period : 0);
}


class ITimer : public Timer
{
public:
    virtual void start(int ms,bool ran=true);
    virtual void fire();
    virtual void action()=0;
};

class SPFtime
{
public:
    uns32 sec;
    uns32 ms;
};

bool time_less(SPFtime &a,SPFtime &b);
void time_add(SPFtime &a,SPFtime &b,SPFtime *res);
void time_add(SPFtime &a,int ms,SPFtime *res);
bool time_equal(SPFtime &a,SPFtime &b);
int time_diff(SPFtime &a,SPFtime &b);

#endif // TIMER_H
