#include <windows.h>
#include <stdio.h>
#include <errno.h>

#include "header.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("You need to specify the COM port, comand and number of the picture\n \
                (e.g. ./serial_sender COM8 opic 1)");
        exit(0);
    }

    HANDLE hSerial;
    DCB dcbSerialParams = {0};
    uint8_t txBuffer[PACKET_SIZE];
    DWORD bytesWritten;

    printf("Opening port %s...\n", argv[1]);
    char com_port[16];
    snprintf(com_port, sizeof(com_port), "\\\\.\\%s", argv[1]);

    char command[5];
    snprintf(command, sizeof(command), argv[2]);


    if (strcmp(command, "opic") == 0) {
        txBuffer[0] = 0x4F;                     // 'O' in ASCII
        txBuffer[1] = 0x50;                     // 'P' in ASCII
    }
    else if (strcmp(command, "lpic") == 0) {
        txBuffer[0] = 0x4F;                     // 'O' in ASCII
        txBuffer[1] = 0x4C;                     // 'L' in ASCII
    }
    else {
        printf("Supported commands are only \'opic\' and \'lpic\'. Your input: %s", argv[2]);
        return 1;
    }

    char *end;
    errno = 0;
    unsigned long val = strtoul(argv[3], &end, 10);

    if (errno == ERANGE || val > UINT32_MAX) {
        printf("Number of pic too large\n");
        return 2;
    }
    else if (end == argv[3]) {
        printf("No number of pic\n");
        return 3;
    }
    else if (*end != '\0') {
        printf("Invalid characters after number\n");
        return 4;
    }
    else {
        txBuffer[2] = (val >> 24) & 0xFF;
        txBuffer[3] = (val >> 16) & 0xFF;
        txBuffer[4] = (val >> 8)  & 0xFF;
        txBuffer[5] = val & 0xFF;
    }

    printf("Sending Packet: ");

    for (uint8_t i = 0; i < PACKET_SIZE; i++) {
        printf("%02X ", txBuffer[i]);
    }
    printf("\n");

    hSerial = CreateFile(com_port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("Error opening %s\n", argv[1]);
        return 3;
    }

    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    SetCommState(hSerial, &dcbSerialParams);

    Sleep(500);

    txBuffer[0] = 0x4F; 
    txBuffer[1] = 0x50;
    
    txBuffer[2] = 0x4F;
    txBuffer[3] = 0x50;
    txBuffer[4] = 0x49;
    txBuffer[5] = 0x4E;     // 4E or 4D 

    if (WriteFile(hSerial, txBuffer, PACKET_SIZE, &bytesWritten, NULL)) {
        printf("Sent %ld bytes\n", bytesWritten);
    } 
    else {
        printf("Write Error\n");
        CloseHandle(hSerial);
        return 1;
    }

    CloseHandle(hSerial);
    return 0;
}