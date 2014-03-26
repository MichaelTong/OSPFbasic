#include "include.h"
#include "NeighborFSM.h"
#include "system.h"
LSA *LSA::AgeBins[MaxAge+1];	// �ϻ�����
int LSA::Bin0;			// ����Ϊ0��Ԫ��
int32 LSA::RefreshBins[MaxAgeDiff];// ˢ�¶���
int LSA::RefreshBin0;		// ��ǰˢ�µ�Ԫ��

Neighbor *GetNextAdj()

{
    Neighbor *np;

    if (!(np = ospf->g_adj_head))
        return(0);
    if (!(ospf->g_adj_head = np->next_pend))
        ospf->g_adj_tail = 0;

    np->n_adj_pend = false;
    return(np);
}

LSA::LSA(Interface *ip, LShdr *lshdr, int blen)
    : TblItem(ntoh32(lshdr->ls_id), ntoh32(lshdr->ls_org)),
      lsa_type(lshdr->ls_type)

{
    Table *table;

    hdr_parse(lshdr);
    lsa_body = 0;
    lsa_ifp = ip;
    lsa_agefwd = 0;
    lsa_agerv = 0;
    lsa_agebin = 0;
    lsa_rxmt = 0;

    in_agebin = false;
    deferring = false;
    changed = true;
    exception = false;
    rollover = false;
    e_bit = false;
    parsed = false;
    sent_reply = false;
    checkage = false;
    min_failed = false;
    we_orig = false;

    if (blen)
    {
        //���뵽���ݿ�
        table = ospf->FindLSdb(ip,lsa_type);
        table->add((TblItem *) this);
    }
}

LSA::~LSA()

{
    delete [] lsa_body;
}

void LSA::parse(LShdr *)
{
}
void LSA::unparse()
{
}
void LSA::build(LShdr *)
{
}
void LSA::delete_actions()
{
}
RtrTblEntry *LSA::rtentry()
{
    return(0);
}



TNode::TNode(LShdr *lshdr, int blen) : LSA(0, lshdr, blen), Q_Elt()
{
    t_links = 0;
    dijk_run = ospf->n_dijkstras & 1;
    t_state = DS_UNINIT;
    t_epath = 0;
}
//����ͷ��
void LSA::hdr_parse(LShdr *lshdr)
{
    lsa_rcvage = ntoh16(lshdr->ls_age);
    if ((lsa_rcvage & ~DoNotAge) >= MaxAge)
        lsa_rcvage = MaxAge;
    lsa_opts = lshdr->ls_opts;
    lsa_seqno = (seq_t) ntoh32((uns32) lshdr->ls_seqno);
    lsa_xsum = ntoh16(lshdr->ls_xsum);
    lsa_length = ntoh16(lshdr->ls_length);
}

LShdr& LShdr::operator=(class LSA &lsa)

{
    ls_age = hton16(lsa.lsa_age());
    ls_opts = lsa.lsa_opts;
    ls_type = lsa.lsa_type;
    ls_id = hton32(lsa.ls_id());
    ls_org = hton32(lsa.adv_rtr());
    ls_seqno = (seq_t) hton32((uns32) lsa.ls_seqno());
    ls_xsum = hton16(lsa.lsa_xsum);
    ls_length = hton16(lsa.lsa_length);

    return(*this);
}

//�齨�����ʽ��LSA
LShdr *OSPF::BuildLSA(LSA *lsap, LShdr *hdr)
{
    int	blen;

    if (hdr == 0)
    {
        if (lsap->lsa_length > build_size)
        {
            delete [] build_area;
            build_size = lsap->lsa_length;
            build_area = new byte[lsap->lsa_length];
        }
        hdr = (LShdr *) ospf->build_area;
    }
    //�����׼��ͷ��
    *hdr = *lsap;
    //��������
    if (!lsap->exception)
        lsap->build(hdr);
    else
    //֮ǰ�Ѿ�������ݣ�ֱ�ӿ���
    {
        blen = lsap->lsa_length - sizeof(LShdr);
        memcpy((hdr + 1), lsap->lsa_body, blen);
    }

    return(hdr);
}

