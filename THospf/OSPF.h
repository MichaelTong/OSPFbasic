#ifndef OSPF_H
#define OSPF_H


extern Queue timerq;

//ÿ��1s����һ�Σ������·״̬���ݿ��ϻ����������ݿ���
//���������������ڱ�Ҫ��ʱ������������·��ѡ����㡣
class DBageTimer : public ITimer
{
public:
    virtual void action();
};
class OSPF
{
    const rtid_t myid; //·����ID
    uns16 max_retrans_window; //����䴰��
    byte max_dds; //���ͬʱ����DD
    InAddr myaddr;
    //��̬����
    Interface *ifcs;//�ӿ��б�
    Table phyints; //����ӿ��б�
	Table krtdeletes; //��Ҫɾ�����ں�·��
    Table ospf_member; //OSPF�鲥��Ա�б����ڽ���224.0.0.5��224.0.0.6���鲥����
    uns16 ospf_mtu;//���нӿ�����MTU
    bool delete_neighbors;

    //������������һ���ȴ������ڽӹ�ϵ�ھ�����
    Neighbor *g_adj_head;//ͷ
    Neighbor *g_adj_tail;//β
	uns32 PPAdjLimit;
	
    //DD����
	int nbrs;
	int	n_dbx_nbrs;
    int local_inits;
    int remote_inits;

    //LSA����
    byte *build_area;//�齨LSA�������������ʽ��
    uns16 build_size;//LSA��������С
    byte *orig_buff;//LSA������
    uns16 orig_size;//��������С
    bool orig_buff_in_use;//�������Ƿ�ʹ��
    uns32 n_orig_allocs;

	//LSA���ݿ����
    uns32 db_xsum;//LS���ݿ�У���
    Table rtrLSAs;//router-LSAs
    Table netLSAs;//network-LSAs
    LsaList replied_list;//������͵�LSA����Ӧ�յ��ľɵ�LSA
    LsaList MaxAge_list;//�ﵽ����ϻ���LSA����Ҫ��ˢ��
    uns32 total_lsas;//���ݿ���LSA����
    LsaList dbcheck_list;//��֤ͨ����LSA
    LsaList pending_refresh;//�ȴ���ˢ�µ�LSA
    bool OverflowState;



    //����
    Pkt	a_update; //��ǰ���з����update
    Pkt	a_demand_upd;//��ǰ����Ҫ����Ľӿڽ��з����update



    //·�ɴ���
    bool need_remnants; //�Ƿ���Ҫ�ں��ϴ�·����Ϣ
    int full_sched:1;//�Ƿ�Ҫ����·�ɹ滮
    uns32 n_dijkstras;//����Dijkstras�㷨�Ĵ���
    Table abr_tbl;//abr�б�





    int run_fsm(FSMmachine *table, int &i_state,int event);    //�������״̬ת�Ʊ�״̬��ʱ�䣬���õ������Ķ����͸ı���״̬
    Interface *find_ifc(uns32 addr, int phyint = -1);    //���ݵ�ַ������˿ں������Ҷ˿�
    Interface *next_ifc(uns32 addr, int phyint);    //�ҵ�����������ַ������˿ںŴ����С��һ���˿�
    Interface *find_ifc(Pkt *pdesc);    //�������ݰ����Ҷ˿�
    Interface *find_nbr_ifc(InAddr nbr_addr);    //�ҵ������ھӵ�ַ�Ķ˿�
    int getpkt(Pkt *pkt,int type,uns16 size);    //���ɻ�����IP��
    void freepkt(Pkt *pkt);    //�ͷŰ�
    void delete_down_neighbors();    //�����ھӱ�ɾ���Ͽ����ھ�
    void multi_join(int phyint,InAddr group);    //����ಥ��
    void multi_leave(int phyint,InAddr group);    //�뿪�ಥ��
    void phy_attach(int phyint);    //������õ�����˿ڵ���¼��
    void phy_detach(int phyint, InAddr addr);    //ɾ���˿ڼ�¼



    DBageTimer dbtim;
    Table *FindLSdb(Interface *,byte lstype);
    LSA	*FindLSA(Interface *, byte lstype, lsid_t lsid, rtid_t rtid);
    LSA	*myLSA(Interface *, byte lstype, lsid_t lsid);
    LSA	*AddLSA(Interface *, LSA *current, LShdr *hdr, bool changed);
    void DeleteLSA(LSA *lsap);
    LSA *NextLSA(byte, lsid_t, rtid_t);
    LSA *NextLSA(InAddr, int, byte, lsid_t, rtid_t);
    void update_lsdb_xsum(LSA *, bool add);
    void ParseLSA(LSA *lsap, LShdr *hdr);
    void UnParseLSA(LSA *lsap);
    LShdr *BuildLSA(LSA *lsap, LShdr *hdr=0);
    void send_updates();
    bool maxage_free(byte lstype);
    LShdr *orig_buffer(int ls_len);
    void free_orig_buffer(LShdr *);    //�ͷ�LS������
    void add_to_ifmap(Interface *ip);    //������Ҫ����update�Ķ˿ڱ�
    void delete_from_ifmap(Interface *ip);    //�Ӷ˿ڱ�ɾ������

