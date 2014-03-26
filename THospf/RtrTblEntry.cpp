#include "include.h"
#include "InterfaceFSM.h"


RouterRtrTblEntry::RouterRtrTblEntry(uns32 id) : RtrTblEntry(id, 0)

{
    rtype = 0;
}

RouterRtrTblEntry::~RouterRtrTblEntry()

{

}

//创建新的路径
EPath *EPath::create(Interface *ip,InAddr addr)
{
    ENode paths[MAXPATH];

    paths[0].if_addr=ip->if_addr;
    paths[0].phyint=ip->if_physic;
    paths[0].gateway=addr;
    return (create(1,paths));
}

EPath *EPath::create(int phyint,InAddr addr)
{
    ENode paths[MAXPATH];

    paths[0].if_addr=(InAddr) - 1;
    paths[0].phyint=phyint;
    paths[0].gateway=addr;
    return (create(1,paths));
}

EPath *EPath::create(int n_paths, ENode *paths)
{
    int i;
    EPath *entry;
    int len;

    // 其他路径节点清零
    for (i = n_paths; i < MAXPATH; i++)
    {
        paths[i].if_addr = 0;
        paths[i].phyint = 0;
        paths[i].gateway = 0;
    }
    //  如果已经存在于路径数据库中，直接返回存在的条目
    len = sizeof(ENode[MAXPATH]);
    if ((entry = (EPath *) nhdb.find((byte *)paths, len)))
        return(entry);

    // 创建心条目
    entry = new EPath;
    entry->npaths = n_paths;
    for (i = 0; i < MAXPATH; i++)
        entry->Nodes[i] = paths[i];
    entry->s_phyint = -1;
    entry->s_EPath = 0;
    // 加入到数据库
    entry->key = (byte *) entry->Nodes;
    entry->keylen = len;
    nhdb.add(entry);
    return(entry);
}

//合并两条路径
EPath *EPath::merge(EPath *p1,EPath *p2)
{
    int n_paths=0;
    ENode paths[MAXPATH];
    int i,j;

    if(!p1)
        return p2;
    else if(!p2)
        return p1;
    else if(p1->npaths == MAXPATH)
        return p1;
    else if(p2->npaths == MAXPATH)
        return p2;

    i=0;
    j=0;
    for(; i<p1->npaths&&n_paths<MAXPATH; i++)
    {
        for(; j<p2->npaths&&n_paths<MAXPATH; j++)
        {
            if(p1->Nodes[i].if_addr>p2->Nodes[j].if_addr)
                paths[n_paths++]=p2->Nodes[j];
            else if(p1->Nodes[i].phyint < p2->Nodes[j].phyint)
                break;
            else if(p1->Nodes[i].phyint > p2->Nodes[j].phyint)
                paths[n_paths++]=p2->Nodes[j];
            else if(p1->Nodes[i].gateway < p2->Nodes[j].gateway)
                break;
            else if(p1->Nodes[i].gateway > p2->Nodes[j].gateway)
                paths[n_paths++]=p2->Nodes[j];
            else
                continue;
        }
        if(n_paths < MAXPATH)
            paths[n_paths++]=p1->Nodes[i];
    }

    for(; j<p2->npaths && n_paths<MAXPATH; j++)
        paths[n_paths++]=p2->Nodes[j];

    return (create(n_paths,paths));
}

//添加网关
EPath *EPath::addgateway(EPath *p, InAddr gw)
{
    bool modified=false;
    ENode paths[MAXPATH];
    int i,j;
    int n_paths;

    if(!p)
        return 0;
    n_paths = p->npaths;
    for (i = 0, j = 0; i < p->npaths; i++, j++)
    {
        Interface *ip;
        paths[j] = p->Nodes[i];
        if (!(ip = ospf->find_ifc(paths[j].if_addr, paths[j].phyint)))
            continue;
        else if (ip->net() == 0 || (ip->mask() & gw) != ip->net())
            continue;
        else if (paths[j].gateway == 0)
        {
            paths[j].gateway = gw;
            modified = true;
        }
        else if (n_paths == MAXPATH || paths[j].gateway == gw || (p->Nodes[i+1].if_addr==paths[j].if_addr && paths[j].gateway < gw))
            continue;
        else
        {
            modified = true;
            j++, n_paths++;
            paths[j].if_addr = paths[j-1].if_addr;
            paths[j].phyint = paths[j-1].phyint;
            if (paths[j-1].gateway < gw)
                paths[j].gateway = gw;
            else
            {
                paths[j].gateway = paths[j-1].gateway;
                paths[j-1].gateway = gw;
            }
        }
    }

    return (modified ? create(n_paths, paths) : p);
}

