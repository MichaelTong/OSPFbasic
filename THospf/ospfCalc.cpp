#include "include.h"
#include "system.h"
#include "NeighborFSM.h"

//决定是否要重新进行路由规划
void OSPF::rtsched(LSA *newlsa, RtrTblEntry *old_rte)
{
    byte lstype;

    lstype = newlsa->ls_type();

    switch(lstype)
    {
    case LST_RTR:
    case LST_NET:
        full_sched = true;
        break;
    default:
        break;
    }
}

//进行路由规划
void OSPF::full_calculation()
{
    full_sched = false;
    // Dijkstra
    dijkstra();
    rt_scan();
    fa_tbl->resolve();
}

void OSPF::dijk_init(Queue &cand)
{
    // 将自己生成的router LSA放入候选列表
    rtrLSA *root;
    root = (rtrLSA *) myLSA(0,LST_RTR, myid);
    if (root != 0 && root->parsed)
    {
        root->cost0 = 0;
        root->cost1 = 0;
        root->tie1 = root->lsa_type;
        cand.add(root);
        root->t_state = DS_ONCAND;
        mylsa = root;
    }
}

//最短路径计算
void OSPF::dijkstra()
{
    Queue cand;
    TNode *V;

    n_dijkstras++;

    rtrLSA *rtr;
    netLSA *net;

    n_routers = 0;
    rtr = (rtrLSA *) rtrLSAs.sllhead;
    for (; rtr; rtr = (rtrLSA *) rtr->sll)
        rtr->t_state = DS_UNINIT;
    net = (netLSA *) netLSAs.sllhead;
    for (; net; net = (netLSA *) net->sll)
        net->t_state = DS_UNINIT;

    // 初始化候选列表，加入自己产生的router LSA

    dijk_init(cand);

    while ((V = (TNode *) cand.rmhead()))
    {
        RtrTblEntry *dest;
        Link *lp;
        TNode *W;
        int i;

        // 将V放到最小路径树上
        V->t_state = DS_ONTREE;
        dest = V->t_dest;
        dest->new_intra(V, false, 0, 0);

        if (V->ls_type() == LST_RTR)
            n_routers++;

        // 遍历邻居节点，将可能的节点加入到候选列表
        for (lp = V->t_links, i = 0; lp != 0; lp = lp->l_next, i++)
        {
            TLink *tlp;
            uns32 new_cost;
            //将STUB加入到路由表中
            if (lp->l_ltype == LT_STUB)
            {
                SLink *slp;
                slp = (SLink *)lp;
                if (!slp->sl_rte)
                    continue;
                slp->sl_rte->new_intra(V,true,slp->l_fwdcst,i);
                continue;
            }
            tlp = (TLink *) lp;

            if (!(W = tlp->tl_nbr))
                continue;
            if (W->t_state == DS_ONTREE)
                continue;
            new_cost = V->cost0 + tlp->l_fwdcst;
            if (W->t_state == DS_ONCAND)
            {
                if (new_cost > W->cost0)
                    continue;
                else if (new_cost < W->cost0)
                    cand.del(W);
            }
            // 找到更好的路径，设置路径的参数
            if (W->t_state != DS_ONCAND || new_cost < W->cost0)
            {
                W->t_direct = (ospf->mylsa==(rtrLSA *)V);
                W->cost0 = new_cost;
                W->cost1 = 0;
                W->tie1 = W->lsa_type;
                cand.add(W);
                W->t_state = DS_ONCAND;
                W->t_parent = V;
                W->t_epath = 0;
            }
            else if (ospf->mylsa==(rtrLSA *)V)
                W->t_direct = true;
            // 现在找到了到达W的最短路径，加入下一跳
            W->add_next_hop(V, i);
        }
    }
}

RtrTblEntry::RtrTblEntry(uns32 key_a, uns32 key_b) : TblItem(key_a, key_b)

