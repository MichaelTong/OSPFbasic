#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <asm/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <linux/sysctl.h>
#include <linux/version.h>
#define _LINUX_SOCKIOS_H
#define _LINUX_IN_H
#include <linux/mroute.h>
#include <netinet/ip.h>
#include <linux/if_tunnel.h>
#include <net/if_arp.h>
#include "include.h"
#include "system.h"
#include <time.h>
#include <stdio.h>
char buffer[MAX_IP_PKTSIZE];

//打开ip转发
void SysCalls::ip_forward(bool enable)
{
    int mib[3];
    int value;
    mib[0] = CTL_NET;
    mib[1] = NET_IPV4;
    mib[2] = NET_IPV4_FORWARD;
    value = enable? 1: 0;
    sysctl(&mib[0], 3, 0, 0, &value, sizeof(value));
    printf("Enable IP forward\n");
}

//发送数据包
/*该函数首先为pkt设置目的地址和校验和，然后判断目的地址
* 是否是组播地址，如果是，将socket设置为组播。然后，设置
* 好msg，调用sendmsg发送消息。
*/
void SysCalls::sendpkt(InPkt *pkt,int phyint,InAddr gw)
{
    msghdr msg;
    iovec iov;
    size_t len;
    sockaddr_in to;

    if(gw!=0)
        pkt->i_dest=hton32(gw);
    pkt->i_chksum=~incksum((uns16 *)pkt,sizeof(pkt));

    if(IN_CLASSD(ntoh32(pkt->i_dest)))
    {
        ip_mreqn mreq;
        mreq.imr_ifindex=phyint;
        mreq.imr_address.s_addr=0;

        if(setsockopt(netfd,IPPROTO_IP,IP_MULTICAST_IF,(char *)&mreq,sizeof(mreq))<0)
        {
            printf("IP_MULTICAST_IF phyint %d: error",phyint);
            return;
        }
    }

    len=ntoh16(pkt->i_len);
    to.sin_family=AF_INET;
    to.sin_addr.s_addr=pkt->i_dest;
    msg.msg_name=(caddr_t)&to;
    msg.msg_namelen=sizeof(to);
    iov.iov_len=len;
    iov.iov_base=pkt;
    msg.msg_iov=&iov;
    msg.msg_iovlen=1;
    msg.msg_flags=MSG_DONTROUTE;

    byte cmsgbuf[128];
    cmsghdr *cmsg;
    in_pktinfo *pktinfo;
    msg.msg_control=cmsgbuf;
    msg.msg_controllen=CMSG_SPACE(sizeof(in_pktinfo));
    cmsg=CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len=CMSG_LEN(sizeof(in_pktinfo));
    cmsg->cmsg_level=SOL_IP;
    cmsg->cmsg_type=IP_PKTINFO;
    pktinfo=(in_pktinfo*)CMSG_DATA(cmsg);
    pktinfo->ipi_ifindex=phyint;
    pktinfo->ipi_spec_dst.s_addr=0;

    if(sendmsg(netfd,&msg,MSG_DONTROUTE)==-1)
    {
        printf("sendmsg failed...:%s\n",strerror(errno));
        return;
    }
}

//检查物理端口是否开启
bool SysCalls::phy_operational(int phyint)
{
    BSDPhyInt *phyp;

    if(phyint==-1)
        return false;
    if(!(phyp=(BSDPhyInt*)phyints.find(phyint,0)))
        return false;
    return ((phyp->flags&IFF_UP)!=0);
}

void SysCalls::phy_open(int phyint)
{

}

void SysCalls::phy_close(int phyint)
{

}

//得到物理端口名字
char *SysCalls::phyname(int phyint)

{
    BSDPhyInt *phyp;

    phyp = (BSDPhyInt *)phyints.find(phyint, 0);
    return(phyp ? (char *) phyp->phyname : 0);
}

//每一秒更新一次
void SysCalls::one_second_timer()
{
    timeval now;

    (void)gettimeofday(&now,NULL);
    sys_etime.sec++;
    sys_etime.ms=0;
    last_time=now;
}

