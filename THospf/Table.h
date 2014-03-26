#ifndef TABLE_H
#define TABLE_H

class TblItem {
    uns32 _index1;	// 首要的比较值
    uns32 _index2;	// 次要的比较值
    TblItem *right; 	// 右结点
    TblItem *left; 	// 左节点
    int16 balance:3,	// AVL平衡因子
	in_db:1;	// 在数据库中？
protected:
    int16 refct; 	// 被引用数
public:
    TblItem *sll; 	// 在顺序列表中的下一个结点Next in ordered list

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


//将一个AVL结点去掉引用，如果引用数为零并且不在数据库中就删去
inline void TblItem::deref()

{
    if (refct >= 1)
	refct--;
    if (refct == 0 && !in_db)
	delete this;
}


//检查结点是否要删除，如果引用数为0并且不在数据库中删除
inline void TblItem::chkref()

{
    if (refct == 0 && !in_db)
	delete this;
}

//Table结构，对外表现出是一个顺序表，内部用AVL来维持
class Table
{
    TblItem *_root; 	//AVL根结点
    uns32 count; 	//树上节点数
    uns32 instance;	// 加入或减少动作
public:
    TblItem *sllhead;	// 顺序表的头部

    inline Table();
    inline TblItem *root();// 获取根节点
    inline int size();
    TblItem *find(uns32 key1, uns32 key2=0);
    TblItem *previous(uns32 key1, uns32 key2=0);
    void add(TblItem *); // 向树上加一个结点
    void remove(TblItem *); // 从树上删除一个结点
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


//搜索AVL树，深度优先搜索。
class TblSearch {
    Table *tree;	 // 需要搜索的表
    uns32 instance;	// 相关的条目
    uns32 c_index1;	// 当前的位置index1
    uns32 c_index2; // 当前的位置index2
    TblItem *current;//当前的条目
public:
    inline TblSearch(Table *);
    void seek(uns32 key1, uns32 key2);//设置起点
    inline void seek(TblItem *item);
    TblItem *next();//第一次调用时，返回最小关键字的结点，下一次返回次小关键字结点，依次类推
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

//用于记录搜索的栈
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
