#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"
#include "system.h"
//接收Update处理
void Neighbor::recv_update(Pkt *pdesc)

{
    Interface *ip;
    int count;
    UpdPkt *upkt;
    LShdr *hdr;
    byte *end_lsa;

    if (n_state < NBS_EXCHANGE)
    {
        if (n_ifc->type() == IFT_PP)
            n_ifc->send_hello(true);
        return;
    }

    ip = n_ifc;
    upkt = (UpdPkt *) pdesc->spfpkt;
    ip->recv_update = true;
    //LSA个数
    count = ntoh32(upkt->upd_no);
    //第一个LSA头部
    hdr = (LShdr *) (upkt+1);

    for (; count > 0; count--, hdr = (LShdr *) end_lsa)
    {
        int errval=0;
        int lslen;
        int lstype;
        lsid_t lsid;
        rtid_t orig;
        age_t lsage;
        LSA *olsap;
        int compare;
        int rq_cmp;

        lstype = hdr->ls_type;
        lsage = ntoh16(hdr->ls_age);
        if ((lsage & ~DoNotAge) >= MaxAge)
            lsage = MaxAge;
        lslen = ntoh16(hdr->ls_length);
        end_lsa = ((byte *)hdr) + lslen;
        if (end_lsa > pdesc->end)
            break;
        //校验
        if (!hdr->verify_cksum())
            errval = 1;
        //获取某种类型LSA的数据库
        else if (!ospf->FindLSdb(ip, lstype))
            errval = 2;

        if (errval != 0)
        {
            continue;
        }

        //找到当前的数据库副本
        lsid = ntoh32(hdr->ls_id);
        orig = ntoh32(hdr->ls_org);
        olsap = ospf->FindLSA(ip,lstype, lsid, orig);

        //如果数据库为空或已经到达最大时间，就发送ACK并释放LSA
        if (lsage == MaxAge && (!olsap) && ospf->maxage_free(lstype))
        {
            build_imack(hdr);
            continue;
        }

        /*与LS数据库中的LSA比较，1表示接收到的更新，0表示一样，-1 表示接收到的比较旧*/
        compare = (olsap ? olsap->cmp_instance(hdr) : 1);

        /*收到的LSA更新*/
        if (compare > 0)
        {
            bool changes;
            LSA *lsap;
            if (olsap && olsap->since_received() < MinArrival)
            {
                if (olsap->min_failed)
                {
                    continue;
                }
            }
            //如果是自己长生的LSA，强制重新创建
            changes = (olsap ? olsap->cmp_contents(hdr) : true);
            if (changes && ospf->originated(this, hdr, olsap))
                continue;
            //否则加入数据库
            lsap = ospf->AddLSA(ip,olsap, hdr, changes);
            //泛洪发送
            lsap->flood(this, hdr);
        }
        else if (ospf_rmreq(hdr, &rq_cmp))
        //数据库交换出错
        {
            nbr_fsm(NBE_BADLSREQ);
        }
        else if (compare == 0)
        {
            if (!remove_from_retranslist(olsap))
                build_imack(hdr);
        }
        else
        {
            LShdr *ohdr;
            //更新数据库副本
            if (olsap->ls_seqno() == MaxLSSeq)
                continue;
            if (olsap->sent_reply)
                continue;
            ohdr = ospf->BuildLSA(olsap);
            add_to_update(ohdr);
            olsap->sent_reply = true;
            ospf->replied_list.addEntry(olsap);
        }
    }
    //从其他接口泛洪出去
    ospf->send_updates();
    ip->Neighbor_send(&n_imack, this);
    ip->Neighbor_send(&n_update, this);
    ip->recv_update = false;
    //如果有必要，继续发送请求
    if (reqlist.count() && reqlist.count() <= req_goal)
    {
        lsrt.restart();
        send_req();
    }
}
bool OSPF::maxage_free(byte lstype)
{
    if (n_dbx_nbrs == 0)
        return(true);
    else
        return(false);
}
//泛洪，需要用重传
void LSA::flood(Neighbor *from, LShdr *hdr)
{
    IfcList ifcIter(ospf);
    Interface *r_ip;
    Interface *ip;
    bool flood_it=false;
    bool on_demand=false;
    bool on_regular=false;
    //得到要发送的接口
    r_ip = (from ? from->ifc() : 0);
    if (!hdr)
        hdr = ospf->BuildLSA(this);
    //遍历系统所有接口的邻居，记录需要发送的邻居
    while ((ip = ifcIter.get_next()))
    {
        Neighbor *np;
        NbrList nbrIter(ip);
        int n_nbrs;
        n_nbrs = 0;
        while ((np = nbrIter.getnext()))
        {
            int rq_cmp;

            if (np->state() < NBS_EXCHANGE)
                continue;
            if (np->ospf_rmreq(hdr, &rq_cmp) && rq_cmp <= 0)
                continue;
            if (np == from)
                continue;

            n_nbrs++;
            np->add_to_retranslist(this);
        }
        //向update加入需要发送的LSA
        if (ip == r_ip && (ip->state() == IFS_DR && !from->is_bdr() && n_nbrs != 0))
        {
            ip->add_to_update(hdr);
        }
        else if ((r_ip == 0 && n_nbrs != 0)&&ip->recv_update)
            ip->add_to_update(hdr);
        else if (ip != r_ip)
        {
            if (n_nbrs == 0)
                continue;
            flood_it = true;
            ip->flood=true;
            on_regular = true;
        }
        else
            ip->if_build_dack(hdr);
    }

    if (!flood_it)
        return;
    else
    {
        if (on_regular)
            ospf->add_to_update(hdr, false);
        if (on_demand)
            ospf->add_to_update(hdr, true);
    }
}

