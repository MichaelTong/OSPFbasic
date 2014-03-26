#include "include.h"
#include "InterfaceFSM.h"
#include "NeighborFSM.h"

rtrLSA::rtrLSA(LShdr *hdr, int blen) : TNode(hdr, blen)
{
    rtype = 0;
}
int Interface::rl_size()

{
    return(sizeof(RtrLink));
}

bool Neighbor::adv_as_full()
{
    if (n_state == NBS_FULL)
        return(true);
    return(false);
}

//如果端口状态是DR、并且完全邻接数大于0；
//或者端口类型是BDR、其他类型，并且存在FULL状态的DR，
//则加入Transit类型LSA
//否则加入STUB类型LSA
RtrLink *DRIfc::rl_insert(RTRhdr *rtrhdr, RtrLink *rlp)
{
    if ((if_state == IFS_DR && if_fullcnt > 0) ||
            ((if_state == IFS_BACKUP || if_state == IFS_OTHER) &&
             (if_dr_n && if_dr_n->adv_as_full())))
    {
        rlp->link_id = hton32(if_dr);
        rlp->link_data = hton32(if_addr);
        rlp->link_type = LT_TNET;
        rlp->n_tos = 0;
        rlp->metric = hton16(if_cost);
        ospf->add_to_ifmap(this);
        rlp++;
        rtrhdr->nlinks++;
    }
    else
    {
        rlp->link_id = hton32(if_net);
        rlp->link_data = hton32(if_mask);
        rlp->link_type = LT_STUB;
        rlp->n_tos = 0;
        rlp->metric = hton16(if_cost);
        ospf->add_to_ifmap(this);
        rlp++;
        rtrhdr->nlinks++;
    }
    return(rlp);
}
//生成Router LSA
void rtrLSA::reoriginate(int forced)
{
    ospf->rl_orig(forced);
}

//解析Router LSA
void rtrLSA::parse(LShdr *hdr)
{
    RTRhdr  *rhdr;
    RtrLink *rtlp;
    Link *lp;
    Link **lpp;
    byte *end;
    int i;
    Link *nextl;
    TOSmetric *mp;

    rhdr = (RTRhdr *) (hdr+1);
    rtlp = (RtrLink *) (rhdr+1);
    n_links = ntoh16(rhdr->nlinks);
    end = ((byte *) hdr) + ntoh16(hdr->ls_length);

    rtype = rhdr->rtype;
    t_dest = ospf->add_abr(ls_id());
    t_dest->changed = true;
    if (rhdr->zero != 0)
        exception = true;

    lpp = &t_links;
    for (i = 0; i < n_links; i++)
    {
        if (((byte *) rtlp) > end)
        {
            exception = true;
            break;
        }
        if ((!(lp = *lpp)) || lp->l_ltype != rtlp->link_type)
        {
            Link *nextl;
            nextl = lp;
            if (rtlp->link_type == LT_STUB)
                lp = new SLink;
            else
                lp = new TLink;
            *lpp = lp;
            lp->l_next = nextl;
        }
        lp->l_ltype = rtlp->link_type;
        lp->l_id = ntoh32(rtlp->link_id);
        lp->l_fwdcst = ntoh16(rtlp->metric);
        lp->l_data = ntoh32(rtlp->link_data);
        switch (rtlp->link_type)
        {
            IPrte *srte;
            TLink *tlp;
        case LT_STUB:
            srte = inrttbl->add(lp->l_id, lp->l_data);
            ((SLink *) lp)->sl_rte = srte;
            break;
        case LT_PP:
        case LT_TNET:
        case LT_VL:
            tlp = (TLink *) lp;
            tlp->tl_nbr = 0;
            tlp->tl_rvcst = MAX_COST;
            tlp_link(tlp);
            break;
        default:
            break;
        }
        if (rtlp->n_tos)
            exception = true;
        lpp = &lp->l_next;
        mp = (TOSmetric *)(rtlp+1);
        mp += rtlp->n_tos;
        rtlp = (RtrLink *)mp;
    }

    if (((byte *) rtlp) != end)
        exception = true;

    lp = *lpp;
    *lpp = 0;
    for (; lp; lp = nextl)
    {
        nextl = lp->l_next;
        delete lp;
    }
}

void rtrLSA::unparse()
{
    unlink();
}
//组建Router LSA
void rtrLSA::build(LShdr *hdr)
{
    RTRhdr *rtrhdr;
    RtrLink *rtrlink;
    Link *lp;

    rtrhdr = (RTRhdr *) (hdr + 1);
    rtrhdr->rtype = rtype;
    rtrhdr->zero = 0;
    rtrhdr->nlinks = hton16(n_links);

    rtrlink = (RtrLink *) (rtrhdr + 1);
    for (lp = t_links; lp; lp = lp->l_next, rtrlink++)
    {
        rtrlink->link_id = hton32(lp->l_id);;
        rtrlink->link_data = hton32(lp->l_data);
        rtrlink->link_type = lp->l_ltype;
        rtrlink->n_tos = 0;
        rtrlink->metric = hton16(lp->l_fwdcst);
    }
}
void TNode::tlp_link(TLink *tlp)

