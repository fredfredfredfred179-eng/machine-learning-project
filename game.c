#define EMPTY 0
#define BLACK -1
#define WHITE 1

typedef struct {
    int board[8][8];

    int turn;

    int p1_fd;
    int p2_fd;

    int black_cnt;
    int white_cnt;

} game_t;