SysCalls::SysCalls()
{
    rlimit rlim;
    sys_etime.sec=0;
    sys_etime.ms=0;
    next_phyint=0;
    (void)gettimeofday(&last_time,NULL);
    changing_routerid=false;
    changing_complete=false;
    dumping_remnants=false;

    rlim.rlim_max=RLIM_INFINITY;
    (void)setrlimit(RLIMIT_CORE,&rlim);
    printf("Open network...\n");
    //开启网络network socket
    if((netfd=socket(AF_INET,SOCK_RAW,PROT_OSPF))==-1)
    {
        printf("Network open failed...\n");
        printf("%s\n",strerror(errno));
        exit(1);
    }

    int hincl = 1;
    setsockopt(netfd, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof(hincl));
    rtsock = -1;

    int pktinfo = 1;
    setsockopt(netfd, IPPROTO_IP, IP_PKTINFO, &pktinfo, sizeof(pktinfo));

    nlm_seq = 0;
    sockaddr_nl addr;
    //开启router netlink socket
    if ((rtsock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1)
    {
        printf("Failed to create rtnetlink socket\n");
        exit(1);
    }
    addr.nl_family = AF_NETLINK;
    addr.nl_pad = 0;
    addr.nl_pid = 0;
    addr.nl_groups = (RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE);
    if (bind(rtsock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("Failed to bind to rtnetlink socket\n");
        exit(1);
    }

    //打开ioctl socket
    if((udpfd=socket(AF_INET,SOCK_DGRAM,0))<0)
    {
        printf("Failed to open UDP socket.\n");
        exit(1);
    }
    srand(getpid());
    igmpfd=-1;
}




rtid_t new_router_id;
/*由于设置文件中的ID是用点分十进制表示，
* 首先需要对ID进行转化，然后会创建一个
* OSPF实例。返回设置的路由ID。
*/
int SetRouterID(char *buf)
{
    in_addr addr;
    if(inet_aton(buf,&addr)==1)
    {
        InAddr ifaddr;
        ifaddr=ntoh32(addr.s_addr);
        new_router_id=ifaddr;
        if(!ospf)
            ospf=new OSPF(new_router_id,sys_etime);
    }
    else
    {
        printf("Bad Router ID: %s\n",buf);
    }
    return 0;
}
/*
 * 设置端口。设置文件中，可以写端口的IP或名称。首先判断传入的参数是否是IP，
 * 再查找相应的端口，并设置一些列参数。并调用ospf的ConfigInterface
 * 为路由设置端口
 */
void SysCalls::SetInterface(char *buf)
{
    InterfaceConfig m;
    in_addr addr;
    BSDPhyInt *phyp;
    FILE *in;
    if(inet_aton(buf,&addr)==1)//IP
    {
        BSDIfMap *map;
        InAddr ifaddr;
        ifaddr=ntoh32(addr.s_addr);
        map=(BSDIfMap *)interface_map.find(ifaddr,0);
        if(map!=0)
            phyp=map->phyp;

    }
    else//名称
    {
        TblSearch iter(&phyints);
        while((phyp=(BSDPhyInt*)iter.next()))
        {
            if(strcmp(buf,phyp->phyname))
                continue;
            addr.s_addr=hton32(phyp->addr);
            break;
        }
    }

    if(!phyp)
    {

        printf("No such interface: %s\n",buf);
        exit(-1);
    }

    m.address=phyp->addr;
    m.phyint=phyp->phyint();
    m.mask=phyp->mask;
    m.mtu = phyp->mtu;
    m.IfIndex=1;
    char filename[100]="";
    char b[100];
    sprintf(filename,"conf/%s.config",phyp->phyname);
    if((in=fopen(filename,"r"))==NULL)
    {
        printf("No configure file for %s.\n",phyp->phyname);
        exit(-1);
    }
    if ((phyp->flags & IFF_BROADCAST) != 0)
        m.IfType = IFT_BROADCAST;
    else if ((phyp->flags & IFF_POINTOPOINT) != 0)
        m.IfType = IFT_PP;
    else
        m.IfType = IFT_NBMA;
    fscanf(in,"%s",b);
    fscanf(in,"%d",&m.if_cost);
    fscanf(in,"%s",b);
    fscanf(in,"%d",&m.dr_pri);
    fscanf(in,"%s",b);
    fscanf(in,"%d",&m.xmt_dly);
    fscanf(in,"%s",b);
    fscanf(in,"%d",&m.rxmt_int);
    fscanf(in,"%s",b);
    fscanf(in,"%d",&m.hello_int);
    fscanf(in,"%s",b);
    fscanf(in,"%d",&m.dead_int);
    fclose(in);
    m.poll_int = 40;
    m.auth_type = 0;
    memset(m.auth_key, 0, 8);
    m.mc_fwd = 0;
    m.demand = 1;
    m.passive = 0;
    switch (3)
    {
    case 0:
        m.igmp = 0;
        break;
    case 1:
        m.igmp = 1;
        break;
    default:
        m.igmp = ((m.IfType == IFT_BROADCAST) ? 1 : 0);
        break;
    }

    ospf->ConfigInterface(&m, ADD_ITEM);
    return;

}

//读取配置文件，设置路由ID和端口。
void SysCalls::configure()
{
    if(changing_routerid)
        return;
    new_router_id = 0;

    read_kernel_interfaces();
    FILE *in;
    printf("Read configuraion from config file...\n");
    if((in=fopen("conf/ospf.config","r"))==NULL)
    {
        printf("Open config file error.\n");
        exit(-1);
    }
    char buff[256];
    int ifcs;
    fscanf(in,"===THospf Config File===");
    fscanf(in,"%s",buff);
    fscanf(in,"%s",buff);
    SetRouterID(buff);
    printf("\tRouter ID:\t%s\n",buff);
    fscanf(in,"%s",buff);
    fscanf(in,"%d",&ifcs);

    if(ifcs<=0)
    {
        printf("No interfaces in config file.\n");
        exit(-1);
    }
    while(ifcs>0)
    {
        fscanf(in,"%s",buff);
        fscanf(in,"%s",buff);
        SetInterface(buff);
        printf("\tInterface '%s' up.\n",buff);
        ifcs--;
    }

    if(!ospf||new_router_id==0)
    {
        printf("Configure Failed.\n");
        exit(-1);
    }
    fclose(in);
}

//得到IP对应的物理端口
int SysCalls::get_phyint(InAddr addr)

{
    TblSearch iter(&phyints);
    BSDPhyInt *phyp;
    while ((phyp = (BSDPhyInt *)iter.next()))
    {
        if ((phyp->addr & phyp->mask) == (addr & phyp->mask))
            return(phyp->phyint());
    }

    return(-1);
}

//读取内核端口
//通过ioctl来读取内核端口，并存入系统的端口表中
void SysCalls::read_kernel_interfaces()
{
    ifconf cfgreq;
    ifreq *ifrp;
    ifreq *end;
    size_t size;
    char *ifcbuf;
    int blen;
    TblSearch iter(&directs);
    DirectRoute *rte;
    TblSearch iter2(&phyints);
    BSDPhyInt *phyp;

    blen = MAXIFs*sizeof(ifreq);
    ifcbuf = new char[blen];
    cfgreq.ifc_buf = ifcbuf;
    cfgreq.ifc_len = blen;

    printf("Reading kernel interfaces...\n");
    if (ioctl(udpfd, SIOCGIFCONF, (char *)&cfgreq) < 0)
    {
        printf("Failed to read interface config: %s\n",strerror(errno));
        exit(1);
    }


    interface_map.clear();
    while ((rte = (DirectRoute *)iter.next()))
        rte->valid = false;

    while((phyp = (BSDPhyInt *)iter2.next()))
    {
        phyp->addr = 0;
        phyp->mask = 0;
    }

    ifrp = (ifreq *) ifcbuf;
    end = (ifreq *)(ifcbuf + cfgreq.ifc_len);
    for (; ifrp < end; ifrp = (ifreq *)(((byte *)ifrp) + size))
    {
        ifreq ifr;
        sockaddr_in *insock;
        InAddr addr;
        size = sizeof(InAddr) + sizeof(ifrp->ifr_name);
        if (size < sizeof(ifreq))
            size = sizeof(ifreq);
        if (ifrp->ifr_addr.sa_family != AF_INET)
            continue;
        short ifflags;
        memcpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));
        if (ioctl(udpfd, SIOCGIFFLAGS, (char *)&ifr) < 0)
        {
            printf("SIOCGIFFLAGS Failed: %s\n",strerror(errno));
            exit(1);
        }
        if (strncmp(ifrp->ifr_name, "tunl0", 5) == 0)
            continue;
        ifflags = ifr.ifr_flags;

        int ifindex;
        memcpy(ifr.ifr_name, ifrp->ifr_name, sizeof(ifr.ifr_name));
        if (ioctl(udpfd, SIOCGIFINDEX, (char *)&ifr) < 0)
        {
            printf("SIOCGIFINDEX Failed: %s\n",strerror(errno));
            exit(1);
        }
        ifindex = ifr.ifr_ifindex;

        if (!(phyp = (BSDPhyInt *) phyints.find(ifindex, 0)))
        {
            phyp = new BSDPhyInt(ifindex);
            phyp->phyname = new char[strlen(ifrp->ifr_name)];
            strcpy(phyp->phyname, ifrp->ifr_name);
            phyp->flags = 0;
            phyints.add(phyp);
            ioctl(udpfd, SIOCGIFHWADDR, &ifr);
            if (ifr.ifr_hwaddr.sa_family == ARPHRD_TUNNEL)
            {
                ip_tunnel_parm tp;
                phyp->tunl = true;
                ifr.ifr_ifru.ifru_data = (char *)&tp;
                ioctl(udpfd, SIOCGETTUNNEL, &ifr);
                phyp->tsrc = ntoh32(tp.iph.saddr);
                phyp->tdst = ntoh32(tp.iph.daddr);

            }
        }
        if (!memchr(ifrp->ifr_name, ':', sizeof(ifrp->ifr_name)))
            set_flags(phyp, ifflags);
        // 得到接口的MTU
        phyp->mtu = ((phyp->flags & IFF_BROADCAST) != 0) ? 1500 : 576;
        if (ioctl(udpfd, SIOCGIFMTU, (char *)&ifr) >= 0)
            phyp->mtu = ifr.ifr_mtu;
        // 存储接口信息
        insock = (sockaddr_in *) &ifrp->ifr_addr;
        if ((ntoh32(insock->sin_addr.s_addr) & 0xff000000) == 0x7f000000)
            continue;
        addr = ntoh32(insock->sin_addr.s_addr);
        // 得到子网掩码
        if (ioctl(udpfd, SIOCGIFNETMASK, (char *)&ifr) < 0)
        {
            printf("SIOCGIFNETMASK Failed: %s\n",strerror(errno));
            exit(1);
        }
        insock = (sockaddr_in *) &ifr.ifr_addr;
        if (phyp->tunl && ntoh32(insock->sin_addr.s_addr) == 0xffffffff)
            continue;
        phyp->addr = addr;
        phyp->mask = ntoh32(insock->sin_addr.s_addr);
        add_direct(phyp, addr, phyp->mask);
        add_direct(phyp, addr, 0xffffffffL);
        phyp->dstaddr = 0;
        if ((phyp->flags & IFF_POINTOPOINT) != 0 &&
                (ioctl(udpfd, SIOCGIFDSTADDR, (char *)&ifr) >= 0))
        {
            addr = phyp->dstaddr = ntoh32(insock->sin_addr.s_addr);
            add_direct(phyp, addr, 0xffffffffL);
        }
        printf("\tKernel Interface found:\n");
        printf("\t\tName:\t%s\n",phyp->phyname);

        printf("\t\tAddr:\t");
        InAddr paddr=phyp->addr;
        printf("%d.",paddr>>24);
        paddr=paddr%0xffffff;
        printf("%d.",paddr>>16);
        paddr=paddr%0xffff;
        printf("%d.",paddr>>8);
        paddr=paddr%0xff;
        printf("%d\n",paddr);

        printf("\t\tMask:\t");
        InAddr pmask=phyp->mask;
        printf("%d.",pmask>>24);
        pmask=pmask%0xffffff;
        printf("%d.",pmask>>16);
        pmask=pmask%0xffff;
        printf("%d.",pmask>>8);
        pmask=pmask%0xff;
        printf("%d\n",pmask);

        printf("\t\tMTU:\t%d\n",phyp->mtu);
        // 记录端口IP映射
        if (!interface_map.find(addr, 0))
        {
            BSDIfMap *map;
            map = new BSDIfMap(addr, phyp);
            interface_map.add(map);
        }
    }

    iter.seek(0, 0);
    while ((rte = (DirectRoute *)iter.next()))
    {
        InAddr net=rte->index1();
        InMask mask=rte->index2();
        if (!rte->valid)
        {
            directs.remove(rte);
            delete rte;
            ospf->krt_delete_notification(net, mask);
        }
        else
            sys->rtdel(net, mask, 0);
    }

        printf("Kernel interfaces read finished.\n");
    delete [] ifcbuf;
}

