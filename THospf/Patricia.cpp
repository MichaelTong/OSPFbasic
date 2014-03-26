#include "define.h"
#include "Patricia.h"

PatriciaTree::PatriciaTree()
{
    init();
}

void PatriciaTree::init()
{
    root = new PatriciaEntry();
    root->zeroptr=root;
    root->oneptr=root;
    root->chkbit=MaxBit;
    root->key=0;
    root->keylen=0;
    size=0;
}

PatriciaEntry *PatriciaTree::find(byte *key,int keylen)
{
    PatriciaEntry *entry;
    uns32 chkbit;

    entry=root;
    do
    {
        chkbit=entry->chkbit;
        if(bit_check(key,keylen,chkbit))
            entry=entry->oneptr;
        else
            entry=entry->zeroptr;
    }
    while(entry->chkbit>chkbit);

    if(keylen==entry->keylen&&memcmp(key,entry->key,keylen)==0)
        return (entry);

    return (0);
}

PatriciaEntry *PatriciaTree::find(char *key)

{
    int keylen;

    keylen = strlen(key);
    return(find((byte *) key, keylen));
}

void PatriciaTree::add(PatriciaEntry *entry)

{
    PatriciaEntry *ptr;
    uns32 chkbit;
    uns32 newbit;
    PatriciaEntry **parent;

    ptr = root;
    do
    {
        chkbit = ptr->chkbit;
        if (entry->bit_check(chkbit))
            ptr = ptr->oneptr;
        else
            ptr = ptr->zeroptr;
    }
    while (ptr->chkbit > chkbit);

    for (newbit = 0; ; newbit++)
    {
        if (ptr->bit_check(newbit) != entry->bit_check(newbit))
            break;
    }

    parent = &root;
    for (ptr = *parent; ptr->chkbit < newbit;)
    {
        chkbit = ptr->chkbit;
        if (entry->bit_check(chkbit))
            parent = &ptr->oneptr;
        else
            parent = &ptr->zeroptr;

        ptr = *parent;
        if (ptr->chkbit <= chkbit)
            break;
    }

    *parent = entry;
    entry->chkbit = newbit;
    if (entry->bit_check(newbit))
    {
        entry->oneptr = entry;
        entry->zeroptr = ptr;
    }
    else
    {
        entry->zeroptr = entry;
        entry->oneptr = ptr;
    }
    size++;
}


void PatriciaTree::remove(PatriciaEntry *entry)

{
    PatriciaEntry *ptr;
    uns32 chkbit;
    PatriciaEntry *upptr;
    PatriciaEntry **parent;
    PatriciaEntry **upparent;
    bool upzero;
    PatriciaEntry **fillptr;

    ptr = root;
    upptr = 0;
    parent = &root;
    upparent = &root;
    fillptr = 0;
    do
    {
        if (ptr == entry && !fillptr)
            fillptr = parent;
        upparent = parent;
        upptr = ptr;
        chkbit = ptr->chkbit;
        if (entry->bit_check(chkbit))
            parent = &ptr->oneptr;
        else
            parent = &ptr->zeroptr;
        ptr = *parent;
    }
    while (ptr->chkbit > chkbit);

    if (ptr != entry)
        return;

    upzero = (upptr->zeroptr == entry);
    *upparent = (upzero ? upptr->oneptr : upptr->zeroptr);
    if (upptr != entry)
    {
        *fillptr = upptr;
        upptr->chkbit = entry->chkbit;
        upptr->oneptr = entry->oneptr;
        upptr->zeroptr = entry->zeroptr;
    }

    size--;
}

void PatriciaTree::clear()
{
    clear_subtree(root);
    init();
}

void PatriciaTree::clear_subtree(PatriciaEntry *entry)
{
    if(!entry)
        return;
    if(entry->zeroptr && entry->zeroptr->chkbit > entry->chkbit)
        clear_subtree(entry->zeroptr);
    if(entry->oneptr && entry->oneptr->chkbit > entry->chkbit)
        clear_subtree(entry->oneptr);
    delete entry;
}
