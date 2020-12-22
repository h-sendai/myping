#include <inttypes.h>
#include <stdio.h>

/* From Unix Network Programming (3rd edition) section 28.5 */
uint16_t
in_cksum(uint16_t *addr, int len)
{
    int             nleft = len;
    uint32_t        sum = 0;
    uint16_t        *w = addr;
    uint16_t        answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)  {
        sum += *w++;
        nleft -= 2;
    }

        /* 4mop up an odd byte, if necessary */
    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w ;
        sum += answer;
    }

        /* 4add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff); /* add hi 16 to low 16 */
    sum += (sum >> 16);         /* add carry */
    answer = ~sum;              /* truncate to 16 bits */
    return(answer);
}

#if 0
int main(int argc, char *argv[])
{
    unsigned char buf[8];
    buf[0] = 0x00;
    buf[1] = 0x01;
    buf[2] = 0x02;
    buf[3] = 0x03;
    buf[4] = 0x04;
    buf[5] = 0x05;
    buf[6] = 0x06;
    buf[7] = 0x07;

    uint16_t a = in_cksum((unsigned short *) buf, sizeof(buf));
    printf("%x\n", a);

    return 0;
}
#endif
