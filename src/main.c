#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/keyvalq_struct.h>

#include <gtb-probe.h>

#define tb_init syzygy_init
#include "tbprobe.h"
#undef tb_init

#define square(r, f)            (8 * (r) + (f))
#define rank(s)                 ((s) >> 3)
#define file(s)                 ((s) & 0x07)
#define board(s)                ((uint64_t)1 << (s))

struct pos {
    uint64_t white;
    uint64_t black;
    uint64_t kings;
    uint64_t queens;
    uint64_t rooks;
    uint64_t bishops;
    uint64_t knights;
    uint64_t pawns;
    uint8_t castling;
    uint8_t rule50;
    uint8_t ep;
    bool turn;
    uint16_t move;
};

bool parse_fen(struct pos *pos, const char *fen) {
    uint64_t white = 0, black = 0;
    uint64_t kings, queens, rooks, bishops, knights, pawns;
    kings = queens = rooks = bishops = knights = pawns = 0;
    bool turn;
    unsigned rule50 = 0, move = 1;
    unsigned ep = 0;
    unsigned castling = 0;
    char c;
    int r, f;

    if (fen == NULL)
        goto fen_parse_error;

    for (r = 7; r >= 0; r--)
    {
        for (f = 0; f <= 7; f++)
        {
            unsigned s = (r * 8) + f;
            uint64_t b = board(s);
            c = *fen++;
            switch (c)
            {
                case 'k':
                    kings |= b;
                    black |= b;
                    continue;
                case 'K':
                    kings |= b;
                    white |= b;
                    continue;
                case 'q':
                    queens |= b;
                    black |= b;
                    continue;
                case 'Q':
                    queens |= b;
                    white |= b;
                    continue;
                case 'r':
                    rooks |= b;
                    black |= b;
                    continue;
                case 'R':
                    rooks |= b;
                    white |= b;
                    continue;
                case 'b':
                    bishops |= b;
                    black |= b;
                    continue;
                case 'B':
                    bishops |= b;
                    white |= b;
                    continue;
                case 'n':
                    knights |= b;
                    black |= b;
                    continue;
                case 'N':
                    knights |= b;
                    white |= b;
                    continue;
                case 'p':
                    pawns |= b;
                    black |= b;
                    continue;
                case 'P':
                    pawns |= b;
                    white |= b;
                    continue;
                default:
                    break;
            }
            if (c >= '1' && c <= '8')
            {
                unsigned jmp = (unsigned)c - '0';
                f += jmp-1;
                continue;
            }
            goto fen_parse_error;
        }
        if (r == 0)
            break;
        c = *fen++;
        if (c != '/')
            goto fen_parse_error;
    }
    c = *fen++;
    if (c != ' ')
        goto fen_parse_error;
    c = *fen++;
    if (c != 'w' && c != 'b')
        goto fen_parse_error;
    turn = (c == 'w');
    c = *fen++;
    if (c != ' ')
        goto fen_parse_error;
    c = *fen++;
    if (c != '-')
    {
        do
        {
            switch (c)
            {
                case 'K':
                    castling |= TB_CASTLING_K; break;
                case 'Q':
                    castling |= TB_CASTLING_Q; break;
                case 'k':
                    castling |= TB_CASTLING_k; break;
                case 'q':
                    castling |= TB_CASTLING_q; break;
                default:
                    goto fen_parse_error;
            }
            c = *fen++;
        }
        while (c != ' ');
        fen--;
    }
    c = *fen++;
    if (c != ' ')
        goto fen_parse_error;
    c = *fen++;
    if (c >= 'a' && c <= 'h')
    {
        unsigned file = c - 'a';
        c = *fen++;
        if (c != '3' && c != '6')
            goto fen_parse_error;
        unsigned rank = c - '1';
        ep = square(rank, file);
        if (rank == 2 && turn)
            goto fen_parse_error;
        if (rank == 5 && !turn)
            goto fen_parse_error;
        if (rank == 2 && ((tb_pawn_attacks(ep, true) & (black & pawns)) == 0))
            ep = 0;
        if (rank == 5 && ((tb_pawn_attacks(ep, false) & (white & pawns)) == 0))
            ep = 0;
    }
    else if (c != '-')
        goto fen_parse_error;
    c = *fen++;
    if (c != ' ')
        goto fen_parse_error;
    char clk[4];
    clk[0] = *fen++;
    if (clk[0] < '0' || clk[0] > '9')
        goto fen_parse_error;
    clk[1] = *fen++;
    if (clk[1] != ' ')
    {
        if (clk[1] < '0' || clk[1] > '9')
            goto fen_parse_error;
        clk[2] = *fen++;
        if (clk[2] != ' ')
        {
            if (clk[2] < '0' || clk[2] > '9')
                goto fen_parse_error;
            c = *fen++;
            if (c != ' ')
                goto fen_parse_error;
            clk[3] = '\0';
        }
        else
            clk[2] = '\0';
    }
    else
        clk[1] = '\0';
    rule50 = atoi(clk);
    move = atoi(fen);

    pos->white = white;
    pos->black = black;
    pos->kings = kings;
    pos->queens = queens;
    pos->rooks = rooks;
    pos->bishops = bishops;
    pos->knights = knights;
    pos->pawns = pawns;
    pos->castling = castling;
    pos->rule50 = rule50;
    pos->ep = ep;
    pos->turn = turn;
    pos->move = move;
    return true;

fen_parse_error:
    return false;
}

void get_api(struct evhttp_request *req, void *context) {
    const char *uri = evhttp_request_get_uri(req);
    if (!uri) {
        fputs("evhttp_request_get_uri failed\n", stderr);
        return;
    }

    struct evkeyvalq query;
    const char *fen = NULL;
    if (0 == evhttp_parse_query(uri, &query)) {
        fen = evhttp_find_header(&query, "fen");
    }
    if (!fen || !strlen(fen)) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Missing FEN");
        return;
    }

    struct pos root_pos;
    if (!parse_fen(&root_pos, fen)) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Invalid FEN");
        return;
    }

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
            printf("  Cardinality = %d\n", TB_LARGEST);
            printf("\n");
        }
    }

    return serve(port);
}
