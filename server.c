#include <stdio.h>
#include <sys/types.h>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netdb.h>
    #include <unistd.h>
#endif

#define USING_RESPONSE_BUFFER(x)    char x[PAYLOAD_BUFFER]
#define RECV(x, y)                  recv(x, y, sizeof(y) - 1, 0)
#define RECV_TO(to, x, y)           int to = recv(x, y, sizeof(y) - 1, 0);y[to] = '\0'
#define SEND(x, y)                  send(x, y, strlen(y), 0)
#define SAME_STR(x, y)              !strcmp((x), (y))
#ifdef _WIN32
#define CLOSE(fd) closesocket(fd);
#else
#define CLOSE(fd) close(fd);
#endif

#define DEBUG
#ifdef DEBUG
#define LOG(msg)                    printf("%s", msg)
#else
#define LOG(mas)
#endif

#define SUCCESS                     0
#define WHAT_ERR                    2
#define SOCK_ERR                    3
#define BIND_ERR                    4
#define LSTN_ERR                    5
#define ACPT_ERR                    6
#define WIN_ERR                     100

#define NAME_LEN                    256
#define PAYLOAD_BUFFER              2048

#define INITIAL_BOARD               "R......B................................................B......R"

typedef struct addrinfo sAddrinfo;
typedef char bool;


typedef struct {
    char username[2][NAME_LEN];
    int score[2];
} Scores;

typedef struct {
    //common
    char type[32];

    //reqs
    char username[NAME_LEN];
    int sx, sy, tx, ty;
    char players[2][NAME_LEN];
    char first_player[NAME_LEN];
    char board[8][8];
    float timeout;
    Scores scores;

    //resps
    char reason[32];
    char next_player[NAME_LEN];

} Request;

typedef struct {
    char username[NAME_LEN];
    int clientfd;
} Client;

char* stringfy(const Request* obj);
void parsing(const char* json, Request* out);

bool setNetwork(int* sockfd);
void verifyRegistration(int sockfd, Client* pclients);

int main() {
    #ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            return WIN_ERR;
        }
    #endif

    int socket;
    bool res;
    USING_RESPONSE_BUFFER(buffer);
    Client clients[2] = { {.clientfd = -1}, {.clientfd = -1} }; // init as invalid fd


    if (res = setNetwork(&socket)) {
        return res;
    } 
    
    verifyRegistration(socket, clients);

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
        int client = accept(sockfd, NULL, NULL);
        if (client == -1) {
            continue;
        }

        printf("asda\n");

        Request req = { .type = "", .username = "" };
        RECV_TO(len, client, buffer);
        
        parsing(buffer, &req);

        if (indices == 1 && SAME_STR(req.username, pclients[0].username) || indices == 2 && SAME_STR(req.username, pclients[1].username)) {
            CLOSE(client);
            LOG("existing username\n");
            continue;
        } else if (SAME_STR(req.type, "register") && !SAME_STR("", req.username)) {
            // register
            const Request res = { .type = "register_ack" };

            if (SEND(client, stringfy(&res)) < 0) {
                // err
                CLOSE(client);
                continue;
            }

            LOG("user registered\n");

            int newIdx = indices % 2 == 0 ? 0 : 1;
            pclients[newIdx].clientfd = client;
            strcpy(pclients[newIdx].username, req.username);

            indices += 1 << newIdx;
        }

        if (indices == 3) {
            // game-start
            Request req = { .type = "game_start" };

            strcpy(req.players[0], pclients[0].username);
            strcpy(req.players[1], pclients[1].username);
            strcpy(req.first_player, pclients[0].username);

            int i;
            bool unconnected = FALSE;
            for (i = 0; i < 2; i++) {
                if (SEND(pclients[i].clientfd, stringfy(&req)) < 0) {
                    // reset when unconnected
                    CLOSE(pclients[i].clientfd);

                    pclients[i].clientfd = -1;
                    strcpy(pclients[i].username, "");

                    unconnected = TRUE;
                    indices -= 1 << i;

                    LOG("unconnected\n");
                }
            }
            if (unconnected) continue;
            LOG("game start\n");
            break; // succeed
        }
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
    #ifdef _WIN32
        setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
    #else
        setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    #endif

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

