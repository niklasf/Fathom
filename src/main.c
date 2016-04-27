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

#define BOARD_RANK_1            0x00000000000000FFull
#define BOARD_FILE_A            0x8080808080808080ull
#define BOARD_DARK_SQUARES      0xAA55AA55AA55AA55ull
#define square(r, f)            (8 * (r) + (f))
#define rank(s)                 ((s) >> 3)
#define file(s)                 ((s) & 0x07)
#define board(s)                ((uint64_t)1 << (s))

// --verbose flag
static int verbose = 0;

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

#define do_bb_move(b, from, to)                                         \
    (((b) & (~board(to)) & (~board(from))) |                            \
        ((((b) >> (from)) & 0x1) << (to)))
static void do_move(struct pos *pos, unsigned move)
{
    unsigned from     = TB_GET_FROM(move);
    unsigned to       = TB_GET_TO(move);
    unsigned promotes = TB_GET_PROMOTES(move);
    bool turn         = !pos->turn;
    uint64_t white    = do_bb_move(pos->white, from, to);
    uint64_t black    = do_bb_move(pos->black, from, to);
    uint64_t kings    = do_bb_move(pos->kings, from, to);
    uint64_t queens   = do_bb_move(pos->queens, from, to);
    uint64_t rooks    = do_bb_move(pos->rooks, from, to);
    uint64_t bishops  = do_bb_move(pos->bishops, from, to);
    uint64_t knights  = do_bb_move(pos->knights, from, to);
    uint64_t pawns    = do_bb_move(pos->pawns, from, to);
    unsigned ep       = 0;
    unsigned rule50   = pos->rule50;
    if (promotes != TB_PROMOTES_NONE)
    {
        pawns &= ~board(to);
        switch (promotes)
        {
            case TB_PROMOTES_QUEEN:
                queens |= board(to); break;
            case TB_PROMOTES_ROOK:
                rooks |= board(to); break;
            case TB_PROMOTES_BISHOP:
                bishops |= board(to); break;
            case TB_PROMOTES_KNIGHT:
                knights |= board(to); break;
        }
        rule50 = 0;
    }
    else if ((board(from) & pos->pawns) != 0)
    {
        rule50 = 0;
        if (rank(from) == 1 && rank(to) == 3 &&
            (tb_pawn_attacks(from+8, true) & pos->pawns & pos->black) != 0)
            ep = from+8;
        else if (rank(from) == 6 && rank(to) == 4 &&
            (tb_pawn_attacks(from-8, false) & pos->pawns & pos->white) != 0)
            ep = from-8;
        else if (TB_GET_EP(move))
        {
            unsigned ep_to = (pos->turn? to+8: to-8);
            uint64_t ep_mask = ~board(ep_to);
            white &= ep_mask;
            black &= ep_mask;
            pawns &= ep_mask;
        }
    }
    else if ((board(to) & (pos->white | pos->black)) != 0)
        rule50 = 0;
    else
        rule50++;
    pos->white   = white;
    pos->black   = black;
    pos->kings   = kings;
    pos->queens  = queens;
    pos->rooks   = rooks;
    pos->bishops = bishops;
    pos->knights = knights;
    pos->pawns   = pawns;
    pos->ep      = ep;
    pos->rule50  = rule50;
    pos->turn    = turn;
    pos->move += turn;
}