{
    r_epath = 0;
    last_epath = 0;
    r_type = RT_NONE;
    changed = false;
    dijk_run = 0;
    r_ospf = 0;
    cost = Infinity;
    t2cost = Infinity;
}


//更新路由表条目
void RtrTblEntry::new_intra(TNode *V, bool stub, uns16 stub_cost, int _index)

{
    uns32 total_cost;
    bool merge=false;
    EPath *newnh=0;

    total_cost = V->cost0 + stub_cost;

    if (r_type == RT_DIRECT)
        return;
    else if (r_type != RT_SPF)
        changed = true;
    else if (dijk_run != (ospf->n_dijkstras & 1))
    {
        save_state();
    }
    // 找到了更好的路径？
    else if (total_cost > cost)
        return;
    else if (total_cost == cost)//花费相等，将两条路径合并
        merge = true;

    dijk_run = ospf->n_dijkstras & 1;
    // 更新路由表
    r_type = RT_SPF;
    cost = total_cost;
    set_origin(V);
    set_area(0);
    // 除了跟节点都有下一跳
    if (V != ospf->mylsa)
        newnh = V->t_epath;
    // 直接链接的STUB
    else if (stub)
    {
        Interface *o_ifc;
        if ((o_ifc = ospf->ifmap[_index]))
            newnh = EPath::create(o_ifc, 0);
    }
    // 没有下一跳
    else
        return;

    // 花费相等则合并
    if (merge)
        newnh = EPath::merge(newnh, r_epath);
    update(newnh);
}

void RtrTblEntry::save_state()

{
    if (!intra_AS())
        return;
    if (!r_ospf)
        return;
    r_ospf->old_epath = r_epath;
    r_ospf->old_cost = cost;
}

/* 记录路由表的区域号
 */

void RtrTblEntry::set_area(aid_t id)

{
    if (!r_ospf)
        r_ospf = new SpfData;
    r_ospf->r_area = id;
}



bool RtrTblEntry::state_changed()

{
    if (!r_ospf)
        return(false);
    else if (r_epath != r_ospf->old_epath)
        return (true);
    else if (r_ospf->old_cost != cost)
        return(true);
    else
        return(false);
}


//将某个路由表条目的起点设置为某个特定的LSA。
void RtrTblEntry::set_origin(LSA *V)

{
    if (!r_ospf)
        r_ospf = new SpfData;
    r_ospf->lstype = V->ls_type();
    r_ospf->lsid = V->ls_id();
    r_ospf->rtid = V->adv_rtr();
}

void RouterRtrTblEntry::set_origin(LSA *V)

{
    RtrTblEntry::set_origin(V);
    rtype = ((rtrLSA *)V)->rtype;
}


//获取某路由表条目的源LSA
LSA *RtrTblEntry::get_origin()

{
    if (!r_ospf)
        return(0);
    if (area()!=0)
        return(0);
    return (ospf->FindLSA(0, r_ospf->lstype, r_ospf->lsid, r_ospf->rtid));
}

/* 声明某个条目不可达
 */

void RtrTblEntry::declare_unreachable()
{
    r_type = RT_NONE;
    delete r_ospf;
    r_ospf = 0;
    cost = LSInfinity;
    r_epath = 0;
}


void IPrte::declare_unreachable()

{

    if (r_type == RT_DIRECT)
        ospf->full_sched = true;

    RtrTblEntry::declare_unreachable();
}


//找到下一跳地址
InAddr TNode::ospf_find_gw(TNode *V, InAddr net, InAddr mask)

{
    Link *lp;
    InAddr gw_addr=0;

    for (lp = t_links; lp != 0; lp = lp->l_next)
    {
        TLink *tlp;
        if (lp->l_ltype == LT_STUB)
            continue;
        tlp = (TLink *) lp;
        if (tlp->tl_nbr == V)
            gw_addr = tlp->l_data;
        if (net && (gw_addr & mask) == net)
            break;
    }

    return(gw_addr);
}

