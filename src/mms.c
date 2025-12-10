
// src/mms.c
#include "mms.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

int mms_send(double value) {
    int sockfd;
    struct sockaddr_in addr;
    char buffer[128];
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(buffer, sizeof(buffer),
             "MMS: value=%.2f ts=%ld.%09ld",
             value, ts.tv_sec, ts.tv_nsec);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    // --- Multicast options ---
    // TTL = 1 (ne pas sortir du LAN)
    unsigned char ttl = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        fprintf(stderr, "[WARN] setsockopt(IP_MULTICAST_TTL): %s\n", strerror(errno));
    }

    // Interface d'envoi = eth0 de BB1 (adapter si nécessaire)
    struct in_addr iface;
    iface.s_addr = inet_addr("192.168.0.101"); // <-- IP de BB1 sur eth0
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof(iface)) < 0) {
        fprintf(stderr, "[WARN] setsockopt(IP_MULTICAST_IF): %s\n", strerror(errno));
    }

    // Adresse de destination = groupe multicast
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(MMS_PORT);
    addr.sin_addr.s_addr = inet_addr(MMS_GROUP);
    // printf("je prerape lenvoie\n");
    int ret = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&addr, sizeof(addr));
    // printf("jai envoye\n");
    if (ret < 0) {
        fprintf(stderr, "[ERROR] sendto(%s:%d): %s\n", MMS_GROUP, MMS_PORT, strerror(errno));
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}