void SysCalls::set_flags(BSDPhyInt *phyp,short flags)
{
    short old_flags=phyp->flags;

    phyp->flags=flags;
    if(((old_flags^flags)&IFF_UP)!=0&&ospf)
    {
        if((flags&IFF_UP)!=0)
            ospf->phy_up(phyp->phyint());
        else
            ospf->phy_down(phyp->phyint());
    }
}
//接收原始的数据包
//调用recvmsg从fd收取消息。recvmsg是阻塞调用，但如果没有收到消息
//会在一定时间内被系统终止。收取到消息后，调用ospf类的resolvePkt函
//数对OSPF协议消息进行后续处理。
void SysCalls::raw_receive(int fd)
{
    int rcvlen;
    int rcvinterface = -1;

    msghdr msg;
    iovec iov;
    byte cmsgbuf[128];
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    iov.iov_len = sizeof(buffer);
    iov.iov_base = buffer;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    rcvlen = recvmsg(fd, &msg, 0);//阻塞调用
    if (rcvlen < 0)
    {
        return;
    }
    else
    {
        cmsghdr *cmsg;
        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
            if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_PKTINFO)
            {
                in_pktinfo *pktinfo;
                pktinfo = (in_pktinfo *) CMSG_DATA(cmsg);
                rcvinterface = pktinfo->ipi_ifindex;
                break;
            }
        }
    }

    InPkt *pkt = (InPkt *) buffer;//改变数据类型
    switch (pkt->i_prot)
    {
    case PROT_OSPF:
        ospf->resolvePkt(rcvinterface, pkt, rcvlen);//解析OSPF数据包，并产生相应动作
        break;
    default:
        break;
    }
}

