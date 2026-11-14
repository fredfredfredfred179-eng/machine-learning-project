#include	"unp.h"
#include <stdio.h>
// #include <dos.h>
// #include <conio.h>
// #include <io.h>
#include <fcntl.h>
// #include "\lib\odoors41\opendoor.h"
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>  // for tcxxxattr, ECHO, etc ..
#include <unistd.h>    // for STDIN_FILENO
#include <string.h>

#define MARK_O	 1
#define EMPTY	 0
#define MARK_X	-1
 #define TRUE  1 
#define FALSE	 0
#define ENTER	10
#define ESC	27

enum {BIG_O, BIG_X, COLUMN, COMPUTER, PLAYER, GO_FIRST, GO, WINNER, DRAW,
      LEVEL, FILENAME, INPUT, KEY_C, KEY_PLUS, KEY_M, KEY_RET, MESSAGE,
      HINT, ALGO, SAVE, LOAD, EDIT, QUIT, TRY_AGAIN_, Y_N, MARK, MSG_NO};
enum {OK, ERR_NO_CONN, ERR_NO_CMD, ERR_INVALID_CMD, ERR_INVALID_PARA, ERR_WRONG_NUM_PARA, ERR_INVALID_VALUE
} err_codes;      
static char board[8][8];
static int  init_weight[8][8] = {200, -5,40,35,35,40, -5,200,
				  -5,-99, 3, 4, 4, 3,-99, -5,
				  40,  3, 9, 7, 7, 9,  3, 40,
				  35,  4, 7, 7, 7, 7,  4, 35,
				  35,  4, 7, 7, 7, 7,  4, 35,
				  40,  3, 9, 7, 7, 9,  3, 40,
				  -5,-99, 3, 4, 4, 3,-99, -5,
				 200, -5,40,35,35,40, -5,200 };
static int board_weight[8][8];
static int user_got, my_got, think_depth, adjusted;
static int turn, user_turn;
static int row_pos, col_pos, row_pos2, col_pos2;
static int side_mean, inner_mean;
static struct step{
       int row;
       int col;
       struct step *next;
} *step_list, *valid_steps = NULL, *hint_step;

int valid_go(int turn, int row, int col);
int test_step(int turn, int row, int col, int level);
void go(int row, int col);
int check_pass(int test_turn);
int get_ret_code(int sockfd);

/* functions starting with "od_" use ANSI Escape Sequence */
/* refer to https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797 */

void od_set_cursor(int row, int col) {
	printf("\x1B[%d;%dH", row, col);
	fflush(stdout);
}

void od_clr_scr() {
	printf("\x1B[2J");
}

void od_disp_str(const char *str) {
	printf(str);
}

int getch (void)
{
    int ch;
    struct termios oldt, newt;
 
    tcgetattr(STDIN_FILENO, &oldt);
    memcpy(&newt, &oldt, sizeof(newt));
    newt.c_lflag &= ~( ECHO | ICANON | ECHOE | ECHOK |
                       ECHONL | ECHOPRT | ECHOKE | ICRNL);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
 
    return ch;
}

void draw_chess(int row, int col, int patrn)
{
  od_set_cursor(row*2+3, col*4+5);
  switch (patrn) {
    case MARK_O: od_disp_str("O");
		 break;
    case MARK_X: od_disp_str("X");
		 break;
  }
}

void putxy(int row, int col, const char *str)
{
  od_set_cursor(row, col);
  od_disp_str(str);
}

