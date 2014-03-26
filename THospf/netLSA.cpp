#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"

netLSA::netLSA(LShdr *hdr, int blen) : TNode(hdr, blen)
{
}
//生成
void netLSA::reoriginate(int forced)
{
    Interface *ip;

    if ((ip = ospf->find_ifc(ls_id())) == 0)
        lsa_flush(this);
    else
        ip->nl_orig(forced);
}
//解析头部
void netLSA::parse(LShdr *hdr)
{
    uns32 net;
    uns32 mask;
    NetLShdr *nethdr;
    int len;
    rtid_t *idp;
    Link *lp;
    TLink **tlpp;
    TLink *tlp;
    Link *nextl;

    nethdr = (NetLShdr *) (hdr + 1);
    mask = ntoh32(nethdr->netmask);
    net = ls_id() & mask;

    if (!(t_dest = inrttbl->add(net, mask)))
        exception = true;

    tlpp = (TLink **) &t_links;

    len = ntoh16(hdr->ls_length);
    len -= sizeof(LShdr);
    len -= sizeof(NetLShdr);
    idp = (rtid_t *) (nethdr + 1);
    for (; len >= (int) sizeof(rtid_t); tlpp = (TLink **)&tlp->l_next)
    {
        rtid_t rtid;
        rtid = ntoh32(*idp);
        if (!(tlp = *tlpp))
        {
            tlp = new TLink;
            *tlpp = tlp;
        }
        tlp->tl_nbr = 0;
        tlp->l_ltype = LT_PP;
        tlp->l_id = rtid;
        tlp->l_fwdcst = 0;
        tlp->tl_rvcst = MAX_COST;
        tlp_link(tlp);
        len -= sizeof(rtid_t);

        idp++;
    }

    if (len != 0)
        exception = true;

    lp = *tlpp;
    *tlpp = 0;
    for (; lp; lp = nextl)
    {
        nextl = lp->l_next;
        delete lp;
    }
}
//反解析
void netLSA::unparse()
{
    t_dest = 0;
    unlink();
}
//组建netLSA
void netLSA::build(LShdr *hdr)
{
    rtid_t *idp;
    NetLShdr *nethdr;
    Link *lp;

    nethdr = (NetLShdr *) (hdr + 1);
    nethdr->netmask = hton32(t_dest->index2());

    // 填入路由
    idp = (rtid_t *) (nethdr + 1);
    for (lp = t_links; lp; lp = lp->l_next, idp++)
	*idp = hton32(lp->l_id);
}

void netLSA::delete_actions()

{
    netLSA *olsap;
    Table *tree;

    //重新解析具有相同LS ID的network lsa
    tree = ospf->FindLSdb(0,  LST_NET);
    olsap = (netLSA *) tree->previous(ls_id()+1, 0);
    while (olsap)
    {
        LShdr *hdr;
        if (olsap->ls_id() != ls_id())
            break;
        else if (olsap != this)
        {
            hdr = ospf->BuildLSA(olsap);
            olsap->parse(hdr);
            break;
        }
        olsap = (netLSA *) tree->previous(ls_id(), adv_rtr());
    }
}

//端口生成network lsa
void Interface::nl_orig(int forced)

{
    LSA *olsap;
    LShdr *hdr;

    olsap = ospf->myLSA(0, LST_NET, if_addr);
    hdr = nl_raw_orig();

    if (hdr == 0)
        lsa_flush(olsap);
    else
    {
        seq_t seqno;
        seqno = ospf->ospf_get_seqno(LST_NET, olsap, forced);
        if (seqno != InvalidLSSeq)
        {
            hdr->ls_seqno = hton32(seqno);
            (void) ospf->lsa_reorig(0, olsap, hdr, forced);
        }
        ospf->free_orig_buffer(hdr);
    }
}

LShdr *Interface::nl_raw_orig()

{
    LShdr *hdr;
    NetLShdr *nethdr;
    uns16 length;
    rtid_t *nbr_ids;
    NbrList iter(this);
    Neighbor *np;

//只有路由器本身是网络的指定路由器，且有一个或更多的完全相邻路由
//器都与接口连接时，路由器才创建一个network-LSA。如果另外一个路由器
//是指定路由器，则本路由器的router-LSA，会指向那个指定路由器创建的
//network-LSA。如果不存在完全邻接的相邻路由器,则网络被当作一个STUB
//在router-LSA 中被通告。如果不打算创建一个network-LSA，则必须清除
//以前创建的任何network-LSA。
    if (if_state != IFS_DR)
        return(0);
    else if (if_fullcnt == 0)
        return(0);

    // 组建新的LSA
    length = sizeof(NetLShdr) + (if_fullcnt+1)*sizeof(rtid_t);
    length += sizeof(LShdr);
    // 填入内容
    hdr = ospf->orig_buffer(length);
    hdr->ls_opts |= SPO_EXT;
    hdr->ls_type = LST_NET;
    hdr->ls_id = hton32(if_addr);
    hdr->ls_org = hton32(ospf->my_id());
    hdr->ls_length = hton16(length);

    nethdr = (NetLShdr *) (hdr + 1);
    nethdr->netmask = hton32(if_mask);
    // 填入FULL状态的邻居ID
    nbr_ids = (rtid_t *) (nethdr + 1);
    *nbr_ids = hton32(ospf->my_id());
    while ((np = iter.getnext()) != 0)
    {
        if (np->adv_as_full())
            *(++nbr_ids) = hton32(np->id());
    }
    return(hdr);
}