    inline RouterRtrTblEntry *find_abr(uns32 rtrid);
    RouterRtrTblEntry *add_abr(uns32 rtrid);

    void dbage(); //���ݿ��ϻ�
    void deferred_lsas();   //�ӳٴ���LSA
    void checkages();   //���һ������LSA��У���
    void refresh_lsas();    //ˢ��LSA
    void maxage_lsas(); //����ǡ�ôﵽMaxAge��LSA���·��飬���ϻ��б�ȡ�������ŵ�������LSA�����ʵ���ʱ��ᱻ����
    void free_maxage_lsas();//�ͷŴﵽ��������LSA
    void schedule_refresh(LSA *);//�滮Ҫˢ�µ�LSA
    void do_refreshes();//ˢ��LSA

    int	originated(Neighbor *, LShdr *hdr, LSA *database_copy);     //�ж�LSA�Ƿ����Լ����ɵ�
    seq_t ospf_get_seqno(byte lstype, LSA *lsap, int forced);   //�õ�һ�����к�
    void age_prematurely(LSA *);    //��һ��LSA����
    LSA	*lsa_reorig(Interface *,LSA *olsap, LShdr *hdr, int forced);    //��������LSA

    void add_to_update(LShdr *hdr, bool demand_upd);    //��LSA���뵽update��
    void build_update(Pkt *pkt, LShdr *hdr, uns16 mtu, bool demand);    //�齨update

    void rtsched(LSA *newlsa, RtrTblEntry *old_rte);    //��·���Ƿ���Ҫ���¼���
    void full_calculation();    //����·�ɼ���
    void dijk_init(Queue &cand);    //��ʼ��dijkstra
    void add_cand_node(Interface *ip, TNode *node, Queue &cand);    //�����ѡ
    void dijkstra();
    void rt_scan(); //ɨ��·�ɱ�ɾ���Ѿ�ʧЧ��·��
    void krt_sync();//���ں�ͬ��

public:
    class rtrLSA *mylsa;//��·�ɲ�����LSA
    int n_active_if;
    int n_routers;//�ɵ����·��
    class Interface **ifmap;//router-LSA��ӿڵ�ӳ���ϵ
    int ifmap_valid;//ӳ���ϵ��Ч��
    int size_ifmap;//ӳ���ϵ���С
    int n_ifmap;//��ǰӳ���ϵ����Ŀ��

    void rl_orig(int forced);
    OSPF(uns32 rtid_t,SPFtime grace);
    ~OSPF();
    inline rtid_t my_id();
    inline InAddr my_addr();
    InAddr if_addr(int phyint);
    void ConfigInterface(struct InterfaceConfig *m,int opt);    //���ö˿�
    void phy_up(int phyint);    //��������˿�
    void phy_down(int phyint);  //�ر�����˿�
    void resolvePkt(int phyint, InPkt *pkt, int plen);  //�������ݰ�
	void krt_delete_notification(InAddr net, InMask mask);
	void remnant_notification(InAddr net, InMask mask);

    void tick();
    int timeout();

    friend class IfcList;
    friend class Timer;
    friend class ITimer;
    friend class Neighbor;
    friend class Interface;
    friend class LSA;
    friend class TNode;
    friend class PPIfc;
    friend class P2MPIfc;
    friend class DRIfc;
    friend class RtrTblEntry;
    friend class rtrLSA;
    friend class netLSA;
    friend class EPath;
    friend class IPrte;
    friend class FWDtbl;
    friend class FWDrte;
    friend void lsa_flush(class LSA *);
    friend class DBageTimer;
    friend Neighbor *GetNextAdj();
};

extern OSPF *ospf;
inline RouterRtrTblEntry *OSPF::find_abr(uns32 rtrid)
{
    return ((RouterRtrTblEntry *) abr_tbl.find(rtrid));
}
inline rtid_t OSPF::my_id()
{
    return (myid);
}

inline InAddr OSPF::my_addr()
{
    return(myaddr);
}

enum
{
    ADD_ITEM,
    DELETE_ITEM,
};
void lsa_flush(LSA *lsap);

extern IPRtrTbl *inrttbl;
extern FWDtbl *fa_tbl;
extern IPrte *default_route;

#endif // OSPF_H