void draw_board()
{
  od_set_cursor(1, 1);
  od_disp_str(" 　 A　 B　 C　 D　 E　 F　 G　 H\n\r");
  od_disp_str("　┌───┬───┬───┬───┬───┬───┬───┬───┐\n\r");
  od_disp_str("1 │   │   │   │   │   │   │   │   │\n\r");
  od_disp_str("　├───┼───┼───┼───┼───┼───┼───┼───┤\n\r");
  od_disp_str("2 │   │   │   │   │   │   │   │   │\n\r");
  od_disp_str("　├───┼───┼───┼───┼───┼───┼───┼───┤\n\r");
  od_disp_str("3 │   │   │   │   │   │   │   │   │\n\r");
  od_disp_str("　├───┼───┼───┼───┼───┼───┼───┼───┤\n\r");
  od_disp_str("4 │   │   │   │   │   │   │   │   │\n\r");
  od_disp_str("　├───┼───┼───┼───┼───┼───┼───┼───┤\n\r");
  od_disp_str("5 │   │   │   │   │   │   │   │   │\n\r");
  od_disp_str("　├───┼───┼───┼───┼───┼───┼───┼───┤\n\r");
  od_disp_str("6 │   │   │   │   │   │   │   │   │\n\r");
  od_disp_str("　├───┼───┼───┼───┼───┼───┼───┼───┤\n\r");
  od_disp_str("7 │   │   │   │   │   │   │   │   │\n\r");
  od_disp_str("　├───┼───┼───┼───┼───┼───┼───┼───┤\n\r");
  od_disp_str("8 │   │   │   │   │   │   │   │   │\n\r");
  od_disp_str("　└───┴───┴───┴───┴───┴───┴───┴───┘\n\r");
}

void get_mean()
{
  int sum;
  int i, j;

  sum = 0;
  for (i = 2; i < 6; i++) {
    sum += board_weight[0][i];
    sum += board_weight[7][i];
    sum += board_weight[i][0];
    sum += board_weight[i][7];
  }
  side_mean = sum / 16;
  sum = 0;
  for (i = 2; i < 6; i++)
    for (j = 2; j < 6; j++)
      sum += board_weight[i][j];
  inner_mean = sum / 16;
}

void init_game()
{
  int i,j;

  for (i = 0; i < 8; i++)
     for (j = 0; j < 8; j++) {
	 board[i][j] = EMPTY;
	 board_weight[i][j] = init_weight[i][j];
     }
  get_mean();
  board[3][3] = MARK_O;
  board[3][4] = MARK_X;
  board[4][3] = MARK_X;
  board[4][4] = MARK_O;
  draw_chess(3,3,MARK_O);
  draw_chess(3,4,MARK_X);
  draw_chess(4,3,MARK_X);
  draw_chess(4,4,MARK_O);
  row_pos = row_pos2 = col_pos = col_pos2 = 0;
  user_turn = MARK_X;
  user_got = my_got = 2;
  think_depth = 3;
}

void draw_cursor(int row, int col)
{
  od_set_cursor(row*2+3, col*4+5);
}

void show_got()
{
  char s[10];

  sprintf(s,"%2i",user_got);
  putxy(5, 52, s);
  sprintf(s,"%2i",my_got);
  putxy(6, 52, s);
}

void show_level()
{
  char s[MAXLINE];

  sprintf(s,"%i",think_depth);
  putxy(3, 50, s);
}

void show_time(int reshow)
{
  static int time = 0;
  char s[5];

//  if (reshow || time != od_control.caller_timelimit) {
//     time = od_control.caller_timelimit;
//     itoa(time, s, 10);
     sprintf(s,"%d",time);
     putxy(8,55,s);
     od_disp_str(" mins ");
//  }
}

void show_first_turn(int first_turn)
{
	char msg_comp[] = " AI ";
	char msg_player[] = "You ";
  	putxy(14,49, (first_turn == COMPUTER) ? msg_comp : msg_player);
}

void show_side(int turn)
{
  putxy(13, 56, (turn == MARK_X) ? "X" : "O");
  putxy(5, 47, (user_turn == MARK_X) ? "X" : "O");
  putxy(6, 47, (user_turn == MARK_O) ? "X" : "O");
}

void show_current_turn()
{
  putxy((turn == user_turn) ? 5 : 6, 39, "->");
  putxy((turn == user_turn) ? 6 : 5, 39, "  ");
}

void show_msg(char *msg, int pause) {
  putxy(16,43, msg);
  if (pause) {
  	while(getch() == 0);
  	putxy(16,43, "                ");  // erase message
  };	
}

