#ifndef CONF_H
#define CONF_H

/* Environment configuration */

static char SERVER_HUXI[] = "10.254.7.4";
static char HOST_IP_HUXI[4] = {10, 253, 176, 22};
static char DHCP_SERVER_HUXI[4] = {10, 253, 7, 7};

static char CHALLENGE_PARAM1_HUXI = 0x02;
static char CHALLENGE_PARAM2_HUXI = 0x09;
static char CHALLENGE_PARAM1_A = 0x03;
static char CHALLENGE_PARAM2_A = 0x25;

static char SERVER_A[] = "202.202.0.180";
static char HOST_IP_A[4] = {172, 24, 12, 1};
static char DHCP_SERVER_A[4] = {202, 202, 2, 50};

static char HOST_NAME[] = "test";
static char HOST_OS[] = "8089D";
static int PORT = 61440;
static uint64_t MAC_ADDR = 0x123456789ABC;

#endif
