#include <stdio.h>
#include <string.h>
#include <time.h>
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
#define ADD_REAL(json, value)               json_array_append_new(json, json_real(value))
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
#define LOG(format, ...)                    printf(format, ##__VA_ARGS__)
#else
#define LOG(format, ...)
#endif
#define LOGG(format, ...)                   printf(format, ##__VA_ARGS__) // gloabl logging

#pragma region CONSTS
#define TRUE                        1
#define FALSE                       0
#define RED_IDX                     0
#define BLUE_IDX                    1

#define NAME_LEN                    256
#define PAYLOAD_BUFFER              2048

#define RED                     'R'
#define BLUE                    'B'
#define EMPTY                   '.'
#define WALL                    '#'
#pragma endregion

#pragma region MACRO_UTIL
#define SAME_STR(x, y)              (x != NULL && y != NULL && !strcmp((x), (y)))
#define IN_RANGE(x, min, max)   ((x) >= min && (x) <= max)

#define CUR_IDX(turn)               (turn % 2)
#define NXT_IDX(turn)               ((turn + 1) % 2)

#define POS(x, y)                   [y - 1][x - 1]

#define INITIAL_BOARD               "R......B\n........\n........\n........\n........\n........\n........\nB......R\n"
#pragma endregion

#pragma region  TYPEDEF
typedef struct addrinfo sAddrinfo;
typedef char bool;
typedef char TILE;

typedef enum { CONNECTED, DISCONNECTED } STATUS;
typedef struct {
    char username[NAME_LEN];
    int* pfd;
    STATUS status;
    clock_t timer;
} Client;

typedef struct pollfd sPollFD;
#pragma endregion

#pragma region FUNC_DECL
void frees(int num, ...);
void initBoard(TILE board[8][8]);
void printBoard(const TILE pboard[8][8]);
bool isFullBoard(const TILE board[8][8]);
void getUnitCnt(const TILE board[8][8], int* pred, int* pblue);

bool parse(json_t** json, const char* input);
char* stringfy(json_t** json);
int sendJson(const int dst, json_t* data);
int sendStr(const int dst, char* data);
void consume(int num, ...);
json_t* recvPayload(const int src);

bool setNetwork(int* sockfd);
void disconnect(int* sockfd, STATUS* status);
bool registerPlayer(json_t* req, int client, Client* pclients, const bool gameProc);
void reject(const int client);
bool gameStart(Client* pclients, bool* gameProc);
void nextTurn(int* curTurn, Client* pclients, const TILE board[8][8]);
void validation(const int curTurn, Client* pclients, TILE board[8][8], json_t* movereq, bool* pass);
json_t* getBoardJson(const TILE board[8][8]);
void flip(const int x, const int y, const TILE cur, TILE board[8][8]);
#pragma endregion