int get_par(int sockfd)
{
  int key, done = FALSE, ret_code;
  static int first_turn = COMPUTER;
  char  buf[MAXLINE];
//  ssize_t n;


  putxy(1, 40, "Othello    Ver 1.00 ");
  putxy(3, 41, "Level:");
  putxy(5,41, "You:          Pieces");
  putxy(6,41, "AI :          Pieces");
  putxy(8,39, "* Message:");
  putxy(10,41, "┌──────────────────┐");
  putxy(11,41, "│                  │");
  putxy(12,41, "│ [+,-] Adj. Level │");
  putxy(13,41, "│ [ M ] You're     │");
  putxy(14,41, "│ [ C ]     First  │");
  putxy(15,41, "│ [ S ] Start      │");
  putxy(16,41, "│                  │");
  putxy(17,41, "│ [Q] Quit Game    │");
  putxy(18,41, "└──────────────────┘");
//  putxy(19,4, "程式設計: 嚴力行         版權所有: 新竹【神秘戀人】資訊站");
  show_level();
//  show_time(TRUE);
  show_side(user_turn);
  show_first_turn(first_turn);
  turn = (first_turn == PLAYER) ? user_turn : -user_turn;
  show_current_turn();
  show_got();
  od_set_cursor(16,44);
  while (!done) {
//    od_kernal();
//    while((key = od_get_key(TRUE)) == 0);
    while((key = getch()) == 0);
//    show_time(FALSE);
    switch(key) {
      case '?': /* help */
		break;
      case '+': if (think_depth < 6) {
		    think_depth++;
		    show_level();
		}
		od_set_cursor(12,45);
		break;
      case '-': if (think_depth > 1) {
		    think_depth--;
		    show_level();
		}
		od_set_cursor(12,47);
		break;
      case 'c':
      case 'C':
		first_turn = (first_turn == COMPUTER) ? PLAYER : COMPUTER;
		show_first_turn(first_turn);
		turn = (first_turn == PLAYER) ? user_turn : -user_turn;
		show_current_turn();
		od_set_cursor(14,46);
		break;
      case 'm':
      case 'M':
		user_turn = -user_turn;
		show_side(user_turn);
		od_set_cursor(13,46);
		break;
      case 'q':
	  case 'Q': 
		snprintf(buf, sizeof(buf), "6\n");
		Writen(sockfd, buf, strlen(buf));
		show_msg("Quit", FALSE);
		ret_code = get_ret_code(sockfd);	  
		show_msg("    ", FALSE);
	  	return QUIT;
	  case ENTER:	
      case 'S':
      case 's':	
	  	snprintf(buf, sizeof(buf), "1 %d %d %d %d\n", think_depth, first_turn, user_turn, turn);
//	  	putxy(8,50, buf);  // for debug
		Writen(sockfd, buf, strlen(buf)); 
		show_msg("Done", FALSE);
		if (get_ret_code(sockfd) != 0)
			return QUIT;  
		show_msg("    ", FALSE);			
	  	done = TRUE;
    }
  }
  return 0;
}

void adjust2()
{
  int i;

  if (adjusted) return;
  if (row_pos == 0 || row_pos == 7) {
     if (col_pos > 1 && board[row_pos][col_pos-1] == EMPTY) {
	board_weight[row_pos][col_pos-1] = -inner_mean;
     }
     if (col_pos < 6 && board[row_pos][col_pos+1] == EMPTY) {
	board_weight[row_pos][col_pos+1] = -inner_mean;
     }
     for (i = 1; i < 7; i++)
	if (board[row_pos][i-1] == user_turn && board[row_pos][i+1] == user_turn)
	    board_weight[row_pos][i] = side_mean;
  }
  if (col_pos == 0 || col_pos == 7) {
     if (row_pos > 1 && board[row_pos-1][col_pos] == EMPTY)
	board_weight[row_pos-1][col_pos] = -inner_mean;
     if (row_pos < 6 && board[row_pos+1][col_pos] == EMPTY)
	board_weight[row_pos+1][col_pos] = -inner_mean;
     for (i = 1; i < 7; i++)
	if (board[i-1][col_pos] == user_turn && board[i+1][col_pos] == user_turn)
	    board_weight[i][col_pos] = side_mean;
  }
}