//简化路径
EPath *EPath::simple_phyint(int phyint)
{
    bool modified=false;
    ENode paths[MAXPATH];
    int i,j;

    if(phyint==s_phyint)
        return s_EPath;

    for(i=0,j=0; i<npaths; i++)
    {
        if(Nodes[i].phyint==phyint)
        {
            modified=true;
            continue;
        }
        paths[j++]=Nodes[i];
    }

    s_phyint=phyint;
    if(j==0)
        s_EPath=0;
    else if(!modified)
        s_EPath=this;
    else
        s_EPath=create(j,paths);

    return s_EPath;
}

//向IP路由表添加IP路由
IPrte *IPRtrTbl::add(uns32 net, uns32 mask)
{
    IPrte *rte;
    IPrte *parent;
    IPrte *child;
    IPiterator iter(this);

    if((rte=(IPrte *)root.find(net,mask)))
        return (rte);
    rte = new IPrte(net, mask);
    root.add(rte);
    // 设置前缀
    rte->_prefix = 0;
    parent = (IPrte *) root.previous(net, mask);
    for (; parent; parent = parent->prefix())
    {
        if (rte->is_child(parent))
        {
            rte->_prefix = parent;
            break;
        }
    }

    iter.seek(rte);
    while ((child = iter.nextrte()) && child->is_child(rte))
    {
        if (child->prefix() && child->prefix()->is_child(rte))
            continue;
        child->_prefix = rte;
    }

    return(rte);
}

//找到最符合的路由
IPrte *IPRtrTbl::match(uns32 addr)
{
    IPrte *rte;
    IPrte *prev;

    rte = (IPrte *) root.root();
    prev = 0;
    while (rte)
    {
        if (addr < rte->net())
            rte = (IPrte *) rte->go_left();
        else if (addr > rte->net() || 0xffffffff > rte->mask())
        {
            prev = rte;
            rte = (IPrte *) rte->go_right();
        }
        else
            break;
    }

    // 如果没有符合的，返回前一个
    if (!rte)
        rte = prev;
    // 遍历前缀，找到一个有效的路由
    for (; rte; rte = rte->prefix())
    {
        if ((addr & rte->mask()) != rte->net())
            continue;
        if (rte->valid())
            break;
    }

    return(rte);
}

Interface *RtrTblEntry::ifc()
{
    if(r_epath && r_epath->npaths)
    {
        ENode *enode=&r_epath->Nodes[0];
        return (ospf->find_ifc(enode->if_addr,enode->phyint));
    }
    else
        return 0;
}




//加入下一跳
void TNode::add_next_hop(TNode *V, int _index)

{
    EPath *new_nh;
    Interface *t_ifc;
    InAddr t_gw;

    // 如果是根
    if (V == ospf->mylsa)
    {
        if (!(t_ifc = ospf->ifmap[_index]))
            return;
        if (lsa_type == LST_NET)
            t_gw = 0;
        else
            t_gw = ospf_find_gw(V, t_ifc->net(), t_ifc->mask());
        new_nh = EPath::create(t_ifc, t_gw);
        new_nh = t_ifc->add_parallel_links(new_nh, this);
    }

    else if (!V->t_direct || V->ls_type() != LST_NET)
        new_nh = V->t_epath;
    else
    {
        IPrte *rte;
        rte = (IPrte *) V->t_dest;
        t_gw = ospf_find_gw(V, rte->net(), rte->mask());
        new_nh = EPath::addgateway(V->t_epath, t_gw);
    }

    t_epath = EPath::merge(t_epath, new_nh);

}

//加入平行的链路
EPath *Interface::add_parallel_links(EPath *new_nh, TNode *dst)
{
    return(new_nh);
}
