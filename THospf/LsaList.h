#ifndef LSALIST_H_INCLUDED
#define LSALIST_H_INCLUDED

//构成LSA列表
//提供迭代接口
//可对LSA进行遍历

class LsaListElement
{
    LsaListElement *next;
    LSA	*lsap;

    enum
    {
        BlkSize = 256
    };
    static LsaListElement *freelist;
    static int n_allocated;
    static int n_free;

    void * operator new(size_t size);
    void operator delete(void *ptr, size_t);
    inline LsaListElement(LSA *);
    inline ~LsaListElement();

    friend class LsaList;
    friend class LsaListIterator;

public:
    inline int n_free_elements();
    inline int n_alloc_elements();
};


inline LsaListElement::LsaListElement(LSA *adv) : next(0), lsap(adv)
{
    adv->ref();
}
inline LsaListElement::~LsaListElement()
{
    lsap->deref();
}
inline int LsaListElement::n_free_elements()
{
    return(n_free);
}
inline int LsaListElement::n_alloc_elements()
{
    return(n_allocated);
}


class LsaList
{
    LsaListElement *head;
    LsaListElement *tail;
    int	size;

public:
    inline LsaList();
    inline void addEntry(LSA *);
    inline LSA *FirstEntry();
    int	remove(LSA *);
    void clear();
    int	garbage_collect();
    void append(LsaList *);
    inline bool is_empty();
    inline int count();

    friend class LsaListIterator;
};

inline LsaList::LsaList() : head(0), tail(0), size(0)
{
}
inline LSA *LsaList::FirstEntry()
{
    return(head ? head->lsap : 0);
}
inline bool LsaList::is_empty()
{
    return(head == 0);
}
inline int LsaList::count()
{
    return(size);
}


inline void LsaList::addEntry(LSA *lsap)

{
    LsaListElement *ep;

    ep = new LsaListElement(lsap);
    if (!head)
    {
        head = ep;
        tail = ep;
    }
    else
    {
        tail->next = ep;
        tail = ep;
    }
    size++;
}

class LsaListIterator
{
    LsaList *list;
    LsaListElement *prev;
    LsaListElement *current;

public:
    inline LsaListIterator(LsaList *);
    inline LSA *get_next();
    inline void remove_current();
    inline LSA	*search(int lstype, lsid_t id, rtid_t org);
};

inline LsaListIterator::LsaListIterator(LsaList *xlist)
{
    list = xlist;
    prev = 0;
    current = 0;
}


inline LSA *LsaListIterator::get_next()

{
    prev = current;
    current = (current ? current->next : list->head);

    return(current ? current->lsap : 0);
}


inline void LsaListIterator::remove_current()

{
    if (current)
    {
        if (list->tail == current)
            list->tail = prev;
        list->size--;
        if (!prev)
            list->head = current->next;
        else
            prev->next = current->next;
        delete current;
        current = prev;
    }
}


inline LSA *LsaListIterator::search(int lstype, lsid_t id, rtid_t org)

{
    LSA	*lsap;

    while ((lsap = get_next()))
    {
        if (lsap->lsa_type != lstype)
            continue;
        if (lsap->ls_id() != id)
            continue;
        if (lsap->adv_rtr() == org)
            return(lsap);
    }

    return(0);
}

#endif // LSALIST_H_INCLUDED
