/*** Includes ***/
#include <ctype.h>
#include<errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


/*** defines ***/
#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ESC(n) "\x1b"#n 
typedef struct termios Terminal;
typedef int bool;


/*** data ***/
typedef struct editorConfig
{
	int cx, cy;
	int screenrows;
	int screencols;
	Terminal terminal_original;
} editorConfig;

editorConfig E = { 0 };

/*** terminal ***/
//error handling
void die(const char* s)
{
	write(STDOUT_FILENO, ESC([2J), 4);// clear screen escape sequence
	write(STDOUT_FILENO, ESC([H), 3);

	perror(s);
	exit(1);
}
//terminal control
void terminal_disableRawMode()
{
	if(tcsetattr(STDIN_FILENO,TCSAFLUSH, &E.terminal_original) == -1) die("tcsetattr");
}
void terminal_enableRawMode()
{
	if(tcgetattr(STDIN_FILENO, &E.terminal_original) == -1) die("tcgetattr");
	atexit(terminal_disableRawMode);
	Terminal terminal_raw = { 0 };
	terminal_raw = E.terminal_original;

	//enable cannonical mode
	terminal_raw.c_cflag |= ~(CS8);
	terminal_raw.c_iflag &= ~(BRKINT|ICRNL|INPCK|ISTRIP|IXON);
	terminal_raw.c_lflag &= ~(ECHO | ICANON |IEXTEN | ISIG);
	terminal_raw.c_oflag &= ~(OPOST);

	//read() timeout
	terminal_raw.c_cc[VMIN] = 0; // minimu # of bytes of inputs needed before read can return
	terminal_raw.c_cc[VTIME] = 1;// max amount of time to wait before read() return in 100ms.
	if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&terminal_raw) == -1) die("tcsetattr");
}

//keyboard input
char terminal_ReadKey()
{
	int byteRead = 0;
	char c = '\0';
	while ((byteRead = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (byteRead == -1 && errno != EAGAIN) die("read");
	}
	return c;
}
//window size
int terminal_getCursorPosition(int *rows, int*cols)
{
	char buf[32];// 4* 8 = 32
	if (write(STDOUT_FILENO, ESC([6n), 4) != 4) return -1;
	unsigned int i = 0;

	while (i < sizeof(buf) - 1)
	{
		if (read(STDIN_FILENO, &buf[i], 1) == -1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if(sscanf(&buf[2],"%d;%d", rows, cols) != 2) return -1;

	//printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
	/*char c;
	while (read(STDIN_FILENO, &c, 1) == 1)
	{
		if (iscntrl(c))
		{
			printf("%d\r\n", c);
		}
		else
		{
			printf("%d ('%c)", c, c);
		}
	}*/
	
	return 0;
}
int terminal_getWindowSize(int* rows, int* cols)
{
	struct winsize ws;
	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
		//fallback if ioctrl fails
		if (write(STDOUT_FILENO, ESC([999C)ESC([999B), 12) != 12) return -1;
		return terminal_getCursorPosition(rows,cols);
	}
	else
	{
		*rows = ws.ws_row;
		*cols = ws.ws_col;
		return 0;
	}
}
/*** append buffer ***/
typedef struct abuf
{
	char* b;
	int len;
}abuf;

#define ABUF_INIT {NULL, 0}

void abAppend(abuf* ab, const char* s, int len)
{
	char* new = realloc(ab->b, ab->len + len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(abuf* ab)
{
	free(ab->b);
}
/***editor output to screen ***/
void editorOut_DrawRows(abuf *ab)
{
	int y;
	for (y = 0; y < E.screenrows; y++)
	{
		//welcome message
		if (y == E.screenrows / 3)
		{
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION); // print the upper 1/3 of screen
			if (welcomelen > E.screencols) welcomelen = E.screencols;
			//center text
			int padding = (E.screencols - welcomelen) / 2;
			if (padding)
			{
				abAppend(ab, "~", 1);
				padding--;
				
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
		}else
		{ 
			abAppend(ab, "~", 1);
		}		
		abAppend(ab, ESC([k), 3); //clear from cursor to end of line
		if (y < E.screenrows - 1)
		{
			abAppend(ab, "\r\n", 2);
		}
	}
}
void editorOut_RefreshScreen()
{
	abuf appendBuf = ABUF_INIT;
	abAppend(&appendBuf, ESC([?25l), 6); //escape sequence l_ reset mode
	//abAppend(&appendBuf, ESC([2J), 4); clear screen escape sequence
	abAppend(&appendBuf, ESC([H), 3);

	editorOut_DrawRows(&appendBuf);

	/*** cursor ***/
	char buf[32];
	snprintf(buf,sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);

	abAppend(&appendBuf,buf,strlen(buf));

	abAppend(&appendBuf,ESC([?25h), 6);//escape sequence H- setMode

	write(STDOUT_FILENO, appendBuf.b, appendBuf.len);
}

/*** editor input ***/
void editorMoveCursor(char key)
{
	switch (key)
	{
	case 'a':
		E.cx--;
		break;
	case 'd':
		E.cx++;
		break;
	case 'w':
		E.cy--;
		break;
	case 's':
		E.cy++;
		break;
	}
}
void editorIn_ProcessKeypress()
{
	char c = terminal_ReadKey();

	switch (c)
	{
		case CTRL_KEY('q'):
			//clear screen at exit
			write(STDOUT_FILENO, ESC([2J), 4);// clear screen escape sequence
			write(STDOUT_FILENO, ESC([H), 3);
			exit(0);
			break;
		case 'w':
		case 's':
		case 'a':
		case 'd':
			editorMoveCursor(c);
			break;
	}
}

/*** init ***/

void initEditor() //intialize all fields in the editor struct
{
	E.cx = 0;
	E.cy = 0;
	if(terminal_getWindowSize(&E.screenrows,&E.screencols) == -1) die("terminal_getWindowSize");
}

/*** main ***/
int main(void)
{
	
	// terminal control
	initEditor();
	terminal_enableRawMode();
	
	//program loop
	while (1)
	{
		//editor operations
		editorOut_RefreshScreen();
		editorIn_ProcessKeypress();
		//E.cx++;
		//E.cy = E.cx*E.cx;
	}
	printf("Finally success\r\n");
	return 0;
}