void find_steps()
{
  int i, j;
  struct step *sp;

  for (i = 7; i >= 0; i--)
    for (j = 7; j >= 0; j--)
      if (board[i][j] == EMPTY && valid_go(turn,i,j)) {
	 while (step_list != NULL) {
	   sp = step_list;
	   step_list = step_list->next;
	   free(sp);
	 }
	 hint_step = (struct step *) malloc(sizeof(struct step));
	 hint_step->row = i;
	 hint_step->col = j;
	 hint_step->next = valid_steps;
	 valid_steps = hint_step;
      }
}

void release_steps()
{
  while (valid_steps != NULL) {
    hint_step = valid_steps;
    valid_steps = valid_steps->next;
    free(hint_step);
  }
}

void think_move()
{
  int i, j, max;
  int value;

  max = -32768;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      	if (board[i][j] == EMPTY && valid_go(turn, i,j)) {
	 		value = test_step(turn, i, j, think_depth);
	 		if (value > max) {
	    		max = value;
	    		row_pos = i;
	    		col_pos = j;
	 		}
      	}
}

int get_cmd(int sockfd) {
	size_t n;
	int cmd;
	char line[MAXLINE];
	
    n = Readline(sockfd, line, MAXLINE);
	if (n == 0) {
		show_msg("Err: No Conn", TRUE);
		return -1;
	} 
    if (sscanf(line, "%d", &cmd) != 1) {
        show_msg("Err: Format", TRUE);
        return -1;
	}
	if (cmd < 0 || cmd > 9) { // out of range
		show_msg("Err: Inv Cmd", TRUE);
		return -1;		
	}
	return cmd;		
}

int get_move(int sockfd, int *row_pos, int *col_pos) {
	size_t n;
	int cmd, row, col;
	char line[MAXLINE];
	
    n = Readline(sockfd, line, MAXLINE);
	if (n == 0) {
		show_msg("Err: No Conn", TRUE);
		return ERR_NO_CMD;
	} 
    if (sscanf(line, "%d %d %d", &cmd, &row, &col) != 3) {
        show_msg("Err: Format", TRUE);
        return ERR_INVALID_PARA;
	}
	if (cmd != 2) { // invalid command
		show_msg("Err: Inv Cmd", TRUE);
		return ERR_INVALID_CMD;		
	}
	if (row < 0 || row > 7 || col < 0 || col > 7) {
		show_msg("Err: Inv Val", TRUE);
		return ERR_INVALID_VALUE;
	}
	*row_pos = row;
	*col_pos = col;
	return OK;		
}

void send_ret_code(int sockfd, int ret_code) {
	char buf[MAXLINE];
	
	snprintf(buf, sizeof(buf), "9 %d\n", ret_code);
    Writen(sockfd, buf, strlen(buf));
}

int get_ret_code(int sockfd) {
	size_t n;
	int cmd, ret_code;
	char line[MAXLINE], buf[MAXLINE];
	
    n = Readline(sockfd, line, MAXLINE);
	if (n == 0) {
		show_msg("Err: No Conn", TRUE);
		return -1;
	} 
    if (sscanf(line, "%d %d", &cmd, &ret_code) != 2) {
        show_msg("Err: Format", TRUE);
        return -1;
	}
	if (cmd != 9) { // the first cmd should be ACK
		show_msg("Err: Not Ack", TRUE);
		return -1;		
	}
	if (ret_code == OK) {
//        show_msg("Ack: OK", FALSE);
        return 0;
    }
    else {
        snprintf(buf, sizeof(buf), "Err: Code %d", ret_code);
        show_msg(buf, TRUE);
        return(ret_code);
	}		
}

void send_level(int sockfd, int level) {
	char buf[MAXLINE];
	
	snprintf(buf, sizeof(buf), "5 %d\n", level);
    Writen(sockfd, buf, strlen(buf));
}