//更新时间
void SysCalls::time_update()
{
    timeval now;
    int timediff;

    (void)gettimeofday(&now,NULL);
    timediff=1000*(now.tv_sec-last_time.tv_sec);
    timediff+=(now.tv_usec-last_time.tv_usec)/1000;
    if((timediff+sys_etime.ms)<1000)
        sys_etime.ms+=timediff;
    last_time=now;
}

//加入多播组
//通过设置socket让指定端口加入某多播组。
//本程序暂时只让端口加入OSPF多播组。
void SysCalls::join(InAddr group,int phyint)
{
    ip_mreq mreq;
    BSDPhyInt *phyp;

    phyp=(BSDPhyInt*)phyints.find(phyint,0);
    if((phyp->flags&IFF_MULTICAST)==0)
        return;
    mreq.imr_multiaddr.s_addr=hton32(group);
    mreq.imr_interface.s_addr=hton32(phyp->addr);

    if(setsockopt(netfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *)&mreq,sizeof(mreq))<0)
    {
        printf("Join error.\n");
    }
}

//离开多播组
//通过设置socket让指定端口离开某多播组。
void SysCalls::leave(InAddr group, int phyint)

{
    ip_mreq mreq;

    BSDPhyInt *phyp;

    phyp = (BSDPhyInt *)phyints.find(phyint, 0);
    if ((phyp->flags & IFF_MULTICAST) == 0)
        return;
    mreq.imr_multiaddr.s_addr = hton32(group);
    mreq.imr_interface.s_addr = hton32(phyp->addr);


    if (setsockopt(netfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                   (char *)&mreq, sizeof(mreq)) < 0)
        syslog(LOG_ERR, "Leave error, phyint %d: %m", phyint);
}


