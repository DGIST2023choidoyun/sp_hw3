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

        while (1) {
            LOG("fetching...\n");
            RECV_TO(len, socket, buffer);

            parsing(buffer, &req);
            printf("%s %s %s\n", buffer, req.type, req.first_player);

            int i, j;
            for (i = 0; i < 8; i++) {
                for (j = 0; j < 8; j++) {
                    printf("%d ", req.board[i][j]);
                }
            }

            if (SAME_STR(req.type, "game_start") && !SAME_STR(req.players[0], "") && !SAME_STR(req.players[1], "") && !SAME_STR(req.first_player, "")) {
                LOG("server: game start\n");
                break;
            }
        }
    }

     while(1);


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
    char boardString[512] = "[";
    char scoresString[NAME_LEN] = "";

    // board
    int i, j;
    for (i = 0; i < 8; i++) {
        strcat(boardString, "[");
        for (j = 0; j < 8; j++) {
            char temp[5];
            char ch = obj->board[i][j];
            sprintf(temp, "\"%c\"%s", ch == 0 ? '.' : ch, (j < 7 ? "," : ""));
            strcat(boardString, temp);
        }
        strcat(boardString, "]");
        if (i < 7)
            strcat(boardString, ",");
    }
    strcat(boardString, "]");

    // scores
    sprintf(scoresString, "{\"%s\":%d,\"%s\":%d}", obj->scores.username[0], obj->scores.score[0], obj->scores.username[1], obj->scores.score[1]);

    sprintf(buffer, 
        "{\"type\":\"%s\",\"username\":\"%s\",\"sx\":%d,\"sy\":%d,\"tx\":%d,\"ty\":%d,\"players\":[\"%s\",\"%s\"],\"first_player\":\"%s\",\"board\":%s,\"timeout\":%.1f,\"scores\":%s,\"next_player\":\"%s\",\"reason\":\"%s\"}\n", // newline needed
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
    char boardFormat[600] = "[";
    char format[1024] = "{\"type\":\"%31[^\"]\",\"username\":\"%255[^\"]\",\"sx\":%d,\"sy\":%d,\"tx\":%d,\"ty\":%d,\"players\":[\"%255[^\"]\",\"%255[^\"]\"],\"first_player\":\"%255[^\"]\",\"board\":";

    int i, j;
    for (int i = 0; i < 8; i++) {
        strcat(boardFormat, "[");
        for (int j = 0; j < 8; j++) {
            char temp[10];
            sprintf(temp, "\"%%1[^\"]\"%s", (j < 7 ? "," : ""));
            strcat(boardFormat, temp);
        }
        strcat(boardFormat, "]");
        if (i < 7)
            strcat(boardFormat, ",");
    }
    strcat(boardFormat, "]");

    strcat(format, boardFormat);
    strcat(format, ",\"timeout\":%f,\"scores\":{\"%255[^\"]\":%d,\"%255[^\"]\":%d},\"next_player\":\"%255[^\"]\",\"reason\":\"%31[^\"]\"}");

    printf("form:%s\n", format);

    sscanf(json, "{\"type\":\"%31[^\"]\",\"username\":\"%255[^\"]\",\"sx\":%d,\"sy\":%d,\"tx\":%d,\"ty\":%d,\"players\":[\"%255[^\"]\",\"%255[^\"]\"],\"first_player\":\"%255[^\"]\",\"board\":[[\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],[\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],[\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],[\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],[\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],[\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],[\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],[\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"],\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\",\"%1[^\"]\"]],\"timeout\":%f,\"scores\":{\"%255[^\"]\":%d,\"%255[^\"]\":%d},\"next_player\":\"%255[^\"]\",\"reason\":\"%31[^\"]\"}",
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