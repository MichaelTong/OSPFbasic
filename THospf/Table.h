#ifndef TABLE_H
#define TABLE_H

class TblItem {
    uns32 _index1;	// ��Ҫ�ıȽ�ֵ
    uns32 _index2;	// ��Ҫ�ıȽ�ֵ
    TblItem *right; 	// �ҽ��
    TblItem *left; 	// ��ڵ�
    int16 balance:3,	// AVLƽ������
	in_db:1;	// �����ݿ��У�
protected:
    int16 refct; 	// ��������
public:
    TblItem *sll; 	// ��˳���б��е���һ�����Next in ordered list

    TblItem(uns32, uns32);
    virtual ~TblItem();

    inline int valid();
    inline void ref();
    inline void deref();
    inline void chkref();
    inline bool not_referenced();
    inline uns32 index1();
    inline uns32 index2();
    inline TblItem *go_right();
    inline TblItem *go_left();

    friend class Table;
    friend class TblSearch;
    friend class LSA;
    friend void left_shift(TblItem **);
    friend void dbl_left_shift(TblItem **);
    friend void right_shift(TblItem **);
    friend void dbl_right_shift(TblItem **);
};

inline int TblItem::valid()
{
    return(in_db);
}
inline void TblItem::ref()
{
    refct++;
}
inline bool TblItem::not_referenced()
{
    return(refct == 0);
}
inline uns32 TblItem::index1()
{
    return(_index1);
}
inline uns32 TblItem::index2()
{
    return(_index2);
}
inline TblItem *TblItem::go_right()
{
    return(right);
}
inline TblItem *TblItem::go_left()
{
    return(left);
}


//��һ��AVL���ȥ�����ã����������Ϊ�㲢�Ҳ������ݿ��о�ɾȥ
inline void TblItem::deref()

{
    if (refct >= 1)
	refct--;
    if (refct == 0 && !in_db)
	delete this;
}


//������Ƿ�Ҫɾ�������������Ϊ0���Ҳ������ݿ���ɾ��
inline void TblItem::chkref()

{
    if (refct == 0 && !in_db)
	delete this;
}

//Table�ṹ��������ֳ���һ��˳����ڲ���AVL��ά��
class Table
{
    TblItem *_root; 	//AVL�����
    uns32 count; 	//���Ͻڵ���
    uns32 instance;	// �������ٶ���
public:
    TblItem *sllhead;	// ˳����ͷ��

    inline Table();
    inline TblItem *root();// ��ȡ���ڵ�
    inline int size();
    TblItem *find(uns32 key1, uns32 key2=0);
    TblItem *previous(uns32 key1, uns32 key2=0);
    void add(TblItem *); // �����ϼ�һ�����
    void remove(TblItem *); // ������ɾ��һ�����
    void clear();

    friend class TblSearch;
};

inline Table::Table() : _root(0), count(0), instance(0), sllhead(0)
{
}
inline TblItem *Table::root()
{
    return(_root);
}
inline int Table::size()
{
    return(count);
}


//����AVL�����������������
class TblSearch {
    Table *tree;	 // ��Ҫ�����ı�
    uns32 instance;	// ��ص���Ŀ
    uns32 c_index1;	// ��ǰ��λ��index1
    uns32 c_index2; // ��ǰ��λ��index2
    TblItem *current;//��ǰ����Ŀ
public:
    inline TblSearch(Table *);
    void seek(uns32 key1, uns32 key2);//�������
    inline void seek(TblItem *item);
    TblItem *next();//��һ�ε���ʱ��������С�ؼ��ֵĽ�㣬��һ�η��ش�С�ؼ��ֽ�㣬��������
};

inline TblSearch::TblSearch(Table *avltree)
: tree(avltree), c_index1(0), c_index2(0), current(0)

{
    instance = tree->instance;
}
void TblSearch::seek(TblItem *item)
{
    seek(item->index1(), item->index2());
}

//���ڼ�¼������ջ
class Stack {
    enum {StkSz = 32};
    void *stack[StkSz];
    void **sp;
    void **marker;

public:
    Stack() : sp(stack) {};
    inline void push(void *ptr);
    inline void *pop();
    inline void mark();
    inline void replace_mark(void *ptr);
    inline int is_empty();
    inline void *truncate_to_mark();
    inline void reset();
};

inline void Stack::push(void *ptr)
{
    *sp++ = ptr;
}
inline void *Stack::pop()
{
    if (sp > stack)
	return(*(--sp));
    else return(0);
}
inline void Stack::mark()
{
    marker = sp;
}
inline void Stack::replace_mark(void *ptr)
{
    *marker = ptr;
}
inline int Stack::is_empty()
{
    return(sp == stack);
}
inline void *Stack::truncate_to_mark()
{
    sp = marker;
    return(*sp);
}
inline void Stack::reset()
{
    sp = stack;
}
#endif // TABLE_H
