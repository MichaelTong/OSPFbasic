#ifndef RTRTBLENTRY_H
#define RTRTBLENTRY_H

class TNode;
class LSA;
class Interface;
class SpfData;
class IPrte;
class FWDrte;
struct ENode
{
    InAddr if_addr;//出口
    int phyint;//物理端口
    InAddr gateway;//下一跳网关
};

class EPath : public PatriciaEntry
{
public:
    int npaths;
    ENode Nodes[MAXPATH];
    int s_phyint;
    EPath *s_EPath;
    static PatriciaTree nhdb;

    static EPath *create(int, ENode *);
    static EPath *create(Interface *, InAddr);
    static EPath *create(int, InAddr);
    static EPath *merge(EPath *, EPath *);
    static EPath *addgateway(EPath *, InAddr);
    EPath *simple_phyint(int phyint);
};

enum
{
    RT_DIRECT=1,  // Directly attached
    RT_SPF,	// Intra-area
    RT_SPFIA,	// Inter-area
    RT_EXTT1,	// External type 1
    RT_EXTT2,	// External type 2
    RT_REJECT,	// Reject route, for own area ranges
    RT_STATIC,	// External routes we're importing
    RT_NONE,	// Deleted, inactive
};

class IPRtrTbl
{
protected:
    Table root;
public:
    IPrte *add(uns32 net,uns32 mask);
    inline IPrte *find(uns32 net,uns32 mask);
    IPrte *match(uns32 addr);
    void printf();
    friend class IPiterator;
    friend class OSPF;
};

inline IPrte *IPRtrTbl::find(uns32 net,uns32 mask)
{
    return ((IPrte*)root.find(net,mask));
}

class SpfData
{
public:
    byte lstype;
    lsid_t lsid;
    rtid_t rtid;
    aid_t r_area;
    EPath *old_epath;
    uns32 old_cost;
};

class RtrTblEntry : public TblItem
{
protected:
    byte r_type; 	// Type of routing table entry
    SpfData *r_ospf;	// Intra-AS OSPF-specific data

public:
    byte changed:1,	// Entry changed?
         dijk_run:1;	// Dijkstra run, sequence number
    EPath *r_epath; 	// Next hops
    EPath *last_epath; 	// Last set of Next hops given to kernel
    uns32 cost;		// Cost of routing table entry
    uns32 t2cost;	// Type 2 cost of entry

    RtrTblEntry(uns32 key_a, uns32 key_b);
    void new_intra(TNode *V, bool stub, uns16 stub_cost, int index);
    void host_new_intra(Interface *ip, uns32 new_cost);
    virtual void set_origin(LSA *V);
    virtual void declare_unreachable();
    LSA	*get_origin();
    void save_state();
    bool state_changed();
    void run_transit_areas(class rteLSA *lsap);
    void set_area(aid_t);
    Interface *ifc();
    inline void update(EPath *newnh);
    inline byte type();
    inline int valid();
    inline int intra_area();
    inline int inter_area();
    inline int intra_AS();
    inline aid_t area();


};

inline void RtrTblEntry::update(EPath *newnh)
{
    r_epath = newnh;
}
inline byte RtrTblEntry::type()
{
    return(r_type);
}
inline int RtrTblEntry::valid()
{
    return(r_type != RT_NONE);
}
inline int RtrTblEntry::intra_area()
{
    return(r_type == RT_SPF);
}
inline int RtrTblEntry::inter_area()
{
    return(r_type == RT_SPFIA);
}
inline int RtrTblEntry::intra_AS()
{
    return(r_type == RT_SPF || r_type == RT_SPFIA);
}
inline aid_t RtrTblEntry::area()
{
    return(0);
}

class IPrte : public RtrTblEntry
{
protected:
    IPrte *_prefix;
    uns32 tag;

public:

    inline IPrte(uns32 xnet, uns32 xmask);
    inline uns32 net();
    inline uns32 mask();
    inline bool matches(InAddr addr);
    inline uns32 broadcast_address();
    inline IPrte *prefix();

    inline int is_child(IPrte *o);
    int within_range();

    void sys_install();
    virtual void declare_unreachable();

    friend class IPiterator;
    friend class IPRtrTbl;
    friend class OSPF;
};

inline IPrte::IPrte(uns32 xnet, uns32 xmask) : RtrTblEntry(xnet, xmask)
{
    _prefix = 0;
    tag = 0;
}
inline uns32 IPrte::net()
{
    return(index1());
}
inline uns32 IPrte::mask()
{
    return(index2());
}
inline bool IPrte::matches(InAddr addr)
{
    return((addr & mask()) == net());
}
inline uns32 IPrte::broadcast_address()
{
    return(net() | ~mask());
}
inline IPrte *IPrte::prefix()
{
    return(_prefix);
}
inline int IPrte::is_child(IPrte *o)
{
    return((net() & o->mask()) == o->net() && mask() >= o->mask());
}

class IPiterator : public TblSearch
{
public:
    inline IPiterator(IPRtrTbl *);
    inline IPrte *nextrte();
};

inline IPiterator::IPiterator(IPRtrTbl *t) : TblSearch(&t->root)
{
}
inline IPrte *IPiterator::nextrte()
{
    return((IPrte *) next());
}

class RouterRtrTblEntry:public RtrTblEntry
{
public:
    byte rtype;		// Router type
    RouterRtrTblEntry(uns32 rtrid/*, class SpfArea *ap*/);
    virtual ~RouterRtrTblEntry();

    inline uns32 rtrid();
    virtual void set_origin(LSA *V);
};

inline uns32 RouterRtrTblEntry::rtrid()
{
    return index1();
}
class FWDtbl {
  protected:
    Table root; 	// Root of AVL tree
  public:
    FWDrte *add(uns32 addr);
    void resolve();
    void resolve(IPrte *changed_rte);
    friend class OSPF;
};

class FWDrte : public RtrTblEntry {
    Interface *ifp;	// On this interface
    IPrte *match;	// Best matching routing table entry
  public:
    inline FWDrte(uns32 addr);
    inline InAddr address();
    void resolve();
    friend class OSPF;
    friend class INrte;
    friend class ExRtData;
    friend class FWDtbl;
};

// Inline functions
inline FWDrte::FWDrte(uns32 addr) : RtrTblEntry(addr, 0)
{
    ifp = 0;
    match = 0;
}
inline InAddr FWDrte::address()
{
    return(index1());
}



class KrtSync : public TblItem {
  public:
    SPFtime tstamp;
    KrtSync(InAddr net, InMask mask);
};


#endif // RTRTBLENTRY_H