LSA *OSPF::FindLSA(Interface *ip,byte lstype, lsid_t lsid, rtid_t rtid)
{
    Table *btable;

    if(!(btable=FindLSdb(ip,lstype)))
        return (0);
    return ((LSA *)btable->find((uns32)lsid,(uns32)rtid));
}

LSA *OSPF::myLSA(Interface *ip,byte lstype,lsid_t lsid)
{
    return (FindLSA(ip,lstype,lsid,myid));
}

void Interface::AddTypesToList(byte lstype, LsaList *lp)

{
    Table *table;
    LSA *lsap;

    if (!(table = ospf->FindLSdb(this, lstype)))
        return;

    lsap = (LSA *) table->root();
    for (; lsap; lsap = (LSA *) lsap->sll)
        lp->addEntry(lsap);
}


void LSA::update_in_place(LSA *)

{
}

TNode::~TNode()

{
    Link *lp;
    Link *nextl;

    for (lp = t_links; lp; lp = nextl)
    {
        nextl = lp->l_next;
        delete lp;
    }
}


//�յ���LSA�����ݿ����е�һ��LSA�Ƚϣ��Ƚ�ͷ��������ֵΪ-1��0��1.
//����յ���LSA�ȽϾɣ�����-1
//�����ͬ������0
//����Ƚ��£�����1
//�㷨��RFC2328 13.1��
int LSA::cmp_instance(LShdr *hdr)

{
    age_t rcvd_age;
    age_t db_age;
    seq_t rcvd_seq;
    xsum_t rcvd_xsum;

    if ((rcvd_age = (ntoh16(hdr->ls_age) & ~DoNotAge)) > MaxAge)
        rcvd_age = MaxAge;
    if ((db_age = (lsa_age() & ~DoNotAge)) > MaxAge)
        db_age = MaxAge;

    rcvd_seq = ntoh32(hdr->ls_seqno);
    rcvd_xsum = ntoh16(hdr->ls_xsum);

    if (rcvd_seq > lsa_seqno)
        return(1);
    else if (rcvd_seq < lsa_seqno)
        return(-1);
    else if (rcvd_xsum > lsa_xsum)
        return(1);
    else if (rcvd_xsum < lsa_xsum)
        return(-1);
    else if (rcvd_age == MaxAge && db_age != MaxAge)
        return(1);
    else if (rcvd_age != MaxAge && db_age == MaxAge)
        return(-1);
    else if ((rcvd_age + MaxAgeDiff) < db_age)
        return(1);
    else if (rcvd_age > (db_age + MaxAgeDiff))
        return(-1);
    else
        return(0);
}


//���յ��ķ����LSA�����ݿ��е�LSA�Ƚ�
//�����������ͬ���򷵻�1�������·�����¼���
//���򷵻�0
int     LSA::cmp_contents(LShdr *hdr)

{
    LShdr *dbcopy;
    age_t rcvd_age;
    age_t db_age;
    int size;

    // Create network-ready version of database copy
    dbcopy = ospf->BuildLSA(this);

    // Mask out DoNotAge bit
    if ((rcvd_age = (ntoh16(hdr->ls_age) & ~DoNotAge)) > MaxAge)
        rcvd_age = MaxAge;
    if ((db_age = (lsa_age() & ~DoNotAge)) > MaxAge)
        db_age = MaxAge;

    // �Ƚ�ͷ����û��������ͬ
    if (rcvd_age != db_age && (rcvd_age == MaxAge || db_age == MaxAge))
        return(1);
    if (hdr->ls_opts != dbcopy->ls_opts)
        return(1);
    if (hdr->ls_length != dbcopy->ls_length)
        return(1);
    if (lsa_length <= sizeof(LShdr))
        return(0);

    // �Ƚ�LSA����
    size = lsa_length - sizeof(LShdr);
    if (memcmp((hdr + 1), (dbcopy + 1), size))
        return(1);

    return(0);
}

//�ҵ���Ӧ����LSA�����ݿ�
Table *OSPF::FindLSdb(Interface *ip,byte lstype)
{
    switch(lstype)
    {
    case LST_RTR:
        return &rtrLSAs;
    case LST_NET:
        return &netLSAs;
    default:
        break;
    }
    return 0;
}

