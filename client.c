#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <jansson.h>

#pragma region MACRO_TCP
#define USING_RESPONSE_BUFFER(x)            char x[PAYLOAD_BUFFER]
#pragma endregion

#pragma region MACRO_JSON
#define GET_STR(json, field)                json_string_value(json_object_get(json, field))
#define GET_INT(json, field)                json_integer_value(json_object_get(json, field))
#define GET_REAL(json, field)               json_real_value(json_object_get(json, field))
#define GET_ARR(json, field)                GET_OBJ(json, field)
#define GET_OBJ(json, field)                json_object_get(json, field)
#define GET_IDX(json, index)                json_array_get(json, index)

#define SET_STR(json, field, value)         json_object_set_new(json, field, json_string(value))
#define SET_INT(json, field, value)         json_object_set_new(json, field, json_integer(value))
#define SET_REAL(json, field, value)        json_object_set_new(json, field, json_real(value))
#define SET_ARR(json, field, value)         SET_OBJ(json, field, value)
#define SET_OBJ(json, field, value)         json_object_set_new(json, field, value)
#define SET_IDX_STR(json, index, value)     json_array_set_new(json, index, json_string(value))
#define SET_IDX_INT(json, index, value)     json_array_set_new(json, index, json_integer(value))
#define SET_IDX_REAL(json, index, value)    json_array_set_new(json, index, json_real(value))
#define SET_IDX_ARR(json, index, value)     SET_IDX_OBJ(json, index, value)
#define SET_IDX_OBJ(json, index, value)     json_array_set_new(json, index, value)
#define ADD_STR(json, value)                json_array_append_new(json, json_string(value))
#define ADD_INT(json, value)                json_array_append_new(json, json_integer(value))
#define ADD_READ(json, value)               json_array_append_new(json, json_real(value))
#define ADD_ARR(json, value)                ADD_OBJ(json, value)
#define ADD_OBJ(json, value)                json_array_append_new(json, value)

#define TO_STR(value)                       json_string_value(value)
#define TO_INT(value)                       json_integer_value(value)
#define TO_REAL(value)                      json_real_value(value)

#define IS_STR(json)                        json_is_string(json)
#define IS_INT(json)                        json_is_integer(json)
#define IS_REAL(json)                       json_is_real(json)
#define IS_ARR(json)                        json_is_array(json)
#define IS_OBJ(json)                        json_is_object(json)
#define IS_NULL(json)                       json_is_null(json)

#define IS_STR_F(json, field)               (IS_OBJ(json) && IS_STR(GET_OBJ(json, field)))
#define IS_INT_F(json, field)               (IS_OBJ(json) && IS_INT(GET_OBJ(json, field)))
#define IS_REAL_F(json, field)              (IS_OBJ(json) && IS_REAL(GET_OBJ(json, field)))
#define IS_OBJ_F(json, field)               (IS_OBJ(json) && IS_OBJ(GET_OBJ(json, field)))
#define IS_ARR_F(json, field)               (IS_OBJ(json) && IS_ARR(GET_OBJ(json, field)))
#define IS_VALID_ARR(json, index)           (IS_ARR(json) && index < json_array_size(json))
#define LEN_ARR(json)                       json_array_size(json)

#define STRINGFY(json)                      json_dumps(json, JSON_COMPACT)
#pragma endregion

#ifdef DEBUG
#define LOG(format, args...)                    printf(format, ##args)
#else
#define LOG(format, args...)                    
#endif
#define LOGG(format, args...)                   printf(format, ##args) // gloabl logging

#pragma region CONSTS
#define TRUE                        1
#define FALSE                       0

#define NAME_LEN                    256
#define PAYLOAD_BUFFER              2048
#pragma endregion

#pragma region MACRO_UTIL
#define SAME_STR(x, y)                      (x != NULL && y != NULL && !strcmp((x), (y)))
#define IN_RANGE(x, min, max)   ((x) >= min && (x) <= max)
#pragma endregion

#pragma region  TYPEDEF
typedef struct pollfd sPollFD;
typedef struct addrinfo sAddrinfo;
typedef char bool;
typedef char TILE;
typedef struct {
    int sx, sy, tx, ty;
} Move;

