#include "include.h"
#include "NeighborFSM.h"
#include "system.h"
LSA *LSA::AgeBins[MaxAge+1];	// 老化队列
int LSA::Bin0;			// 年龄为0的元素
int32 LSA::RefreshBins[MaxAgeDiff];// 刷新队列
int LSA::RefreshBin0;		// 当前刷新的元素

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
        //加入到数据库
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
//解析头部
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

//组建网络格式的LSA
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
    //填入标准的头部
    *hdr = *lsap;
    //填入内容
    if (!lsap->exception)
        lsap->build(hdr);
    else
    //之前已经存过内容，直接拷贝
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


//收到的LSA与数据库中有的一条LSA比较，比较头部，返回值为-1、0、1.
//如果收到的LSA比较旧，返回-1
//如果相同，返回0
//如果比较新，返回1
//算法见RFC2328 13.1节
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


//将收到的泛洪的LSA与数据库中的LSA比较
//如果有显著不同，则返回1，会造成路由重新计算
//否则返回0
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

    // 比较头部有没有显著不同
    if (rcvd_age != db_age && (rcvd_age == MaxAge || db_age == MaxAge))
        return(1);
    if (hdr->ls_opts != dbcopy->ls_opts)
        return(1);
    if (hdr->ls_length != dbcopy->ls_length)
        return(1);
    if (lsa_length <= sizeof(LShdr))
        return(0);

    // 比较LSA内容
    size = lsa_length - sizeof(LShdr);
    if (memcmp((hdr + 1), (dbcopy + 1), size))
        return(1);

    return(0);
}

//找到对应类型LSA的数据库
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

//向数据库中添加LSA
LSA *OSPF::AddLSA(Interface *ip,LSA *current,LShdr *hdr,bool changed)

{
    LSA *lsap;
    int blen;
    RtrTblEntry *old_rte = 0;
    bool min_failed=false;

    blen = ntoh16(hdr->ls_length) - sizeof(LShdr);
    if (current)
    //如果存在一个当前的LSA，则将其替换掉
    {
        min_failed = current->since_received() < MinArrival;
        old_rte = current->rtentry();
        current->stop_aging();
        update_lsdb_xsum(current, false);
    }

    if (current && current->refct == 0)
    //如果存在一个LSA，而且不被任何LSA列表引用，
    //则直接存入或更新
    {
        // 更新
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
    //如果不能使用当前的LSA，则新建一个
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
        // 如果先前存在，则需要反解析
        {
            lsap->changed = changed;
            lsap->rollover = current->rollover;
            lsap->min_failed = min_failed;
            if (current->lsa_rxmt != 0)
                lsap->changed |= current->changed;
            lsap->update_in_place(current);
            UnParseLSA(current);
        }
        //开始老化
        lsap->start_aging();
    }

    //解析新的LSA
    ParseLSA(lsap, hdr);
    update_lsdb_xsum(lsap, true);
    //如果发生变化，则需要重新计算路由
    if (changed)
    {
        rtsched(lsap, old_rte);
    }
    return(lsap);
}

//解析LSA，将网络格式的LSA转化为本机存储的LSA
void OSPF::ParseLSA(LSA *lsap, LShdr *hdr)

{
    int blen;
    //如果已经被解析过，则不需要再解析了
    if (lsap->parsed)
        return;

    blen = ntoh16(hdr->ls_length) - sizeof(LShdr);
    //除了到达最大年龄的LSA之外
    //其他LSA需要调用与自己类型合适的解析程序解析
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
    //需要保留整个LSA内容
        lsap->lsa_body = new byte[blen];
        memcpy(lsap->lsa_body, (hdr + 1), blen);
    }
}

//当从数据库删除一个LSA时，需要撤销对他的解析处理，
//从而保证路由计算时不会被使用，也不会被其他的引用
//之后，便从数据库中删除
void OSPF::UnParseLSA(LSA *lsap)

{
    if (lsap->parsed)
    {
    //调用类型专用的泛解析程序
        lsap->parsed = false;
        lsap->unparse();
        total_lsas--;
    }
}