//内核上传路由，使其启用
void SysCalls::upload_remnants()

{
    nlmsghdr *nlm;
    rtmsg *rtm;
    int size;

    dumping_remnants = true;

    size = NLMSG_SPACE(sizeof(*rtm));
    nlm = (nlmsghdr *) new char[size];
    nlm->nlmsg_len = size;
    nlm->nlmsg_type = RTM_GETROUTE;
    nlm->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    nlm->nlmsg_seq = nlm_seq++;
    nlm->nlmsg_pid = 0;
    rtm = (rtmsg *) NLMSG_DATA(nlm);
    rtm->rtm_family = AF_INET;
    rtm->rtm_dst_len = 0;
    rtm->rtm_src_len = 0;
    rtm->rtm_tos = 0;
    rtm->rtm_table = 0;
    rtm->rtm_protocol = PROT_OSPF;
    rtm->rtm_scope = RT_SCOPE_UNIVERSE;
    rtm->rtm_type = RTN_UNICAST;
    rtm->rtm_flags = 0;

    if (-1 == send(rtsock, nlm, size, 0))
        printf("routing table dump\n");

    delete [] ((char *)nlm);
}

//向内核添加路由
//通过router socket来添加路由。如果存在直接连接的路由，会根据旧
//的路径删除直连路由。否则，向内核添加最新的路由。如果reject为true，
//则添加的路由类型被声明为不可达。
void SysCalls::rtadd(InAddr net, InMask mask, EPath *mpp,
                     EPath *ompp, bool reject)