int user_choice(int sockfd)
{
  	int key, ret_code;
  	char  buf[MAXLINE];
//	ssize_t n;

  draw_cursor(row_pos,col_pos);
  find_steps();
  do {
//     while ((key = od_get_key(TRUE)) == 0);
     while ((key = getch()) == 0);
     switch(key) {
     	case 'p' :
     	case 'P' :
     		if (check_pass(user_turn)) { // check if it's legal to pass
				snprintf(buf, sizeof(buf), "3\n");
        		Writen(sockfd, buf, strlen(buf));
        		show_msg("Pass     ", FALSE);
				ret_code = get_ret_code(sockfd);						
				return FALSE;			 // just pass this round; not end game
			}
			else {
				show_msg("Can't Pass", TRUE);				
			}
			break;
       case 'l'  :
       case 'L'  :
       		if (check_pass(user_turn)) {
       			show_msg("Must Pass", FALSE);
			}
			else {
		    	if (hint_step == NULL)
					hint_step = valid_steps;
		    	row_pos = hint_step->row;
		    	col_pos = hint_step->col;
		    	draw_cursor(row_pos,col_pos);
		    	row_pos2 = row_pos;
		    	col_pos2 = col_pos;
		    	hint_step = hint_step->next;
			}
		    break;
	   case ENTER:    
//       case 'S':
//	   case 's':  
	   		if (board[row_pos][col_pos] == EMPTY && valid_go(turn, row_pos, col_pos)) {
		       	release_steps();
		       	go(row_pos,col_pos);
               	snprintf(buf, sizeof(buf), "2 %d %d\n", row_pos, col_pos);
           	   	Writen(sockfd, buf, strlen(buf));		       
//		       adjust2();
				ret_code = get_ret_code(sockfd);
		       	return FALSE;
		    }
		 	break;
       case 'Q': 
	   case 'q':
	   		draw_cursor(row_pos,col_pos);
		 	release_steps();
		 	snprintf(buf, sizeof(buf), "6\n");
		 	Writen(sockfd, buf, strlen(buf));
//		 	ret_code = get_ret_code(sockfd);
		 	return TRUE;
       case 'H':
       case 'h':
			if (check_pass(user_turn)) {
       			show_msg("Should Pass", FALSE);
			}
			else {       	
		 		think_move();
		 		draw_cursor(row_pos,col_pos);
		 		row_pos2 = row_pos;
		 		col_pos2 = col_pos;
		 	}
		 	break;
       case '+': 
	   		if (think_depth < 6) {
		    	think_depth++;
		    	show_level();
		    	send_level(sockfd, think_depth);
		    	ret_code = get_ret_code(sockfd);	
		 	}
		 	break;
       case '-': 
	   		if (think_depth > 1) {
		    	think_depth--;
		    	show_level();
		    	send_level(sockfd, think_depth);
		    	ret_code = get_ret_code(sockfd);			    
		 	}
		 	break;
     }
     if (col_pos != col_pos2 || row_pos != row_pos2) {
       draw_cursor(row_pos,col_pos);
       draw_cursor(row_pos2,col_pos2);
       row_pos = row_pos2;
       col_pos = col_pos2;
     }
  } while (TRUE);
}

void eat(int row, int col)
{
  draw_chess(row,col,turn);
  board[row][col] = turn;
}

void link_step(int row, int col)
{
  struct step *step_ptr;

  step_ptr = (struct step *) malloc(sizeof(struct step));
  step_ptr->row = row;
  step_ptr->col = col;
  step_ptr->next = step_list;
  step_list = step_ptr;
}