int Neighbor::add_to_update(LShdr *hdr)
{
    int lsalen;
    Pkt *pkt;

    lsalen = ntoh16(hdr->ls_length);
    pkt = &n_update;
    // 没有空间可以加入LSA，则发送
    if (pkt->iphead && (pkt->dptr + lsalen) > pkt->end)
        n_ifc->Neighbor_send(pkt, this);
    // 组建update
    ospf->build_update(pkt, hdr, n_ifc->mtu, false);
    // 返回剩余空间
    return((int)(pkt->end - pkt->dptr));

}

int Interface::add_to_update(LShdr *hdr)

{
    int lsalen;
    Pkt *pkt;

    lsalen = ntoh16(hdr->ls_length);
    pkt = &if_update;
    // 没有空间可以加入LSA，则发送
    if (pkt->iphead && (pkt->dptr + lsalen) > pkt->end)
        if_send(pkt, if_flood);
    // 组建update
    ospf->build_update(pkt, hdr, mtu, false);
    // 返回剩余空间
    return((int)(pkt->end - pkt->dptr));
}

void OSPF::add_to_update(LShdr *hdr, bool demand_upd)
{
    int lsalen;
    Pkt *pkt;

    lsalen = ntoh16(hdr->ls_length);
    pkt = demand_upd ? &a_demand_upd : &a_update;
    pkt->hold = true;
    // 没有空间可以加入LSA，则发送
    if (pkt->iphead && (pkt->dptr + lsalen) > pkt->end)
    {
        Interface *ip;
        IfcList iter(this);
        while ((ip = iter.get_next()))
        {
            if (!demand_upd)
                ip->if_send(pkt, ip->if_flood);
        }
        pkt->hold = false;
        ospf->freepkt(pkt);
        pkt->hold = true;
    }
    // 组建update
    ospf->build_update(pkt, hdr, ospf_mtu, demand_upd);
}

//组建update
void OSPF::build_update(Pkt *pkt, LShdr *hdr, uns16 mtu, bool demand)
{
    int lsalen;
    UpdPkt *upkt;
    age_t c_age, new_age;
    LShdr *new_hdr;
    int donotage;

    lsalen = ntoh16(hdr->ls_length);
    if (!pkt->iphead)
    {
        uns16 size;
        size = MAX(lsalen+sizeof(InPkt)+sizeof(UpdPkt), mtu);
        if (getpkt(pkt, SPT_UPD, size) == 0)
            return;
        upkt = (UpdPkt *) (pkt->spfpkt);
        upkt->upd_no = 0;
        pkt->dptr = (byte *) (upkt + 1);
    }

    upkt = (UpdPkt *) (pkt->spfpkt);
    upkt->upd_no = hton32(ntoh32(upkt->upd_no) + 1);
    new_hdr = (LShdr *) pkt->dptr;

    c_age = ntoh16(hdr->ls_age);
    donotage = (c_age & DoNotAge) != 0;
    c_age &= ~DoNotAge;
    new_age = MIN(MaxAge, c_age + 1);
    if (new_age < MaxAge && (donotage || demand))
        new_age |= DoNotAge;
    new_hdr->ls_age = hton16(new_age);
    // 将剩余的LSA拷贝到update中
    memcpy(&new_hdr->ls_opts, &hdr->ls_opts, lsalen - sizeof(age_t));
    pkt->dptr += lsalen;
}

//发送update
void OSPF::send_updates()
{

    Interface *ip;
    IfcList iter(this);

    while((ip=iter.get_next()))
        ip->if_send_update();

    Pkt *pkt;
    if(a_demand_upd.partial_checksum() || a_update.partial_checksum())
    {
        while ((ip = iter.get_next()))
        {
            if (!ip->flood)
                continue;
            pkt = &a_update;
            ip->if_send(pkt, ip->if_flood);
            ip->flood = false;
        }

        a_demand_upd.hold = false;
        freepkt(&a_demand_upd);
        a_update.hold = false;
        freepkt(&a_update);
    }
}

//移除req
int Neighbor::ospf_rmreq(LShdr *hdr, int *compare)
{
    byte ls_type;
    lsid_t ls_id;
    rtid_t adv_rtr;
    LsaListIterator iter(&reqlist);
    LSA *lsap;

    ls_type = hdr->ls_type;
    ls_id = ntoh32(hdr->ls_id);
    adv_rtr = ntoh32(hdr->ls_org);

    if (!(lsap = iter.search(ls_type, ls_id, adv_rtr)))
        return(0);
    if ((*compare = lsap->cmp_instance(hdr)) >= 0)
    {
        iter.remove_current();
        if (reqlist.is_empty())
        {
            lsrt.stop();
            if (n_state == NBS_LOADING)
                nbr_fsm(NBE_LOADINGDONE);
            else
                send_dd();
        }
    }
    return(1);
}