{
    nlmsghdr *nlm;
    rtmsg *rtm;
    rtattr *rta_dest;
    rtattr *rta_gw;
    int size;
    int prefix_length;
    //直接连接已有，先删除
    if (directs.find(net, mask) || !mpp)
    {
        //rtdel(net, mask, ompp);
        return;
    }

    for (prefix_length = 32; prefix_length > 0; prefix_length--)
    {
        if ((mask & (1 << (32-prefix_length))) != 0)
            break;
    }
    size = NLMSG_SPACE(sizeof(*rtm)); // 路由信息本身
    if (prefix_length > 0)
        size += RTA_SPACE(4);	// 目的地址长度
    if (!reject)
        size += RTA_SPACE(4);	// 下一跳长度

    nlm = (nlmsghdr *) new char[size];
    nlm->nlmsg_len = size;
    nlm->nlmsg_type = RTM_NEWROUTE;
    nlm->nlmsg_flags = NLM_F_REQUEST|NLM_F_REPLACE|NLM_F_CREATE;
    nlm->nlmsg_seq = nlm_seq++;
    nlm->nlmsg_pid = 0;
    rtm = (rtmsg *) NLMSG_DATA(nlm);
    rtm->rtm_family = AF_INET;
    rtm->rtm_dst_len = prefix_length;
    rtm->rtm_src_len = 0;
    rtm->rtm_tos = 0;
    rtm->rtm_table = 0;
    rtm->rtm_protocol = PROT_OSPF;
    rtm->rtm_scope = RT_SCOPE_UNIVERSE;
    rtm->rtm_type = RTN_UNICAST;
    rtm->rtm_flags = 0;
    if (prefix_length > 0)
    {
        uns32 swnet;
        int attrlen;
        rta_dest = (rtattr *) RTM_RTA(rtm);
        rta_dest->rta_len = attrlen = RTA_SPACE(4);
        rta_dest->rta_type = RTA_DST;
        swnet = hton32(net);//目的网段
        memcpy(RTA_DATA(rta_dest), &swnet, sizeof(swnet));
        rta_gw = (rtattr *) RTA_NEXT(rta_dest, attrlen);
    }
    else
        rta_gw = (rtattr *) RTM_RTA(rtm);
    if (reject)
    {
        rtm->rtm_scope = RT_SCOPE_HOST;
        rtm->rtm_type = RTN_UNREACHABLE;//不可达
    }
    else
    {
        InAddr gw;
        BSDPhyInt *phyp=0;
        int phyint;
        gw = hton32(mpp->Nodes[0].gateway);//吓一跳地址
        if ((phyint = mpp->Nodes[0].phyint) != -1)
            phyp = (BSDPhyInt *)phyints.find(phyint, 0);
        if (phyp && (phyp->flags & IFF_POINTOPOINT) != 0)
        {
            rta_gw->rta_len = RTA_SPACE(sizeof(phyint));
            rta_gw->rta_type = RTA_OIF;
            memcpy(RTA_DATA(rta_gw), &phyint, sizeof(phyint));
        }
        else
        {
            rta_gw->rta_len = RTA_SPACE(4);
            rta_gw->rta_type = RTA_GATEWAY;
            memcpy(RTA_DATA(rta_gw), &gw, sizeof(gw));
        }
    }

    if (-1 == send(rtsock, nlm, size, 0))
    {
        printf("add route though routing socket fail.\n");
    }

    delete [] ((char *)nlm);
}
//通过router socket来删除路由
void SysCalls::rtdel(InAddr net, InMask mask, EPath *ompp)