char* stringfy(const Request* obj) {
    static char buffer[PAYLOAD_BUFFER];
    char boardString[274] = "[";
    char scoresString[NAME_LEN] = "";

    // board
    int i, j;
    for (i = 0; i < 8; i++) {
        strcat(boardString, "[");
        for (j = 0; j < 8; j++) {
            sprintf(boardString, "\"%c\"%c", obj->board[i][j], (j < 7 ? "," : ""));
        }
        strcat(boardString, "]");
        if (i < 7)
            strcat(boardString, ",");
    }
    strcat(boardString, "]");

    // scores
    sprintf(scoresString, "{\"%s\":%d,\"%s\":%d}", obj->scores.username[0], obj->scores.score[0], obj->scores.username[1], obj->scores.score[1]);

    sprintf(buffer, 
        "{\"type\":\"%s\",\"username\":\"%s\",\"sx\":%d,\"sy\":%d,\"tx\":%d,\"ty\":%d,\"players\":[\"%s\",\"%s\"],\"first_player\":\"%s\",\"board\":%s,\"timeout\":%.1f,\"scores\":{%s},\"next_player\":\"%s\",\"reason\":\"%s\"}\n", // newline needed
        obj->type,
        obj->username,
        obj->sx, obj->sy, obj->tx, obj->ty,
        obj->players[0], obj->players[1],
        obj->first_player,
        boardString,
        obj->timeout,
        scoresString,
        obj->next_player,
        obj->reason
    );

    return buffer;
}

void parsing(const char* json, Request* out) {
    char boardFormat[256] = "[";
    char format[1024] = "{\"type\":\"%%31[^\"]\",\"username\":\"%%255[^\"]\",\"sx\":%%d,\"sy\":%%d,\"tx\":%%d,\"ty\":%%d,\"players\":[\"%%255[^\"]\",\"%%255[^\"]\"],\"first_player\":\"%%255[^\"]\",\"board\":";

    int i, j;
    for (i = 0; i < 8; i++) {
        strcat(boardFormat, "[");
        for (j = 0; j < 8; j++) {
            sprintf(boardFormat, "\"%%1[^\"]\"%c", (j < 7 ? "," : ""));
        }
        strcat(boardFormat, "]");
        if (i < 7)
            strcat(boardFormat, ",");
    }
    strcat(boardFormat, "]");

    strcat(format, boardFormat);
    strcat(format, ",\"timeout\":%%f,\"scores\":{\"%%255[^\"]\":%%d,\"%%255[^\"]\":%%d},\"next_player\":\"%%255[^\"]\",\"reason\":\"%%31[^\"]\"}");

    sscanf(json, format,
        out->type,
        out->username,
        &out->sx, &out->sy, &out->tx, &out->ty,
        out->players[0], out->players[1],
        out->first_player,
        out->board[0][0], out->board[0][1], out->board[0][2], out->board[0][3],
        out->board[0][4], out->board[0][5], out->board[0][6], out->board[0][7],
        out->board[1][0], out->board[1][1], out->board[1][2], out->board[1][3],
        out->board[1][4], out->board[1][5], out->board[1][6], out->board[1][7],
        out->board[2][0], out->board[2][1], out->board[2][2], out->board[2][3],
        out->board[2][4], out->board[2][5], out->board[2][6], out->board[2][7],
        out->board[3][0], out->board[3][1], out->board[3][2], out->board[3][3],
        out->board[3][4], out->board[3][5], out->board[3][6], out->board[3][7],
        out->board[4][0], out->board[4][1], out->board[4][2], out->board[4][3],
        out->board[4][4], out->board[4][5], out->board[4][6], out->board[4][7],
        out->board[5][0], out->board[5][1], out->board[5][2], out->board[5][3],
        out->board[5][4], out->board[5][5], out->board[5][6], out->board[5][7],
        out->board[6][0], out->board[6][1], out->board[6][2], out->board[6][3],
        out->board[6][4], out->board[6][5], out->board[6][6], out->board[6][7],
        out->board[7][0], out->board[7][1], out->board[7][2], out->board[7][3],
        out->board[7][4], out->board[7][5], out->board[7][6], out->board[7][7],
        &out->timeout,
        out->scores.username[0], &out->scores.score[0],
        out->scores.username[1], &out->scores.score[1],
        out->next_player, out->reason
    );
}