#pragma once

#define POCKETPY_SOCKET_AF_INET  (2)
#define POCKETPY_SOCKET_AF_INET6 (10)

#define POCKETPY_SOCKET_SOCK_STREAM (1)
#define POCKETPY_SOCKET_SOCK_DGRAM  (2)
#define POCKETPY_SOCKET_SOCK_RAW    (3)

#define POCKETPY_SOCKET_STA_IF (0)
#define POCKETPY_SOCKET_AP_IF  (1)

void pk__add_module_socket(void);
