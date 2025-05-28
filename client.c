#include <stdio.h>
#include <string.h>
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

#define SUCCESS         0
#define EXE_ERR         1
#define WHAT_ERR        2
#define SOCK_ERR        3
#define CONCT_ERR       4
#define WIN_ERR         100
#define PROC_ERR        5

#define NAME_LEN        256
#define PAYLOAD_BUFFER  2048


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

char* stringfy(const Request* obj);
void parsing(const char* json, Request* out);

void executionCheck(int argc, char* argv[], const char** pip, const char** pport, const char** pname);
bool setNetwork(const char* ip, const char* port, int* sockfd);
bool registerPlayer(const int sockfd, const char* username);

int main(int argc, char* argv[]) {
    const char* ip;
    const char* port;
    const char* username;

    executionCheck(argc, argv, &ip, &port, &username);

    #ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            return WIN_ERR;
        }
    #endif

    int socket;
    bool res;

    if (res = setNetwork(ip, port, &socket)) {
        return res;
    } else if (!registerPlayer(socket, username)) {
        return PROC_ERR;
    }

    USING_RESPONSE_BUFFER(buffer);

    Request req = { .type = "", .players = { "", "" }, .first_player = "" };

    RECV_TO(len, socket, buffer);

    parsing(buffer, &req);
    if (SAME_STR(req.type, "register_ack")) {
        LOG("register ack\n");

        RECV_TO(len, socket, buffer);

        parsing(buffer, &req);

        if (SAME_STR(req.type, "game_start") && !SAME_STR(req.players[0], "") && !SAME_STR(req.players[1], "") && !SAME_STR(req.first_player, "")) {
            LOG("server: game start\n");

            while(1);
        }
    }


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
    Request dto = { .type = "register" };
    strcpy(dto.username, username);

    const char* message = stringfy(&dto);

    if (SEND(sockfd, message) < 0) {
        // err 
        return FALSE;
    }

    return TRUE;
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
        out->board[0], out->board[1], out->board[2], out->board[3],
        out->board[4], out->board[5], out->board[6], out->board[7],
        &out->timeout,
        out->scores.username[0], &out->scores.score[0],
        out->scores.username[1], &out->scores.score[1],
        out->next_player, out->reason
    );
}