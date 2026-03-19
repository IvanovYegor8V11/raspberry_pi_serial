#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "header.h"

HANDLE setup_serial(const char* port_name) {
    HANDLE hSerial = CreateFileA(port_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        printf("Error: Cannot open port %s\n", port_name);
        return INVALID_HANDLE_VALUE;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(hSerial, &dcb)) {
        printf("Error: Cannot get port state\n");
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hSerial, &dcb)) {
        printf("Error: Cannot set port state\n");
        CloseHandle(hSerial);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.ReadTotalTimeoutConstant = 1000;
    SetCommTimeouts(hSerial, &timeouts);

    PurgeComm(hSerial, PURGE_TXCLEAR | PURGE_RXCLEAR);
    return hSerial;
}

int send_file(HANDLE hSerial, const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open file %s\n", filename);
        return -1;
    }

    fseek(file, 0, SEEK_END);
    uint32_t fileSize = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("Sending file: %s (%u bytes)\n", filename, fileSize);

    DWORD bytesWritten;
    WriteFile(hSerial, &fileSize, sizeof(fileSize), &bytesWritten, NULL);

    uint8_t ack;
    DWORD bytesRead;
    if (!ReadFile(hSerial, &ack, 1, &bytesRead, NULL) || bytesRead != 1 || ack != 0xAA) {
        printf("Error: No acknowledgment from receiver\n");
        fclose(file);
        return -1;
    }

    uint8_t buffer[BUFFER_SIZE];
    uint32_t totalSent = 0;

    while (totalSent < fileSize) {
        uint32_t toRead = (fileSize - totalSent > BUFFER_SIZE) ? BUFFER_SIZE : (fileSize - totalSent);
        size_t readBytes = fread(buffer, 1, toRead, file);
        if (readBytes == 0) break;

        if (!WriteFile(hSerial, buffer, (DWORD)readBytes, &bytesWritten, NULL)) {
            printf("Error: Write failed\n");
            fclose(file);
            return -1;
        }
        totalSent += bytesWritten;
        printf("\rProgress: %u/%u bytes (%.1f%%)", totalSent, fileSize, (100.0f * totalSent / fileSize));
        fflush(stdout);
    }

    uint8_t endMarker = 0xFF;
    WriteFile(hSerial, &endMarker, 1, &bytesWritten, NULL);

    printf("\nFile sent successfully!\n");

    fclose(file);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s <COM_port> <command> <pic_number>\n", argv[0]);
        printf("Example: %s COM8 lpic 1\n", argv[0]);
        printf("Supported commands: 'opic' and 'lpic'\n");
        printf("Note: 'lpic' command will also send the BMP file after the command\n");
        return 0;
    }

    HANDLE hSerial;
    uint8_t txBuffer[PACKET_SIZE];
    DWORD bytesWritten;

    printf("Opening port %s...\n", argv[1]);
    char com_port[16];
    snprintf(com_port, sizeof(com_port), "\\\\.\\%s", argv[1]);

    char command[5];
    snprintf(command, sizeof(command), "%s", argv[2]);

    if (strcmp(command, "opic") == 0) {
        txBuffer[0] = 0x4F;                     // 'O' in ASCII
        txBuffer[1] = 0x50;                     // 'P' in ASCII
    }
    else if (strcmp(command, "lpic") == 0) {
        txBuffer[0] = 0x4C;                     // 'L' in ASCII
        txBuffer[1] = 0x50;                     // 'P' in ASCII
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

    hSerial = setup_serial(com_port);
    if (hSerial == INVALID_HANDLE_VALUE) {
        return 1;
    }

    Sleep(500);

    if (!WriteFile(hSerial, txBuffer, PACKET_SIZE, &bytesWritten, NULL)) {
        printf("Error: Failed to send command\n");
        CloseHandle(hSerial);
        return 1;
    }
    printf("Command sent: %ld bytes\n", bytesWritten);

    if (strcmp(command, "lpic") == 0) {
        printf("\n--- LPIC command detected ---\n");
        printf("Starting BMP file transfer...\n");

        char filename[64];
        snprintf(filename, sizeof(filename), "%lu.bmp", val);
        
        printf("Looking for file: %s\n", filename);
        
        Sleep(500);
        
        if (send_file(hSerial, filename) != 0) {
            printf("Error: File transfer failed\n");
            CloseHandle(hSerial);
            return 1;
        }
        
        printf("File transfer completed successfully!\n");
    }

    CloseHandle(hSerial);
    printf("Done.\n");
    return 0;
}
