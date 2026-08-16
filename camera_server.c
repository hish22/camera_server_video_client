#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9999
#define BUFFER_SIZE 60000

int main() {
    int server_fd;
    struct sockaddr_in address, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (const struct sockaddr *)&address, sizeof(address));

    printf(">>> High-Quality & Fast Stream Server ready on port %d...\n", PORT);

    recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
    printf(">>> Client connected! Streaming High-Quality Audio & Video...\n");

    FILE *ffmpeg = popen("ffmpeg -f v4l2 -i /dev/video0 -f alsa -i default "
                         "-vf scale=640:480 -r 30 "
                         "-c:v mpeg2video -b:v 2M -maxrate 2.5M -bufsize 1M "
                         "-c:a mp2 -b:a 192k -ar 44100 "
                         "-f mpegts -fflags nobuffer+fastseek - 2>/dev/null", "r");

    if (!ffmpeg) {
        printf("[ERROR] Could not access camera or microphone.\n");
        return -1;
    }

    while (1) {
        int bytes_read = fread(buffer, 1, BUFFER_SIZE, ffmpeg);
        if (bytes_read > 0) {
            sendto(server_fd, buffer, bytes_read, 0, (struct sockaddr *)&client_addr, addr_len);
        }
        usleep(200);
    }

    pclose(ffmpeg);
    close(server_fd);
    return 0;
}