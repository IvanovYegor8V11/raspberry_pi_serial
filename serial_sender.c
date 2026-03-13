#include <windows.h>
#include <stdio.h>
#include <stdint.h>

const uint8_t PACKET_SIZE = 6;

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("You need to specify the COM port (e.g. ./serial_sender COM8)");
        exit(0);
    }

    HANDLE hSerial;
    DCB dcbSerialParams = {0};
    uint8_t txBuffer[PACKET_SIZE];
    DWORD bytesWritten;

    printf("Opening port %s...\n", argv[1]);
    char com_port[16];
    snprintf(com_port, sizeof(com_port), "\\\\.\\%s", argv[1]);

    hSerial = CreateFile(com_port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("Error opening %s\n", argv[1]);
        return 1;
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

    printf("Sending Packet: ");

    for (uint8_t i = 0; i < PACKET_SIZE; i++) {
        printf("%02X ", txBuffer[i]);
    }
    printf("\n");

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