int main() {
    int socket;
    Client clients[2] = { { .username = { 0 }, .status = DISCONNECTED }, { .username = { 0 }, .status = DISCONNECTED } };
    sPollFD listener[3];
    

    if (!setNetwork(&socket)) {
        LOGG("[netset error]\n");
        return 1;
    }

    listener[0].fd = socket;
    listener[0].events = POLLIN;
    listener[1].fd = -1;
    listener[1].events = POLLIN;
    listener[2].fd = -1;
    listener[2].events = POLLIN;

    // fd binding
    clients[0].pfd = &listener[1].fd;
    clients[1].pfd = &listener[2].fd;
    LOGG("[server hosted]\n");

    bool gameProc = FALSE;
    bool pass = FALSE;
    TILE board[8][8];
    bool err = FALSE;
    int turn = -1;

    while(1) {
        if (gameProc && clients[0].status == DISCONNECTED && clients[1].status == DISCONNECTED) {
            LOGG("[fatal error: both disconnected]\n");
            break;
        }

        int ready = poll(listener, 3, 0);
        if (ready == -1) {
            err = TRUE;
            break;
        }
        
        if (listener[0].revents & POLLIN) { //server
            int client = accept(socket, NULL, NULL);
            if (client == -1) {
                err = TRUE;
                break;
            }

            int i;
            for (i = 0; i < 2; i++) {
                if (listener[i + 1].fd == -1) {
                    listener[i + 1].fd = client;
                    LOG("accept %d\n", client);
                    break;
                }
            }
            if (i < 2) continue;

            reject(client);
        } 
        
        // clients
        int i;
        for (i = 0; i < 2; i++) {
            int client = *clients[i].pfd;

            if (client == -1) {
                continue;
            }
            
            else if (listener[i + 1].revents & POLLIN) {
                bool invalid = FALSE;

                json_t* req = recvPayload(client);
                char* type = NULL;

                if (IS_NULL(req)) {
                    LOGG("[network error: %s]\n", clients[i].username);
                    if (gameProc && CUR_IDX(turn) == i) { // current user turn
                        nextTurn(&turn, clients, board); // but pass
                    }
                    pass = TRUE; // network pass
                    invalid = TRUE;
                } else if (IS_STR_F(req, "type")) {
                    const char* fType = GET_STR(req, "type");
                    type = strdup(fType);

                    if (type == NULL) {
                        LOG("type is null\n");
                        invalid = TRUE;
                    } else {
                        // type branching
                        if (SAME_STR(type, "register")) {
                            if (!registerPlayer(req, client, clients, gameProc)) {
                                invalid = TRUE;
                            } else if (gameStart(clients, &gameProc)) { // game_start
                                // clients check game_start after register_ack.

                                if (turn < 0) {
                                    initBoard(board);
                                    nextTurn(&turn, clients, board); // turn 1 start
                                }
                                
                            }
                        } else if (gameProc && SAME_STR(type, "move")) {
                            bool prevPass = pass;
                            validation(turn, clients, board, req, &pass);
                            if (pass && prevPass) {
                                frees(1, &type);
                                consume(1, &req);
                                goto END_GAME;
                            } else if (gameProc && CUR_IDX(turn) == i) { // current user turn
                                printBoard(board);
                                int red, blue;
                                getUnitCnt(board, &red, &blue);
                                if (isFullBoard(board) || red == 0 || blue == 0) {
                                    frees(1, &type);
                                    consume(1, &req);
                                    goto END_GAME;
                                }
                                nextTurn(&turn, clients, board); // turn must increase unconditionally
                            }
                        }
                    }
                } else invalid = TRUE;

                if (invalid)
                    disconnect(&listener[i + 1].fd, &clients[i].status);
                frees(1, &type);
                consume(1, &req);
            }
        }

        if (gameProc) {
            if ((clock() - clients[CUR_IDX(turn)].timer)  / CLOCKS_PER_SEC > 5) {
                json_t* req = json_object();
                SET_STR(req, "type", "pass");
                SET_STR(req, "next_player", clients[NXT_IDX(turn)].username);

                LOGG("[timeout: %s]\n", clients[CUR_IDX(turn)].username);
                sendJson(*(clients[CUR_IDX(turn)].pfd), req); // disconnection will be delegated
                consume(1, &req);

                bool prevPass = pass;
                pass = TRUE;
                if (pass && prevPass) {
                    break;
                }
                
                nextTurn(&turn, clients, board);
            }
        } 
    }

    END_GAME:
    int red, blue;
    getUnitCnt(board, &red, &blue);

    json_t* req = json_object();
    json_t* scores = json_object();
    SET_STR(req, "type", "game_over");
    SET_INT(scores, clients[0].username, red);
    SET_INT(scores, clients[1].username, blue);
    SET_OBJ(req, "scores", scores);
    LOGG("==========Game Over===========\n");
    printBoard(board);
    LOGG("%s\t\t%d\n", clients[0].username, red);
    LOGG("%s\t\t%d\n", clients[1].username, blue);
    
    int i;
    for (i = 0; i < 2; i++) {
        int client = listener[i + 1].fd;
        if (client != -1) {
            sendJson(client, req);

            close(client);
        }
    }
    consume(1, &req);
    close(socket);
    return err;
}

