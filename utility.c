#include <stdio.h>

#ifdef _WIN32

#include <winsock2.h>

#endif

#ifdef _WIN32

void sleep(int second) {
    Sleep(second * 1000);
}

#endif

void println_hex(char *data, int length) {
    int i;
    for (i = 0; i < length; ++i) {
        printf("%02x", (unsigned char) data[i]);
    }
    printf("\n");
}

#ifdef _WIN32

void error(char *message) {
    printf("%s: %d\n", message, WSAGetLastError());
}

#else

void error(char* message) {
    perror(message);
}

#endif
