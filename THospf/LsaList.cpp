#include "include.h"

LsaListElement *LsaListElement::freelist;
int LsaListElement::n_allocated; // �Ѿ�ռ�õĸ���
int LsaListElement::n_free; 	// ����ĸ���


//�Զ����new�������Զ���Ԫ�طŵ��б���
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

//ɾ��һ��Ԫ�أ��Զ�������б�ȥ��
void LsaListElement::operator delete(void *ptr, size_t)

{
    LsaListElement *ep;

    ep = (LsaListElement *) ptr;
    ep->next = freelist;
    freelist = ep;
    n_free++;
}


//���lsa�б�
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

//��olst��ŵ���ǰ�б��ĩβ
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

//�Ƴ�һ������������Ч��LSA�������Ƴ�����Ŀ
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


//�Ƴ�һ���ض���LSA�������Ƿ��ҵ��˸�LSA
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