void move_san(const struct pos *pos, unsigned move, char *str) {
    uint64_t occ      = pos->black | pos->white;
    uint64_t us       = (pos->turn? pos->white: pos->black);
    unsigned from     = TB_GET_FROM(move);
    unsigned to       = TB_GET_TO(move);
    unsigned r        = rank(from);
    unsigned f        = file(from);
    unsigned promotes = TB_GET_PROMOTES(move);
    bool     capture  = (occ & board(to)) != 0 || (TB_GET_EP(move) != 0);
    uint64_t b = board(from), att = 0;
    if (b & pos->kings)
        *str++ = 'K';
    else if (b & pos->queens)
    {
        *str++ = 'Q';
        att = tb_queen_attacks(to, occ) & us & pos->queens;
    }
    else if (b & pos->rooks)
    {
        *str++ = 'R';
        att = tb_rook_attacks(to, occ) & us & pos->rooks;
    }
    else if (b & pos->bishops)
    {
        *str++ = 'B';
        att = tb_bishop_attacks(to, occ) & us & pos->bishops;
    }
    else if (b & pos->knights)
    {
        *str++ = 'N';
        att = tb_knight_attacks(to) & us & pos->knights;
    }
    else
    {
        if (capture)
            *str++ = 'a' + f;
    }
    if (tb_pop_count(att) > 1)
    {
        if (tb_pop_count(att & (BOARD_FILE_A >> f)) <= 1)
            *str++ = 'a' + f;
        else if (tb_pop_count(att & (BOARD_RANK_1 >> r)) <= 1)
            *str++ = '1' + r;
        else
        {
            *str++ = 'a' + f;
            *str++ = '1' + r;
        }
    }
    if (capture)
        *str++ = 'x';
    *str++ = 'a' + file(to);
    *str++ = '1' + rank(to);
    if (promotes != TB_PROMOTES_NONE)
    {
        *str++ = '=';
        switch (promotes)
        {
            case TB_PROMOTES_QUEEN:
                *str++ = 'Q'; break;
            case TB_PROMOTES_ROOK:
                *str++ = 'R'; break;
            case TB_PROMOTES_BISHOP:
                *str++ = 'B'; break;
            case TB_PROMOTES_KNIGHT:
                *str++ = 'N'; break;
        }
    }

    struct pos pos_after = *pos;
    do_move(&pos_after, move);

    if (is_mate(&pos_after)) {
        *str++ = '#';
    } else if (is_check(&pos_after)) {
        *str++ = '+';
    }

    *str++ = '\0';
}

void move_uci(unsigned move, char *str) {
    unsigned from = TB_GET_FROM(move);
    *str++ = 'a' + file(from);
    *str++ = '1' + rank(from);

    unsigned to = TB_GET_TO(move);
    *str++ = 'a' + file(to);
    *str++ = '1' + rank(to);

    unsigned promotes = TB_GET_PROMOTES(move);
    switch (promotes) {
        case TB_PROMOTES_QUEEN:
            *str++ = 'q'; break;
        case TB_PROMOTES_ROOK:
            *str++ = 'r'; break;
        case TB_PROMOTES_BISHOP:
            *str++ = 'b'; break;
        case TB_PROMOTES_KNIGHT:
            *str++ = 'n'; break;
    }

    *str++ = '\0';
}

bool is_insufficient_material(const struct pos *pos) {
    // Easy mating material
    if (pos->pawns || pos->rooks || pos->queens) {
        return false;
    }

    // A single knight or a single bishop
    if (tb_pop_count(pos->knights | pos->bishops) == 1) {
        return true;
    }

    // More than a single knight
    if (pos->knights) {
        return false;
    }

    // All bishops on the same color
    if (!(pos->bishops & BOARD_DARK_SQUARES)) {
        return true;
    } else if (!(pos->bishops & ~BOARD_DARK_SQUARES)) {
        return true;
    } else {
        return false;
    }
}

struct move_info {
    unsigned move;
    struct pos pos_after;
    char san[32];
    char uci[6];
    bool insufficient_material;
    bool checkmate;
    bool stalemate;
    int dtz;
    int wdl;
};

