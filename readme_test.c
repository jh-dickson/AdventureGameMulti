#include <stdio.h>
#include <ncurses.h>
#include <menu.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);

    //allocate enough space for the readme file
    char *readme = malloc(1000000);

    //load file like in write_settings, just with read perms
    FILE *fp;

    fp = fopen("readme.txt", "r");
    char c;
    int i=0;
    //go through char by char and add to readme array
    while ((c = fgetc(fp)) != EOF)
    {
        readme[i] = c;
        i++;
    }
    fclose(fp);

    //allow the user to scroll the text
    scrollok(stdscr, TRUE);
    idlok(stdscr, TRUE);
    printw("README:\n\n\n");
    printw(readme);
    refresh();
    for(;;)
    {
        int input;
        if((input = getch()) == ERR)
        {

        }
        else
        {
            switch(input)
            {
                case 10:
                    //Go back to main menu
                    break;
            }
        }
    }
}
