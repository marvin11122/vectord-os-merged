/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - HTTP Client
 * 
 * Simple HTTP GET request implementation
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* HTTP response parser state */
typedef struct {
    char *buf;
    u16 buflen;
    u16 pos;
    int header_done;
    int content_length;
    int status_code;
} http_state_t;

/* ═══════════════════════════════════════════════════════════════════════
 * HTTP REQUEST
 * ═══════════════════════════════════════════════════════════════════════ */

int http_get(const char *hostname, const char *path, char *buf, u16 buflen) {
    /* Resolve hostname to IP */
    ip_addr_t server_ip;
    if (dns_resolve(hostname, &server_ip) != 0) {
        serial_puts("HTTP: DNS resolution failed\n");
        return -1;
    }
    
    /* Create TCP socket */
    int sockfd = socket_create(SOCKET_TCP);
    if (sockfd < 0) {
        serial_puts("HTTP: Failed to create socket\n");
        return -2;
    }
    
    /* Connect to HTTP port 80 */
    if (socket_connect(sockfd, server_ip, 80) != 0) {
        serial_puts("HTTP: Connection failed\n");
        socket_close(sockfd);
        return -3;
    }
    
    /* Build HTTP GET request */
    char request[512];
    int len = snprintf(request, sizeof(request),
                       "GET /%s HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Connection: close\r\n"
                       "User-Agent: VectordOS/1.0\r\n"
                       "Accept: */*\r\n"
                       "\r\n",
                       path, hostname);
    
    /* Send request */
    if (socket_send(sockfd, (u8*)request, len) != len) {
        serial_puts("HTTP: Failed to send request\n");
        socket_close(sockfd);
        return -4;
    }
    
    /* Receive response */
    http_state_t state;
    state.buf = buf;
    state.buflen = buflen;
    state.pos = 0;
    state.header_done = 0;
    state.content_length = 0;
    state.status_code = 0;
    
    while (state.pos < buflen - 1) {
        u16 chunk = buflen - state.pos - 1;
        u16 received = socket_receive(sockfd, (u8*)(buf + state.pos), &chunk);
        if (received <= 0) {
            break;
        }
        state.pos += received;
        buf[state.pos] = '\0';
        
        /* Check if we've received headers */
        if (!state.header_done) {
            char *header_end = strstr(buf, "\r\n\r\n");
            if (header_end) {
                state.header_done = 1;
                /* Parse status line */
                char *line = buf;
                char *space = strchr(line, ' ');
                if (space) {
                    state.status_code = atoi(space + 1);
                }
                /* Look for Content-Length */
                char *cl = strstr(buf, "Content-Length: ");
                if (cl) {
                    state.content_length = atoi(cl + 16);
                }
                /* If we have all headers, we can break if we want only headers */
                /* For now, continue reading body */
            }
        }
        
        /* If we have content length and received enough, break */
        if (state.content_length > 0 && 
            state.pos >= (header_end - buf) + 4 + state.content_length) {
            break;
        }
    }
    
    socket_close(sockfd);
    
    if (state.status_code != 200) {
        serial_puts("HTTP: Status ");
        serial_putdec(state.status_code);
        serial_puts("\n");
    }
    
    return state.pos;
}
