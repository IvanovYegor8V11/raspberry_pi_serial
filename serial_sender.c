#include <windows.h>
#include <stdio.h>
#include <stdint.h>

//дефайны по траснпорту и системе команд выноси в отдельный хедер, общий для девайса и хоста

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

    
    //по идее и скорость COM тоже 
    // в аругменты добавь картинку!
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

    //все мэджик намберы выноси в дефайны или енумы
    txBuffer[0] = 0x4F;     //зачем 2 раза?
    txBuffer[1] = 0x50; 
    
    txBuffer[2] = 0x4F;     //зачем 2 раза?
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

//чует мое сердце это винда. под убунтой код немного другой. Поэтому давай в майне вызывать функции-обертки, внутри которой разная реализация
//в зависимости от типа ОС.
