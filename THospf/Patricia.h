#ifndef PATRICIA_H
#define PATRICIA_H

//PatriciaλУ��
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

//Patricia���еĽ��

const uns32 MaxBit = 0xffffffffL;

class PatriciaEntry
{
    PatriciaEntry *zeroptr;//0ָ��
    PatriciaEntry *oneptr;//1ָ��
    uns32 chkbit;
public:
    byte *key;//�ؼ���
    int	keylen;//�ؼ��ֳ�

    inline bool	bit_check(int bit);
    friend class PatriciaTree;
};

inline bool PatriciaEntry::bit_check(int bit)
{
    return(::bit_check(key, keylen, bit));
}

//Patricia��
class PatriciaTree
{
    PatriciaEntry *root;
    int	size;
public:
    PatriciaTree();
    void init();
    void add(PatriciaEntry *);//����һ����Ŀ
    PatriciaEntry *find(byte *key, int keylen);//���ݹؼ����ҵ���Ŀ
    PatriciaEntry *find(char *key);
    void remove(PatriciaEntry *);//ɾ��һ����Ŀ
    void clear();
    void clear_subtree(PatriciaEntry *);//ɾ������
};
#endif // PATRICIA_H
