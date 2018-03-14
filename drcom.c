#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <time.h>

#include <setjmp.h>

#include "conf.h"
#include "drcom.h"
#include "md5.h"
#include "utility.h"

#ifdef _WIN32

#include <winsock2.h>

typedef SOCKET sock_type;

#else

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int sock_type;

#endif

static char *SERVER;
static char *HOST_IP;
static char *DHCP_SERVER;

static str USERNAME, PASSWORD;

static jmp_buf exit_point;

static int IN_HUXI;
static str SALT, TAIL;
static int SALT_LEN = 4;
static int TAIL_LEN = 16;
static sock_type sock;

static struct sockaddr_in local_address, server_address;
#define s_send(data)                                                    \
    sendto(sock, data, data##_len, 0, (struct sockaddr *)&server_address, \
           sizeof(server_address))
#define s_recv(data) recvfrom(sock, data, BUF_SIZE, 0, NULL, NULL)

static void append(char **dest, char *src, int length) {
    int i;
    for (i = 0; i < length; ++i) {
        (*dest)[i] = src[i];
    }
    (*dest) += length;
}

static int set_keep_alive_data(str keep_alive_data, char number, str tail,
                               char type) {
    memset(keep_alive_data, 0, BUF_SIZE);
    char *data_writer = keep_alive_data;

    append(&data_writer, "\x07", 1);
    *(data_writer++) = number;
    append(&data_writer, "\x28\x00\x0b", 3);
    *(data_writer++) = type;
    if (IN_HUXI) {
        append(&data_writer, "\xdc\x02\xe7\x45", 4);
    } else {
        append(&data_writer, "\xd8\x02\x2f\x12", 4);
    }
    data_writer += 6;

    append(&data_writer, tail, 4);

    data_writer += 4;

    if (type == 3) {
        data_writer += 4;
        int i;
        for (i = 0; i < 4; ++i) {
            *(data_writer++) = HOST_IP[i];
        }
        data_writer += 8;
    } else {
        data_writer += 24;
    }

    return (int) (data_writer - keep_alive_data);
}

static void keep_alive1() {
    str keep_alive1_data = {0};
    char *data_writer = keep_alive1_data;

    /* Set keep alive1 data */

    append(&data_writer, "\xff", 1);

    md5_state_t md5_context;
    str md5_result = {0};
    md5_init(&md5_context);
    md5_append(&md5_context, (md5_byte_t *) "\x03\x01", 2);
    md5_append(&md5_context, (md5_byte_t *) SALT, SALT_LEN);
    md5_append(&md5_context, (md5_byte_t *) PASSWORD, (int) strlen(PASSWORD));
    md5_finish(&md5_context, (md5_byte_t *) md5_result);
    append(&data_writer, md5_result, 16);

    data_writer += 3;

    append(&data_writer, TAIL, TAIL_LEN);

    int time_now = (int) time(NULL);
    time_now %= 0xFFFF;
    *(data_writer++) = (char) (time_now / 256);
    *(data_writer++) = (char) (time_now % 256);

    data_writer += 4;

    /* Communicate with server */

    int keep_alive1_data_len = (int) (data_writer - keep_alive1_data);

    if (s_send(keep_alive1_data) < 0) {
        error("keep_alive1_send");
        longjmp(exit_point, 1);
    }
    printf("keepalive1 sent:");
    println_hex(keep_alive1_data, keep_alive1_data_len);

    str recv_data = {0};
    if (s_recv(recv_data) < 0) {
        error("keep_alive1_recv");
        longjmp(exit_point, 1);
    }
    if (recv_data[0] == '\x07') {
        printf("Keepalive1 succeed\n");
    } else {
        printf("Keep alive failed:");
        println_hex(recv_data, BUF_SIZE);
        longjmp(exit_point, 1);
    }
}

static void keep_alive2_step(char number, int tail, char type) {
    str keep_alive2_data = {0};
    int keep_alive2_data_len = 0;
    static char keep_alive2_tail[5];
    keep_alive2_data_len =
            set_keep_alive_data(keep_alive2_data, number,
                                tail ? keep_alive2_tail : "\x00\x00\x00\x00", type);

    str recv_data = {0};
    if (s_send(keep_alive2_data) < 0) {
        error("keep_alive2_send");
        longjmp(exit_point, 1);
    }
    printf("keep_alive2_data sent:");
    println_hex(keep_alive2_data, keep_alive2_data_len);
    if (s_recv(recv_data) < 0) {
        error("keep_alive2_recv");
        longjmp(exit_point, 1);
    }
    if (recv_data[0] == '\x07') {
        printf("Step succeed\n");
    } else {
        printf("keepalive 2 failed:");
        printf("recv_data");
        println_hex(recv_data, BUF_SIZE);
        longjmp(exit_point, 1);
    }

    /* Retain the 16 to 20 bit of recv_data as tail */
    memcpy(keep_alive2_tail, recv_data + 16, 4);
    keep_alive2_tail[4] = 0;
}

