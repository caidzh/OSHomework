#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

void packet_handler(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes) {
    printf("Packet captured: length %d\n", h->len);
    struct ip *ip_hdr = (struct ip *)(bytes + 14);
    switch (ip_hdr->ip_p) {
        case IPPROTO_TCP:
            printf("TCP packet captured\n");
            break;
        case IPPROTO_UDP:
            printf("UDP packet captured\n");
            break;
        default:
            printf("Other protocol packet captured\n");
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <interface> <filter_rule>\n", argv[0]);
        return 1;
    }

    char *dev = argv[1]; // network dev (in /sys/class/net)
    char *filter_exp = argv[2]; // filter (test TCP/UDP)
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    struct bpf_program fp;
    bpf_u_int32 net = 0;

    handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        printf("Couldn't open device");
        return 2;
    }
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        printf("Couldn't install filter");
        return 2;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        printf("Couldn't install filter");
        return 2;
    }

    printf("Start capturing on %s with filter: %s\n", dev, filter_exp);
    pcap_loop(handle, 10, packet_handler, NULL);

    pcap_freecode(&fp);
    pcap_close(handle);
    return 0;
}