int valid_go(int turn, int row, int col)
{
  int i, j;
  int k, m, next_turn;

  step_list = NULL;
  next_turn = -turn;

  for (i = row-1, j = col-1; i > 0 && j > 0 && board[i][j] == next_turn;
       i--, j--);
  if (i >= 0 && j >= 0 && board[i][j] == turn)
     for (k = i + 1, m = j + 1; k < row; k++, m++)
	 link_step(k,m);

  for (i = col - 1; i > 0 && board[row][i] == next_turn; i--);
  if (i >= 0 && board[row][i] == turn)
     for (j = i + 1; j < col; j++)
	 link_step(row,j);

  for (i = row+1, j = col-1; i < 7 && j > 0 && board[i][j] == next_turn;
       i++, j--);
  if (i <= 7 && j >= 0 && board[i][j] == turn)
     for (k = i - 1, m = j + 1; k > row; k--, m++)
	 link_step(k,m);

  for (i = row + 1; i < 7 && board[i][col] == next_turn; i++);
  if (i <= 7 && board[i][col] == turn)
     for (j = i - 1; j > row; j--)
	 link_step(j, col);

  for (i = row+1, j = col+1; i < 7 && j < 7 && board[i][j] == next_turn;
       i++, j++);
  if (i <= 7 && j <= 7 && board[i][j] == turn)
     for (k = i - 1, m = j - 1; k > row; k--, m--)
	 link_step(k, m);

  for (i = col + 1; i < 7 && board[row][i] == next_turn; i++);
  if (i <= 7 && board[row][i] == turn)
     for (j = i - 1; j > col; j--)
	 link_step(row, j);

  for (i = row-1, j = col+1; i > 0 && j < 7 && board[i][j] == next_turn;
       i--, j++);
  if (i >= 0 && j <= 7 && board[i][j] == turn)
     for (k = i + 1, m = j - 1; k < row; k++, m--)
	 link_step(k, m);

  for (i = row-1; i > 0 && board[i][col] == next_turn; i--);
  if (i >= 0 && board[i][col] == turn)
     for (j = i + 1; j < row; j++)
	 link_step(j, col);

  return (step_list == NULL) ? FALSE : TRUE;
}

void go(int row, int col)
{
  struct step *sp;
  int num;

  eat(row,col);
  num = 1;
  if (row > 0 && row < 7 && col > 0 && col < 7)
      board_weight[row][col] = inner_mean;
  else if (!adjusted)
      board_weight[row][col] = side_mean;
  while (step_list != NULL) {
     eat(step_list->row, step_list->col);
     num++;
     sp = step_list;
     step_list = step_list->next;
     free(sp);
  }
  if (turn == user_turn) {
     user_got += num;
     my_got -= num;
     my_got++;
  }
  else {
     user_got -= num;
     user_got++;
     my_got += num;
  }
  show_got();
}

void adjust1()
{
  int i;

  if (adjusted) return;
  if (row_pos == 0 && col_pos == 0) {
      for (i = 1; i < 6 && board[0][i] != EMPTY; i++);	/* find first EMPTY */
      board_weight[0][i] = side_mean;
      for (i = 1; i < 6 && board[i][0] != EMPTY; i++);
      board_weight[i][0] = side_mean;
      board_weight[1][1] = inner_mean;
  }
  if (row_pos == 0 && col_pos == 7) {
      for (i = 6; i > 1 && board[0][i] != EMPTY; i--);
      board_weight[0][i] = side_mean;
      for (i = 1; i < 6 && board[i][7] != EMPTY; i++);
      board_weight[i][7] = side_mean;
      board_weight[1][6] = inner_mean;
  }
  if (row_pos == 7 && col_pos == 0) {
      for (i = 6; i > 1 && board[i][0] != EMPTY; i--);
      board_weight[i][0] = side_mean;
      for (i = 1; i < 6 && board[7][i] != EMPTY; i++);
      board_weight[7][i] = side_mean;
      board_weight[6][1] = inner_mean;
  }
  if (row_pos == 7 && col_pos == 7) {
      for (i = 6; i > 1 && board[i][7] != EMPTY; i--);
      board_weight[i][7] = side_mean;
      for (i = 6; i > 1 && board[7][i] != EMPTY; i--);
      board_weight[7][i] = side_mean;
      board_weight[6][6] = inner_mean;
  }
}

