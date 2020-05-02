#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//Ncurses library
#include <ncurses.h>

#define COLORNUM 7

///// FUNCTION DECLARATIONS
void quit();

void initBoard(int colorNum);

int positionColor(int color);

void youLose();

///// MAIN FUNCTION
int main()
{
    initscr();
    // Disable input buffering ( will also disable control sequences like Ctrl-C )
    raw();
    atexit(quit);

    
    start_color();
    clear();
    


    //Title
    mvaddstr(5, 5, "FABULOUS FRED");
    refresh();
    sleep(3);
    //Color buttons
    initBoard(COLORNUM);
    //beep();
    //flash();

    
    //doupdate();
    refresh();
    getch();

    return 0;
}

void quit()
{
    endwin();
}


void initBoard(int colorNum)
{   

    for(int i = 1; i <= colorNum; i++)
    {
        init_pair(i, i, i); 
        color_set(i, 0);

        int x = positionColor(i);
        mvaddstr(10, x, "  ");
    }
}

int positionColor(int color)
{
    //Padding to the left side of the window
    int offset = 2;
    int distance = 3;
    int x;

    x = offset + (distance * color);

    return x;
}

void youLose()
{
    //Give audio-visual feedback when losing the game
    beep();
    flash();
}