//向内核安装路由表
void IPrte::sys_install()
{

    TblItem *item;

    switch(r_type)
    {
    case RT_DIRECT:
        fa_tbl->resolve();
        break;
    case RT_NONE:
    case RT_STATIC:
    case RT_EXTT1:
    case RT_EXTT2:
        fa_tbl->resolve(this);
        break;
    default:
        break;
    }


    if ((item = ospf->krtdeletes.find(net(), mask())))
    {
        ospf->krtdeletes.remove(item);
        delete item;
    }

    // 更新内核的转发表
    switch(r_type)
    {
    case RT_NONE:
        sys->rtdel(net(), mask(), last_epath);
        break;
    case RT_REJECT:
        sys->rtadd(net(), mask(), 0, last_epath, true);
        break;
    default:
        sys->rtadd(net(), mask(), r_epath, last_epath, false);
        break;
    }
    last_epath = r_epath;
}

//扫描路由表，删除旧的路由
void OSPF::rt_scan()
{
    IPrte *rte;
    IPiterator iter(inrttbl);

    while ((rte = iter.nextrte()))
    {
        if (rte->intra_area() && ((n_dijkstras & 1) != rte->dijk_run))
        {
            rte->declare_unreachable();
            rte->changed = true;
        }
        if (rte->intra_AS() && rte->r_epath == 0)
            rte->declare_unreachable();
        if (rte->intra_area()) {
            rte->tag = 0;
        }
        if (rte->changed || rte->state_changed())
        {
            rte->changed = false;
            rte->sys_install();
        }
    }
}

//与内核路由表同步
void OSPF::krt_sync()
{
    TblSearch iter(&krtdeletes);
    KrtSync *item;

    while ((item = (KrtSync *)iter.next()))
    {
        InAddr net;
        InMask mask;
        IPrte *rte;
        if (time_diff(sys_etime, item->tstamp) < 5*Timer::SECOND)
            continue;
        net = item->index1();
        mask = item->index2();
        krtdeletes.remove(item);
        delete item;
        rte = inrttbl->find(net, mask);
        if (!rte || !rte->valid())
            continue;
        sys->rtadd(net, mask, rte->r_epath, rte->last_epath,
                   rte->r_type == RT_REJECT);
    }
}

KrtSync::KrtSync(InAddr net, InMask mask) : TblItem(net, mask)

{
    tstamp = sys_etime;
}

void FWDtbl::resolve()
 //解析所有转发地址的可达性和花费。
 //如果转发地址变了，就要重新计算路由，并且要重新生成LSA
{
    TblSearch iter(&root);
    FWDrte *faddr;

    while ((faddr = (FWDrte *) iter.next()))
    {
        faddr->resolve();
    }
}

void FWDtbl::resolve(IPrte *changed_rte)
 //解析与刚刚被修改的路由表条目符合的转发地址
{
    InAddr start;
    InAddr end;
    TblSearch iter(&root);
    FWDrte *faddr;

    start = changed_rte->net() - 1;
    end = changed_rte->broadcast_address();
    if (changed_rte != default_route)
        iter.seek(start, 0);

    while ((faddr = (FWDrte *) iter.next()))
    {
        if (faddr->address() > end)
            break;
        if (faddr->match && !changed_rte->is_child(faddr->match))
            continue;
        faddr->resolve();
    }
}

void FWDrte::resolve()
//解析一个转发地址，如果花费或状态发生变化则返回true
{
    aid_t oa;
    byte otype;
    EPath *new_path;

    save_state();
    oa = area();
    otype = r_type;

    ifp = ospf->find_nbr_ifc(address());
    match = inrttbl->match(address());
    if (!match || !match->intra_AS())
        r_type = RT_NONE;
    else
    {
        r_type = match->type();
        new_path = EPath::addgateway(match->r_epath, address());
        set_area(match->area());
        update(new_path);
        cost = match->cost;
    }

    changed = (state_changed() || otype != r_type || oa != area());
}