//当LSA被新的实例代替时，旧的就要从LSA删除，
void OSPF::DeleteLSA(LSA* lsap)
{
    Table *table;
    //更新数据库校验和
    update_lsdb_xsum(lsap, false);
    //停止老化
    lsap->stop_aging();
    //反解析
    UnParseLSA(lsap);
    table = FindLSdb(lsap->lsa_ifp, lsap->lsa_type);
    //从数据库表中移除
    table->remove((TblItem *) lsap);
    lsap->delete_actions();
    //检查引用，并删除
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


//从数据库中找到下一个LSA
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


//从数据库中找到下一个LSA
LSA *OSPF::NextLSA(InAddr if_addr, int phyint, byte ls_type, lsid_t id, rtid_t advrtr)

{
    LSA *lsap;
    Interface *ip;
    Table *table;

    lsap = 0;
    ip = find_ifc(if_addr, phyint);
    do
    {
        // 遍历类型
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
        //找到下一个接口
        ip = next_ifc(if_addr,phyint);
    }
    while (ip);

    return(lsap);
}

//开始老化
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


//停止老化
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

//老化计时器，触发
void DBageTimer::action()

{
    LSA *lsap;
    LsaListIterator iter(&ospf->replied_list);

    // 老化
    ospf->dbage();
    // 删掉inj
    ospf->delete_down_neighbors();
    // 可以建立更多链接
    while (ospf->local_inits < ospf->max_dds)
    {
        Neighbor *np;
        if (!(np = GetNextAdj()))
            break;
        np->nbr_fsm(NBE_EVAL);
    }
    //清除LSA的sent_reply标志
    while ((lsap = iter.get_next()))
    {
        lsap->sent_reply = false;
        iter.remove_current();
    }
    // 重新计算路由
    if (ospf->full_sched)
        ospf->full_calculation();
    //与内核同步路由表
    ospf->krt_sync();
    //内核上传路由
    if (ospf->need_remnants)
    {
        ospf->need_remnants = false;
        sys->upload_remnants();
    }

}
//数据库老化行为
void OSPF::dbage()

{
    // 每秒增加一次年龄
    LSA::Bin0++;
    if (LSA::Bin0 > MaxAge)
        LSA::Bin0 = 0;

    // 处理特定年龄的LSA
    //重新创建为保持了两次创建间隔为MinLSInterval而被延迟创建的
    //LSA
    deferred_lsas();
    //检查特定年龄LSA校验和。
    checkages();
    //刷新LSA
    refresh_lsas();
    //对达到最大年龄的LSA进行处理
    maxage_lsas();
    do_refreshes();
    // 完成由于老化到最大年龄的LSA进行泛洪
    send_updates();
    // 删除一些达到最大年龄的LSA
    free_maxage_lsas();
}
//重新创建为保持了两次创建间隔为MinLSInterval而被延迟创建的
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

//平均每十五分钟检查LSA校验和。
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

    //删除无效的条目
    dbcheck_list.garbage_collect();
}


//刷新自创建的并且年龄达到刷新时间的LSA
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


//年龄恰好达到MaxAge的LSA重新泛洪
//从老化列表取出来，放到单独的LSA表
//在适当的时候会被回收
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
//遍历达到最大年龄LSA列表，看是否有符合条件的能够被释放
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
        //可以释放，从数据库中移除
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

//刷新LSA
void OSPF::schedule_refresh(LSA *lsap)
{
    int slot;
    slot = Timer::random_period(MaxAgeDiff);
    if (slot < 0 || slot >= MaxAgeDiff)
        slot = LSA::RefreshBin0;
    LSA::RefreshBins[slot]++;
    pending_refresh.addEntry(lsap);
}


//遍历LSA，刷新已经被规划过的数目。
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
    int c0; // 校验和高字节
    int c1; // 校验和低字节
    uns16 cksum;	// 校验和
    int	iq;	// 调整message位置，高字节
    int	ir;	// 低字节

    // 校验和设置为0
    if (offset)
    {
        message[offset-1] = 0;
        message[offset] = 0;
    }

    // I初始化
    c0 = 0;
    c1 = 0;
    ptr = message;
    end = message + mlen;

    // 累加校验和
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

    // 得到16位结果
    cksum = (c1 << 8) + c0;

    // 计算并填充校验和字段
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

//检验LSA 校验和
bool LShdr::verify_cksum()

{
    byte *message;
    int	mlen;

    message = (byte *) &ls_opts;
    mlen = ntoh16(ls_length) - sizeof(age_t);
    return (fletcher(message, mlen, 0) == (uns16) 0);
}

//生成LSA校验和
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