{
    TNode *nbr;
    uns32 nbr_id;
    Link *nlp;

    nbr_id = tlp->l_id;

    if (tlp->l_ltype == LT_TNET)
    {
        nbr = (TNode *) ospf->netLSAs.previous(nbr_id+1);
        if (nbr == 0 || nbr->ls_id() != nbr_id)
            return;
    }
    else if (!(nbr = (TNode *) ospf->rtrLSAs.find(nbr_id, nbr_id)))
        return;

    if (!nbr->parsed)
        return;
    if (nbr->ls_type() == LST_RTR)
        nbr->t_dest->changed = true;
    for (nlp = nbr->t_links; nlp; nlp = nlp->l_next)
    {
        TLink *ntlp;
        if (nlp->l_ltype == LT_STUB)
            continue;
        if (nlp->l_id != ls_id())
            continue;
        if (nlp->l_ltype == LT_TNET && lsa_type != LST_NET)
            continue;
        if (nlp->l_ltype != LT_TNET && lsa_type == LST_NET)
            continue;
        ntlp = (TLink *) nlp;
        tlp->tl_nbr = nbr;
        ntlp->tl_nbr = this;
        if (ntlp->l_fwdcst < tlp->tl_rvcst)
            tlp->tl_rvcst = ntlp->l_fwdcst;
        if (tlp->l_fwdcst < ntlp->tl_rvcst)
            ntlp->tl_rvcst = tlp->l_fwdcst;
    }
}

void TNode::unlink()

{
    Link *lp;

    for (lp = t_links; lp; lp = lp->l_next)
    {
        TLink *tlp;
        TNode *nbr;
        Link *nlp;
        if (lp->l_ltype == LT_STUB)
            continue;
        tlp = (TLink *) lp;
        if (!(nbr = tlp->tl_nbr))
            continue;
        if (nbr->ls_type() == LST_RTR)
            nbr->t_dest->changed = true;
        for (nlp = nbr->t_links; nlp; nlp = nlp->l_next)
        {
            TLink *ntlp;
            if (nlp->l_ltype == LT_STUB)
                continue;
            ntlp = (TLink *) nlp;
            if (ntlp->l_id != ls_id())
                continue;
            if (ntlp->l_ltype == LT_TNET && lsa_type != LST_NET)
                continue;
            if (ntlp->l_ltype != LT_TNET && lsa_type == LST_NET)
                continue;
            ntlp->tl_nbr = 0;
            ntlp->tl_rvcst = MAX_COST;
        }
        tlp->tl_nbr = 0;
    }
}
//创建Router LSA
void OSPF::rl_orig(int forced)
{
    LSA *olsap;
    LShdr *hdr;
    RTRhdr *rtrhdr;
    int maxlen;
    int maxifc;
    int length;
    Interface *ip;
    IfcList iiter(this);
    seq_t seqno;
    RtrLink *rlp;

    // 得到当前LSA数据库
    olsap = ospf->myLSA(0,LST_RTR, ospf->my_id());

    // 计算LSA最大长度
    maxlen = sizeof(LShdr) + sizeof(RTRhdr);
    while ((ip = iiter.get_next()))
        maxlen += ip->rl_size();

    // 得到一个序列号
    seqno = ospf->ospf_get_seqno(LST_RTR, olsap, forced);
    if (seqno == InvalidLSSeq)
        return;

    maxifc = maxlen/sizeof(RtrLink);
    if (maxifc > size_ifmap)
    {
        delete [] ifmap;
        ifmap = new Interface* [maxifc];
        size_ifmap = maxifc;
    }


    n_ifmap = 0;
    ifmap_valid = true;

    // 组建LSA头部
    hdr = ospf->orig_buffer(maxlen);
    hdr->ls_opts =0;
    hdr->ls_opts |= SPO_EXT;
    hdr->ls_type = LST_RTR;
    hdr->ls_id = hton32(ospf->my_id());
    hdr->ls_org = hton32(ospf->my_id());
    hdr->ls_seqno = (seq_t) hton32((uns32) seqno);
    // 填入Router lsa 部分
    rtrhdr = (RTRhdr *) (hdr+1);
    rtrhdr->rtype = 0;
    rtrhdr->zero = 0;
    rtrhdr->nlinks = 0;

    rlp = (RtrLink *) (rtrhdr+1);
    iiter.reset();
    //遍历端口，加入与端口邻接的路由信息
    while ((ip = iiter.get_next()))
    {
        if (ip->state() == IFS_DOWN)
            continue;
        else if (ip->state() == IFS_LOOP)
        {
            rlp->link_id = hton32(ip->if_addr);
            rlp->link_data = hton32(0xffffffffL);
            rlp->link_type = LT_STUB;
            rlp->n_tos = 0;
            rlp->metric = hton16(ip->cost());
            add_to_ifmap(ip);
            rlp++;
            rtrhdr->nlinks++;
        }
        else
            rlp = ip->rl_insert(rtrhdr, rlp);
    }

    //如果没有活动接口，则清除LSA。
    if (rtrhdr->nlinks == 0)
    {
        lsa_flush(olsap);
        delete [] ifmap;
        ifmap = 0;
        size_ifmap = 0;
        ospf->free_orig_buffer(hdr);
        return;
    }


    length = ((byte *) rlp) - ((byte *) hdr);
    hdr->ls_length = hton16(length);
    rtrhdr->nlinks = hton16(rtrhdr->nlinks);
    ospf->lsa_reorig(0,olsap,hdr,forced);
    ospf->free_orig_buffer(hdr);
}

//加入端口表
void OSPF::add_to_ifmap(Interface *ip)

{
    ifmap[n_ifmap++] = ip;
}

void OSPF::delete_from_ifmap(Interface *ip)

{

    int i;
    for (i= 0; i < n_ifmap; i++)
        if (ifmap[i] == ip)
        {
            ifmap[i] = 0;
            ifmap_valid = false;
        }

}