#pragma region GAME_PROC
bool registerPlayer(json_t* req, int client, Client* pclients, const bool gameProc) {
    // req will be freed outside. pclients will be disconnected outside.

    bool ret = TRUE;
    char* username = NULL;
    int i;

    // valid type check
    if (IS_STR_F(req, "username")) {
        const char* fUsername = GET_STR(req, "username");
        username = strdup(fUsername);
        if (username == NULL) {
            LOG("username is null\n");
            return FALSE;
        } else {
            // existing username check
            bool overlap = FALSE;
            int filled = 100;
            for (i = 0; i < 2; i++) {
                if (*(pclients[i].pfd) != -1 && pclients[i].status == CONNECTED) { // for connected user
                    if (SAME_STR(username, pclients[i].username)) {
                        overlap = TRUE;
                    }
                } else if (pclients[i].status == DISCONNECTED) {
                    if (i < filled)
                        filled = i;
                }
            }

            if (overlap || (gameProc && !SAME_STR(pclients[0].username, "") && !SAME_STR(pclients[1].username, "") && !SAME_STR(pclients[0].username, username) && !SAME_STR(pclients[1].username, username))) {
                LOGG("[user rejected: invalid username]\n");
                json_t* req = json_object();
                SET_STR(req, "type", "register_nack");
                SET_STR(req, "reason", "invalid");

                sendJson(client, req);
                consume(1, &req);

                ret = FALSE;
            } else {
                // register
                json_t* req = json_object();
                SET_STR(req, "type", "register_ack");

                int res = sendJson(client, req);
                consume(1, &req);
                
                if (res <= 0) {
                    // err
                    LOG("unexpected error\n");
                    ret = FALSE;
                } else {
                    LOGG("[registered: %s]\n", username);

                    pclients[filled].status = CONNECTED;
                    strcpy(pclients[filled].username, username);
                }
            }
        }
    }

    frees(1, &username);
    if (!ret)
        LOGG("[register failed]\n");
    return ret;
}
void reject(const int client) {
    LOGG("[user rejected: full]\n");

    json_t* req = json_object();
    SET_STR(req, "type", "register_nack");
    SET_STR(req, "reason", "invalid");

    sendJson(client, req);
    consume(1, &req);

    close(client);
}
bool gameStart(Client* pclients, bool* gameProc) {
    // game-start
    if (pclients[0].status == DISCONNECTED || pclients[1].status == DISCONNECTED)
        return FALSE;

    bool ret = TRUE;
    json_t* res = json_object();
    json_t* players = json_array();
    ADD_STR(players, pclients[0].username);
    ADD_STR(players, pclients[1].username);
    SET_STR(res, "type", "game_start");
    SET_STR(res, "first_player", pclients[0].username);
    SET_ARR(res, "players", players);

    char* resString = stringfy(&res);

    if (!resString) {
        ret = FALSE;
    } else {
        int i;
        for (i = 0; i < 2; i++) {
            int client = *(pclients[i].pfd);
            sendStr(client, resString);
        }
    }

    consume(1, &res);
    frees(1, &resString);

    if (ret) {
        LOGG("[game start]\n");
        *gameProc = TRUE;
    }
   
    return ret;
}
void nextTurn(int* curTurn, Client* pclients, const TILE board[8][8]) {
    json_t* req = json_object();
    json_t* boardArr = getBoardJson(board);
    int i;

    SET_STR(req, "type", "your_turn");
    SET_REAL(req, "timeout", 5.0);
    SET_ARR(req, "board", boardArr);

    (*curTurn)++;
    // ----- depricated -----
    // if (pclients[CUR_IDX(*curTurn)].status == DISCONNECTED){
    //     LOGG("[turn skipped: %s]\n", pclients[CUR_IDX(*curTurn)].username);
    //     (*curTurn)++;
    // }

    sendJson(*(pclients[CUR_IDX(*curTurn)].pfd), req);
    consume(1, &req);
    pclients[CUR_IDX(*curTurn)].timer = clock(); // start timer

    LOGG("[turn: %s]\n", pclients[CUR_IDX(*curTurn)].username);
}
void flip(const int x, const int y, const TILE cur, TILE board[8][8]) {
    int i, j;

    for (i = -1; i <= 1; i++) {
        for (j = -1; j <= 1; j++) {
            const size_t aroundCol = x + j;
            const size_t aroundRow = y + i;
            TILE* sur;

            if (!IN_RANGE(aroundRow, 1, 8) || !IN_RANGE(aroundCol, 1, 8)) { // board boundary
                continue;
            } else if (*(sur = &board POS(aroundCol, aroundRow)) != EMPTY && *sur != WALL && *sur != cur ) { // check surround
                *sur = cur;
            }
        }
    }
}
void validation(const int curTurn, Client* pclients, TILE board[8][8], json_t* movereq, bool* pass) {
    const char* username = NULL;
    int sx, sy, tx, ty;
    const int client = *(pclients[CUR_IDX(curTurn)].pfd);

    bool isValid = TRUE;
    bool notTurn = FALSE;
    if (IS_STR_F(movereq, "username") && IS_INT_F(movereq, "sx") && IS_INT_F(movereq, "sy") && IS_INT_F(movereq, "tx") && IS_INT_F(movereq, "ty")) {
        username = GET_STR(movereq, "username");
        sx = GET_INT(movereq, "sx"); sy = GET_INT(movereq, "sy");
        tx = GET_INT(movereq, "tx"); ty = GET_INT(movereq, "ty");

        LOGG("[user try to move: %d %d -> %d %d]\n", sx, sy, tx, ty);
        if (!SAME_STR(username, pclients[CUR_IDX(curTurn)].username)) {
            LOGG("[validation: turn %s but %s]\n", pclients[CUR_IDX(curTurn)].username, username); 
            notTurn = TRUE;
            isValid = FALSE;
        } else {
            TILE cur = CUR_IDX(curTurn) ? BLUE : RED;
            const int gap[2] = { tx - sx, ty - sy };
            int i;
            
            if (!sx && !sy && !tx && !ty) {
                int i, j;
                for (i = 0; i < 8; i++) {
                    for (j = 0; j < 8; j++) {
                        if (board[i][j] == cur) {
                            int x, y;
                            for (y = -2; y <= 2; y++) {
                                for (x = -2; x <= 2; x++) {
                                    if (!(x == 0 || y == 0 || x == y || x == -y) || !IN_RANGE(i + y, 0, 7) || !IN_RANGE(j + x, 0, 7))
                                        continue;
                                    else if (board[i + y][j + x] == EMPTY){
                                        LOG("jump failed\n");
                                        isValid = FALSE;
                                        goto INVALID; // invalid pass
                                    }
                                }
                            }
                        }
                    }
                }
                *pass = TRUE;
            } else if (!IN_RANGE(sx, 1, 8) || !IN_RANGE(sy, 1, 8) || !IN_RANGE(tx, 1, 8) || !IN_RANGE(ty, 1, 8)) {
                isValid = FALSE;
                goto INVALID; // board out
            } else {
                    if (board POS(sx, sy) != cur || board POS(tx, ty) != EMPTY) {
                    isValid = FALSE;
                    goto INVALID; // invalid tile
                }else if (IN_RANGE(gap[0], -1, 1) && IN_RANGE(gap[1], -1, 1)) {
                    // clone
                    board POS(tx, ty) = cur;
                    flip(tx, ty, cur, board);
                } else if (IN_RANGE(gap[0], -2, 2) && IN_RANGE(gap[1], -2, 2) && (gap[0] == 0 || gap[1] == 0 || gap[1] == gap[0] || gap[1] == -gap[0])) {
                    // jump
                    board POS(sx, sy) = EMPTY;
                    board POS(tx, ty) = cur;
                    flip(tx, ty, cur, board);
                } else {
                    isValid = FALSE;
                    goto INVALID;
                }
            }

            // TODO: validation
            json_t* req = json_object();
            json_t* arr = getBoardJson(board);

            SET_STR(req, "type", "move_ok");
            SET_STR(req, "next_player", pclients[NXT_IDX(curTurn)].username);
            SET_ARR(req, "board", arr);

            sendJson(client, req);
            consume(1, &req);

            *pass = FALSE;

            LOGG("[validation: move ok]\n");
        }
    }

    INVALID:
    if (!isValid) {
        json_t* req = json_object();
        json_t* arr = json_null();
        SET_STR(req, "type", "invalid_move");
        if (notTurn) {
            SET_STR(req, "reason", "not your turn");
        } else {
            SET_STR(req, "next_player", pclients[NXT_IDX(curTurn)].username);
            arr = getBoardJson(board);
    
            SET_ARR(req, "board", arr);

            LOGG("[validation: invalid move]\n");
        }

        sendJson(client, req);
        consume(1, &req);

    }
}
json_t* getBoardJson(const TILE board[8][8]) {
    json_t* arr = json_array();

    int i;
    for (i = 0; i < 8; i++) {
        char row[9];
        memcpy(row, board[i], 8);
        row[8] = '\0';
        ADD_STR(arr, row);
    }

    return arr;
}

