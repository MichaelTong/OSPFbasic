#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <net/route.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <linux/version.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#define _LINUX_SOCKIOS_H
#define _LINUX_IN_H
#include <linux/mroute.h>
#include <netinet/ip.h>
#include <linux/if_tunnel.h>
#include <net/if_arp.h>
#include "include.h"
#include "system.h"
#include <time.h>

SysCalls *ospfd_sys;

void timer(int)
{
    signal(SIGALRM, timer);
    ospfd_sys->one_second_timer();
}

int main(int argc, char *argv[])
{
    int n_fd;
    itimerval itim;
    fd_set fdset;
    fd_set wrset;
    sigset_t sigset,osigset;

    sys=ospfd_sys=new SysCalls();
    printf("Start initializing...\n");

    ospfd_sys->configure();
    if(!ospf)
    {
        printf("Initialization failed\n");
        exit(1);
    }

    signal(SIGALRM,timer);
    itim.it_interval.tv_sec = 1;
    itim.it_value.tv_sec = 1;
    itim.it_interval.tv_usec = 0;
    itim.it_value.tv_usec = 0;
    if (setitimer(ITIMER_REAL, &itim, NULL) < 0)
        printf("Set Timer error\n");
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);
    sigprocmask(SIG_BLOCK, &sigset, &osigset);

    sys->ip_forward(true);

    printf("Initializtation finished.\n");
    //主循环，进行所有动作
    while(1)
    {
        int msec_tmo;
        int err;
        FD_ZERO(&fdset);
        FD_ZERO(&wrset);
        n_fd = ospfd_sys->netfd;
        FD_SET(ospfd_sys->netfd, &fdset);
        if (ospfd_sys->rtsock != -1)
        {
            FD_SET(ospfd_sys->rtsock, &fdset);
            n_fd = MAX(n_fd, ospfd_sys->rtsock);
        }
        ospf->tick();
        msec_tmo=ospf->timeout();
        sigprocmask(SIG_SETMASK, &osigset, NULL);
        if (msec_tmo != -1)
        {
            timeval timeout;
            timeout.tv_sec = msec_tmo/1000;
            timeout.tv_usec = (msec_tmo % 1000) * 1000;
            err = select(n_fd+1, &fdset, &wrset, 0, &timeout);
        }
        else
            err = select(n_fd+1,&fdset,&wrset,0,0);
        if (err == -1 && errno != EINTR)
        {
            printf("select failed \n");
            exit(1);
        }

        ospfd_sys->time_update();
        sigprocmask(SIG_BLOCK, &sigset, &osigset);
        if(err<=0)
            continue;
        //接收网络分组
        if (FD_ISSET(ospfd_sys->netfd, &fdset))
            ospfd_sys->raw_receive(ospfd_sys->netfd);
        //if (ospfd_sys->rtsock != -1 && FD_ISSET(ospfd_sys->rtsock, &fdset))
        //    ospfd_sys->netlink_receive(ospfd_sys->rtsock);
    }
}
