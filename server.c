#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <unistd.h>
#include <jansson.h>

#pragma region MACRO_TCP
#define USING_RESPONSE_BUFFER(x)            char x[PAYLOAD_BUFFER]
#define RECV(x, y)                          recv(x, y, sizeof(y) - 1, 0)
#define RECV_TO(to, x, y)                   int to = RECV(x, y);y[to] = '\0'
#define SEND(x, y)                          send(x, y, strlen(y), 0)
#define SEND_TO(to, x, y)                   int to = SEND(x, y)
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

#define IS_STR_F(json, field)               IS_OBJ(json) && IS_STR(GET_OBJ(json, field))
#define IS_INT_F(json, field)               IS_OBJ(json) && IS_INT(GET_OBJ(json, field))
#define IS_REAL_F(json, field)              IS_OBJ(json) && IS_REAL(GET_OBJ(json, field))
#define IS_OBJ_F(json, field)               IS_OBJ(json) && IS_OBJ(GET_OBJ(json, field))
#define IS_ARR_F(json, field)               IS_OBJ(json) && IS_ARR(GET_OBJ(json, field))
#define IS_VALID_ARR(json, index)           IS_ARR(json) && index < json_array_size(json)
#define LEN_ARR(json)                       json_array_size(json)

#define STRINGFY(json)                      json_dumps(json, JSON_COMPACT)

#define FREE_AFTER_STRINGFY(json)           json_decref(json)
#define FREE_AFTER_GET(json)                json_decref(json)
#define FREE_STR_AFTER_SEND(pstr)           free(pstr)
#pragma endregion

#ifdef DEBUG
#define LOG(msg)                            printf("%s", msg)
#else
#define LOG(mas)
#endif

#pragma region CONSTS
#define TRUE                        1
#define FALSE                       0
#define SUCCESS                     0
#define ADDR_ERR                    2
#define SOCK_ERR                    3
#define BIND_ERR                    4
#define LSTN_ERR                    5
#define ACPT_ERR                    6
#define WIN_ERR                     100
#define POLL_ERR                    7

#define NAME_LEN                    256
#define PAYLOAD_BUFFER              2048
#pragma endregion

#pragma region MACRO_UTIL
#define SAME_STR(x, y)              !strcmp((x), (y))

#define INITIAL_BOARD               "R......B\n........\n........\n........\n........\n........\n........\nB......R\n"
#pragma endregion

#pragma region  TYPEDEF
typedef struct addrinfo sAddrinfo;
typedef char bool;
typedef char TILE;

typedef enum { CONNECTED, UNCONNECTED } STATUS;
typedef struct {
    char username[NAME_LEN];
    int* pfd;
    STATUS status;
} Client;

typedef struct pollfd sPollFD;
#pragma endregion

#pragma region FUNC_DECL
void frees(int num, ...);

bool parse(json_t** json, const char* input);
char* stringfy(json_t** json);
int sendJson_Consume(const int dst, json_t* data);
int sendStr(const int dst, char* data);
json_t* recvPayload(const int src);

bool setNetwork(int* sockfd);
void disconnect(int* sockfd, STATUS* status);
bool registerPlayer(const int sockfd, json_t* req, int client, Client* pclients);
#pragma endregion
void gameStart(Client* pclients);