//�����ݿ������LSA
LSA *OSPF::AddLSA(Interface *ip,LSA *current,LShdr *hdr,bool changed)

{
    LSA *lsap;
    int blen;
    RtrTblEntry *old_rte = 0;
    bool min_failed=false;

    blen = ntoh16(hdr->ls_length) - sizeof(LShdr);
    if (current)
    //�������һ����ǰ��LSA�������滻��
    {
        min_failed = current->since_received() < MinArrival;
        old_rte = current->rtentry();
        current->stop_aging();
        update_lsdb_xsum(current, false);
    }

    if (current && current->refct == 0)
    //�������һ��LSA�����Ҳ����κ�LSA�б����ã�
    //��ֱ�Ӵ�������
    {
        // ����
        if (changed)
            UnParseLSA(current);
        lsap = current;
        lsap->hdr_parse(hdr);
        lsap->start_aging();
        lsap->changed = changed;
        lsap->deferring = false;
        lsap->rollover = current->rollover;
        lsap->min_failed = min_failed;
        if (!changed)
        {
            update_lsdb_xsum(lsap, true);
            return(lsap);
        }
    }
    else
    //�������ʹ�õ�ǰ��LSA�����½�һ��
    {
        switch (hdr->ls_type)
        {
        case LST_RTR:
            lsap = new rtrLSA(hdr, blen);
            break;
        case LST_NET:
            lsap = new netLSA(hdr, blen);
            break;
        default:
            lsap = 0;
            break;
        }

        if (!current)
            lsap->changed = true;
        else
        // �����ǰ���ڣ�����Ҫ������
        {
            lsap->changed = changed;
            lsap->rollover = current->rollover;
            lsap->min_failed = min_failed;
            if (current->lsa_rxmt != 0)
                lsap->changed |= current->changed;
            lsap->update_in_place(current);
            UnParseLSA(current);
        }
        //��ʼ�ϻ�
        lsap->start_aging();
    }

    //�����µ�LSA
    ParseLSA(lsap, hdr);
    update_lsdb_xsum(lsap, true);
    //��������仯������Ҫ���¼���·��
    if (changed)
    {
        rtsched(lsap, old_rte);
    }
    return(lsap);
}

//����LSA���������ʽ��LSAת��Ϊ�����洢��LSA
void OSPF::ParseLSA(LSA *lsap, LShdr *hdr)

{
    int blen;
    //����Ѿ���������������Ҫ�ٽ�����
    if (lsap->parsed)
        return;

    blen = ntoh16(hdr->ls_length) - sizeof(LShdr);
    //���˵�����������LSA֮��
    //����LSA��Ҫ�������Լ����ͺ��ʵĽ����������
    if (lsap->lsa_age() != MaxAge)
    {
        lsap->exception = false;
        lsap->parsed = true;
        lsap->parse(hdr);
        total_lsas++;
    }
    else
        lsap->exception = true;

    delete [] lsap->lsa_body;
    lsap->lsa_body = 0;

    if (lsap->exception)
    {
    //��Ҫ��������LSA����
        lsap->lsa_body = new byte[blen];
        memcpy(lsap->lsa_body, (hdr + 1), blen);
    }
}

//�������ݿ�ɾ��һ��LSAʱ����Ҫ���������Ľ�������
//�Ӷ���֤·�ɼ���ʱ���ᱻʹ�ã�Ҳ���ᱻ����������
//֮�󣬱�����ݿ���ɾ��
void OSPF::UnParseLSA(LSA *lsap)

{
    if (lsap->parsed)
    {
    //��������ר�õķ���������
        lsap->parsed = false;
        lsap->unparse();
        total_lsas--;
    }
}

//��LSA���µ�ʵ������ʱ���ɵľ�Ҫ��LSAɾ����
void OSPF::DeleteLSA(LSA* lsap)
{
    Table *table;
    //�������ݿ�У���
    update_lsdb_xsum(lsap, false);
    //ֹͣ�ϻ�
    lsap->stop_aging();
    //������
    UnParseLSA(lsap);
    table = FindLSdb(lsap->lsa_ifp, lsap->lsa_type);
    //�����ݿ�����Ƴ�
    table->remove((TblItem *) lsap);
    lsap->delete_actions();
    //������ã���ɾ��
    lsap->chkref();
}

