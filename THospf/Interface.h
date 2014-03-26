#ifndef INTERFACE_H
#define INTERFACE_H

struct InterfaceConfig
{
    InAddr address;
    int phyint;

    InMask mask;
    uns16 mtu;
    uns16 IfIndex;

    int IfType;
    byte dr_pri;
    byte xmt_dly;
    byte rxmt_int;
    uns16 hello_int;
    uns16 if_cost;
    uns32 dead_int;
    uns32 poll_int;
    int auth_type;
    byte auth_key[8];
    int mc_fwd;
    int demand;
    int passive;
    int igmp;
};
//�ӳٽӿ��ϵ�ָ��·�������㣬ֱ�����ֺ�ָ��·
//�������ڡ�
class WaitTimer : public Timer
{
    class Interface *ifc;
public:
    inline WaitTimer(class Interface *);
    virtual void action();
};

inline WaitTimer::WaitTimer(class Interface *i)
{
    ifc=i;
}
//���ڴ�ĳ���˿ڷ���Hello�ļ�ʱ��
class HelloTimer_Ifc : public ITimer
{
    class Interface *ifc;
public:
    inline HelloTimer_Ifc(class Interface *);
    virtual void action();
};

inline HelloTimer_Ifc::HelloTimer_Ifc(class Interface  *i)
{
    ifc=i;
}
//�ӳ���·״̬ȷ�Ϸ����ڽӿ��ϵ��鲥��
//�ȻὫҪACK��LSA���뵽update�У�
//ֱ����ʱ�佫�䷢��
class DAckTimer : public Timer
{
    class Interface *ifc;
public:
    inline DAckTimer(class Interface *);
    virtual void action();
};

inline DAckTimer::DAckTimer(class Interface *i)
{
    ifc=i;
}

enum
{
    IFT_BROADCAST = 1,
    IFT_PP,
    IFT_NBMA,
    IFT_P2MP,
};
class Interface
{
protected:
    InMask if_mask; //�ӿ�����
    uns16 mtu; //�ӿ�mtu
    int if_IfIndex; //�ӿ����
    int if_type;//�ӿ����ͣ�Ŀǰֻ֧�ֹ㲥�ӿ�
    int if_cost;//�ӿڻ���
    byte if_rxmt;//�ӿ��ش����ʱ��
    byte if_xdelay;//�����ӳ�ʱ��

    uns16 if_hellointerval;//Hello���ͼ��
    uns32 if_deadinterval;//�ھ�Deadʱ��
    uns32 if_pollinterval;
    byte if_drpri; //���ȼ�
    uns32 if_demand:1;
    autyp_t if_autype;//��֤���� Ŀǰֻ֧�ֿ�
    byte if_passwd[8];//��֤����
    int passive; //�����ӿڣ�Ŀǰû��ʵ��
    int if_mcfwd; //�ಥת��
    bool igmp_enabled; //�Ƿ����鲥
    bool updated;

    //�������ú����й��̾�����
    InAddr if_net; //����
    InAddr if_multi;//�ಥ��ַ
    InAddr if_flood;//�����ַ



    InAddr if_dr;//DR��ַ
    InAddr if_bdr;//BDR��ַ
    class Neighbor *if_dr_n;//��ΪDR���ھ�
    int if_state;//�ӿ�״̬
    Interface *next;//�ӿ�������һ���ӿ�

    Pkt if_update;//��Ҫ���͵�update
    Pkt if_dack;//�ӳٷ��͵�ACK
    bool recv_update;//�յ���Update�����ڴ���
    uns32 if_demand_time;
    bool flood;//�Ƿ�Ҫ����
    //һЩά�����ܵļ�ʱ��
    WaitTimer if_waitt;//�ȴ���ʱ��
    HelloTimer_Ifc if_hellot;//Hello��ʱ��
    DAckTimer if_ackt;//�ӳٷ���ACK��ʱ��

    uns32 db_xsum;	// Database checksum

    virtual void ifa_start() = 0;
    void ifa_Neighbor_start(int base_priority);
    void ifa_elect();
    void ifa_reset();
    void ifa_allNeighbors_event(int event);


public:
    InAddr if_addr;
    int if_physic;

    class Neighbor *if_Neighborlist;//�ھ��б�
    int if_Neighborcnt;//�ھ���
    int if_fullcnt;//��ȫ�ڽӵ��ھ���

    Interface(InAddr addr, int phy);
    virtual ~Interface();
    inline int state(); //�õ�״̬
    inline InAddr net();    //�õ������
    inline InAddr mask();   //�õ�����
    inline uns16 cost();    //�õ�����

    inline InAddr dr(); //DR
    inline InAddr bdr();//BDR
    inline byte rxmt_interval();    //�ش����
    inline void if_build_dack(LShdr *hdr);  //�齨�ӳ�ACK
    inline void if_send_dack(); //�����ӳ�ACK
    inline void if_send_update();   //����update
    inline int unnumbered();

    void restart();
    void gen_msg( Pkt *pdesc);
    int verify(Pkt *pdesc, Neighbor *np);      //��֤��
    void recv_hello( Pkt *pdesc);
    void send_hello(bool empty=false);
    int build_hello( Pkt *,uns16 size);
    int add_to_update(LShdr *hdr);
    void if_build_ack(LShdr *hdr, Pkt *pkt=0, class Neighbor *np=0);


    void AddTypesToList(byte lstype, class LsaList *lp);
    void delete_lsdb();


    void finish_pkt(Pkt *pdesc,InAddr addr);
    int type();
    void ifc_fsm(int event);

    virtual void if_send(Pkt *, InAddr);    //�Ӷ˿ڷ������ݰ�
    virtual void Neighbor_send( Pkt *, Neighbor *); //��ĳ�ھӷ����ݰ�
    virtual Neighbor *find_Neighbor(InAddr, rtid_t);    //�ҵ�ĳ�ھ�
    virtual void set_IdAddr(Neighbor *, rtid_t, InAddr);    //�����ھ�ID���ַ
    virtual RtrLink *rl_insert(RTRhdr *, RtrLink *) = 0;    //����router lsa
    virtual int rl_size();
    void nl_orig(int forced);   //����network lsa
    LShdr *nl_raw_orig();
    virtual int adjacency_wanted(class Neighbor *np);
    virtual void send_hello_response(Neighbor *np);
    virtual void start_hellos();
    virtual void restart_hellos();
    virtual void stop_hellos();
    virtual bool elects_dr();
    virtual EPath *add_parallel_links(EPath *, TNode *);

    friend class IfcList;
    friend class NbrList;
    friend class WaitTimer;
    friend class HelloTimer_Ifc;
    friend class InactTimer;
    friend class HelloTimer;
    friend class HoldTimer;
    friend class DDTimer;
    friend class LSRTimer;
    friend class LSATimer;
    friend class OSPF;
    friend class Neighbor;
    friend class LSA;

};

inline int Interface::state()
{
    return if_state;
}

inline InAddr Interface::net()
{
    return if_net;
}

inline InAddr Interface::mask()
{
    return if_mask;
}
inline uns16 Interface::cost()
{
    return(if_cost);
}
inline InAddr Interface::dr()
{
    return if_dr;
}

inline InAddr Interface::bdr()
{
    return if_bdr;
}
inline byte Interface::rxmt_interval()
{
    return(if_rxmt);
}

inline  void Interface::if_build_dack(LShdr *hdr)
{
    if_build_ack(hdr);
    if (!if_ackt.is_running())
        if_ackt.start(1*Timer::SECOND);
}
inline  void Interface::if_send_dack()
{
    if_send(&if_dack, if_flood);
}
inline void Interface::if_send_update()
{
    if (if_update.iphead)
        if_send(&if_update, if_flood);
}
inline int Interface::unnumbered()
{
    return (if_addr == 0);
}



class DRIfc : public Interface
{
public:
    inline DRIfc(InAddr addr, int phy);
    virtual int adjacency_wanted(class Neighbor *np);
    virtual RtrLink *rl_insert(RTRhdr *, RtrLink *);
    virtual void ifa_start();
    virtual bool elects_dr();
};

inline DRIfc::DRIfc(InAddr addr, int phy) : Interface(addr, phy)
{
}

class BroadcastIfc:public DRIfc
{
public:
    inline BroadcastIfc(InAddr addr,int phy);
};

inline BroadcastIfc::BroadcastIfc(InAddr addr, int phy): DRIfc(addr, phy)
{
    if_type=IFT_BROADCAST;
}

#endif // INTERFACE_H
