#include <stdio.h>
#include <ncurses.h>
#include <menu.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <json-c/json.h>

/*This program is the 'launcher' for each of the levels, it generates a settings file and then runs the game.c file*/

/*TODO: Calculate actual size of readme file
        Add scroll support to readme view*/

/*GLOBAL VARS*/
char *option[] = {"Start Game", "Difficulty", "Seed", "How-To", "Multiplayer", "Exit", (char *)NULL,};
char *value[] = {"", "140", "1000", "", };
int selection;

int numLevels;
int show_readme();
void allocate_menu_items(ITEM ** menuItems, MENU* mainMenu);


void incr_item_val(int choice) {
    if(choice == 3) {
        value[3]="true";
    }
}

void decr_item_val(int choice) {

}

int write_settings(char *multiplayer)
{
    FILE *fp;

    //couldn't work out a way to (easily...) fully overwrite an existing file so we'll delete it just in case
    remove("settings.txt");

    //json object of needed settings values
    json_object *settings = json_object_new_object();
    json_object *difficulty_string = json_object_new_string(value[1]);
    json_object *seed_string = json_object_new_string(value[2]);
    json_object *mp_string = json_object_new_string(multiplayer);

    json_object_object_add(settings, "Difficulty", difficulty_string);
    json_object_object_add(settings, "Seed", seed_string);
    json_object_object_add(settings, "Multiplayer", mp_string);

    //write corresponding options and values to file
    fp = fopen("settings.json", "w");
    fprintf(fp, "%s", json_object_to_json_string(settings));
    fclose(fp);
    return 0;
}

int selection_handler(int selection, MENU* mainMenu, ITEM** menuItems)
{
    switch(selection)
    {
        case 0:
            write_settings("false");
            for(int i = 0; i < numLevels; ++i)
            {
                free_item(menuItems[i]);
            }
	        free_menu(mainMenu);
	        endwin();
            exit(0);
            break;

        case 1:
            /*if (*value[1] >= 130)
            {
                value[1] -= 5;
            }
            allocate_menu_items(menuItems, mainMenu);*/
            break;

        case 2:
            /*if (*value[2] > 100)
            {
                value[2] -= 100;
            }
            allocate_menu_items(menuItems, mainMenu);*/
            break;

        case 3:
            show_readme();
            break;
        case 4:
            write_settings("true");
            for(int i = 0; i < numLevels; ++i)
            {
                free_item(menuItems[i]);
            }
	        free_menu(mainMenu);
	        endwin();
            exit(0);
            break;
        case 5:
            //make sure we deallocate memory before exiting
            for(int i = 0; i < numLevels; ++i)
            {
                free_item(menuItems[i]);
            }
	        free_menu(mainMenu);
	        endwin();
            exit(1);
            break;
    }
    return 0;
}

void allocate_menu_items(ITEM ** menuItems, MENU* mainMenu)
{

    int numOptions = sizeof(option) / sizeof(option[0]);
    menuItems = (ITEM **)calloc(numOptions + 1, sizeof(ITEM *));

    //populate menu data
    for(int i = 0; i < numOptions; ++i)
    {
        menuItems[i] = new_item(option[i], value[i]);
    }
	menuItems[numOptions] = (ITEM *)NULL;
    mainMenu = new_menu((ITEM **)menuItems);

    //print the menu at the correct position in the terminal window
    set_menu_sub(mainMenu, derwin(stdscr, 0, 0, 15, 31));
	post_menu(mainMenu);
    refresh();
}

void start_menu()
{
    ITEM** menuItems;
    MENU* mainMenu;


    //initialise screen and capture keyboard input
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);
    erase();

    //ascii art "logo"
    printw("  __\n");
    printw(" /__  _  ._   _  ._ o  _    _.  _|     _  ._ _|_     ._ _     _   _. ._ _   _\n");
    printw(" \\_| (/_ | | (/_ |  | (_   (_| (_| \\/ (/_ | | |_ |_| | (/_   (_| (_| | | | (/_\n ");
    printw("                                                             _|\n");

    int numOptions = sizeof(option) / sizeof(option[0]);
    menuItems = (ITEM **)calloc(numOptions + 1, sizeof(ITEM *));

    //populate menu data
    for(int i = 0; i < numOptions; ++i)
    {
        menuItems[i] = new_item(option[i], value[i]);
    }
	menuItems[numOptions] = (ITEM *)NULL;
    mainMenu = new_menu((ITEM **)menuItems);

    //print the menu at the correct position in the terminal window
    set_menu_sub(mainMenu, derwin(stdscr, 0, 0, 15, 31));
	post_menu(mainMenu);
    refresh();

    //capture keyboard input and assign to choice
    int choice;
    while((choice = getch()) != ERR)
	{
        int currItem = item_index(current_item(mainMenu));
        switch(choice)
	    {
            case KEY_DOWN:
		        menu_driver(mainMenu, REQ_DOWN_ITEM);
				break;
			case KEY_UP:
				menu_driver(mainMenu, REQ_UP_ITEM);
				break;
            case 10:
                //10 represents enter, so get current item and pass to selection handler
                selection_handler(currItem, mainMenu, menuItems);
                break;
		}

	}

    //something's gone wrong to get here, so deallocate memory before exiting...
    for(int i = 0; i < numLevels; ++i)
    {
        free_item(menuItems[i]);
    }
	free_menu(mainMenu);
	endwin();
}

int show_readme()
{
    //allocate (more than) enough space for the readme file
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

    //TODO: allow the user to scroll the text
    erase();
    scrollok(stdscr, TRUE);
    idlok(stdscr, TRUE);
    printw("README:\n\n");
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
                    start_menu();
                    break;
            }
        }
    }
}

int main()
{
   start_menu();
}