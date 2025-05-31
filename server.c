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
#define WHAT_ERR                    2
#define SOCK_ERR                    3
#define BIND_ERR                    4
#define LSTN_ERR                    5
#define ACPT_ERR                    6
#define WIN_ERR                     100

#define NAME_LEN                    256
#define PAYLOAD_BUFFER              2048
#pragma endregion

#pragma region MACRO_UTIL
#define SAME_STR(x, y)                      !strcmp((x), (y))

#define INITIAL_BOARD               "R......B................................................B......R"
#pragma endregion

typedef struct addrinfo sAddrinfo;
typedef char bool;

typedef struct {
    char username[NAME_LEN];
    int clientfd;
} Client;

void frees(int num, ...);

bool parse(json_t** json, const char* input);
char* stringfy(json_t** json);

bool setNetwork(int* sockfd);
void verifyRegistration(int sockfd, Client* pclients);

int main() {
    int socket;
    bool res;
    USING_RESPONSE_BUFFER(buffer);
    Client clients[2] = { {.clientfd = -1}, {.clientfd = -1} }; // init as invalid fd


    if (res = setNetwork(&socket)) {
        return res;
    }
    LOG("hosted\n");
    verifyRegistration(socket, clients);

    while(1);
    // while(1) {
    //     int client = accept(socket, NULL, NULL);
    //     if (client == -1) {
    //         continue;
    //     }

    //     int bytes_received = recv(client, buffer, sizeof(buffer) - 1, 0);
    //     buffer[bytes_received] = '\0';
    //     printf("Received message: %s", buffer);

    //     const char *response = "Hello, client!\n";
    //     send(client, response, strlen(response), 0);
    //     printf("Sent response to client\n");

    //     #ifdef _WIN32
    //         closesocket(client);
    //     #else
    //         close(client);
    //     #endif
    //     printf("Closed connection to client\n");
    // }

    
    return 0;
}

void verifyRegistration(int sockfd, Client* pclients) {
    USING_RESPONSE_BUFFER(buffer);

    char indices = 0; // index mask

    while (TRUE) {
        LOG("waiting...\n");
        int client = accept(sockfd, NULL, NULL);
        if (client == -1) {
            continue;
        }

        json_t* req;
        char *type, *username;
        RECV_TO(len, client, buffer);
        
        if(!parse(&req, buffer)) {
            // req == NULL
            close(client);
            continue;
        }

        if (IS_STR_F(req, "type") && IS_STR_F(req, "username")) {
            const char* fType = GET_STR(req, "type");
            const char* fUsername = GET_STR(req, "username");
            type = strdup(fType);
            username = strdup(fUsername);
        } else goto INVALID_FREE;
        

        if (username == NULL || type == NULL) {
            LOG("empty\n");
            goto INVALID_FREE;
        } else if (indices == 1 && SAME_STR(username, pclients[0].username) || indices == 2 && SAME_STR(username, pclients[1].username)) {
            LOG("existing username\n");
            json_t* req = json_object();
            SET_STR(req, "type", "register_nack");
            SET_STR(req, "reason", "invalid");

            char* resString = stringfy(&req);

            FREE_AFTER_STRINGFY(req);

            SEND_TO(res, client, resString);
            FREE_STR_AFTER_SEND(resString);

            goto INVALID_FREE;
        } else if (SAME_STR(type, "register") && !SAME_STR("", username)) {
            // register
            json_t* req = json_object();
            SET_STR(req, "type", "register_ack");
            
            char* resString = stringfy(&req);

            FREE_AFTER_STRINGFY(req);

            #ifdef DEBUG
                sleep(2);
            #endif

            SEND_TO(res, client, resString);
            FREE_STR_AFTER_SEND(resString);
            if (res <= 0) {
                // err
                LOG("unexpected error\n");
                goto INVALID_FREE;
            }

            LOG("user registered\n");

            int newIdx = indices % 2 == 0 ? 0 : 1;
            pclients[newIdx].clientfd = client;
            strcpy(pclients[newIdx].username, username);

            indices += 1 << newIdx;
        }

        if (indices == 3) {
            // game-start
            json_t* res = json_object();
            json_t* players = json_array();
            ADD_STR(players, pclients[0].username);
            ADD_STR(players, pclients[1].username);
            SET_STR(res, "type", "game_start");
            SET_STR(res, "first_player", pclients[0].username);
            SET_ARR(res, "players", players);

            char* resString = stringfy(&res);

            printf("res: %s \n", resString);

            FREE_AFTER_STRINGFY(players);
            FREE_AFTER_STRINGFY(res);

            #ifdef DEBUG
                sleep(2);
            #endif

            int i;
            bool unconnected = FALSE;
            for (i = 0; i < 2; i++) {
                if (SEND(pclients[i].clientfd, resString) <= 0) {
                    // reset when unconnected
                    close(pclients[i].clientfd);

                    pclients[i].clientfd = -1;
                    strcpy(pclients[i].username, "");

                    unconnected = TRUE;
                    indices -= 1 << i;

                    LOG("unconnected\n");
                }
            }

            FREE_STR_AFTER_SEND(resString);

            if (unconnected) goto VALID_FREE;
            LOG("game start\n");
            frees(2, username, type);
            FREE_AFTER_GET(req);
            break; // succeed
        }

        
        VALID_FREE:
            frees(2, username, type);
            FREE_AFTER_GET(req);
        continue;
        INVALID_FREE:
            frees(2, username, type);
            FREE_AFTER_GET(req);
            close(client);
    }
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
        return WHAT_ERR;
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

    status = listen(*sockfd, 1);
    if (status == -1) {
        return LSTN_ERR;
    }

    return SUCCESS;
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