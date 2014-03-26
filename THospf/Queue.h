#ifndef QUEUE_H
#define QUEUE_H

//优先级队列的一个元素
class Q_Elt
{
    Q_Elt *left;//左节点
    Q_Elt *right;//右结点
    Q_Elt *parent;
    uns32 dist;
protected://比较顺序：cost0 cost1 tie1 tie2
    uns32 cost0;
    uns32 cost1;
    byte tie1;
    uns32 tie2;
public:
    friend class Queue;
    friend class OSPF;
    inline Q_Elt();
    inline bool cmp_cost(Q_Elt *);//如果自己比传入的参数花费少则返回true
    friend int main(int argc,char *argv[]);
};

inline Q_Elt::Q_Elt()
{
    left = 0;
    right = 0;
    parent = 0;
    dist = 0;
    cost0 = 0;
    cost1 = 0;
    tie1 = 0;
    tie2 = 0;
}

inline bool Q_Elt::cmp_cost(Q_Elt *oqe)
{
    if (cost0 < oqe->cost0)
        return(true);
    else if (cost0 > oqe->cost0)
        return(false);
    else if (cost1 < oqe->cost1)
        return(true);
    else if (cost1 > oqe->cost1)
        return(false);
    else if (tie1 > oqe->tie1)
        return(true);
    else if (tie1 < oqe->tie1)
        return(false);
    else if (tie2 > oqe->tie2)
        return(true);
    else
        return(false);
}

class Queue
{
    Q_Elt *root;
    void adjust(Q_Elt *a,int del);
    void combine(Queue &q);
public:
    inline Queue();
    inline Q_Elt *gethead();//返回队列头部
    Q_Elt *rmhead();//将头部从队列去掉并返回原来的头部
    void add(Q_Elt *item);
    void del(Q_Elt *item);

};

inline Queue::Queue(): root(0)
{
}

inline Q_Elt *Queue::gethead()
{
    return root;
}
#endif // QUEUE_H
