/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Socket API
 * 
 * Userspace socket interface
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* External functions from udp.c and tcp.c */
extern int udp_socket_create(u16 port);
extern int udp_socket_close(int sockfd);
extern int tcp_connect(ip_addr_t dst, u16 dst_port);
extern int tcp_listen(u16 port);
extern int tcp_send(socket_t *sock, const u8 *data, u16 len);
extern int tcp_receive(socket_t *sock, u8 *buf, u16 *len);
extern void tcp_close(socket_t *sock);

/* ═══════════════════════════════════════════════════════════════════════
 * SOCKET CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════ */

#define MAX_SOCKETS  32

/* ═══════════════════════════════════════════════════════════════════════
 * SOCKET TABLE
 * ═══════════════════════════════════════════════════════════════════════ */

static socket_t sockets[MAX_SOCKETS];
static int socket_count = 0;

/* Helper to convert internal index to/from pointer */
#define SOCKET_TO_INDEX(sock) ((int)((sock)->internal))
#define INDEX_TO_SOCKET(idx)  (&sockets[idx])

/* ═══════════════════════════════════════════════════════════════════════
 * SOCKET API IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════ */

int socket_create(socket_type_t type) {
    if (socket_count >= MAX_SOCKETS) {
        return -1;
    }
    
    /* Find free slot */
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].state == SOCK_STATE_CLOSED) {
            sockets[i].type = type;
            sockets[i].state = SOCK_STATE_OPEN;
            sockets[i].internal = NULL;
            sockets[i].local_port = 0;
            sockets[i].remote_port = 0;
            socket_count++;
            return i;
        }
    }
    return -1;
}

int socket_bind(int sockfd, u16 port) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    sockets[sockfd].local_port = port;
    sockets[sockfd].state = SOCK_STATE_OPEN;
    
    /* For UDP, create internal socket */
    if (sockets[sockfd].type == SOCKET_UDP) {
        int udp_idx = udp_socket_create(port);
        if (udp_idx < 0) {
            return -1;
        }
        sockets[sockfd].internal = (void*)(udp_idx + 1); /* Store index+1 to differentiate from NULL */
    }
    /* For TCP, we will bind later when listening/connecting */
    
    return 0;
}

int socket_listen(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type != SOCKET_TCP) {
        return -1;
    }
    
    /* Create listening TCP socket */
    int tcp_idx = tcp_listen(sockets[sockfd].local_port);
    if (tcp_idx < 0) {
        return -1;
    }
    
    sockets[sockfd].internal = (void*)(tcp_idx + 1);
    sockets[sockfd].state = SOCK_STATE_LISTEN;
    return 0;
}

int socket_accept(int sockfd) {
    /* For simplicity, we accept the first pending connection */
    /* This is a placeholder */
    return -1;
}

int socket_connect(int sockfd, ip_addr_t dst, u16 port) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type == SOCKET_TCP) {
        int tcp_idx = tcp_connect(dst, port);
        if (tcp_idx < 0) {
            return -1;
        }
        sockets[sockfd].internal = (void*)(tcp_idx + 1);
        sockets[sockfd].remote_ip = dst;
        sockets[sockfd].remote_port = port;
        sockets[sockfd].state = SOCK_STATE_CONNECTING;
        return 0;
    }
    
    return -1;
}

int socket_send(int sockfd, const u8 *data, u16 len) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type == SOCKET_UDP) {
        /* Use UDP send */
        return udp_send(sockets[sockfd].remote_ip, sockets[sockfd].remote_port,
                        sockets[sockfd].local_port, data, len);
    }
    else if (sockets[sockfd].type == SOCKET_TCP) {
        /* Use TCP send */
        return tcp_send(&sockets[sockfd], data, len);
    }
    
    return -1;
}

int socket_receive(int sockfd, u8 *buf, u16 *len) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    if (sockets[sockfd].type == SOCKET_UDP) {
        return udp_receive(&sockets[sockfd], buf, len);
    }
    else if (sockets[sockfd].type == SOCKET_TCP) {
        return tcp_receive(&sockets[sockfd], buf, len);
    }
    
    return -1;
}

int socket_close(int sockfd) {
    if (sockfd < 0 || sockfd >= MAX_SOCKETS || sockets[sockfd].state == SOCK_STATE_CLOSED) {
        return -1;
    }
    
    /* Close protocol-specific socket */
    if (sockets[sockfd].type == SOCKET_UDP && sockets[sockfd].internal != NULL) {
        int udp_idx = SOCKET_TO_INDEX(&sockets[sockfd]);
        udp_socket_close(udp_idx);
    }
    else if (sockets[sockfd].type == SOCKET_TCP && sockets[sockfd].internal != NULL) {
        tcp_close(&sockets[sockfd]);
    }
    
    sockets[sockfd].state = SOCK_STATE_CLOSED;
    sockets[sockfd].internal = NULL;
    socket_count--;
    return 0;
}