void OSPF::update_lsdb_xsum(LSA *lsap, bool add)

{
    uns32 *db_xsum;

    db_xsum = &ospf->db_xsum;


    if (add)
        (*db_xsum) += lsap->lsa_xsum;
    else
        (*db_xsum) -= lsap->lsa_xsum;
}


//�����ݿ����ҵ���һ��LSA
LSA *OSPF::NextLSA(byte ls_type, lsid_t id, rtid_t advrtr)

{
    LSA *lsap;
    Table *table;

    lsap = 0;
    for (; ls_type <= MAX_LST; id = advrtr = 0, ++ls_type)
    {
        table = FindLSdb(0,ls_type);
        if (table)
        {
            TblSearch iter(table);
            iter.seek(id, advrtr);
            if ((lsap = (LSA *) iter.next()))
                return(lsap);
        }
    }

    return(lsap);
}


//�����ݿ����ҵ���һ��LSA
LSA *OSPF::NextLSA(InAddr if_addr, int phyint, byte ls_type, lsid_t id, rtid_t advrtr)

{
    LSA *lsap;
    Interface *ip;
    Table *table;

    lsap = 0;
    ip = find_ifc(if_addr, phyint);
    do
    {
        // ��������
        for (; ls_type <= MAX_LST; id = advrtr = 0, ++ls_type)
        {
            table = FindLSdb(ip,ls_type);
            if (table)
            {
                TblSearch iter(table);
                iter.seek(id, advrtr);
                if ((lsap = (LSA *) iter.next()))
                    return(lsap);
            }
        }
        ls_type = 0;
        if (ip)
        {
            if_addr = ip->if_addr;
            phyint = ip->if_physic;
        }
        //�ҵ���һ���ӿ�
        ip = next_ifc(if_addr,phyint);
    }
    while (ip);

    return(lsap);
}

//��ʼ�ϻ�
void LSA::start_aging()

{
    uns16 bin;

    if (in_agebin)
        stop_aging();
    if (lsa_rcvage == MaxAge)
    {
        lsa_agebin = Age2Bin((age_t) 0);
        ospf->MaxAge_list.addEntry(this);
        return;
    }
    else if ((lsa_rcvage & DoNotAge) != 0)
        bin = Age2Bin((age_t) 0);
    else
        bin = Age2Bin(lsa_rcvage);

    in_agebin = true;
    lsa_agebin = bin;
    lsa_agefwd = AgeBins[bin];
    lsa_agerv = 0;
    if (AgeBins[bin])
        AgeBins[bin]->lsa_agerv = this;
    AgeBins[bin] = this;
}


//ֹͣ�ϻ�
void LSA::stop_aging()

{
    if (!in_agebin)
        return;

    if (lsa_agerv)
        lsa_agerv->lsa_agefwd = lsa_agefwd;
    else
        AgeBins[lsa_agebin] = lsa_agefwd;
    if (lsa_agefwd)
        lsa_agefwd->lsa_agerv = lsa_agerv;

    in_agebin = false;
    lsa_agefwd = 0;
    lsa_agerv = 0;
}

//�ϻ���ʱ��������
void DBageTimer::action()

{
    LSA *lsap;
    LsaListIterator iter(&ospf->replied_list);

    // �ϻ�
    ospf->dbage();
    // ɾ��inj
    ospf->delete_down_neighbors();
    // ���Խ�����������
    while (ospf->local_inits < ospf->max_dds)
    {
        Neighbor *np;
        if (!(np = GetNextAdj()))
            break;
        np->nbr_fsm(NBE_EVAL);
    }
    //���LSA��sent_reply��־
    while ((lsap = iter.get_next()))
    {
        lsap->sent_reply = false;
        iter.remove_current();
    }
    // ���¼���·��
    if (ospf->full_sched)
        ospf->full_calculation();
    //���ں�ͬ��·�ɱ�
    ospf->krt_sync();
    //�ں��ϴ�·��
    if (ospf->need_remnants)
    {
        ospf->need_remnants = false;
        sys->upload_remnants();
    }

}
//���ݿ��ϻ���Ϊ
void OSPF::dbage()

