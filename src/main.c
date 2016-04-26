#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>

#include <gtb-probe.h>

#define tb_init syzygy_init
#include "tbprobe.h"
#undef tb_init

void get_api(struct evhttp_request *req, void *context) {
    struct evbuffer *res = evbuffer_new();
    if (!res) {
        fputs("could not allocate response buffer\n", stderr);
        abort();
    }

    evbuffer_add_printf(res, "hello world");

    evhttp_send_reply(req, HTTP_OK, "OK", res);

    evbuffer_free(res);
}

int serve(int port) {
    struct event_base *base = event_base_new();
    if (!base) {
        fputs("could not initialize event_base\n", stderr);
        abort();
    }

    struct evhttp *http = evhttp_new(base);
    if (!http) {
        fputs("could not initialize evhttp\n", stderr);
        abort();
    }

    evhttp_set_gencb(http, get_api, NULL);

    struct evhttp_bound_socket *socket =  evhttp_bind_socket_with_handle(http, "127.0.0.1", port);
    if (!socket) {
        fprintf(stderr, "could not bind socket to http://127.0.0.1:%d/\n", port);
        return 1;
    }

    printf("listening on http://127.0.0.1:%d/ ...\n", port);

    return event_base_dispatch(base);
}

int main(int argc, char *argv[]) {
    // Options
    static int port = 5000;
    static int verbose = 0;

    const char **gaviota_paths = tbpaths_init();
    if (!gaviota_paths) {
        fputs("tbpaths_init failed\n", stderr);
        abort();
    }

    const char *syzygy_path = NULL;

    // Parse command line options
    while (1) {
        static struct option long_options[] = {
            {"verbose", no_argument,       &verbose, 1},
            {"port",    required_argument, 0, 'p'},
            {"gaviota", required_argument, 0, 'g'},
            {"syzygy",  required_argument, 0, 's'},
            {0, 0, 0, 0},
        };

        int option_index;
        int c = getopt_long(argc, argv, "p:g:s:", long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                break;

            case 'p':
                port = atoi(optarg);
                if (!port) {
                    fprintf(stderr, "invalid port: %d\n", port);
                    return 78;
                }
                break;

            case 'g':
                gaviota_paths = tbpaths_add(gaviota_paths, optarg);
                if (!gaviota_paths) {
                    fputs("tbpaths_add failed\n", stderr);
                    abort();
                }
                break;

            case 's':
                syzygy_path = strdup(optarg);
                break;

            case '?':
                return 78;

            default:
                abort();
        }
    }

    // Initialize Gaviota tablebases
    char *info = tb_init(verbose, tb_CP4, gaviota_paths);
    if (info) {
        fputs(info, stdout);
    }

    tbcache_init(32 * 1024 * 1024, 10);  // 32 MiB, 10% WDL
    tbstats_reset();

    // Initialize Syzygy tablebases
    if (syzygy_path) {
        syzygy_init(syzygy_path);
        if (verbose) {
            printf("SYZYGY initialization\n");
            printf("  Cardinality: %d\n", TB_LARGEST);
            printf("\n");
        }
    }

    return serve(port);
}
