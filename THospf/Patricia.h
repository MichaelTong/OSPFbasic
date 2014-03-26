#ifndef PATRICIA_H
#define PATRICIA_H

//Patricia位校验
inline bool bit_check(byte *str,int len,int bit)
{
    int32 bitlen;

    bitlen = len << 3;
    if (bit < bitlen)
        return((str[bit>>3] & (0x80 >> (bit & 0x07))) != 0);
    else if (bit < bitlen + 32)
        return((bitlen & (1 << (bit - bitlen))) != 0);
    else
        return(false);
}

//Patricia树中的结点

const uns32 MaxBit = 0xffffffffL;

class PatriciaEntry
{
    PatriciaEntry *zeroptr;//0指针
    PatriciaEntry *oneptr;//1指针
    uns32 chkbit;
public:
    byte *key;//关键字
    int	keylen;//关键字长

    inline bool	bit_check(int bit);
    friend class PatriciaTree;
};

inline bool PatriciaEntry::bit_check(int bit)
{
    return(::bit_check(key, keylen, bit));
}

//Patricia树
class PatriciaTree
{
    PatriciaEntry *root;
    int	size;
public:
    PatriciaTree();
    void init();
    void add(PatriciaEntry *);//更加一个条目
    PatriciaEntry *find(byte *key, int keylen);//根据关键字找到条目
    PatriciaEntry *find(char *key);
    void remove(PatriciaEntry *);//删除一个条目
    void clear();
    void clear_subtree(PatriciaEntry *);//删除子树
};
#endif // PATRICIA_H