int test_step(int turn, int row, int col, int level)
{
  int i, j;
  int value, may, max_may;
  struct step *sp;

  board[row][col] = turn;
  value = board_weight[row][col];
  sp = step_list;
  while (step_list != NULL) {
     board[step_list->row][step_list->col] = turn;
     value += board_weight[step_list->row][step_list->col];
     step_list = step_list->next;
  }
  if (level != 1) {
     max_may = -9999;
     for (i = 0; i < 8; i++)
	for (j = 0; j < 8; j++)
	  if (board[i][j] == EMPTY && valid_go(-turn,i,j)) {
	     may = test_step(-turn,i,j,level-1);
	     if (may > max_may) max_may = may;
	  }
     if (!adjusted || max_may != -9999)
	value -= max_may;
  }
  while (sp != NULL) {
    board[sp->row][sp->col] = -turn;
    step_list = sp;
    sp = sp->next;
    free(step_list);
  }
  board[row][col] = EMPTY;
  return value;
}


int check_pass(int test_turn)
{
  int i,j;
  struct step *sp;

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      if (board[i][j] == EMPTY && valid_go(test_turn,i,j)) {
	 while (step_list != NULL) {
	   sp = step_list;
	   step_list = step_list->next;
	   free(sp);
	 }
	 return FALSE;
      }
  return TRUE;
}

void final_adjust()
{
  int i, j;

  for (i = 0; i < 8; i++)
      for (j = 0; j < 8; j++)
	  board_weight[i][j] = inner_mean;
}

int get_result(int sockfd) {
	size_t n;
	int cmd, user_cnt, comp_cnt;
	char line[MAXLINE], *buf;
	
	n = Readline(sockfd, line, MAXLINE);
    if (n == 0) {
//        send_ret_code(sockfd, ERR_NO_CONN);    	
       	show_msg("Err: No Conn", TRUE);
       	return(-1);
	};
	buf = strtok(line, " ");
    if (buf == NULL) {
        send_ret_code(sockfd, ERR_NO_CMD);
        show_msg("Err: No Cmd", TRUE);
        return(-1);
	};
    cmd = atoi(buf);
	if (cmd != 4) {	// != result
        send_ret_code(sockfd, ERR_INVALID_CMD);
        show_msg("Err: Inv Cmd", TRUE);
        return(-1);
	}
	buf = strtok(NULL, " ");
    if (buf == NULL) {
        send_ret_code(sockfd, ERR_WRONG_NUM_PARA);
        show_msg("Err: Wng Par", TRUE);
        return(-1);
	};
    user_cnt = atoi(buf);
	buf = strtok(NULL, " ");
    if (buf == NULL) {
        send_ret_code(sockfd, ERR_WRONG_NUM_PARA);
        show_msg("Err: Wng Par", TRUE);
        return(-1);
	};
    comp_cnt = atoi(buf);
	if (user_cnt == user_got && comp_cnt == my_got) {
		send_ret_code(sockfd, OK);   	
    	show_msg("Ok", FALSE);
    	return(0);
	}
    else {
       	send_ret_code(sockfd, ERR_INVALID_VALUE);
       	show_msg("Err: Inv Val", TRUE);
		return(-1);		
    };    				
}