int main() {
    int socket;
    bool res;
    Client clients[2] = { { .username = { 0 }, .status = UNCONNECTED }, { .username = { 0 }, .status = UNCONNECTED } };
    sPollFD listener[3];
    char errCode = SUCCESS;
    TILE board[8][8];

    if (res = setNetwork(&socket)) {
        return res;
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
    LOG("hosted\n");

    bool gameProc = FALSE;

    while(1) {
        int ready = poll(listener, 3, 0); // non blocking
        if (ready == -1) {
            errCode = POLL_ERR;
            break;
        }

        if (listener[0].revents & POLLIN) { //server
            int client = accept(socket, NULL, NULL);
            if (client == -1) {
                errCode = ACPT_ERR;
                break;
            }

            int i;
            for (i = 0; i < 2; i++) {
                if (listener[i + 1].fd == -1) {
                    listener[i + 1].fd = client;
                    LOG("accept\n");
                    break;
                }
            }
            if (i < 2) continue;

            LOG("reject\n");

            // reject with full user
            json_t* req = json_object();
            SET_STR(req, "type", "register_nack");
            SET_STR(req, "reason", "invalid");

            sendJson_Consume(client, req);

            close(client);
        } 
        
        // clients
        int i, neterr = 0;
        for (i = 0; i < 2; i++) {
            int client = *clients[i].pfd;
            if (client == -1) continue;
            else if (listener[i + 1].revents & POLLIN) {
                LOG("client listen\n");

                json_t* req = recvPayload(client);
                char* type = NULL;
                if (IS_NULL(req)) {
                    LOG("network err\n");
                    neterr++;
                    goto INVALID_FREE;
                } else if (IS_STR_F(req, "type")) {
                    const char* fType = GET_STR(req, "type");
                    type = strdup(fType);
                    if (type == NULL) {
                        LOG("empty\n");
                        goto INVALID_FREE;
                    }
                } else goto INVALID_FREE;
                
                // type branching
                if (SAME_STR(type, "register")) {
                    if (!registerPlayer(socket, req, client, clients)) {
                        goto INVALID_FREE;
                    }
                } else if (SAME_STR(type, "move")) {

                }

                frees(1, type);
                FREE_AFTER_GET(req);
                continue;
                INVALID_FREE:
                    frees(1, type);
                    FREE_AFTER_GET(req);
                    disconnect(&listener[i + 1].fd, &clients[i].status);
            }
        }
        if (neterr >= 2) {
            break;
        }

        if (clients[0].status == CONNECTED && clients[1].status == CONNECTED && !gameProc) {
            gameStart(clients);
            LOG("game start\n");
            gameProc = TRUE;

            // init board state
            int i;
            char* pIBoard = INITIAL_BOARD;
            for (i = 0; i < 8; i++) {
                char row[9];
                sscanf(pIBoard, "%8[^\n]\n", row);
                memcpy(board[i], row, 8);
                pIBoard += 9;
            }
        }
    }

    // game_over?
    
    close(socket);
    int i;
    for (i = 0; i < 2; i++) {
        int client = listener[i + 1].fd;
        if (client != -1) {
            close(client);
        }
    }
    return errCode;
}

bool registerPlayer(const int sockfd, json_t* req, int client, Client* pclients) {
    // req will be freed outside. pclients will be disconnected outside.

    char* username = NULL;
    int i;

    if (IS_STR_F(req, "username")) {
        const char* fUsername = GET_STR(req, "username");
        username = strdup(fUsername);
        if (username == NULL) {
            LOG("empty\n");
            return FALSE;
        }
    } else goto ERR;


    bool overlap = FALSE;
    int filled = 0;
    for (i = 0; i < 2; i++) {
        if (*(pclients[i].pfd) != -1 && pclients[i].status == CONNECTED) {
            filled++;
            if (SAME_STR(username, pclients[i].username)) {
                overlap = TRUE;
                break;
            }
        }
    }

    if (overlap || (!SAME_STR(pclients[0].username, "") && !SAME_STR(pclients[1].username, "") && !SAME_STR(pclients[0].username, username) && !SAME_STR(pclients[1].username, username))) {
        LOG("existing username or not in waiting list\n");
        json_t* req = json_object();
        SET_STR(req, "type", "register_nack");
        SET_STR(req, "reason", "invalid");

        sendJson_Consume(client, req);

        goto ERR;
    } else {
        // register
        json_t* req = json_object();
        SET_STR(req, "type", "register_ack");

        #ifdef DEBUG
            sleep(2);
        #endif

        int res = sendJson_Consume(client, req);
        
        if (res <= 0) {
            // err
            LOG("unexpected error\n");
            goto ERR;
        }

        LOG("user registered\n");

        pclients[filled].status = CONNECTED;
        strcpy(pclients[filled].username, username);
    }

    return TRUE;
    ERR:
        frees(1, username);
        return FALSE;
}
void gameStart(Client* pclients) {
    // game-start
    json_t* res = json_object();
    json_t* players = json_array();
    ADD_STR(players, pclients[0].username);
    ADD_STR(players, pclients[1].username);
    SET_STR(res, "type", "game_start");
    SET_STR(res, "first_player", pclients[0].username);
    SET_ARR(res, "players", players);

    char* resString = stringfy(&res);

    FREE_AFTER_STRINGFY(players);
    FREE_AFTER_STRINGFY(res);

    #ifdef DEBUG
        sleep(2);
    #endif

    int i;
    for (i = 0; i < 2; i++) {
        int client = *(pclients[i].pfd);
        sendStr(client, resString);
    }

    FREE_STR_AFTER_SEND(resString);
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
        return ADDR_ERR;
    }

    *sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*sockfd == -1) {
        return SOCK_ERR;
    }

    int reuse = 1;
    setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    status = bind(*sockfd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        return BIND_ERR;
    }

    status = listen(*sockfd, 2); // back log queue = 2 => dealing with sudden connection request for 2 users.
    if (status == -1) {
        return LSTN_ERR;
    }

    return SUCCESS;
}
void disconnect(int* sockfd, STATUS* status) {
    if (*sockfd != -1) {
        *status = UNCONNECTED;
        close(*sockfd);
        *sockfd = -1;
    }
}

