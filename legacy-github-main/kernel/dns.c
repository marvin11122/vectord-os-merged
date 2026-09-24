/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - DNS Resolver
 * 
 * Basic DNS client for hostname resolution
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* DNS packet structure */
typedef struct {
    u16 id;
    u16 flags;
    u16 qdcount;
    u16 ancount;
    u16 nscount;
    u16 arcount;
} __attribute__((packed)) dns_header_t;

/* DNS query types */
#define DNS_QUERY_A      1
#define DNS_QUERY_CNAME  5

/* DNS response codes */
#define DNS_RCODE_NOERROR   0
#define DNS_RCODE_FORMAT    1
#define DNS_RCODE_SERVER    2
#define DNS_RCODE_NAME      3

/* DNS cache */
#define DNS_CACHE_SIZE 8
static struct {
    char hostname[64];
    ip_addr_t ip;
    u32 ttl;
} dns_cache[DNS_CACHE_SIZE];
static int dns_cache_count = 0;

/* ═══════════════════════════════════════════════════════════════════════
 * DNS CACHE MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════ */

static int dns_cache_find(const char *hostname) {
    for (int i = 0; i < dns_cache_count; i++) {
        if (strcmp(dns_cache[i].hostname, hostname) == 0) {
            return i;
        }
    }
    return -1;
}

static void dns_cache_add(const char *hostname, ip_addr_t ip, u32 ttl) {
    if (dns_cache_count >= DNS_CACHE_SIZE) {
        /* Replace random entry (simple) */
        int idx = random_u32() % DNS_CACHE_SIZE;
        strcpy(dns_cache[idx].hostname, hostname);
        dns_cache[idx].ip = ip;
        dns_cache[idx].ttl = ttl;
    } else {
        strcpy(dns_cache[dns_cache_count].hostname, hostname);
        dns_cache[dns_cache_count].ip = ip;
        dns_cache[dns_cache_count].ttl = ttl;
        dns_cache_count++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * DNS QUERY CONSTRUCTION
 * ═══════════════════════════════════════════════════════════════════════ */

static int dns_build_query(const char *hostname, u8 *buf, u16 buflen) {
    if (buflen < 512) return -1;
    
    dns_header_t *hdr = (dns_header_t *)buf;
    hdr->id = htons(random_u16());
    hdr->flags = htons(0x0100); /* Standard query, recursion desired */
    hdr->qdcount = htons(1);
    hdr->ancount = 0;
    hdr->nscount = 0;
    hdr->arcount = 0;
    
    int pos = sizeof(dns_header_t);
    
    /* Encode hostname as labels */
    const char *p = hostname;
    while (*p) {
        const char *dot = strchr(p, '.');
        int len = dot ? (dot - p) : strlen(p);
        buf[pos++] = len;
        for (int i = 0; i < len; i++) {
            buf[pos++] = p[i];
        }
        p = dot ? dot + 1 : p + len;
        if (!dot) break;
    }
    buf[pos++] = 0; /* Root label */
    
    /* Question type and class */
    buf[pos++] = 0; buf[pos++] = DNS_QUERY_A; /* Type A */
    buf[pos++] = 0; buf[pos++] = 1; /* Class IN */
    
    return pos;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DNS RESPONSE PARSING
 * ═══════════════════════════════════════════════════════════════════════ */

static int dns_parse_response(const u8 *buf, u16 len, ip_addr_t *out_ip) {
    if (len < sizeof(dns_header_t)) return -1;
    
    dns_header_t *hdr = (dns_header_t *)buf;
    u16 flags = ntohs(hdr->flags);
    u16 rcode = flags & 0x000F;
    if (rcode != DNS_RCODE_NOERROR) {
        return -2;
    }
    
    u16 qdcount = ntohs(hdr->qdcount);
    u16 ancount = ntohs(hdr->ancount);
    if (ancount == 0) return -3;
    
    int pos = sizeof(dns_header_t);
    
    /* Skip question section */
    for (int i = 0; i < qdcount; i++) {
        /* Skip name */
        while (buf[pos] != 0) {
            if ((buf[pos] & 0xC0) == 0xC0) {
                pos += 2;
                break;
            }
            pos += buf[pos] + 1;
        }
        pos++; /* zero root */
        pos += 4; /* type and class */
    }
    
    /* Parse answer section */
    for (int i = 0; i < ancount; i++) {
        /* Skip name (could be compressed) */
        if ((buf[pos] & 0xC0) == 0xC0) {
            pos += 2;
        } else {
            while (buf[pos] != 0) {
                pos += buf[pos] + 1;
            }
            pos++;
        }
        
        u16 type = (buf[pos] << 8) | buf[pos + 1];
        pos += 2;
        u16 class = (buf[pos] << 8) | buf[pos + 1];
        pos += 2;
        u32 ttl = (buf[pos] << 24) | (buf[pos+1] << 16) | (buf[pos+2] << 8) | buf[pos+3];
        pos += 4;
        u16 rdlength = (buf[pos] << 8) | buf[pos + 1];
        pos += 2;
        
        if (type == DNS_QUERY_A && class == 1 && rdlength == 4) {
            out_ip->addr[0] = buf[pos];
            out_ip->addr[1] = buf[pos+1];
            out_ip->addr[2] = buf[pos+2];
            out_ip->addr[3] = buf[pos+3];
            return 0;
        }
        pos += rdlength;
    }
    
    return -4;
}

/* ═══════════════════════════════════════════════════════════════════════
 * DNS RESOLVE (public API)
 * ═══════════════════════════════════════════════════════════════════════ */

int dns_resolve(const char *hostname, ip_addr_t *addr) {
    /* Check cache first */
    int cache_idx = dns_cache_find(hostname);
    if (cache_idx >= 0) {
        *addr = dns_cache[cache_idx].ip;
        return 0;
    }
    
    /* Get DNS server from DHCP */
    ip_addr_t dns_server = dhcp_get_dns_server();
    if (ip_addr_equal(dns_server, (ip_addr_t){{0,0,0,0}})) {
        /* Fallback to Google DNS */
        dns_server = (ip_addr_t){{8,8,8,8}};
    }
    
    /* Build DNS query */
    u8 query[512];
    int qlen = dns_build_query(hostname, query, sizeof(query));
    if (qlen < 0) return -5;
    
    /* Send UDP packet to DNS server port 53 */
    u16 src_port = 12345 + (random_u16() % 1000);
    udp_send(dns_server, 53, src_port, query, qlen);
    
    /* Wait for response (simplified) */
    /* We need to create a UDP socket and receive response */
    /* For now, we'll use a simple loop reading from the UDP socket */
    /* This is a placeholder - real implementation would require socket API */
    serial_puts("DNS: Query sent to ");
    ip_addr_print(dns_server);
    serial_puts(" for ");
    serial_puts(hostname);
    serial_puts("\n");
    
    /* TODO: Implement actual receive and parsing */
    return -6; /* Not implemented */
}
