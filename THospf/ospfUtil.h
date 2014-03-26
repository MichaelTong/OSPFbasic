struct FSMmachine
{
    int states;
    int	event;
    int	action;
    int	new_state;
};




struct Pkt
{
    InPkt *iphead;	// IP首部
    int	phyint;		// 物理接口
    bool llmult;	// 链路层多播？
    bool hold;		// 保留，不要释放
    bool chksummed;	// 内容检验通过？

    SpfPkt *spfpkt;	// OSPF报文首部
    byte *end;		// 数据报末尾
    int	bsize;		// 报文长度

    byte *dptr;		// 当前数据指针
    uns16 body_chksum;	// 报文校验和

    Pkt();
    Pkt(int phy, InPkt *inpkt);
    bool partial_checksum();
} ;


uns16 fletcher(byte *message, int mlen, int offset);
uns16 incksum(uns16 *, int len, uns16 seed=0);

#ifndef MAX
inline int MAX(int a, int b)
{
    return((a < b) ? b : a);
}
#endif
#ifndef MIN
inline int MIN(int a, int b)
{
    return((a < b) ? a : b);
}
#endif