typedef enum { WAIT_START, WAIT_TURN, WAIT_MOVE_ACK } STATUS;
#pragma endregion

#pragma region FUNC_DECL

void frees(int num, ...);
void printMap(TILE pmap[8][8]);

bool parse(json_t** json, const char* input);
char* stringfy(json_t** json);
int sendJson(const int dst, json_t* data);
int sendStr(const int dst, char* data);
void consume(int num, ...);
json_t* recvPayload(const int src);

bool executionCheck(int argc, char* argv[], const char** pip, const char** pport, const char** pname);
bool setNetwork(const char* ip, const char* port, int* sockfd);
bool registerPlayer(const int sockfd, const char* username);
bool gameStart(const int sockfd, bool* meFirst, const char* myname);
char* extractType(const json_t* json);
void move_generate(json_t* movereq, const TILE board[8][8], const bool meFirst);
#pragma endregion

int popRequestQueue(sPollFD* fds, int num, int timeout, json_t** out) {
    int i;

    int ready = poll(fds, num, timeout);
    if (ready < 0) {
        return -1;
    } else {
        for (i = 0; i < num; i++) {
            int src = fds[i].fd;
            if (fds[i].revents & POLLIN) {
                *out = recvPayload(src);
                return i;
            }
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));
    const char* ip;
    const char* port;
    const char* username;

    if (!executionCheck(argc, argv, &ip, &port, &username))
        return 1;

    int socket;
    sPollFD server;
    bool err = FALSE;
    STATUS status;
    bool meFirst;

    if (!setNetwork(ip, port, &socket)) {
        LOGG("[netset error]\n");
        return 1;
    } else if (!registerPlayer(socket, username)) {
        LOGG("[register failed]\n");
        close(socket);
        return 1;
    }

    server.fd = socket;
    server.events = POLLIN;

    status = WAIT_START;

    if (status == WAIT_START && gameStart(socket, &meFirst, username)) {
        status = WAIT_TURN;
        while (1) {
            json_t* req;
            TILE board[8][8];
            int queueIdx = popRequestQueue(&server, 1, -1, &req);
            if (queueIdx < 0) {
                err = TRUE;
                break;
            }

            char* type = extractType(req);
            if (!type) {
                LOGG("[network error]\n");
                err = TRUE;
                break;
            } else if (status == WAIT_TURN && SAME_STR(type, "your_turn")) {
                // move
                LOGG("[my turn: %s]\n", username);
                json_t* res = GET_ARR(req, "board");
                int i;
                
                for (i = 0; i < 8; i++) {
                    memcpy(board[i], TO_STR(GET_IDX(res, i)), 8);
                }

                json_t* movereq = json_object();
                SET_STR(movereq, "type", "move");
                SET_STR(movereq, "username", username);
                
                clock_t st = clock();
                move_generate(movereq, board, meFirst);
                LOGG("[move to %d %d]\n", (int)GET_INT(movereq, "tx"), (int)GET_INT(movereq, "ty"));
                clock_t en = clock();

                if ((double)(en - st) / CLOCKS_PER_SEC <= 5.0)
                    sendJson(socket, movereq);
                consume(1, movereq);
                status = WAIT_MOVE_ACK;
            } else if (status == WAIT_MOVE_ACK && SAME_STR(type, "move_ok")) {
                LOG("move ok\n");
                json_t* res = GET_ARR(req, "board");
                int i;
                
                for (i = 0; i < 8; i++) {
                    memcpy(board[i], TO_STR(GET_IDX(res, i)), 8);
                }
                printMap(board);
                status = WAIT_TURN;
            } else if (status == WAIT_MOVE_ACK && SAME_STR(type, "invalid_move")) {
                LOG("invalid move\n");
                json_t* res = GET_ARR(req, "board");
                if (!IS_ARR(res))
                    continue;
                int i;
                
                for (i = 0; i < 8; i++) {
                    memcpy(board[i], TO_STR(GET_IDX(res, i)), 8);
                }
                printMap(board);
                status = WAIT_TURN;
            } else if (status == WAIT_MOVE_ACK && SAME_STR(type, "pass")) {
                LOG("pass\n");
                printMap(board);
                status = WAIT_TURN;
            } else if (SAME_STR(type, "game_over")) {
                json_t* scores = GET_OBJ(req, "scores");
                int my = GET_INT(scores, username);

                LOGG("==========Game Over===========\n");
                printMap(board);
                LOGG("%s's score: %d\n", username, my);

                frees(1, type);
                consume(1, req);
                break;
            }

            frees(1, type);
            consume(1, req);
        }
    } else err = TRUE;
    
    close(socket);
    return err;
}

