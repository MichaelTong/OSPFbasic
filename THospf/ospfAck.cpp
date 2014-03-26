#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"
#include "system.h"
//j
void Neighbor::recv_ack(Pkt *pdesc)
{
    Interface *ip;
    AckPkt *apkt;
    LShdr *hdr;

    if (n_state < NBS_EXCHANGE)
        return;

    ip = n_ifc;
    apkt = (AckPkt *) pdesc->spfpkt;
    hdr = (LShdr *) (apkt+1);

    for (; ((byte *)(hdr+1)) <= pdesc->end; hdr++)
    {
        int lstype;
        lsid_t lsid;
        rtid_t orig;
        LSA *lsap;
        int compare;

        lstype = hdr->ls_type;
        lsid = ntoh32(hdr->ls_id);
        orig = ntoh32(hdr->ls_org);
        // 将接收到的ACK与LSA重发表中的LSA比较
        lsap = ospf->FindLSA(ip, lstype, lsid, orig);
        compare = lsap ? lsap->cmp_instance(hdr) : 1;
        // 如果受到的ACK更新，从LSA重发中的LSA删除
        if(compare==0)
            remove_from_retranslist(lsap);
    }
}

//清空重发表
void Neighbor::clear_retrans_list()

{
    LsaListIterator iter1(&pend_retrans);
    LsaListIterator iter2(&retrans);
    LsaListIterator iter3(&failed_retrans);
    LSA *lsap;

    while ((lsap = iter1.get_next()))
    {
        lsap->lsa_rxmt--;
        iter1.remove_current();
    }
    while ((lsap = iter2.get_next()))
    {
        lsap->lsa_rxmt--;
        iter2.remove_current();
    }
    while ((lsap = iter3.get_next()))
    {
        lsap->lsa_rxmt--;
        iter3.remove_current();
    }

    retrans_cnt = 0;
    retrans_window = 1;
    lsat.stop();
}

//将LSA加入到最近重发LSA列表，这些还没有到下次重发的时间
void Neighbor::add_to_retranslist(LSA *lsap)
{
    retrans.addEntry(lsap);
    lsap->lsa_rxmt++;
    retrans_cnt++;

    // 如果是第一个加入，启动重发计时器
    if (retrans_cnt == 1)
        lsat.start(n_ifc->rxmt_interval()*Timer::SECOND);
}

//将LSA从重发表中删除
bool Neighbor::remove_from_retranslist(LSA *lsap)
{
    bool retcd;

    if (retrans.remove(lsap))
        retcd = true;
    else if (remove_from_pending_retrans(lsap))
        retcd = true;
    else if (failed_retrans.remove(lsap))
        retcd = true;
    else
        return(false);

    lsap->lsa_rxmt--;
    retrans_cnt--;
    return(retcd);
}

//从等待重发表中删掉LSA
bool Neighbor::remove_from_pending_retrans(LSA *lsap)

{
    if (!pend_retrans.remove(lsap))
        return(false);
    if (pend_retrans.is_empty())
    {
        retrans_window = retrans_window << 1;
        if (retrans_window > ospf->max_retrans_window)
            retrans_window = ospf->max_retrans_window;
        lsat.stop();
        retrans_updates();
    }
    return(true);
}

void LSATimer::action()

{
    LsaList *list;
    uns32 nexttime;

    // 等待重发表移到重发过等待确认的列表
    np->failed_retrans.append(&np->pend_retrans);
    // 如果仍有LSA需要重发，则设置窗口为1
    if (np->get_next_retrans(list, nexttime))
    {
        np->retrans_window = 1;
    }
    np->retrans_updates();
    // 删除一些已经失效的LSA
    np->retrans_cnt -= np->retrans.garbage_collect();
    np->retrans_cnt -= np->failed_retrans.garbage_collect();
}

//从重发列表中得到下一个LSA
LSA *Neighbor::get_next_retrans(LsaList * &list, uns32 &nexttime)

{
    byte interval;
    LSA *lsap=0;

    nexttime = 0;
    interval = n_ifc->rxmt_interval();
    do
    {
        if (!nexttime && (lsap = retrans.FirstEntry()))
        {
            if (lsap->valid() &&
                    lsap->since_received() < interval)
            {
                nexttime = interval - lsap->since_received();
                lsap = 0;
            }
            list = &retrans;
        }
        else if ((lsap = failed_retrans.FirstEntry()))
            list = &failed_retrans;
        else
            return(0);
        // 验证lsa有效
        if (lsap && !lsap->valid())
        {
            list->remove(lsap);
            retrans_cnt--;
            lsap = 0;
        }
    }
    while (lsap == 0);

    return(lsap);
}

//重发LS Update
void Neighbor::retrans_updates()
{
    int space;
    LSA *lsap;
    LShdr *hdr=0;
    int npkts;
    LsaList *list;
    uns32 nexttime;

    space = 0;
    npkts = 0;

    // 从LS重发表得到要发的LSA
    while ((lsap =  get_next_retrans(list, nexttime)))
    {
        bool full;
        // 是否还有剩余空间
        full = space < lsap->ls_length();
        if (full && ++npkts > retrans_window)
            break;

        hdr = ospf->BuildLSA(lsap);
        space = add_to_update(hdr);

        // 将LSA移动到等待重发的列表
        list->remove(lsap);
        pend_retrans.addEntry(lsap);
    }

    if (npkts > 0)
    {
        // 向邻居发送update
        n_ifc->Neighbor_send(&n_update, this);
        // 重启重发计时器
        lsat.start(n_ifc->rxmt_interval()*Timer::SECOND);
    }
    else if (nexttime)
    {
        lsat.start(nexttime*Timer::SECOND);
    }
}

//组建ACK
void Interface::if_build_ack(LShdr *hdr, Pkt *pkt, Neighbor *np)

{
    if (!pkt)
        pkt = &if_dack;
    // 如果已经没有多的空间可以添加LSA，直接发送当前的ACK
    if (pkt->iphead && (pkt->dptr + sizeof(LShdr)) > pkt->end)
    {
        if (np)
            Neighbor_send(pkt, np);
        else
            if_send(pkt, if_flood);
    }

    if (!pkt->iphead)
    {
        if (ospf->getpkt(pkt, SPT_LSACK, mtu) == 0)
            return;
    }
    // 将要确认的LSA的头部加入到包中
    memcpy(pkt->dptr, hdr, sizeof(LShdr));
    pkt->dptr += sizeof(LShdr);
}

void DAckTimer::action()
{
    ifc->if_send_dack();
}

bool Neighbor::changes_pending()

{
    LsaListIterator iter1(&pend_retrans);
    LsaListIterator iter2(&retrans);
    LsaListIterator iter3(&failed_retrans);
    LSA *lsap;

    while ((lsap = iter1.get_next()))
    {
        if (lsap->valid() && lsap->changed)
            return(true);
    }
    while ((lsap = iter2.get_next()))
    {
        if (lsap->valid() && lsap->changed)
            return(true);
    }
    while ((lsap = iter3.get_next()))
    {
        if (lsap->valid() && lsap->changed)
            return(true);
    }

    return(false);
}
