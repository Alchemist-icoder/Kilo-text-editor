#include <ctype.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct termios Terminal;

//original terminal
Terminal terminal_original = { 0 };
//terminal control
void terminal_disableRawMode()
{
	tcsetattr(STDIN_FILENO,TCSAFLUSH, &terminal_original);
}
void terminal_enableRawMode()
{
	tcgetattr(STDIN_FILENO, &terminal_original);
	atexit(terminal_disableRawMode);
	Terminal terminal_raw;
	terminal_raw = terminal_original;
	//cannonical mode
	terminal_raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO,TCSAFLUSH,&terminal_raw);
}

//display keys
void displayKeys(char key)
{
	if (iscntrl(key))
	{
		printf("%d\n", key);
	}
	else {
		printf("%d,%c\n", key, key);
	}
}
int main(void)
{
	char c;
	// terminal control
	terminal_enableRawMode();
	//program loop
	int echo = ECHO;
	while (read(STDIN_FILENO, &c, 1) && c != 'q')
	{
		displayKeys(c);
	}
	printf("Finally success\n");
	return 0;
}