bool executionCheck(int argc, char* argv[], const char** pip, const char** pport, const char** pname) {
    // argv[0]: program name
    if (argc != 7)
        return FALSE;
    
    int i;
    for (i = 1; i < 7; i++) {
        const char* param = argv[i];
        if (SAME_STR(param, "-ip")) {
            *pip = argv[++i];
        } else if (SAME_STR(param, "-port")) {
            *pport = argv[++i];
        } else if (SAME_STR(param, "-username")) {
            *pname = argv[++i];
        }
    }
    
    return TRUE;
}

bool setNetwork(const char* ip, const char* port, int* sockfd) {
    sAddrinfo hints, *res;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(ip, port, &hints, &res);
    if (status != 0) {
        return FALSE;
    }

    *sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*sockfd == -1) {
        return FALSE;
    }

    status = connect(*sockfd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        return FALSE;
    }

    return TRUE;
    
}


bool registerPlayer(const int sockfd, const char* username) {
    bool ret = FALSE;
    json_t* dto = json_object();
    json_t* req = NULL;
    char* type = NULL;
    SET_STR(dto, "type", "register");
    SET_STR(dto, "username", username);

    int res = sendJson(sockfd, dto);
    if (res > 0) {
        // recv register_ack
        req = recvPayload(sockfd);
        type = extractType(req);
        if (!IS_NULL(req) && type && SAME_STR(type, "register_ack")) {
            ret = TRUE;
            LOGG("[registered]\n");
        }
    }

    // err or register_nack
    consume(2, dto, req);
    frees(1, type);

    return ret;
}
bool gameStart(const int sockfd, bool* meFirst, const char* myname) {
    bool ret = FALSE;
    char *type = NULL, *first_player = NULL;
    json_t* res;
    json_t* players;

    res = recvPayload(sockfd);
    type = extractType(res);

    if (!IS_NULL(res) && IS_STR_F(res, "first_player") && IS_ARR_F(res, "players") && type) {
        const char* fFirst_player = GET_STR(res, "first_player");
        first_player = strdup(fFirst_player);

        players = GET_ARR(res, "players");

        if (first_player && LEN_ARR(players) == 2 && IS_STR(GET_IDX(players, 0)) && IS_STR(GET_IDX(players, 1)) && SAME_STR(type, "game_start") && !SAME_STR(TO_STR(GET_IDX(players, 0)), "") && !SAME_STR(TO_STR(GET_IDX(players, 1)), "") && !SAME_STR(first_player, "")) {
            LOG("[game start]\n");
            if (SAME_STR(TO_STR(GET_IDX(players, 0)), myname))
                *meFirst = TRUE;
            else
                *meFirst = FALSE;

            ret = TRUE;
        }
    }

    frees(2, type, first_player);
    consume(1, res);
    return ret;
}
void move_generate(json_t* movereq, const TILE board[8][8], const bool meFirst) {
    const char myTile = (meFirst ? 'R' : 'B');

    Move *validMoves = malloc(sizeof(Move) * 512);
    if (!validMoves) {
        SET_INT(movereq, "sx", 9);
        SET_INT(movereq, "sy", 9);
        SET_INT(movereq, "tx", 9);
        SET_INT(movereq, "ty", 9);
        return;
    }
    int validCount = 0;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (board[row][col] != myTile) continue;

            int sx = col + 1;
            int sy = row + 1;

            // clone
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int tx = sx + dx;
                    int ty = sy + dy;
                    if (!IN_RANGE(tx, 1, 8) || !IN_RANGE(ty, 1, 8)) continue;
                    if (board[ty-1][tx-1] != '.') continue;
                    validMoves[validCount++] = (Move){sx, sy, tx, ty};
                }
            }
            // jump
            for (int dy = -2; dy <= 2; dy += 2) {
                for (int dx = -2; dx <= 2; dx += 2) {
                    if (dx == 0 && dy == 0) continue;
                    int tx = sx + dx;
                    int ty = sy + dy;
                    if (!IN_RANGE(tx, 1, 8) || !IN_RANGE(ty, 1, 8)) continue;
                    if (board[ty-1][tx-1] != '.') continue;
                    validMoves[validCount++] = (Move){sx, sy, tx, ty};
                }
            }
        }
    }

    // if invalid moves => pass
    if (validCount == 0) {
        SET_INT(movereq, "sx", 0);
        SET_INT(movereq, "sy", 0);
        SET_INT(movereq, "tx", 0);
        SET_INT(movereq, "ty", 0);
        free(validMoves);
        return;
    }

    // select valid move
    int idx = rand() % validCount;
    Move sel = validMoves[idx];
    SET_INT(movereq, "sx", sel.sx);
    SET_INT(movereq, "sy", sel.sy);
    SET_INT(movereq, "tx", sel.tx);
    SET_INT(movereq, "ty", sel.ty);

    frees(1, validMoves);
}


