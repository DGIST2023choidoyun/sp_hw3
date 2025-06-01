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
#define LOG(format, ...)                    printf(format, ##__VA_ARGS__)
#else
#define LOG(format, ...)
#endif
#define LOGG(format, ...)                   printf(format, ##__VA_ARGS__) // gloabl logging

#pragma region CONSTS
#define TRUE                        1
#define FALSE                       0

#define NAME_LEN                    256
#define PAYLOAD_BUFFER              2048
#pragma endregion

#pragma region MACRO_UTIL
#define SAME_STR(x, y)                      (x != NULL && y != NULL && !strcmp((x), (y)))
#pragma endregion

#pragma region  TYPEDEF
typedef struct pollfd sPollFD;
typedef struct addrinfo sAddrinfo;
typedef char bool;
typedef char TILE;
#pragma endregion

#pragma region FUNC_DECL

void frees(int num, ...);

bool parse(json_t** json, const char* input);
char* stringfy(json_t** json);
int sendJson(const int dst, json_t* data);
int sendStr(const int dst, char* data);
void consume(int num, ...);
json_t* recvPayload(const int src);

bool executionCheck(int argc, char* argv[], const char** pip, const char** pport, const char** pname);
bool setNetwork(const char* ip, const char* port, int* sockfd);
bool registerPlayer(const int sockfd, const char* username);
bool gameStart(const int sockfd);
json_t* getReqType(const int sockfd, char** ptype);
void move_generate(json_t* movereq, const TILE board[8][8]);
#pragma endregion

int main(int argc, char* argv[]) {
    const char* ip;
    const char* port;
    const char* username;

    if (!executionCheck(argc, argv, &ip, &port, &username))
        return 1;

    int socket;
    bool res;
    sPollFD server;
    bool err = FALSE;

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

    if (gameStart(socket)) {
        while (1) {
            int ready = poll(&server, 1, 0);
            if (ready == -1) { // poll err
                err = TRUE;
                break;
            } else if (server.revents & POLLIN) {
                char* type;
                json_t* req = getReqType(socket, &type);

                if (IS_NULL(req)) {
                    err = TRUE;
                    break;
                } else if (SAME_STR(type, "your_turn")) {
                    // move
                    
                    sleep(2);

                    json_t* movereq = json_object();
                    SET_STR(movereq, "type", "move");
                    SET_STR(movereq, "username", "000");
                    

                    sendJson(socket, movereq);
                    consume(1, movereq);
                } else if (SAME_STR(type, "move_ok")) {
                    LOG("move ok\n");
                } else if (SAME_STR(type, "invalid_move")) {
                    LOG("invalid move\n");
                } else if (SAME_STR(type, "pass")) {
                    LOG("pass\n");
                }

                frees(1, type);
                consume(1, req);
            }
            
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
        req = getReqType(sockfd, &type);
        if (!IS_NULL(req) && SAME_STR(type, "register_ack")) {
            ret = TRUE;
            LOGG("[registered]\n");
        }
    }


    // err or register_nack
    consume(2, dto, req);
    frees(1, type);

    return ret;
}
bool gameStart(const int sockfd) {
    bool ret = FALSE;
    char *type = NULL, *first_player = NULL;
    json_t* res = getReqType(sockfd, &type);
    json_t* players;

    if (!IS_NULL(res) && IS_STR_F(res, "first_player") && IS_ARR_F(res, "players")) {
        const char* fFirst_player = GET_STR(res, "first_player");
        first_player = strdup(fFirst_player);

        players = GET_ARR(res, "players");

        if (LEN_ARR(players) == 2 && IS_STR(GET_IDX(players, 0)) && IS_STR(GET_IDX(players, 1)) && SAME_STR(type, "game_start") && !SAME_STR(TO_STR(GET_IDX(players, 0)), "") && !SAME_STR(TO_STR(GET_IDX(players, 1)), "") && !SAME_STR(first_player, "")) {
            LOG("[game start]\n");
            ret = TRUE;
        }
    }

    frees(2, type, first_player);
    consume(1, res);
    return ret;
}
void move_generate(json_t* movereq, const TILE board[8][8]) {
    srand((unsigned int)time(NULL));

    SET_INT(movereq, "sx", rand() % 9);
    SET_INT(movereq, "sy", rand() & 9);
    SET_INT(movereq, "tx", rand() % 9);
    SET_INT(movereq, "ty", rand() % 9);
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

        if (!json) {
            json_decref(json);
        }
    }
    va_end(ap);
}
json_t* recvPayload(const int src) {
    static USING_RESPONSE_BUFFER(buffer);
    static ssize_t len = 0;

    bool invalid = FALSE;
    char *pnl, *bnl;
    char* payload = NULL;

    do {
        if (len == PAYLOAD_BUFFER - 1) len = 0; // excessive payload
        len += recv(src, buffer + len, sizeof(buffer) - len - 1, 0);
        if (len <= 0) {
            invalid = TRUE;
            break;
        }
        buffer[len] = '\0'; // for separated stream's end
        
        // inefficient but not bad at most case
        ssize_t newLen = len + 1;
        char* payloadBuffer = NULL;

        if (payload) {
            size_t payloadLen = strlen(payload);
            payloadBuffer = (char*)malloc(payloadLen + 1);
            if (!payloadBuffer) { // memory issue
                invalid = TRUE;
                break;
            }
            strcpy(payloadBuffer, payload); // save prev payload

            newLen += (ssize_t)payloadLen;
            free(payload);
            payload = NULL;
        }

        payload = (char*)malloc(newLen);
        payload[0] = '\0';
        if (!payload) { // memory issue;
            invalid = TRUE;
            break;
        }
        if (payloadBuffer) {
            strcpy(payload, payloadBuffer);
            free(payloadBuffer);
        };
        strcat(payload, buffer);

    } while (!(pnl = strchr(payload, '\n')));
    // payload: (short~long payload stream)\n(short payload stream or not)

    if (!invalid) {
        *pnl = '\0';
        bnl = strchr(buffer, '\n'); // have to contain \n since while is broken on new payload has \n.
        *bnl = '\0';

        json_t* json;
        if (parse(&json, payload)) {
            free(payload);

            ssize_t processed = bnl - buffer + 1;
            memmove(buffer, buffer + processed, len - processed);
            len -= processed;
            buffer[len] = '\0';

            return json;
        }  else if (payload) {
            free(payload);
        }
        return json_null();
    } else if (payload) {
        free(payload);
    }
    return json_null();
}
json_t* getReqType(const int sockfd, char** ptype) {
    bool invalid = FALSE;
    json_t* req = recvPayload(sockfd);
    char* type = NULL;
    if (IS_NULL(req)) {
        LOGG("[network error]\n");
        invalid = TRUE;
    } else if (IS_STR_F(req, "type")) {
        const char* fType = GET_STR(req, "type");
        type = strdup(fType);

        if (type == NULL) {
            LOG("type is null\n");
            invalid = TRUE;
        }
    } else invalid = TRUE;

    if (invalid) {
        *ptype = NULL;
        return json_null();
    }
    *ptype = type;
    return req;
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