{
    // ÿ������һ������
    LSA::Bin0++;
    if (LSA::Bin0 > MaxAge)
        LSA::Bin0 = 0;

    // �����ض������LSA
    //���´���Ϊ���������δ������ΪMinLSInterval�����ӳٴ�����
    //LSA
    deferred_lsas();
    //����ض�����LSAУ��͡�
    checkages();
    //ˢ��LSA
    refresh_lsas();
    //�Դﵽ��������LSA���д���
    maxage_lsas();
    do_refreshes();
    // ��������ϻ�����������LSA���з���
    send_updates();
    // ɾ��һЩ�ﵽ��������LSA
    free_maxage_lsas();
}
//���´���Ϊ���������δ������ΪMinLSInterval�����ӳٴ�����
//LSA
void OSPF::deferred_lsas()

{
    uns16 bin;
    LSA	*lsap;
    LsaList defer_list;

    bin = Age2Bin(MinLSInterval);

    for (lsap = LSA::AgeBins[bin]; lsap; lsap = lsap->lsa_agefwd)
    {
        if (!lsap->deferring)
            continue;
        if (lsap->adv_rtr() == myid)
        {
            lsap->deferring = false;
            defer_list.addEntry(lsap);
        }
    }

    LsaListIterator *iter;
    iter = new LsaListIterator(&defer_list);
    while((lsap = iter->get_next()))
    {
        if (lsap->valid() && lsap->lsa_agebin == bin)
            lsap->reoriginate(false);
        iter->remove_current();
    }
    delete iter;
}

//ƽ��ÿʮ����Ӽ��LSAУ��͡�
void OSPF::checkages()

{
    age_t age;
    int limit;
    LSA *lsap;

    for (age = CheckAge; age < MaxAge; age += CheckAge)
    {
        uns16 bin;
        bin = Age2Bin(age);
        for (lsap = LSA::AgeBins[bin]; lsap; lsap = lsap->lsa_agefwd)
        {
            if (!lsap->checkage)
            {
                lsap->checkage = true;
                dbcheck_list.addEntry(lsap);
            }
        }
    }

    LsaListIterator iter(&dbcheck_list);
    limit = total_lsas/CheckAge + 1;

    for (int i = 0; (lsap = iter.get_next()) && i < limit; i++)
    {
        if (lsap->valid())
        {
            LShdr *hdr;
            int xlen;
            hdr = BuildLSA(lsap);
            xlen = ntoh16(hdr->ls_length) - sizeof(uns16);
            if (!hdr->verify_cksum())
                printf("Corrupted LS database\n");
        }
        lsap->checkage = false;
        iter.remove_current();
    }

    //ɾ����Ч����Ŀ
    dbcheck_list.garbage_collect();
}


//ˢ���Դ����Ĳ�������ﵽˢ��ʱ���LSA
void OSPF::refresh_lsas()

{
    uns16 bin;
    LSA	*lsap;
    LSA	*next_lsa;

    bin = Age2Bin(LSRefreshTime);

    for (lsap = LSA::AgeBins[bin]; lsap; lsap = next_lsa)
    {
        next_lsa = lsap->lsa_agefwd;
        if (lsap->do_not_age())
            continue;
        if (lsap->adv_rtr() == myid)
            schedule_refresh(lsap);
    }
}


//����ǡ�ôﵽMaxAge��LSA���·���
//���ϻ��б�ȡ�������ŵ�������LSA��
//���ʵ���ʱ��ᱻ����
void OSPF::maxage_lsas()

{
    uns16 bin;
    LSA	*lsap;
    LSA	*next_lsa;

    bin = Age2Bin(MaxAge);

    for (lsap = LSA::AgeBins[bin]; lsap; lsap = next_lsa)
    {
        next_lsa = lsap->lsa_agefwd;
        if (lsap->do_not_age())
        {
            if (lsap->adv_rtr() == myid)
            {
                lsap->lsa_hour++;
                continue;
            }
            else if (lsap->source->valid())
                continue;
        }
        age_prematurely(lsap);
    }
}
//�����ﵽ�������LSA�б����Ƿ��з����������ܹ����ͷ�
void OSPF::free_maxage_lsas()