void play(int sockfd)
{
  int step_count = 0, ret_code;
  int game_over = FALSE;
  int pass[2] = { FALSE, FALSE };
  char  buf[MAXLINE];
  
  putxy(11,41, "│ [  P  ] Pass     │");
  putxy(13,41, "│ [  L  ] Locate   │");
  putxy(14,41, "│ [  H  ] Hint     │");
  putxy(15,41, "│ [ENTER] Place    │");
  adjusted = FALSE;
  do {
     	show_current_turn();
		if (turn == user_turn) {  // client's turn
	      	pass[0] = check_pass(turn);
	      	pass[1] = check_pass(-turn);
	      	if (pass[0] && pass[1]) { // actively send cmd to end game
	      		game_over = TRUE;
	      		snprintf(buf, sizeof(buf), "6\n");
		 		Writen(sockfd, buf, strlen(buf));
		 		ret_code = get_ret_code(sockfd);
	        }
	      	else {	// not yet game over
	      		game_over = user_choice(sockfd);
	      		step_count++;
			}	
	 	} 
     	else  { // server's turn
     		pass[0] = check_pass(-turn);
	      	pass[1] = check_pass(turn);
	      	if (pass[0] && pass[1]) { // should receive server's cmd to end game
	      		game_over = TRUE;
				if (get_cmd(sockfd) != 6) {
//        			show_msg("Ser Err", TRUE);
        			send_ret_code(sockfd, ERR_INVALID_CMD);
				}
				else {
					send_ret_code(sockfd, OK);
				}	      		
	      	} 
	      	else if (pass[1]) { // should receive server's cmd to pass
				if (get_cmd(sockfd) != 3) {
//        			show_msg("Ser Err", TRUE);
        			send_ret_code(sockfd, ERR_INVALID_CMD);
				}
				else {
					send_ret_code(sockfd, OK);
				}
			}
	      	else if (!pass[1]) {	// should receive server's move
	      		ret_code = get_move(sockfd, &row_pos, &col_pos);
	      		if (ret_code != OK) {
	      			send_ret_code(sockfd, ret_code);
				}
				else if (!valid_go(turn,row_pos,col_pos)) {
	      			send_ret_code(sockfd, ERR_INVALID_VALUE);
	      			show_msg("Inv. Move", TRUE);
				} else {
					go(row_pos, col_pos);
					send_ret_code(sockfd, OK);
	      			row_pos2 = row_pos;
	      			col_pos2 = col_pos;
	      			step_count++;					
				}	      		
	      	}
    
	};
     if (game_over == TRUE) {
//		game_over = TRUE;
	} else	
     	turn = -turn;
  } while (!game_over);
	if (turn != user_turn) {	// to receive game result sent by the server
		get_result(sockfd);
	}
	else { // client's turn to send game result
		snprintf(buf, sizeof(buf), "4 %d %d\n", user_got, my_got);
    	Writen(sockfd, buf, strlen(buf));    
		ret_code = get_ret_code(sockfd);	
    	if (ret_code != OK)
    		show_msg("Err: Result", TRUE);
	};
  putxy(11,41, "│                  │");
  putxy(12,41, "│                  │");
  if (user_got == my_got)
     putxy(12,47, "   Tie  ");
  else if (user_got > my_got)
     putxy(12,47, "You Win ");
  else
     putxy(12,47, "You Lose");
  putxy(13,41, "│                  │");
  putxy(14,41, "│   Try Again?     │");
  putxy(15,41, "│                  │");
  putxy(16,41, "│      [Y/N]       │");
  putxy(17,41, "│                  │");
  od_set_cursor(16, 55);
  return;     /* game over */
}

int want_again()
{
  while (TRUE)
//    switch (od_get_key(TRUE)) {
    switch (getch()) {	
      case 'y':
      case 'Y': return TRUE;
      case 'n':
      case 'N': return FALSE;
    }
}

int main(int argc, char* argv[])
{
	int					sockfd, ret_code;
	struct sockaddr_in	servaddr;
	char buf[MAXLINE];

	if (argc != 2)
		err_quit("usage: ox_cli <IPaddress>");

	sockfd = Socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT+10);
	Inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	Connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
	printf("Connected. Sending request...\n");
	snprintf(buf, sizeof(buf), "0 AT0501\n");
    Writen(sockfd, buf, strlen(buf));
    if ((ret_code = get_ret_code(sockfd)) != 0) {
    	printf("id send error. ret code = %d\n", ret_code);
	}
  	else do {
     	od_clr_scr();
     	draw_board();
     	init_game();
     	if(get_par(sockfd) == QUIT)
			break;
     	play(sockfd);
  	} while (want_again());
	snprintf(buf, sizeof(buf), "6\n");
	Writen(sockfd, buf, strlen(buf));
//	ret_code = get_ret_code(sockfd);
//    if (ret_code != 0) {
//    	printf("quit cmd error. ret code = %d\n", ret_code);
//	}	 	
  	od_clr_scr();
    exit(0);
}
