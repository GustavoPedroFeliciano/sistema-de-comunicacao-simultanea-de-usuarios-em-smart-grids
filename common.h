#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

#define MAX_EQ_ID 10

void logexit(const char *msg);

int addrparse(const char *rcvd_ip, const char *rcvd_port, struct sockaddr_storage *storage);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

// int server_sockaddr_init(const char *rcvd_ip_family, const char *rcvd_port,
//                          struct sockaddr_storage *storage);

enum MSG_TYPE {REQ_ADDPEER = 1, REQ_DCPEER, REQ_ADD, REQ_DC, RES_ADD, RES_ADDPEER, REQ_LS, RES_LS, REQ_ES, RES_ES, REQ_LP, RES_LP, REQ_EP, RES_EP, REQ_MS, RES_MS, REQ_MN, RES_MN, ERROR, OK, REQ_ADDC2P, REQ_REMCFP, RES_LIST, REQ_INF, RES_INF};
unsigned parse_msg_type(const char *msg_type_in);