#pragma region FUNC_JSON
bool parse(json_t** json, const char* input) {
    json_error_t error;
    *json = json_loads(input, 0, &error);

    if (!*json) {
        printf("Parse error: %s (line %d)\n", error.text, error.line);
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

int sendJson_Consume(const int dst, json_t* data) {
    char* payload = stringfy(&data);
    FREE_AFTER_STRINGFY(data);

    if (!payload) {
        return -1; //err
    }

    ssize_t sent = sendStr(dst, payload);

    if (sent <= 0) {
        FREE_STR_AFTER_SEND(payload);
        return -1;
    }

    FREE_STR_AFTER_SEND(payload);

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
json_t* recvPayload(const int src) {
    static USING_RESPONSE_BUFFER(buffer);
    static ssize_t len = 0;

    char *pnl, *bnl;
    char* payload = NULL;

    do {
        if (len == PAYLOAD_BUFFER - 1) len = 0; // excessive payload
        len += recv(src, buffer + len, sizeof(buffer) - len - 1, 0);
        if (len <= 0) goto ERR_RET;
        buffer[len] = '\0'; // for separated stream's end
        
        // inefficient but not bad at most case
        ssize_t newLen = len + 1;
        char* payloadBuffer = NULL;

        if (payload) {
            size_t payloadLen = strlen(payload);
            payloadBuffer = (char*)malloc(payloadLen + 1);
            if (!payloadBuffer) { // memory issue
                goto ERR_RET;
            }
            strcpy(payloadBuffer, payload); // save prev payload

            newLen += (ssize_t)payloadLen;
            free(payload);
            payload = NULL;
        }

        payload = (char*)malloc(newLen);
        payload[0] = '\0';
        if (!payload) { // memory issue;
            goto ERR_RET;
        }
        if (payloadBuffer) {
            strcpy(payload, payloadBuffer);
            free(payloadBuffer);
        };
        strcat(payload, buffer);

    } while (!(pnl = strchr(payload, '\n')));
    // payload: (short~long payload stream)\n(short payload stream or not)

    *pnl = '\0';
    bnl = strchr(buffer, '\n'); // have to contain \n since while is broken on new payload has \n.
    *bnl = '\0';

    json_t* json;
    if (!parse(&json, payload)) goto ERR_RET;

    free(payload);

    ssize_t processed = bnl - buffer + 1;
    memmove(buffer, buffer + processed, len - processed);
    len -= processed;
    buffer[len] = '\0';

    return json;
    ERR_RET:
        if (payload) {
            free(payload);
        }
        return json_null();
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
#pragma endregion
