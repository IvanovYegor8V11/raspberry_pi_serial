#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "header.h"

int setup_serial(const char* port_name) {
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error: Cannot open port %s: %s\n", port_name, strerror(errno));
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        printf("Error: Cannot get port state: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | CSTOPB);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(CRTSCTS);

    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 10;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error: Cannot set port state: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    int status = TIOCM_DTR | TIOCM_RTS;
    ioctl(fd, TIOCMSET, &status);

    return fd;
}

int serial_write(int fd, const void* buf, size_t count) {
    ssize_t total_written = 0;
    while (total_written < count) {
        ssize_t written = write(fd, (const char*)buf + total_written, count - total_written);
        if (written < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (written == 0) break;
        total_written += written;
    }
    return total_written;
}

int serial_read(int fd, void* buf, size_t count, int timeout_ms) {
    ssize_t total_read = 0;
    
    while (total_read < count) {
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        int ret = select(fd + 1, &readfds, NULL, NULL, &timeout);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (ret == 0) {
            return total_read;
        }
        
        ssize_t r = read(fd, (char*)buf + total_read, count - total_read);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;
        total_read += r;
    }
    return total_read;
}

int send_file(int fd, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open file %s: %s\n", filename, strerror(errno));
        return -1;
    }

    fseek(file, 0, SEEK_END);
    uint32_t fileSize = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("Sending file: %s (%u bytes)\n", filename, fileSize);

    if (serial_write(fd, &fileSize, sizeof(fileSize)) != sizeof(fileSize)) {
        printf("Error: Failed to send file size\n");
        fclose(file);
        return -1;
    }

    uint8_t ack;
    int bytesRead = serial_read(fd, &ack, 1, 1000);
    if (bytesRead != 1 || ack != ACK) {
        printf("Error: No acknowledgment from receiver (got %d bytes, ack=0x%02X)\n", bytesRead, ack);
        fclose(file);
        return -1;
    }

    uint8_t buffer[BUFFER_SIZE];
    uint32_t totalSent = 0;

    while (totalSent < fileSize) {
        uint32_t toRead = (fileSize - totalSent > BUFFER_SIZE) ? BUFFER_SIZE : (fileSize - totalSent);
        size_t readBytes = fread(buffer, 1, toRead, file);
        if (readBytes == 0) break;

        int written = serial_write(fd, buffer, readBytes);
        if (written < 0) {
            printf("Error: Write failed: %s\n", strerror(errno));
            fclose(file);
            return -1;
        }
        totalSent += written;
        printf("\rProgress: %u/%u bytes (%.1f%%)", totalSent, fileSize, (100.0f * totalSent / fileSize));
        fflush(stdout);
    }

    uint8_t endMarker = END;
    serial_write(fd, &endMarker, 1);

    printf("\nFile sent successfully!\n");

    fclose(file);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s <serial_port> <command> <pic_number>\n", argv[0]);
        printf("Example: %s /dev/ttyUSB0 lpic 1\n", argv[0]);
        printf("Supported commands: 'opic' and 'lpic'\n");
        printf("Note: 'lpic' command will also send the BMP file after the command\n");
        return 0;
    }

    int fd;
    uint8_t txBuffer[PACKET_SIZE];

    printf("Opening port %s...\n", argv[1]);

    char command[5];
    snprintf(command, sizeof(command), "%s", argv[2]);

    if (strcmp(command, "opic") == 0) {
        txBuffer[0] = OPEN;
        txBuffer[1] = PIC;
    }
    else if (strcmp(command, "lpic") == 0) {
        txBuffer[0] = LOAD;
        txBuffer[1] = PIC;
    }
    else {
        printf("Error: Unsupported command '%s'\n", argv[2]);
        printf("Supported commands are only 'opic' and 'lpic'.\n");
        return 1;
    }

    char *end;
    errno = 0;
    unsigned long val = strtoul(argv[3], &end, 10);

    if (errno == ERANGE || val > UINT32_MAX) {
        printf("Error: Picture number too large\n");
        return 2;
    }
    else if (end == argv[3]) {
        printf("Error: No picture number provided\n");
        return 3;
    }
    else if (*end != '\0') {
        printf("Error: Invalid characters after number\n");
        return 4;
    }
    else {
        txBuffer[2] = (val >> 24) & 0xFF;
        txBuffer[3] = (val >> 16) & 0xFF;
        txBuffer[4] = (val >> 8)  & 0xFF;
        txBuffer[5] = val & 0xFF;
    }

    printf("Sending command packet: ");
    for (uint8_t i = 0; i < PACKET_SIZE; i++) {
        printf("%02X ", txBuffer[i]);
    }
    printf("\n");

    fd = setup_serial(argv[1]);
    if (fd < 0) {
        return 1;
    }

    usleep(500000);

    if (serial_write(fd, txBuffer, PACKET_SIZE) != PACKET_SIZE) {
        printf("Error: Failed to send command\n");
        close(fd);
        return 1;
    }
    printf("Command sent: %d bytes\n", PACKET_SIZE);

    if (strcmp(command, "lpic") == 0) {
        printf("\n--- LPIC command detected ---\n");
        printf("Starting BMP file transfer...\n");

        char filename[64];
        snprintf(filename, sizeof(filename), "%lu.bmp", val);
        
        printf("Looking for file: %s\n", filename);
        
        usleep(500000);
        
        if (send_file(fd, filename) != 0) {
            printf("Error: File transfer failed\n");
            close(fd);
            return 1;
        }
        
        printf("File transfer completed successfully!\n");
    }

    close(fd);
    printf("Done.\n");
    return 0;
}