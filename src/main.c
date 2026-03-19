#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#include "header.h"
#include "DEV_Config.h"
#include "LCD_1in47.h"
#include "GUI_BMP.h"

int serial_port = -1;
UWORD *BlackImage = NULL;

void Handler_1IN47_LCD(int signo) {
    printf("\r\nHandler: Program stop\r\n");
    if (BlackImage != NULL) {
        free(BlackImage);
        BlackImage = NULL;
    }
    if (serial_port >= 0) {
        close(serial_port);
        serial_port = -1;
    }
    exit(0);
}

int read_exact(int fd, uint8_t *buffer, int count, int timeout_sec) {
    int total_read = 0;
    int n;
    
    struct termios orig_tty;
    tcgetattr(fd, &orig_tty);
    
    struct termios tty;
    memcpy(&tty, &orig_tty, sizeof(tty));
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = timeout_sec * 10;
    tcsetattr(fd, TCSANOW, &tty);
    
    while (total_read < count) {
        n = read(fd, buffer + total_read, count - total_read);
        if (n < 0) {
            tcsetattr(fd, TCSANOW, &orig_tty);
            return -1;
        }
        if (n == 0) {
            tcsetattr(fd, TCSANOW, &orig_tty);
            return total_read > 0 ? total_read : -1;
        }
        total_read += n;
    }
    
    tcsetattr(fd, TCSANOW, &orig_tty);
    return total_read;
}

int receive_bmp_file(int fd, const char* output_filename) {
    printf("Waiting for file transfer...\n");
    
    uint8_t size_buf[4];
    if (read_exact(fd, size_buf, 4, 10) != 4) {
        printf("Error: Timeout reading file size\n");
        return -1;
    }
    uint32_t fileSize = ((uint32_t)size_buf[3] << 24) |
                        ((uint32_t)size_buf[2] << 16) |
                        ((uint32_t)size_buf[1] << 8)  |
                        ((uint32_t)size_buf[0]);
    
    printf("Expected file size: %u bytes\n", fileSize);
    
    uint8_t ack = 0xAA;
    if (write(fd, &ack, 1) != 1) {
        printf("Error: Failed to send ACK\n");
        return -1;
    }
    
    FILE* out = fopen(output_filename, "wb");
    if (!out) {
        perror("Error: Cannot create output file");
        return -1;
    }
    
    uint8_t buffer[BUFFER_SIZE];
    uint32_t totalReceived = 0;
    
    struct termios orig_tty, tty;
    tcgetattr(fd, &orig_tty);
    tty = orig_tty;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 50;
    tcsetattr(fd, TCSANOW, &tty);
    
    while (totalReceived < fileSize) {
        ssize_t n = read(fd, buffer, BUFFER_SIZE);
        if (n > 0) {
            fwrite(buffer, 1, n, out);
            totalReceived += n;
            printf("\rProgress: %u/%u bytes (%.1f%%)", totalReceived, fileSize, (100.0f * totalReceived / fileSize));
            fflush(stdout);
        } 
        else if (n < 0) {
            printf("\nError: Read error during transfer\n");
            fclose(out);
            tcsetattr(fd, TCSANOW, &orig_tty);
            return -1;
        }
    }
    
    tcsetattr(fd, TCSANOW, &orig_tty);
    
    printf("\n");
    
    uint8_t endMarker;
    if (read_exact(fd, &endMarker, 1, 5) == 1 && endMarker == 0xFF) {
        printf("Transfer completed successfully!\n");
    } 
    else {
        printf("Warning: End marker missing or corrupted\n");
    }
    
    printf("Received %u bytes\n", totalReceived);
    fclose(out);
    return 0;
}

void display_image(const char *bmp_path) {
    printf("Displaying image: %s\n", bmp_path);
    
    Paint_Clear(WHITE);
    Paint_SetRotate(ROTATE_0);
    
    if (GUI_ReadBmp(bmp_path) == 0) {
        LCD_1IN47_Display(BlackImage);
        printf("Image displayed successfully\n");
    } 
    else {
        printf("Failed to load image: %s\n", bmp_path);
    }
}

