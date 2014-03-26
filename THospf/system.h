#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
class SysCalls
{
    enum
    {
        MAXIFs=255,
    };
    int netfd;//网络socket号
    int udpfd;//UDP socket号
    int igmpfd;//组播socket号
    int rtsock;//Router socket号
    timeval last_time;
    int next_phyint;
    Table phyints;//物理接口列表
    Table interface_map;//接口地址对应表
    Table directs;//直连路由
    rtentry m;
    uns32 nlm_seq;

    bool changing_routerid;
    bool changing_complete;
    bool dumping_remnants;
    int vifs[MAXIFs];
public:
    InPkt *getpkt(uns16 len);
    void freepkt(InPkt *pkt);
    SysCalls();
    virtual ~SysCalls();

    void sendpkt(InPkt *pkt, int phyint, InAddr gw=0);
    bool phy_operational(int phyint);
    void phy_open(int phyint);
    void phy_close(int phyint);
    void join(InAddr group, int phyint);
    void leave(InAddr group, int phyint);
    void ip_forward(bool enabled);
    void rtadd(InAddr, InMask, EPath *, EPath *, bool);
    void rtdel(InAddr, InMask, EPath *ompp);
    char *phyname(int phyint);
    void upload_remnants();

    void configure();
    void read_kernel_interfaces();
    void one_second_timer();
    void time_update();
    void add_direct(class BSDPhyInt *, InAddr, InMask);
    int get_phyint(InAddr);
    void SetInterface(char *buf);
    void raw_receive(int fd);
    void netlink_receive(int fd);
    void set_flags(class BSDPhyInt *, short flags);
    void add_interface_direct(InAddr net, InMask mask,InAddr addr,bool reject);

    friend int main(int argc, char *argv[]);
};

extern SPFtime sys_etime;
extern SysCalls *sys;

//从Linux读取的物理接口
class BSDPhyInt : public TblItem
{
    char *phyname;
    InAddr addr;
    short flags;
    InMask mask;
    InAddr dstaddr;
    int mtu;
    bool tunl;
    int vifno;
    InAddr tsrc;
    InAddr tdst;

    inline BSDPhyInt(int index);
    friend class SysCalls;
    inline int phyint();
};

inline BSDPhyInt::BSDPhyInt(int index) : TblItem(index, 0)
{
    addr = 0;
    mask = 0;
    dstaddr = 0;
    tunl = false;
    vifno = 0;
    tsrc = 0;
    tdst = 0;
}

inline int BSDPhyInt::phyint()
{
    return(index1());
}


class BSDIfMap : public TblItem
{
    BSDPhyInt *phyp;
public:
    inline BSDIfMap(InAddr, BSDPhyInt *);
    friend class SysCalls;
};

inline BSDIfMap::BSDIfMap(InAddr addr, BSDPhyInt *_phyp) : TblItem(addr, 0)
{
    phyp = _phyp;
}



class DirectRoute : public TblItem
{
public:
    bool valid;
    DirectRoute(InAddr addr, InMask mask) : TblItem(addr, mask) {}
};

const int MAX_IP_PKTSIZE = 65535;
