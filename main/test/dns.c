#include <uv.h>

static uv_getaddrinfo_t req;

static void getaddrinfo_cb(uv_getaddrinfo_t *req, int status,
                           struct addrinfo *res) {
    char ip[INET6_ADDRSTRLEN];
    for (struct addrinfo *cur = res; cur; cur = cur->ai_next) {
        const char *addr = NULL;
        int port = 0;
        if (cur->ai_family == AF_INET || cur->ai_family == AF_INET6) {
            addr =
                (const char *)&((struct sockaddr_in *)cur->ai_addr)->sin_addr;
            port = ((struct sockaddr_in *)cur->ai_addr)->sin_port;
        } else {
            addr =
                (const char *)&((struct sockaddr_in6 *)cur->ai_addr)->sin6_addr;
            port = ((struct sockaddr_in6 *)cur->ai_addr)->sin6_port;
        }
        uv_inet_ntop(cur->ai_family, addr, ip, INET6_ADDRSTRLEN);
        fprintf(stderr, "addr: %s port: %d\n", ip, port);
    }
    uv_freeaddrinfo(res);
}

int main() {
    uv_loop_t loop;
    uv_loop_init(&loop);
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    uv_getaddrinfo(&loop, &req, getaddrinfo_cb, "gitee.com", NULL, &hints);
    uv_run(&loop, UV_RUN_DEFAULT);
    uv_loop_close(&loop);
    return 0;
}