{
    LSA *lsap;
    LsaListIterator iter(&MaxAge_list);

    while ((lsap = iter.get_next()))
    {
        if (!lsap->valid())
        {
            iter.remove_current();
            continue;
        }
        if (lsap->lsa_rxmt != 0)
            continue;
        if (!maxage_free(lsap->ls_type()))
            continue;
        //�����ͷţ������ݿ����Ƴ�
        iter.remove_current();
        if (lsap->rollover)
        {
            lsap->rollover = false;
            lsap->refresh(InvalidLSSeq);
        }
        else
            ospf->DeleteLSA(lsap);
    }
}

//ˢ��LSA
void OSPF::schedule_refresh(LSA *lsap)
{
    int slot;
    slot = Timer::random_period(MaxAgeDiff);
    if (slot < 0 || slot >= MaxAgeDiff)
        slot = LSA::RefreshBin0;
    LSA::RefreshBins[slot]++;
    pending_refresh.addEntry(lsap);
}


//����LSA��ˢ���Ѿ����滮������Ŀ��
void OSPF::do_refreshes()

{
    int count;
    LSA *lsap;
    LsaListIterator iter(&pending_refresh);

    count = LSA::RefreshBins[LSA::RefreshBin0];
    LSA::RefreshBins[LSA::RefreshBin0] = 0;
    for (; count > 0 && (lsap = iter.get_next()); count--)
    {
        if (!lsap->valid() || lsap->lsa_age() == MaxAge)
        {
            iter.remove_current();
            continue;
        }

        iter.remove_current();
        lsap->reoriginate(true);
    }

    LSA::RefreshBin0++;
    if (LSA::RefreshBin0 >= MaxAgeDiff)
        LSA::RefreshBin0 = 0;
}

uns16 fletcher(byte *message, int mlen, int offset)
{
    byte *ptr;
    byte *end;
    int c0; // У��͸��ֽ�
    int c1; // У��͵��ֽ�
    uns16 cksum;	// У���
    int	iq;	// ����messageλ�ã����ֽ�
    int	ir;	// ���ֽ�

    // У�������Ϊ0
    if (offset)
    {
        message[offset-1] = 0;
        message[offset] = 0;
    }

    // I��ʼ��
    c0 = 0;
    c1 = 0;
    ptr = message;
    end = message + mlen;

    // �ۼ�У���
    while (ptr < end)
    {
        byte	*stop;
        stop = ptr + MODX;
        if (stop > end)
            stop = end;
        for (; ptr < stop; ptr++)
        {
            c0 += *ptr;
            c1 += c0;
        }

        c0 = c0 % 255;
        c1 = c1 % 255;
    }

    // �õ�16λ���
    cksum = (c1 << 8) + c0;

    // ���㲢���У����ֶ�
    if (offset)
    {
        iq = ((mlen - offset)*c0 - c1) % 255;
        if (iq <= 0)
            iq += 255;
        ir = (510 - c0 - iq);
        if (ir > 255)
            ir -= 255;
        message[offset-1] = iq;
        message[offset] = ir;
    }

    return(cksum);
}

//����LSA У���
bool LShdr::verify_cksum()

{
    byte *message;
    int	mlen;

    message = (byte *) &ls_opts;
    mlen = ntoh16(ls_length) - sizeof(age_t);
    return (fletcher(message, mlen, 0) == (uns16) 0);
}

//����LSAУ���
void LShdr::generate_cksum()

{
    byte *message;
    int	mlen;
    int	offset;

    message = (byte *) &ls_opts;
    mlen = ntoh16(ls_length) - sizeof(age_t);
    offset = (int) (((byte *)&ls_xsum) - message) + 1;
    (void) fletcher(message, mlen, offset);
}

void Interface::delete_lsdb()

{

    db_xsum = 0;
}