int main(int argc, char *argv[]) {
    uint8_t rxBuffer[PACKET_SIZE];

    signal(SIGINT, Handler_1IN47_LCD);

    printf("Initializing LCD...\n");
    if (DEV_ModuleInit() != 0) {
        printf("Failed to initialize LCD module\n");
        return 1;
    }

    LCD_1IN47_Init(HORIZONTAL);
    LCD_1IN47_Clear(BLACK);
    LCD_SetBacklight(1023);

    UDOUBLE Imagesize = LCD_1IN47_HEIGHT * LCD_1IN47_WIDTH * 2;
    if ((BlackImage = (UWORD*)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\n");
        return 1;
    }

    Paint_NewImage(BlackImage, LCD_1IN47_WIDTH, LCD_1IN47_HEIGHT, 90, BLACK, 16);

    printf("Opening serial port /dev/ttyGS0...\n");
    serial_port = open("/dev/ttyGS0", O_RDWR | O_NOCTTY);
    if (serial_port < 0) {
        printf("Error opening serial port: %d\n", serial_port);
        free(BlackImage);
        return 1;
    }

    struct termios tty;
    if (tcgetattr(serial_port, &tty) != 0) {
        printf("Error from tcgetattr\n");
        close(serial_port);
        free(BlackImage);
        return 1;
    }
    
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag |= (CREAD | CLOCAL | CS8);
    tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);

    tty.c_iflag = IGNPAR;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    tty.c_oflag = 0;
    tty.c_lflag = 0;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 50;

    tcflush(serial_port, TCIFLUSH);
    tcsetattr(serial_port, TCSANOW, &tty);

    printf("System ready. Waiting for commands...\n");
    printf("Commands:\n");
    printf("  4F 50 XX XX XX XX - Open image (opic) - display existing file\n");
    printf("  4C 50 XX XX XX XX - Load image (lpic) - receive file then display\n\n");

    while (1) {
        uint8_t header[2];
        if (read_exact(serial_port, header, 2, 30) != 2) {
            continue;
        }

        if (read_exact(serial_port, rxBuffer + 2, PACKET_SIZE - 2, 5) != PACKET_SIZE - 2) {
            printf("Incomplete packet received\n");
            continue;
        }
        memcpy(rxBuffer, header, 2);

        printf("Received [%d bytes]: ", PACKET_SIZE);
        for (int i = 0; i < PACKET_SIZE; i++) {
            printf("%02X ", rxBuffer[i]);
        }
        printf("\n");

        uint32_t image_id = ((uint32_t)rxBuffer[2] << 24) |
                            ((uint32_t)rxBuffer[3] << 16) |
                            ((uint32_t)rxBuffer[4] << 8)  |
                            ((uint32_t)rxBuffer[5]);

        char filename[64];
        snprintf(filename, sizeof(filename), "pic/%u.bmp", image_id);

        if (rxBuffer[0] == 0x4F && rxBuffer[1] == 0x50) {
            printf("Command: OPEN_IMAGE, ID: %u\n", image_id);
            
            if (access(filename, F_OK) == 0) {
                display_image(filename);
            } 
            else {
                printf("Error: File not found: %s\n", filename);
            }
        }
        else if (rxBuffer[0] == 0x4C && rxBuffer[1] == 0x50) {
            printf("Command: LOAD_IMAGE, ID: %u\n", image_id);
            printf("Target file: %s\n", filename);
            
            if (receive_bmp_file(serial_port, filename) == 0) {
                printf("File received: %s\n", filename);
            } 
            else {
                printf("Error: File reception failed\n");
            }
        }
        else {
            printf("Unknown command header: %02X %02X\n", rxBuffer[0], rxBuffer[1]);
        }
        
        printf("\nWaiting for next command...\n\n");
    }

    if (BlackImage != NULL) free(BlackImage);
    if (serial_port >= 0) close(serial_port);
    return 0;
}
