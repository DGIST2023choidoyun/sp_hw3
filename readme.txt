sudo apt install libjansson-dev
gcc client.c -o client -ljansson
gcc server.c -o server -ljansson
