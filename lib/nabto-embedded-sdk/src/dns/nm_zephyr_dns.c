#include "nm_zephyr_dns.h"

#include <zephyr/kernel.h>
#include <zephyr/net/dns_resolve.h>

#include <platform/np_allocator.h>
#include <platform/interfaces/np_dns.h>
#include <platform/np_completion_event.h>
#include <platform/np_ip_address.h>
#include <platform/np_error_code.h>
#include <platform/np_logging.h>

#define DNS_TIMEOUT (10 * MSEC_PER_SEC)
#define LOG NABTO_LOG_MODULE_DNS

struct dns_request {
    uint16_t dnsId;
    struct np_completion_event* completionEvent;
    struct np_ip_address* ips;
    size_t ipsSize;
    size_t* ipsResolved;
};

static void async_resolve_v4(struct np_dns* obj, const char* host, struct np_ip_address* ips, size_t ipsSize, size_t* ipsResolved, struct np_completion_event* completionEvent);
static void async_resolve_v6(struct np_dns*obj, const char* host, struct np_ip_address* ips, size_t ipsSize, size_t* ipsResolved, struct np_completion_event* completionEvent);
static void async_resolve(enum dns_query_type type, const char* host, struct np_ip_address* ips, size_t ipsSize, size_t* ipsResolved, struct np_completion_event* completionEvent);

static struct np_dns_functions module = {
    .async_resolve_v4 = &async_resolve_v4,
    .async_resolve_v6 = &async_resolve_v6
};

struct np_dns nm_zephyr_dns_get_impl()
{
    struct np_dns dns;
    dns.mptr = &module;
    //dns.data = resolver;
    return dns;
}


void dns_result_cb(enum dns_resolve_status status, struct dns_addrinfo* addr, void* userData)
{
    struct dns_request* req = userData;
    if (status == DNS_EAI_INPROGRESS) {
        if (*req->ipsResolved < req->ipsSize) {
            if (addr->ai_family == AF_INET) {
                struct sockaddr_in* sin = (struct sockaddr_in*)&addr->ai_addr;
                struct np_ip_address* entry = &req->ips[*req->ipsResolved];
                entry->type = NN_IPV4;
                memcpy(entry->ip.v4, sin->sin_addr.s4_addr, 4);
                *req->ipsResolved = *req->ipsResolved + 1;
            } else if (addr->ai_family == AF_INET6) {
                struct sockaddr_in6* sin = (struct sockaddr_in6*)&addr->ai_addr;
                struct np_ip_address* entry = &req->ips[*req->ipsResolved];
                entry->type = NN_IPV6;
                memcpy(entry->ip.v6, sin->sin6_addr.s6_addr, 16);
                *req->ipsResolved = *req->ipsResolved + 1;
            }
        }
    } else if (status == DNS_EAI_ALLDONE || status == DNS_EAI_NODATA) {
        np_completion_event_resolve(req->completionEvent, NABTO_EC_OK);
    } else {
        NABTO_LOG_ERROR(LOG, "Dns resolve failed %d", status);
        np_completion_event_resolve(req->completionEvent, NABTO_EC_UNKNOWN);
    }
}

void async_resolve_v4(struct np_dns* obj, const char* host, struct np_ip_address* ips, size_t ipsSize, size_t* ipsResolved, struct np_completion_event* completionEvent)
{
    async_resolve(DNS_QUERY_TYPE_A, host, ips, ipsSize, ipsResolved, completionEvent);
}

void async_resolve_v6(struct np_dns*obj, const char* host, struct np_ip_address* ips, size_t ipsSize, size_t* ipsResolved, struct np_completion_event* completionEvent)
{
    async_resolve(DNS_QUERY_TYPE_AAAA, host, ips, ipsSize, ipsResolved, completionEvent);
}

void async_resolve(enum dns_query_type type, const char* host, struct np_ip_address* ips, size_t ipsSize, size_t* ipsResolved, struct np_completion_event* completionEvent)
{
    struct dns_request* req = np_calloc(1, sizeof(struct dns_request));
    if (req == NULL) {
        np_completion_event_resolve(completionEvent, NABTO_EC_OUT_OF_MEMORY);
        return;
    }
    req->completionEvent = completionEvent;
    req->ips = ips;
    req->ipsSize = ipsSize;
    *ipsResolved = 0;
    req->ipsResolved = ipsResolved;
    // TODO fix dns when we get there.
    //int ec = -42;
    int ec = dns_get_addr_info(host, type, NULL, dns_result_cb, req, DNS_TIMEOUT);
    if (ec < 0) {
        NABTO_LOG_ERROR(LOG, "Dns resolve failed %d error: %s, hoat: %s, type: %d, req: %d", ec, strerror(-ec), host, type, req);
        np_completion_event_resolve(completionEvent, NABTO_EC_UNKNOWN);
        np_free(req);
    } else {
        // request is started;
    }
}