{
    nlmsghdr *nlm;
    rtmsg *rtm;
    rtattr *rta_dest;
    int size;
    int prefix_length;

    for (prefix_length = 32; prefix_length > 0; prefix_length--)
    {
        if ((mask & (1 << (32-prefix_length))) != 0)
            break;
    }

    //计算长度
    size = NLMSG_SPACE(sizeof(*rtm));
    if (prefix_length > 0)
        size += RTA_SPACE(4);
    nlm = (nlmsghdr *) new char[size];
    nlm->nlmsg_len = size;
    nlm->nlmsg_type = RTM_DELROUTE;
    nlm->nlmsg_flags = NLM_F_REQUEST;
    nlm->nlmsg_seq = nlm_seq++;
    nlm->nlmsg_pid = 0;
    rtm = (rtmsg *) NLMSG_DATA(nlm);
    rtm->rtm_family = AF_INET;
    rtm->rtm_dst_len = prefix_length;
    rtm->rtm_src_len = 0;
    rtm->rtm_tos = 0;
    rtm->rtm_table = 0;
    rtm->rtm_protocol = PROT_OSPF;
    rtm->rtm_scope = RT_SCOPE_UNIVERSE;
    rtm->rtm_type = RTN_UNICAST;
    rtm->rtm_flags = 0;
    if (prefix_length > 0)
    {
        uns32 swnet;
        int attrlen;
        rta_dest = (rtattr *) RTM_RTA(rtm);
        rta_dest->rta_len = attrlen = RTA_SPACE(4);
        rta_dest->rta_type = RTA_DST;
        swnet = hton32(net);
        memcpy(RTA_DATA(rta_dest), &swnet, sizeof(swnet));
    }

    if (-1 == send(rtsock, nlm, size, 0))
        printf("del route through routing socket failed.\n");

    delete [] ((char *)nlm);
}
//  为开启的端口添加直连路由
//	reject一般为false。函数先检查直连路由表，如果已经存在该直连路由，
//  先删除，然后向其中添加两条路由。第一条路由是路由器某接口网段的路
//  由，路由目的地址为该接口IP。第二条路由是到路由器接口的路由，掩码
//  为32位，目的地址为回环地址。
void SysCalls::add_interface_direct(InAddr net, InMask mask,InAddr addr,bool reject)
{
        nlmsghdr *nlm;
        rtmsg *rtm;
        rtattr *rta_dest;
        rtattr *rta_gw;
        int size;
        int prefix_length;
        //直接连接已有，先删除
        if (directs.find(net, mask))
            sys->rtdel(net, mask, 0);

        for (prefix_length = 32; prefix_length > 0; prefix_length--)
        {
            if ((mask & (1 << (32-prefix_length))) != 0)
                break;
        }
        size = NLMSG_SPACE(sizeof(*rtm)); // 路由信息本身
        if (prefix_length > 0)
            size += RTA_SPACE(4);	// 目的地址长度
        if (!reject)
            size += RTA_SPACE(4);	// 下一跳长度
		//路由器某接口网段的路由，路由目的地址为该接口IP
        nlm = (nlmsghdr *) new char[size];
        nlm->nlmsg_len = size;
        nlm->nlmsg_type = RTM_NEWROUTE;
        nlm->nlmsg_flags = NLM_F_REQUEST|NLM_F_REPLACE|NLM_F_CREATE;
        nlm->nlmsg_seq = nlm_seq++;
        nlm->nlmsg_pid = 0;
        rtm = (rtmsg *) NLMSG_DATA(nlm);
        rtm->rtm_family = AF_INET;
        rtm->rtm_dst_len = prefix_length;
        rtm->rtm_src_len = 0;
        rtm->rtm_tos = 0;
        rtm->rtm_table = 0;
        rtm->rtm_protocol = PROT_OSPF;
        rtm->rtm_scope = RT_SCOPE_UNIVERSE;
        rtm->rtm_type = RTN_UNICAST;
        rtm->rtm_flags = 0;
        if (prefix_length > 0)
        {
            uns32 swnet;
            int attrlen;
            rta_dest = (rtattr *) RTM_RTA(rtm);
            rta_dest->rta_len = attrlen = RTA_SPACE(4);
            rta_dest->rta_type = RTA_DST;
            swnet = hton32(net);
            memcpy(RTA_DATA(rta_dest), &swnet, sizeof(swnet));
            rta_gw = (rtattr *) RTA_NEXT(rta_dest, attrlen);
        }
        else
            rta_gw = (rtattr *) RTM_RTA(rtm);
        if (reject)
        {
            rtm->rtm_scope = RT_SCOPE_HOST;
            rtm->rtm_type = RTN_UNREACHABLE;
        }
        else
        {
            InAddr gw;
            gw = hton32(addr);
            rta_gw->rta_len = RTA_SPACE(4);
            rta_gw->rta_type = RTA_GATEWAY;
            memcpy(RTA_DATA(rta_gw), &gw, sizeof(gw));
        }

        if (-1 == send(rtsock, nlm, size, 0))
        {
            printf("add route though routing socket fail.\n");
        }

        delete [] ((char *)nlm);

        size = NLMSG_SPACE(sizeof(*rtm)); // 路由信息本身
        if (prefix_length > 0)
            size += RTA_SPACE(4);	// 目的地址长度
        if (!reject)
            size += RTA_SPACE(4);	// 下一跳长度

		//到路由器接口的路由，掩码为32位，目的地址为回环地址
        nlm = (nlmsghdr *) new char[size];
        nlm->nlmsg_len = size;
        nlm->nlmsg_type = RTM_NEWROUTE;
        nlm->nlmsg_flags = NLM_F_REQUEST|NLM_F_REPLACE|NLM_F_CREATE;
        nlm->nlmsg_seq = nlm_seq++;
        nlm->nlmsg_pid = 0;
        rtm = (rtmsg *) NLMSG_DATA(nlm);
        rtm->rtm_family = AF_INET;
        rtm->rtm_dst_len = 32;
        rtm->rtm_src_len = 0;
        rtm->rtm_tos = 0;
        rtm->rtm_table = 0;
        rtm->rtm_protocol = PROT_OSPF;
        rtm->rtm_scope = RT_SCOPE_UNIVERSE;
        rtm->rtm_type = RTN_UNICAST;
        rtm->rtm_flags = 0;
        if (prefix_length > 0)
        {
            uns32 swnet;
            int attrlen;
            rta_dest = (rtattr *) RTM_RTA(rtm);
            rta_dest->rta_len = attrlen = RTA_SPACE(4);
            rta_dest->rta_type = RTA_DST;
            swnet = hton32(addr);
            memcpy(RTA_DATA(rta_dest), &swnet, sizeof(swnet));
            rta_gw = (rtattr *) RTA_NEXT(rta_dest, attrlen);
        }
        else
            rta_gw = (rtattr *) RTM_RTA(rtm);
        if (reject)
        {
            rtm->rtm_scope = RT_SCOPE_HOST;
            rtm->rtm_type = RTN_UNREACHABLE;
        }
        else
        {

            in_addr loopback;
            inet_aton("127.0.0.1",&loopback);
            InAddr LoopBack;
            LoopBack=ntoh32(loopback.s_addr);

            InAddr gw;
            gw = hton32(LoopBack);
            rta_gw->rta_len = RTA_SPACE(4);
            rta_gw->rta_type = RTA_GATEWAY;
            memcpy(RTA_DATA(rta_gw), &gw, sizeof(gw));
        }

        if (-1 == send(rtsock, nlm, size, 0))
        {
            printf("add route though routing socket fail.\n");
        }
        delete [] ((char *)nlm);

}
//添加直接连接路由
void SysCalls::add_direct(BSDPhyInt *phyp,InAddr addr, InMask mask)
{
    DirectRoute *rte;
    if((phyp->flags & IFF_UP)==0)
    {
        return;
    }
    addr=addr&mask;
    if(!(rte=(DirectRoute *)directs.find(addr,mask)))
    {
        rte=new DirectRoute(addr,mask);
        directs.add(rte);
    }
    rte->valid=true;
}
