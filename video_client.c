#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "10.61.1.118"
#define PORT 9999
#define BUFFER_SIZE 65536

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    sendto(sock, "hello", 5, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    printf(">>> Requesting HD Live Stream from %s:%d...\n", SERVER_IP, PORT);

    FILE *ffplay = popen("ffplay -fflags nobuffer -flags low_delay -framedrop -probesize 32 -sync video -i - -window_title \"HD Live Stream\" -loglevel quiet", "w");
    if (!ffplay) {
        printf("[ERROR] Failed to open player window.\n");
        return -1;
    }

    socklen_t addr_len = sizeof(serv_addr);
    while (1) {
        int bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&serv_addr, &addr_len);
        if (bytes_received > 0) {
            fwrite(buffer, 1, bytes_received, ffplay);
            fflush(ffplay);
        }
    }

    pclose(ffplay);
    close(sock);
    return 0;
}