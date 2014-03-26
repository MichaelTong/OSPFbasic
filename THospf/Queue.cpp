#include "define.h"
#include "Queue.h"
//优先级队列，加入
void Queue::add(Q_Elt *item)
{
    Queue temp;
    item->left=NULL;
    item->right=NULL;
    item->parent=NULL;
    item->dist=1;
    temp.root=item;
    combine(temp);
}
//合并两个优先级队列
void Queue::combine(Queue &q)
{
    Q_Elt *parent;
    Q_Elt *temp;
    Q_Elt *x;
    Q_Elt *y;

    if(q.root==NULL)
    {
        if(root)
            root->parent=NULL;
        return;
    }
    else if(root==NULL)
    {
        root=q.root;
        if(root)
            root->parent=NULL;
        return;
    }
    else
    {
        y=q.root;
        if(y->cmp_cost(root))
        {
            temp=root;
            root=y;
            y=temp;
        }
        root->parent=NULL;

        parent=root;
        x=root->right;
        while(x)
        {

            if(y->cmp_cost(x))
            {
                temp=x;
                x=y;
                y=temp;
            }
            x->parent=parent;
            parent->right=x;
            parent=x;
            x=x->right;
        }

        parent->right=y;
        y->parent=parent;
        adjust(parent,0);
    }
}
//从队列中删除某个元素
void Queue::del(Q_Elt *item)
{
    Q_Elt *parent;
    Queue q1;
    Queue q2;

    parent=item->parent;
    q1.root=item->right;
    q2.root=item->left;
    q1.combine(q2);

    if(parent==NULL)
        root=q1.root;
    else if(parent->right==item)
        parent->right=q1.root;
    else
        parent->left=q1.root;
    if(q1.root)
        q1.root->parent=parent;
    adjust(parent,1);
}
//更新合并操作后结点的DIST，保证对于一个结点a，a->dist=a->right->dist;
void Queue::adjust(Q_Elt *a,int del)
{
    Q_Elt *left;
    Q_Elt *right;

    for(; a; a=a->parent)
    {
        uns32 newdist;

        left=a->left;
        right=a->right;

        if(right==NULL)
            newdist=1;
        else if(left==NULL)
        {
            a->left=right;
            a->right=NULL;
            newdist=1;
        }
        else if(left->dist<right->dist)
        {
            a->left=right;
            a->right=left;
            newdist=left->dist+1;
        }
        else
        {
            newdist=right->dist+1;
        }

        if(newdist!=a->dist)
            a->dist=newdist;
        else if(del)
            break;
    }
}

//得到并移除头部
Q_Elt *Queue::rmhead()
{
    Q_Elt *top;
    Queue temp1;
    Queue temp2;

    if (!(top = root))
        return(0);

    temp1.root = top->left;
    temp2.root = top->right;
    temp1.combine(temp2);
    root = temp1.root;
    return(top);
}
