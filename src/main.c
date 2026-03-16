#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

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
    }

    exit(0);
}

int read_exact(int fd, uint8_t *buffer, int count) {
    int total_read = 0;
    int n = 0;
    while (total_read < count) {
        n = read(fd, buffer + total_read, count - total_read);
        if (n < 0) return -1;
        if (n == 0) break;
        total_read += n;
    }
    return total_read;
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

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag |= (CREAD | CLOCAL);
    tty.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);

    tty.c_iflag = IGNPAR;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    tty.c_oflag = 0;
    tty.c_lflag = 0;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 50;

    tcflush(serial_port, TCIFLUSH);
    tcsetattr(serial_port, TCSANOW, &tty);

    printf("System ready. Waiting for commands...\n");
    printf("Send: 4F 50 XX XX XX XX (where XX is image ID)\n\n");

    while (1) {
        int n = read_exact(serial_port, rxBuffer, PACKET_SIZE);

        if (n == PACKET_SIZE) {
            printf("Received [%d bytes]: ", n);

            for (int i = 0; i < n; i++) {
                printf("%02X ", rxBuffer[i]);
            }
            printf("\n");

            if (rxBuffer[0] == 0x4F && rxBuffer[1] == 0x50) {
                uint32_t image_id = 0;
                memcpy(&image_id, &rxBuffer[2], 4);

                image_id = ((image_id & 0x000000FF) << 24) |
                           ((image_id & 0x0000FF00) << 8)  |
                           ((image_id & 0x00FF0000) >> 8)  |
                           ((image_id & 0xFF000000) >> 24);

                char filename[32];
                snprintf(filename, sizeof(filename), "pic/%u.bmp", image_id);

                printf("Command: OPEN_IMAGE, ID: 0x%08X\n", image_id);

                if (access(filename, F_OK) == 0) {
                    display_image(filename);
                }
                else {
                    printf("Unknown image ID: 0x%08X\n", image_id);
                }
            }
            else {
                printf("Unknown command header\n");
            }
        }
        else if (n > 0) {
            printf("Incomplete packet received: %d bytes\n", n);
        }
    }

    return 0;
}