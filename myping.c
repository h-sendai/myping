#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>

#include "in_cksum.h"

int print_bytes(unsigned char *buf, int len)
{
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
        if ((i + 1) % 4 == 0) {
            printf("\n");
        }
        else {
            printf(" ");
        }
    }

    return 0;
}

int usage()
{
    char msg[] = "Usage: myping remote_host";
    fprintf(stderr, "%s\n", msg);

    return 0;
}

int use_raw_sock = 1;
int use_ping_sock = 0;
int debug = 0;


int main(int argc, char *argv[])
{
    int c;
    char *interval_string = "1";

    while ( (c = getopt(argc, argv, "di:rp")) != -1) {
        switch (c) {
            case 'd':
                debug = 1;
                break;
            case 'i':
                interval_string = optarg;
                break;
            case 'r':
                use_raw_sock  = 1;
                use_ping_sock = 0;
                break;
            case 'p':
                use_ping_sock = 1;
                use_raw_sock  = 0;
                break;
            default:
                usage();
                exit(1);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 1) {
        usage();
        exit(1);
    }

    struct sockaddr_in sa_send;
    struct sockaddr_in *resaddr;
    struct addrinfo    hints;
    struct addrinfo    *res;
    int errorcode;
    // socklen_t salen = sizeof(struct sockaddr_in);

    res = 0;
    memset((char *)&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = 0;
    if ( (errorcode = getaddrinfo(argv[0], 0, &hints, &res)) != 0) {
        fprintf(stderr, "%s\n", gai_strerror(errorcode));
        exit(1);
    }

    resaddr = (struct sockaddr_in *)res->ai_addr;
    memset((char *)&sa_send, 0, sizeof(sa_send));
    sa_send.sin_family = AF_INET;
    sa_send.sin_port   = htons(0);
    sa_send.sin_addr   = resaddr->sin_addr;
    freeaddrinfo(res);

    int sockfd;
    if (use_raw_sock) {
        sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    }
    else if (use_ping_sock) {
        sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    }
    else {
        fprintf(stderr, "sock type specification error\n");
        exit(1);
    }

    if (sockfd < 0) {
        err(1, "socket");
    }

    unsigned char sendbuf[8000];
    unsigned char recvbuf[8000];
    int datalen = 56;
    // struct icmp is defined in /usr/include/netinet/ip_icmp.h
    // data member
    // icmp_type
    // icmp_code
    // icmp_seq
    // icmp_seq
    // icmp_cksum
    // icmp_data

    useconds_t interval_usec = strtod(interval_string, NULL)*1000000.0;

    for ( ; ; ) {
        struct icmp *icmp = (struct icmp *)sendbuf;
        icmp->icmp_type = ICMP_ECHO;
        icmp->icmp_code = 0;
        icmp->icmp_seq  = 0xfeed;
        icmp->icmp_id   = getpid();
        memset(icmp->icmp_data, 0xFF, datalen);
        gettimeofday((struct timeval *)icmp->icmp_data, NULL);

        int len = 8 + datalen; /* 8: icmp header size */
        icmp->icmp_cksum = 0;
        icmp->icmp_cksum = in_cksum((unsigned short *)icmp, len);

        size_t n;
        n = sendto(sockfd, sendbuf, len, 0, (struct sockaddr *)&sa_send, sizeof(sa_send));
        if (n < 0) {
            err(1, "sendto");
        }
        
        struct sockaddr_in sa_recv;
        memset(&sa_recv, 0, sizeof(sa_recv));
        socklen_t salen = sizeof(struct sockaddr_in);

        n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&sa_recv, &salen);
        if (n < 0) {
            err(1, "recvfrom");
        }
        if (debug) {
            printf("%ld bytes received\n", n);
            print_bytes(recvbuf, n);
        }

        struct timeval *tv0_p, tv0, tv1, rtt;
        int tv_in_recvbuf = 8; /* 8: icmp header */
        if (use_raw_sock) {
            tv_in_recvbuf += 20; /* XXX: 20: IP header.  should be decode IP header length value */
        }

        tv0_p = (struct timeval *)&recvbuf[tv_in_recvbuf];
        tv0 = *tv0_p;
        gettimeofday(&tv1, NULL);
        timersub(&tv1, &tv0, &rtt);
        if (debug) {
            fprintf(stderr, "tv0: %ld.%06ld\n", tv0.tv_sec, tv0.tv_usec);
            fprintf(stderr, "tv1: %ld.%06ld\n", tv1.tv_sec, tv1.tv_usec);
            fprintf(stderr, "rtt: %ld.%06ld\n", rtt.tv_sec, rtt.tv_usec);
        }

        printf("RTT: %ld usec %ld.%06ld %ld.%06ld\n",
            rtt.tv_sec*1000000 + rtt.tv_usec,
            tv0.tv_sec, tv0.tv_usec,
            tv1.tv_sec, tv1.tv_usec);

        usleep(interval_usec);
    }
    return 0;
}
