#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
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
#define EXE_ERR                     1
#define WHAT_ERR                    2
#define SOCK_ERR                    3
#define CONCT_ERR                   4
#define WIN_ERR                     100
#define PROC_ERR                    5
#define UNDEF_ERR                   6
#define EMPTY_ERR                   7
#define TYPE_ERR                    8

#define NAME_LEN                    256
#define PAYLOAD_BUFFER              2048
#pragma endregion

#pragma region MACRO_UTIL
#define SAME_STR(x, y)                      !strcmp((x), (y))
#pragma endregion

typedef struct addrinfo sAddrinfo;
typedef char bool;

void frees(int num, ...);

bool parse(json_t** json, const char* input);
char* stringfy(json_t** json);

void executionCheck(int argc, char* argv[], const char** pip, const char** pport, const char** pname);
bool setNetwork(const char* ip, const char* port, int* sockfd);
bool registerPlayer(const int sockfd, const char* username);

int main(int argc, char* argv[]) {
    const char* ip;
    const char* port;
    const char* username;

    executionCheck(argc, argv, &ip, &port, &username);

    int socket;
    bool res;

    if (res = setNetwork(ip, port, &socket)) {
        LOG("ERR: SET NETWORK\n");
        return res;
    } else if (!registerPlayer(socket, username)) {
        LOG("ERR: REGISTER\n");
        return PROC_ERR;
    }

    USING_RESPONSE_BUFFER(buffer);

    json_t* req;
    char* type;
    RECV_TO(len, socket, buffer);

    if (!parse(&req, buffer)) {
        close(socket);
        return EMPTY_ERR;
    }

    if (IS_STR_F(req, "type")) {
        const char* fType = GET_STR(req, "type");
        type = strdup(fType);
    } else {
        json_decref(req);
        close(socket);
        return TYPE_ERR;
    }

    FREE_AFTER_GET(req);

    if (type == NULL) {
        close(socket);
        return UNDEF_ERR;
    } else if (SAME_STR(type, "register_ack")) {
        LOG("register ack\n");

        frees(1, type);

        while (1) {
            LOG("fetching...\n");
            json_t* res;
            char *type, *first_player;
            json_t* players;
            RECV_TO(len, socket, buffer);

            if (!parse(&res, buffer)) {
                close(socket);
                return EMPTY_ERR;
            }

            if (IS_STR_F(res, "type") && IS_STR_F(res, "first_player") && IS_ARR_F(res, "players")) {
                const char* fType = GET_STR(res, "type"), *fFirst_player = GET_STR(res, "first_player");
                type = strdup(fType);
                first_player = strdup(fFirst_player);
                players = GET_ARR(res, "players");

                if (LEN_ARR(players) != 2 || !IS_STR(GET_IDX(players, 0)) || !IS_STR(GET_IDX(players, 1))) {
                    frees(2, type, first_player);
                    json_decref(res);
                    close(socket);
                    return UNDEF_ERR;
                }
            } else {
                json_decref(res);
                close(socket);
                return UNDEF_ERR;
            }


            if (type == NULL || players == NULL || first_player == NULL) {
                json_decref(res);
                close(socket);
                return UNDEF_ERR;
            } else if (SAME_STR(type, "game_start") && !SAME_STR(TO_STR(GET_IDX(players, 0)), "") && !SAME_STR(TO_STR(GET_IDX(players, 1)), "") && !SAME_STR(first_player, "")) {
                LOG("server: game start\n");
                json_decref(res);
                frees(2, type, first_player);
                break;
            }

            FREE_AFTER_GET(res);
            frees(2, type, first_player);
        }
    }
    
    close(socket);

    return 0;
}

void executionCheck(int argc, char* argv[], const char** pip, const char** pport, const char** pname) {
    // argv[0]: program name
    if (argc != 7)
        exit(EXE_ERR);
    
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
    
}

bool setNetwork(const char* ip, const char* port, int* sockfd) {
    sAddrinfo hints, *res;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(ip, port, &hints, &res);
    if (status != 0) {
        return WHAT_ERR;
    }

    *sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*sockfd == -1) {
        return SOCK_ERR;
    }

    status = connect(*sockfd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        return CONCT_ERR;
    }

    return SUCCESS;
    
}

bool registerPlayer(const int sockfd, const char* username) {
    json_t* dto = json_object();
    SET_STR(dto, "type", "register");
    SET_STR(dto, "username", username);

    char* message = stringfy(&dto);
    FREE_AFTER_STRINGFY(dto);

    SEND_TO(res, sockfd, message);
    FREE_STR_AFTER_SEND(message);
    if (res <= 0) {
        // err
        return FALSE;
    }

    return TRUE;
}

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
    size_t len = strlen(str);
    char* newline = malloc(len + 2);

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