static void keep_alive2() {
    printf("Sending first keepalive2 1\n");
    keep_alive2_step(0, 0, 1);
    printf("Sending first keepalive2 2\n");
    keep_alive2_step(1, 0, 1);
    printf("Sending first keepalive2 3\n");
    keep_alive2_step(2, 1, 3);

    int i = 3;
    while (1) {
        printf("Sending keepalive 2 1\n");
        keep_alive2_step(i, 1, 1);
        printf("Sending keepalive 2 2\n");
        keep_alive2_step(i + 1, 1, 3);
        i = (i + 2) % 0xFF;
        sleep(20);
        keep_alive1();
    }
}

static int set_login_data(str login_data) {
    memset(login_data, 0, BUF_SIZE);

    char *data_writer = login_data;
    int i, j;

    append(&data_writer, "\x03\x01\x00", 3);
    *(data_writer++) = (char) (strlen(USERNAME) + 20);

    str md5_result = {0};
    md5_state_t md5_context;
    md5_init(&md5_context);
    md5_append(&md5_context, (md5_byte_t *) "\x03\x01", 2);
    md5_append(&md5_context, (md5_byte_t *) SALT, SALT_LEN);
    md5_append(&md5_context, (md5_byte_t *) PASSWORD, (int) strlen(PASSWORD));
    md5_finish(&md5_context, (md5_byte_t *) md5_result);
    append(&data_writer, md5_result, 16);

    memcpy(data_writer, USERNAME, strlen(USERNAME));
    data_writer += 36;

    *(data_writer++) = 0x20;
    *(data_writer++) = 0x02;

    uint64_t sum = 0;
    for (i = 0; i < 6; i++) {
        sum = md5_result[i] + sum * 256;
    }
    sum ^= MAC_ADDR;
    for (i = 6; i > 0; i--) {
        *(data_writer + i - 1) = (char) (sum % 256);
        sum /= 256;
    }
    data_writer += 6;

    memset(md5_result, 0, BUF_SIZE);
    md5_init(&md5_context);
    md5_append(&md5_context, (md5_byte_t *) "\x01", 1);
    md5_append(&md5_context, (md5_byte_t *) PASSWORD, (int) strlen(PASSWORD));
    md5_append(&md5_context, (md5_byte_t *) SALT, SALT_LEN);
    md5_append(&md5_context, (md5_byte_t *) "\x00\x00\x00\x00", 4);
    md5_finish(&md5_context, (md5_byte_t *) md5_result);
    append(&data_writer, md5_result, 16);

    append(&data_writer, "\x01", 1);

    for (i = 0; i < 4; i++) {
        *(data_writer++) = HOST_IP[i];
    }

    data_writer += 12;

    memset(md5_result, 0, BUF_SIZE);
    md5_init(&md5_context);
    md5_append(&md5_context, (md5_byte_t *) login_data, (int) (data_writer - login_data));
    md5_append(&md5_context, (md5_byte_t *) "\x14\x00\x07\x0b", 4);
    md5_finish(&md5_context, (md5_byte_t *) md5_result);
    append(&data_writer, md5_result, 8);

    append(&data_writer, "\x01", 1);

    data_writer += 4;

    memcpy(data_writer, HOST_NAME, strlen(HOST_NAME));
    data_writer += 32;

    append(&data_writer, "\x08\x08\x08\x08", 4);

    for (i = 0; i < 4; i++) {
        *(data_writer++) = DHCP_SERVER[i];
    }

    data_writer += 12;

    append(&data_writer, "\x94\x00\x00\x00", 4);
    append(&data_writer, "\x05\x00\x00\x00", 4);
    append(&data_writer, "\x01\x00\x00\x00", 4);
    append(&data_writer, "\x28\x0a\x00\x00", 4);
    append(&data_writer, "\x02\x00\x00\x00", 4);

    memcpy(data_writer, HOST_OS, strlen(HOST_OS));
    data_writer += 32;
    data_writer += 96;

    append(&data_writer, "\x0a\x00", 2);
    append(&data_writer, "\x02\x0c", 2);

    /* checksum point */
    char *check_point = data_writer;
    append(&data_writer, "\x01\x26\x07\x11\x00\x00", 6);

    sum = MAC_ADDR;
    for (i = 0; i < 6; ++i) {
        *(data_writer + 5 - i) = (char) (sum % 256);
        sum /= 256;
    }
    data_writer += 6;

    sum = 1234;
    for (i = 0; i + 4 < data_writer - login_data; i += 4) {
        uint64_t check = 0;
        for (j = 0; j < 4; j++) {
            check = check * 256 + login_data[i + 3 - j];
        }
        sum ^= check;
    }
    sum = (1968 * sum) & 0xFFFFFFFF;
    for (i = 0; i < 4; i++) {
        *(check_point++) = (unsigned char) (sum >> (i * 8) & 0x000000FF);
    }

    data_writer = check_point;

    append(&data_writer, "\x00\x00", 2);

    sum = MAC_ADDR;
    for (i = 0; i < 6; ++i) {
        *(data_writer + 5 - i) = (char) (sum % 256);
        sum /= 256;
    }
    data_writer += 6;

    append(&data_writer, "\x00\x00\xe9\x13", 4);

    return (int) (data_writer - login_data);
}

