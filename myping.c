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

int set_sockaddr_in(struct sockaddr_in *sa, char *remote_host)
{
    struct sockaddr_in *resaddr;
    struct addrinfo    hints;
    struct addrinfo    *res;
    int errorcode;

    res = 0;
    memset((char *)&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = 0;
    if ( (errorcode = getaddrinfo(remote_host, 0, &hints, &res)) != 0) {
        fprintf(stderr, "%s\n", gai_strerror(errorcode));
        exit(1);
    }

    resaddr = (struct sockaddr_in *)res->ai_addr;
    memset((char *)sa, 0, sizeof(struct sockaddr_in));
    sa->sin_family = AF_INET;
    sa->sin_port   = htons(0);
    sa->sin_addr   = resaddr->sin_addr;
    freeaddrinfo(res);
    
    return 0;
}

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
    char *remote_host = argv[0];

    struct sockaddr_in sa_send;
    set_sockaddr_in(&sa_send, remote_host);

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
    memset(sendbuf, 0, sizeof(sendbuf));
    memset(recvbuf, 0, sizeof(recvbuf));
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

    int seq_num = 0;
    pid_t pid = getpid();
    for ( ; ; ) {
        seq_num ++;
        struct icmp *icmp = (struct icmp *)sendbuf;
        icmp->icmp_type = ICMP_ECHO;
        icmp->icmp_code = 0;
        icmp->icmp_seq  = htons(seq_num);
        icmp->icmp_id   = htons(pid);
        memset(icmp->icmp_data, 0xFF, datalen);
        gettimeofday((struct timeval *)&(icmp->icmp_data), NULL);

        int len = 8 + datalen; /* 8: icmp header size */
        icmp->icmp_cksum = 0;
        icmp->icmp_cksum = in_cksum((unsigned short *)icmp, len);
        if (debug) {
            fprintf(stderr, "---> sendbuf\n");
            print_bytes(sendbuf, len);
        }

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
            fprintf(stderr, "---> recvbuf\n");
            fprintf(stderr, "%ld bytes received\n", n);
            print_bytes(recvbuf, n);
        }

        struct timeval *tv0_p, tv0, tv1, rtt;
        int tv_in_recvbuf = 8; /* 8: icmp header */
        int type_pos = 0;
        if (use_raw_sock) {
            tv_in_recvbuf += 20; /* XXX: 20: IP header.  should be decode IP header length value */
            type_pos += 20;
        }

        //unsigned char *type_p = &recvbuf[type_pos];
        //if (*type_p != ICMP_ECHOREPLY) {
            //continue;
        //}

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

        if (debug) {
            fprintf(stderr, "sleep\n");
        }
        usleep(interval_usec);
    }
    return 0;
}
