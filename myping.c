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
#include "set_timer.h"
#include "my_signal.h"

// struct icmp is defined in /usr/include/netinet/ip_icmp.h
// data member
// icmp_type
// icmp_code
// icmp_seq
// icmp_seq
// icmp_cksum
// icmp_data

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

int debug         = 0;
int use_raw_sock  = 1;
int use_ping_sock = 0;

// global variables used in sig_alrm signal handler
int seq_num       = 0;
int datalen       = 56;
pid_t pid         = 0;
unsigned char sendbuf[8000];
int sockfd;
struct sockaddr_in sa_send;
 
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

void sig_alrm(int signo)
{
    if (debug) {
        fprintf(stderr, "sig_alrm()\n");
    }

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
    
    return;
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

    set_sockaddr_in(&sa_send, remote_host);

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

    pid = getpid(); // will be used in sig_alrm

    unsigned char recvbuf[8000];
    memset(sendbuf, 0, sizeof(sendbuf));
    memset(recvbuf, 0, sizeof(recvbuf));

    struct timeval start, elapsed;
    gettimeofday(&start, NULL);

    struct timeval interval_tv = str2timeval(interval_string);
    my_signal(SIGALRM, sig_alrm);
    set_timer(interval_tv.tv_sec, interval_tv.tv_usec, interval_tv.tv_sec, interval_tv.tv_usec);

    struct sockaddr_in sa_recv;
    memset(&sa_recv, 0, sizeof(sa_recv));
    socklen_t salen = sizeof(struct sockaddr_in);
    for ( ; ; ) {
        int n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&sa_recv, &salen);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                err(1, "recvfrom");
            }
        }
        if (debug) {
            fprintf(stderr, "---> recvbuf\n");
            fprintf(stderr, "%d bytes received\n", n);
            print_bytes(recvbuf, n);
        }

        unsigned char *ptr = recvbuf;
        if (use_raw_sock) {
            /*
             * If we use SOCK_RAW, recvbuf contains IP header at the beginning of the buffer.
             * Decode the IP header length using struct ip in /usr/include/netinet/ip.h
             */
            struct ip *ip = (struct ip *)recvbuf;
            ptr += (ip->ip_hl)*4; // Unit of ip_hl is 4 octets
        }
        struct icmp *icmp = (struct icmp *)ptr;

        if (icmp->icmp_type != ICMP_ECHOREPLY) {
            if (debug) {
                fprintf(stderr, "read not ICMP_ECHOREPLY packet\n");
            }
            continue;
        }
        struct timeval tv0, tv1, rtt;
        tv0 = *((struct timeval *)icmp->icmp_data);
        gettimeofday(&tv1, NULL);
        timersub(&tv1, &tv0, &rtt);
        timersub(&tv0, &start, &elapsed);
        if (debug) {
            fprintf(stderr, "tv0: %ld.%06ld\n", tv0.tv_sec, tv0.tv_usec);
            fprintf(stderr, "tv1: %ld.%06ld\n", tv1.tv_sec, tv1.tv_usec);
            fprintf(stderr, "rtt: %ld.%06ld\n", rtt.tv_sec, rtt.tv_usec);
        }

        printf("%ld.%06ld %ld usec %d\n",
            elapsed.tv_sec, elapsed.tv_usec,
            rtt.tv_sec*1000000 + rtt.tv_usec,
            ntohs(icmp->icmp_seq));
        fflush(stdout);
    }
    return 0;
}