void get_api(struct evhttp_request *req, void *context) {
    const char *uri = evhttp_request_get_uri(req);
    if (!uri) {
        fputs("evhttp_request_get_uri failed\n", stderr);
        return;
    }

    struct evkeyvalq query;
    const char *jsonp = NULL;
    const char *fen = NULL;
    if (0 == evhttp_parse_query(uri, &query)) {
        fen = evhttp_find_header(&query, "fen");
        jsonp = evhttp_find_header(&query, "callback");
    }
    if (!fen || !strlen(fen)) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Missing FEN");
        return;
    }

    struct pos pos;
    if (!parse_fen(&pos, fen)) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Invalid FEN");
        return;
    }

    if (!is_valid(&pos)) {
        evhttp_send_error(req, HTTP_BADREQUEST, "Illegal FEN");
        return;
    }

    if (verbose) {
        printf("probing: %s\n", fen);
    }

    // Set Content-Type
    struct evkeyvalq *headers = evhttp_request_get_output_headers(req);
    if (jsonp && strlen(jsonp)) {
        evhttp_add_header(headers, "Content-Type", "application/javascript");
    } else {
        evhttp_add_header(headers, "Content-Type", "application/json");
    }

    unsigned moves[TB_MAX_MOVES];
    unsigned bestmove = tb_probe_root(&pos, moves);
    if (bestmove == TB_RESULT_FAILED) {
        evhttp_send_error(req, HTTP_NOTFOUND, "Position not found in syzygy tablebases");
        return;
    }

    struct move_info move_info[TB_MAX_MOVES + 1];
    unsigned num_moves = 0;

    for (unsigned i = 0; moves[i] != TB_RESULT_FAILED; i++, num_moves++) {
        move_info[i].move = moves[i];

        move_info[i].pos_after = pos;
        do_move(&move_info[i].pos_after, moves[i]);

        move_san(&pos, moves[i], move_info[i].san);
        move_uci(moves[i], move_info[i].uci);

        move_info[i].insufficient_material = is_insufficient_material(&move_info[i].pos_after);

        unsigned dtz = tb_probe_root(&move_info[i].pos_after, NULL);
        if (dtz == TB_RESULT_FAILED) {
            evhttp_send_error(req, HTTP_NOTFOUND, "Child position not found in syzygy tablebases");
            return;
        }

        move_info[i].checkmate = dtz == TB_RESULT_CHECKMATE;
        move_info[i].stalemate = dtz == TB_RESULT_STALEMATE;
        move_info[i].wdl = TB_GET_WDL(dtz) - 2;
        move_info[i].dtz = TB_GET_DTZ(dtz);
    }

    // Build response
    struct evbuffer *res = evbuffer_new();
    if (!res) {
        fputs("could not allocate response buffer\n", stderr);
        abort();
    }

    if (jsonp && strlen(jsonp)) {
        evbuffer_add_printf(res, "%s(", jsonp);
    }
    evbuffer_add_printf(res, "{\n");
    evbuffer_add_printf(res, "  \"checkmate\": %s,\n", (bestmove == TB_RESULT_CHECKMATE) ? "true" : "false");
    evbuffer_add_printf(res, "  \"stalemate\": %s,\n", (bestmove == TB_RESULT_STALEMATE) ? "true" : "false");
    evbuffer_add_printf(res, "  \"insufficient_material\": %s,\n", is_insufficient_material(&pos) ? "true" : "false");
    evbuffer_add_printf(res, "  \"moves\": [\n");

    for (unsigned i = 0; i < num_moves; i++) {
        evbuffer_add_printf(res, "    {\"uci\": \"%s\", \"san\": \"%s\", \"checkmate\": %s, \"stalemate\": %s, \"insufficient_material\": %s, \"dtz\": %d, \"wdl\": %d}", move_info[i].uci, move_info[i].san, move_info[i].checkmate ? "true" : "false", move_info[i].stalemate ? "true" : "false", move_info[i].insufficient_material ? "true" : "false", move_info[i].dtz, move_info[i].wdl);
        if (i + 1 < num_moves) {
            evbuffer_add_printf(res, ",\n");
        } else {
            evbuffer_add_printf(res, "\n");
        }
    }

    // End response
    evbuffer_add_printf(res, "  ]\n");
    evbuffer_add_printf(res, "}");
    if (jsonp && strlen(jsonp)) {
        evbuffer_add_printf(res, ")\n");
    } else {
        evbuffer_add_printf(res, "\n");
    }

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