static void login() {
    str login_data = {0};

    int login_data_len = set_login_data(login_data);

    int len = s_send(login_data);

    printf("login data sent:");
    println_hex(login_data, login_data_len);

    if (len < 0) {
        error("login_send");
        longjmp(exit_point, 1);
    }
    str recv_data = {0};
    len = s_recv(recv_data);
    if (len < 0) {
        error("login_recv");
        longjmp(exit_point, 1);
    }

    if (recv_data[0] == '\x04') {
        printf("logged in, TAIL:");
        memcpy(TAIL, recv_data + 23, TAIL_LEN);
        println_hex(TAIL, TAIL_LEN);
    } else {
        printf("login failed, ");
        printf("packet received:");
        println_hex(recv_data, BUF_SIZE);
        longjmp(exit_point, 1);
    }
}

static void empty_socket_buffer() {
    str recv_data = {0};

    s_recv(recv_data);
}

static void challenge() {
    str challenge_data = {0}, buf = {0};
    int token = (int) (time(NULL) + rand() % 0xFF + 0xF);
    token %= 0xFFFF;

    /* Set challenge data */

    char param1, param2;
    if (IN_HUXI) {
        param1 = CHALLENGE_PARAM1_HUXI;
        param2 = CHALLENGE_PARAM2_HUXI;
    } else {
        param1 = CHALLENGE_PARAM1_A;
        param2 = CHALLENGE_PARAM2_A;
    }

    char *data_writer = challenge_data;

    *(data_writer++) = 0x01;
    *(data_writer++) = param1;
    *(data_writer++) = (char) token;
    *(data_writer++) = (char) (token >> 8);
    *(data_writer++) = param2;
    data_writer += 15;

    int challenge_data_len = (int) (data_writer - challenge_data);

    /* Communicate with server */

    int len = s_send(challenge_data);
    printf("challenge sent:");
    println_hex(challenge_data, challenge_data_len);
    if (len < 0) {
        error("challenge_send");
        longjmp(exit_point, 1);
    }
    len = s_recv(buf);
    if (len < 0) {
        error("challenge_recv");
        longjmp(exit_point, 1);
    }

    /* Return the 4th to 8th bits as SALT */

    if (buf[0] == '\x02') {
        printf("challenge succeed, SALT:");
        memcpy(SALT, buf + 4, SALT_LEN);
        println_hex(SALT, SALT_LEN);
    } else {
        fprintf(stderr, "challenge failed!\n");
        printf("Recv packet:");
        println_hex(buf, BUF_SIZE);
        longjmp(exit_point, 1);
    }
}

static int init_socket() {
#ifdef _WIN32

    WSADATA wsa_data;
    WORD sock_version = MAKEWORD(2, 2);
    if (WSAStartup(sock_version, &wsa_data) != 0) {
        error("WSAStartup");
        return 1;
    }

#endif
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        error("socket");
        return 1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER);
    server_address.sin_port = htons(PORT);

    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(PORT);

    int timeout_sec = 3;
#ifdef _WIN32
    DWORD timeout = timeout_sec * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, sizeof(timeout));
#else
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
    int reuse_addr = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse_addr, sizeof(int));

    if (bind(sock, (struct sockaddr *) &local_address, sizeof(local_address)) <
        0) {
        error("bind");
        return 1;
    }
    return 0;
}

static void close_socket() {
#ifdef _WIN32

    closesocket(sock);
    WSACleanup();

#else

    shutdown(sock, 2);

#endif
}

int start_impl(const char *username, const char *password, int empty_buffer, int huxi) {
    /* Initializing variables */

    srand(time(NULL));
    memset(SALT, 0, BUF_SIZE);
    memset(TAIL, 0, BUF_SIZE);
    strcpy(USERNAME, username);
    strcpy(PASSWORD, password);
    IN_HUXI = huxi;
    if (huxi) {
        SERVER = SERVER_HUXI;
        HOST_IP = HOST_IP_HUXI;
        DHCP_SERVER = DHCP_SERVER_HUXI;
    } else {
        SERVER = SERVER_A;
        HOST_IP = HOST_IP_A;
        DHCP_SERVER = DHCP_SERVER_A;
    }

    if (init_socket()) return 1;

    if (setjmp(exit_point) == 0) {
        challenge();
        login();
        if (empty_buffer) {
            empty_socket_buffer();
        }
        keep_alive1();
        keep_alive2();
    } else {
        return 1;
    }

    close_socket();
}

DR_CALL_CONV int start(const char *username, const char *password, int huxi) {
    if (start_impl(username, password, 0, huxi)) {
        return start_impl(username, password, 1, huxi);
    } else {
        return 0;
    }
}
