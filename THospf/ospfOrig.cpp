#include "include.h"
#include "system.h"


//生成LSA
int OSPF::originated(Neighbor *np, LShdr *hdr, LSA *database_copy)
{
    LSA *lsap;
    bool flush_it;
    Interface *ip;
    age_t lsage;

    ip = np->n_ifc;

    if ((ntoh32(hdr->ls_org) != my_id()) && (hdr->ls_type != LST_NET || !find_ifc(ntoh32(hdr->ls_id))))
        return(false);
    // 收到更新的自己的LSA
    flush_it = (!database_copy) || database_copy->lsa_age() == MaxAge;
    lsage = ntoh16(hdr->ls_age);
    hdr->ls_age = hton16(lsage & ~DoNotAge);
    lsap = AddLSA(ip,database_copy, hdr, true);

    if (ntoh32(hdr->ls_org) != my_id() || flush_it)
    {
        lsa_flush(lsap);
    }
    else if (ntoh32(hdr->ls_seqno) == (seq_t) MaxLSSeq)
    {
        lsap->rollover = true;
        lsa_flush(lsap);
    }
    else
    {
        lsap->flood(np, hdr);
        lsap->reoriginate(true);
    }

    return(true);
}

//得到一个序列号，如果要创建的LSA时间间隔小于最小时间，
//则推迟创建，返回一个无效的序列号
seq_t OSPF::ospf_get_seqno(byte lstype, LSA *lsap, int forced)
{

    if (!lsap || !lsap->valid())
        return(InitLSSeq);

    if ((!forced) && lsap->in_agebin &&
            (lsap->is_aging() && lsap->lsa_age() < MinLSInterval))
    {
        lsap->deferring = true;
        return(InvalidLSSeq);
    }
    if (lsap->ls_seqno() == MaxLSSeq)
    {
        lsap->rollover = true;
        age_prematurely(lsap);
        return(InvalidLSSeq);
    }

    //一般情况，序号加1
    return(lsap->ls_seqno() + 1);
}

int LSA::refresh(seq_t seqno)

{
    lsa_seqno = seqno;
    reoriginate(true);
    return(true);
}

//重新生成LSA
LSA *OSPF::lsa_reorig(Interface *ip,LSA *olsap,LShdr *hdr,int forced)
{
    int changes;
    LSA *lsap;
    //年龄为0
    hdr->ls_age = 0;
    changes = (olsap ? olsap->cmp_contents(hdr) : true);
    //如果不是强制长生LSA，而且数据库中有这个LSA的副本，则不产生LSA
    if (!changes && !forced && olsap->do_not_age() == hdr->do_not_age())
    {
        olsap->we_orig = true;
        return(0);
    }
    hdr->generate_cksum();
    //加入到数据库，并泛洪发送
    lsap = AddLSA(ip, olsap, hdr, changes);
    lsap->we_orig = true;
    lsap->flood(0, hdr);
    return(lsap);
}


void lsa_flush(LSA *lsap)
{
    if (!lsap)
        return;
    if (lsap->valid() && lsap->lsa_age() != MaxAge)
    {
        ospf->age_prematurely(lsap);
    }
}

//让一个LSA永久过期。将LSA年龄设为最大，然后加入到数据库。
void OSPF::age_prematurely(LSA *lsap)

{
    LShdr *hdr;
    LSA *nlsap;

    hdr = orig_buffer(lsap->lsa_length);
    hdr = ospf->BuildLSA(lsap, hdr);
    hdr->ls_age = hton16(MaxAge);
    hdr->generate_cksum();

    //加入到数据库，并泛洪发送
    nlsap = AddLSA(lsap->lsa_ifp, lsap, hdr, true);
    nlsap->flood(0, hdr);
    free_orig_buffer(hdr);
}
//按长度分配LS缓冲区
LShdr *OSPF::orig_buffer(int ls_len)
{
    // 生成的缓冲区是否被占用
    if (orig_buff_in_use)
    {
        n_orig_allocs++;
        return((LShdr *) new byte[ls_len]);
    }
    //如果超过了当前缓冲区尺寸，分配一个更大的
    if (ls_len > orig_size)
    {
        orig_size = ls_len;
        delete [] orig_buff;
        orig_buff = new byte[orig_size];
    }

    orig_buff_in_use = true;
    return((LShdr *)orig_buff);
}

    //释放缓冲区
void OSPF::free_orig_buffer(LShdr *hdr)
{
    if (((byte *)hdr) != orig_buff)
        delete [] ((byte *)hdr);
    else
        orig_buff_in_use = false;
}
