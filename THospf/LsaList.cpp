#include "include.h"

LsaListElement *LsaListElement::freelist;
int LsaListElement::n_allocated; // 已经占用的个数
int LsaListElement::n_free; 	// 空余的个数


//自定义的new操作，自动将元素放到列表上
void *LsaListElement::operator new(size_t)

{
    LsaListElement *ep;
    int	i;

    if ((ep = freelist))
    {
        freelist = freelist->next;
        n_free--;
        return(ep);
    }

    ep = (LsaListElement *) ::new char[BlkSize * sizeof(LsaListElement)];

    n_free = BlkSize - 1;
    n_allocated += BlkSize;
    for (freelist = ep, i = 0; i < BlkSize - 2; i++, ep++)
        ep->next = ep + 1;
    ep->next = 0;
    return(ep + 1);
}

//删除一个元素，自动将其从列表去除
void LsaListElement::operator delete(void *ptr, size_t)

{
    LsaListElement *ep;

    ep = (LsaListElement *) ptr;
    ep->next = freelist;
    freelist = ep;
    n_free++;
}


//清空lsa列表
void LsaList::clear()

{
    LsaListElement *ep;
    LsaListElement *next;

    for (ep = head; ep; ep = next)
    {
        next = ep->next;
        delete ep;
    }

    head = 0;
    tail = 0;
    size = 0;
}

//将olst表放到当前列表的末尾
void LsaList::append(LsaList *olst)
{
    if (!olst->head)
        return;
    else if (!head)
        head = olst->head;
    else
        tail->next = olst->head;

    tail = olst->tail;
    size += olst->size;
    olst->head = 0;
    olst->tail = 0;
    olst->size = 0;
}

//移除一个表上所有无效的LSA，返回移除的数目
int LsaList::garbage_collect()

{
    LsaListElement *ep;
    LsaListElement *prev;
    LsaListElement *next;
    int oldsize;

    oldsize = size;

    for (prev = 0, ep = head; ep; ep = next)
    {
        next = ep->next;
        if (ep->lsap->valid())
        {
            prev = ep;
            continue;
        }

        if (!prev)
            head = next;
        else
            prev->next = next;
        if (tail == ep)
            tail = prev;
        delete ep;
        size--;
    }

    return(oldsize - size);
}


//移除一个特定的LSA，返回是否找到了该LSA
int LsaList::remove(LSA *lsap)

{
    LsaListElement *ep;
    LsaListElement *prev;
    LsaListElement *next;

    for (prev = 0, ep = head; ep; ep = next)
    {
        next = ep->next;
        if (ep->lsap != lsap)
        {
            prev = ep;
            continue;
        }

        if (!prev)
            head = next;
        else
            prev->next = next;
        if (tail == ep)
            tail = prev;
        delete ep;
        size--;
        return(true);
    }

    return(false);
}