bool setNetwork(int* sockfd) {
    sAddrinfo hints, *res;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    status = getaddrinfo(NULL, "5000", &hints, &res);
    if (status != 0) {
        return FALSE;
    }

    *sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*sockfd == -1) {
        return FALSE;
    }

    int reuse = 1;
    setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    status = bind(*sockfd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        return FALSE;
    }

    status = listen(*sockfd, 2); // back log queue = 2 => dealing with sudden connection request for 2 users.
    if (status == -1) {
        return FALSE;
    }

    freeaddrinfo(res);


    return TRUE;
}
void disconnect(int* sockfd, STATUS* status) {
    if (*sockfd != -1) {
        *status = DISCONNECTED;
        close(*sockfd);
        *sockfd = -1;
    }
}
#pragma endregion

#pragma region FUNC_JSON
bool parse(json_t** json, const char* input) {
    json_error_t error;
    *json = json_loads(input, 0, &error);

    if (!*json) {
        LOGG("fail parse %p\n", *json);
        consume(1, &(*json));
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

    frees(1, &payload);
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
        json_t** json = va_arg(ap, json_t**);

         if (*json == NULL || IS_NULL(*json)) {
            continue;
        }

        json_decref(*json);
        *json = NULL;
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
#pragma endregion


#pragma region FUNC_UTIL
void frees(int num, ...)
{
    va_list ap;
    int i;

    va_start(ap, num);
    
    for (i = 0; i < num; i++) {
        void** ptr = va_arg(ap, void**);

        if (*ptr != NULL) {
            free(*ptr);
            *ptr = NULL;
        }
    }
    va_end(ap);
}
void printBoard(const TILE pboard[8][8]) {
    int i;
    int j;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            putchar(pboard[i][j]);
        }
        printf("\n");
    }
}
void initBoard(TILE board[8][8]) {
    int i;
    char* pIBoard = INITIAL_BOARD;
    for (i = 0; i < 8; i++) {
        char row[10];
        sscanf(pIBoard, "%8[^\n]\n", row);
        memcpy(board[i], row, 8);
        pIBoard += 9;
    }
}
bool isFullBoard(const TILE board[8][8]) {
    int i, j;

    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++)
            if (board[i][j] == EMPTY)
                return FALSE;

    return TRUE;
}
void getUnitCnt(const TILE board[8][8], int* pred, int* pblue) {
    int i, j;
    int red = 0, blue = 0;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            if (board[i][j] == RED)
                red++;
            else if (board[i][j] == BLUE)
                blue++;
        }
    }

    *pred = red;
    *pblue = blue;
}
#pragma endregion