#pragma region FUNC_JSON
bool parse(json_t** json, const char* input) {
    json_error_t error;
    *json = json_loads(input, 0, &error);

    if (!*json) {
        LOG("Parse error: %s (line %d)\n", error.text, error.line);
        return FALSE;
    }

    return TRUE;
}
char* stringfy(json_t** json) {
    char* str = STRINGFY(*json);
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str);
    char* newline = (char*)malloc(len + 2);

    if (!newline) {
        free(str);
        return NULL;
    }

    memcpy(newline, str, len);
    newline[len] = '\n';
    newline[len + 1] = '\0';

    free(str);

    return newline;
}
int sendJson(const int dst, json_t* data) {
    char* payload = stringfy(&data);

    if (!payload) {
        return -1; //err
    }

    ssize_t sent = sendStr(dst, payload);

    frees(1, payload);
    if (sent <= 0) {
        return -1;
    }

    return sent;
}
int sendStr(const int dst, char* data) {
    ssize_t total = strlen(data); 
    ssize_t remain = total;
    ssize_t sent = 0;

    while (remain > 0) {
        ssize_t n = send(dst, data + sent, remain, 0);
        if (n <= 0) { // err
            return -1;
        }
        sent += n;
        remain -= n;
    }

    return sent;
}
void consume(int num, ...) {
    va_list ap;
    int i;

    va_start(ap, num);
    
    for (i = 0; i < num; i++) {
        json_t* json = va_arg(ap, json_t*);

        if (!IS_NULL(json)) {
            json_decref(json);
        }
    }
    va_end(ap);
}

json_t* recvPayload(const int src)
{
    // take only one payload
    USING_RESPONSE_BUFFER(buffer);

    char *nl;
    int n = recv(src, buffer, PAYLOAD_BUFFER, MSG_PEEK);
    if (n <= 0) {
        return json_null();
    }
    
    buffer[n] = '\0';
    
    nl = strchr(buffer, '\n');

    n = recv(src, buffer, (nl - buffer) + 1, 0);
    if (n <= 0) {
        return json_null();
    }

    *nl = '\0';
    json_t *json;
    if (!parse(&json, buffer))
        return json_null();
    
    return json;
}
char* extractType(const json_t* json) {
    if (IS_NULL(json)) {
        return NULL;
    } else if (IS_STR_F(json, "type")) {
        char* type;
        const char* fType = GET_STR(json, "type");
        type = strdup(fType);

        return type;
    }
    return NULL;
}
#pragma endregion


#pragma region FUNC_UTIL
void frees(int num, ...)
{
    va_list ap;
    int i;

    va_start(ap, num);
    
    for (i = 0; i < num; i++) {
        void* ptr = va_arg(ap, void *);

        if (ptr != NULL) {
            free(ptr);
        }
    }
    va_end(ap);
}
void printMap(TILE pmap[8][8]) {
    int i;
    int j;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            putchar(pmap[i][j]);
        }
        printf("\n");
    }
}
#pragma endregion
