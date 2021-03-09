#include <stdio.h>
#include <ncurses.h>
#include <menu.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include "simple_networking.h"
#include <json-c/json.h>
#include <pthread.h> //<- technically speaking ncurses isn't really threadsafe but it works haha... see: https://stackoverflow.com/questions/29910562/why-has-no-one-written-a-threadsafe-branch-of-the-ncurses-library

/*
Begin rulebending: #define ROOM window
I'm classing each window and/or view as a room... :)
Here's the list:
    1) Main menu
    2) Readme
    3) gameWindow
    4) statsWindow
    5) Fight view

FWIW here are the usable items:
    1) Swords, used in fights
    2) Health, again, used in fights
    3) Stars, gained in fights and allows the user to end the game
    4) Hunger, decrements health if you're too hungry

TODO:   check sword quantity at each fight stage
        clean up settings parser code
        clean up key input handling in fight mode (or room should i say...)
        yield thread on getch error?
        investigate shared memory options for inter-terminal comms
        clean up thread joins...
        damage from animals
*/

/*GLOBAL VARIABLES*/
// Send to server
#define PLAYERID 2
#define PORT 42069
char *inventoryItems[4] = {"Health", "Hunger", "Swords", "Stars"}; //<- lets stats keep track of different character traits
int inventoryValues[4] = {10, 0, 0, 0};
int difficulty = 140;
int seed = 1000;
int position[2] = {0, 0};

// Keep local
int recentInput;
char charUnderCursor;
int movements;
bool multiplayer;
connection server_con;

// Other player vars - not to be sent over network
typedef struct otherPlayer {
    int playerID;
    int position[2];
    int difficulty;
    int seed;
    int inventoryValues[4];
    char *inventoryItems[4];
    struct otherPlayer *next;
    struct otherPlayer *prev;
} otherPlayer;
otherPlayer *playerList;

/*pthread definitions - this simplifies the locks and unlocks, see above stackoverflow link*/
pthread_t statsThread;
pthread_t gameThread;
pthread_t checkThread;
pthread_t networkThread;
pthread_mutex_t MUTEX;
#define LOCK pthread_mutex_lock(&MUTEX)
#define UNLOCK pthread_mutex_unlock(&MUTEX)

// Define other functions
void *handle_player_data(json_object *playerData);

json_object *parse_json_file(FILE *fp)
{
    char *buffer;
    int stringlen;

    // Create json object for parsing and error checking
    json_object *parsed_json = json_object_new_object();
    json_tokener *token = json_tokener_new();

    // Create error token for checking file read contains valid json values
    enum json_tokener_error errorToken;

    if (fp != NULL) 
    {
        // Seek to end of file
        if (fseek(fp, 0, SEEK_END) == 0) 
        {
            // Return size of file
            long size = ftell(fp);

            // Assign memory to buffer based on size of file
            buffer = malloc(size + 1);
            
            // Return to beginning of file
            fseek(fp, 0, SEEK_SET);

            // Read whole file in to buffer
            size_t len = fread(buffer, sizeof(char), size, fp);

            // Write EOF
            buffer[len++] = '\0';
        }
    }

    // Store length of string held in buffer
    stringlen = strlen(buffer);

    // Parse contents of buffer to json_object and store error/success token
    parsed_json = json_tokener_parse_ex(token, buffer, stringlen);

    // Check if token returned was success
    if ((errorToken = json_tokener_get_error(token)) != json_tokener_success)
    {
        // errorToken does not hold success - print description of error to stderr and return NULL
        fprintf(stderr, "[-] Error. %s\n", json_tokener_error_desc(errorToken));
        return NULL;
    }
 
    // Success - Return json object
    return parsed_json;
}

// Handles comms with the server
void *connect_to_server() {
    char *ip = "127.0.0.1";
    int port = PORT;

    server_con = connect_to(ip, port);
    if (server_con.dest_ip == NULL)
    {
        exit(-1);
    }
    while(1) {

        send_movements(server_con);

        message data_recieved = recieve_data(server_con);
        
        json_object *recievedData = json_object_new_object();
        recievedData = json_tokener_parse(data_recieved.data);
        handle_player_data(recievedData);
    

        memset(data_recieved.data, '\0', strlen(data_recieved.data));

        // Lets the computer breathe
        sleep(1);
    }
}

void *send_movements() {
    // Creates JSON object with all global variables to be sent to server
    json_object *serverData = json_object_new_object();
    if(!serverData)
        return;

    json_object_object_add(serverData, "playerID", json_object_new_int(PLAYERID));

    // Adds values to JSON
    json_object_object_add(serverData, "Health", json_object_new_int(inventoryValues[0]));
    json_object_object_add(serverData, "Hunger", json_object_new_int(inventoryValues[1]));
    json_object_object_add(serverData, "Swords", json_object_new_int(inventoryValues[2]));
    json_object_object_add(serverData, "Stars", json_object_new_int(inventoryValues[3]));
    json_object_object_add(serverData, "difficulty", json_object_new_int(difficulty));
    json_object_object_add(serverData, "seed", json_object_new_int(seed));
    json_object_object_add(serverData, "positionX", json_object_new_int(position[0]));
    json_object_object_add(serverData, "positionY", json_object_new_int(position[1]));

    // Sends JSON to socket as a string
    char *sDatastr = json_object_get_string(serverData);
    realloc(sDatastr, strlen(sDatastr)+5);
    strcat(sDatastr, "\n\0");
    
    send(server_con.sock_fd, sDatastr, strlen(sDatastr), 0);
}

void *handle_player_data(json_object *playerData) {
    otherPlayer *player = (otherPlayer *)malloc(sizeof(otherPlayer));

    player->position[0] = json_object_get_int(json_object_object_get(playerData, "positionX"));
    player->position[1] = json_object_get_int(json_object_object_get(playerData, "positionY"));
    player->playerID = json_object_get_int(json_object_object_get(playerData, "playerID"));
    player->inventoryItems[0] = inventoryItems[0];
    player->inventoryValues[0] = json_object_get_int(json_object_object_get(playerData, inventoryItems[0]));
    player->inventoryItems[1] = inventoryItems[1];
    player->inventoryValues[1] = json_object_get_int(json_object_object_get(playerData, inventoryItems[1]));
    player->inventoryItems[2] = inventoryItems[2];
    player->inventoryValues[2] = json_object_get_int(json_object_object_get(playerData, inventoryItems[2]));
    player->inventoryItems[3] = inventoryItems[3];
    player->inventoryValues[3] = json_object_get_int(json_object_object_get(playerData, inventoryItems[3]));

    player->difficulty = json_object_get_int(json_object_object_get(playerData, "difficulty"));
    player->seed = json_object_get_int(json_object_object_get(playerData, "seed"));


    // Move through the list and try to find existing player
    
    if(playerList == NULL) {
        playerList = player;
        return;
    }
    otherPlayer *current = playerList;
    if(current->next != NULL)
    {
        while (current->next != NULL)
        {
            if(current->playerID == player->playerID) {
                // We have found the player, splice old data out
                // and return to calling function
                player->next = current->next;
                player->prev = current->prev;
                current = player;
                return;
            }
            current = current->next;
        }
    }
    // If we are here we don't have this player
    current->next = player;
    player->prev = current;
    player->next = NULL;
}

// Add player positions to the map before draw
void *drawPositions(WINDOW *win, int position[]) {

    otherPlayer *otherPlayerPos = playerList;
    if(otherPlayerPos == NULL) {
        return;
    }
    while (otherPlayerPos->next != NULL)
    {
        mvwprintw(win, otherPlayerPos->position[1], otherPlayerPos->position[0], "Y");
        otherPlayerPos = otherPlayerPos->next;
    }
    wmove(win, position[1], position[0]);
}

//simplify window creation process
WINDOW *create_newwin(int height, int width, int starty, int startx) {
    WINDOW *local_win;
    local_win = newwin(height, width, starty, startx);
    box(local_win, 0, 0);
    wrefresh(local_win);
    return local_win;
}

void *check_win_death(void *result)
{
    int returnVal;
    int i = 1;
    while (i == 1)
    {
        if (inventoryValues[0] < 1)
        {
            returnVal = 0; //<-death
            i++;
        }
        else if (inventoryValues[3] > 4)
        {
            returnVal = 1; //<-win
            i++;
        }
        //keep cpu time down
        sleep(1);
    }

    //not a pretty way of killing the threads, ideally we would do it from within each thread...
    pthread_cancel(statsThread);
    pthread_cancel(gameThread);

    //return integer to main thread: https://stackoverflow.com/questions/2251452/how-to-return-a-value-from-pthread-threads-in-c
    if (returnVal != 0)
    {
        *((int *)result) = returnVal;
        pthread_exit(result);
    }
    else
    {
        pthread_exit(0);
    }


}

void start_game_window(WINDOW *gameWindow, int cursorX, int cursorY)
{
    char map[LINES][COLS];

    //generate world and print to game window
    const char *charToAdd;
    start_color();
    //initialise colour settings for printing - assume the terminal has colour support... it probably does...
    init_pair(2, COLOR_BLUE, COLOR_CYAN); //<- water
    init_pair(3, COLOR_BLACK, COLOR_GREEN); //<- trees
    init_pair(1, COLOR_YELLOW, COLOR_GREEN); //<- grass (green background and yellow characters)
    init_pair(5, COLOR_GREEN, COLOR_GREEN);//<- enemies - invisible (unused), would be enabled in the actual game, but for ease of marking it's not used...
    init_pair(4, COLOR_WHITE, COLOR_RED); //<- boxes

    srand(seed);
    for (int j = 1; j < COLS-1; j++)
    {
        for (int i = 1; i < (LINES*0.8)-1; i++)
        {

            int nextCharType = rand() % 1000;

            if (nextCharType <= 86)
            {
                charToAdd = "T";
                wattron(gameWindow, COLOR_PAIR(3)); //<- this has to be done inside the if statement as the charTypes have different colours
                mvwprintw(gameWindow, i, j, charToAdd);
                wattroff(gameWindow, COLOR_PAIR(3));
            }
            else if (nextCharType > 86 && nextCharType <= 120)
            {
                charToAdd = "W";
                wattron(gameWindow, COLOR_PAIR(2));
                mvwprintw(gameWindow, i, j, charToAdd);
                wattroff(gameWindow, COLOR_PAIR(2));
            }
            else if (nextCharType > 120 && nextCharType <= 125)
            {
                charToAdd = "B";
                wattron(gameWindow, COLOR_PAIR(4));
                mvwprintw(gameWindow, i, j, charToAdd);
                wattroff(gameWindow, COLOR_PAIR(4));
            }
            else if (nextCharType > 125 && nextCharType <= difficulty)
            {
                charToAdd = "X";//X marks the spot
                wattron(gameWindow, COLOR_PAIR(3));//<-enemies underneath the user are should be invisible but it would be harder to test the functionality.
                mvwprintw(gameWindow, i, j, charToAdd);
                wattroff(gameWindow, COLOR_PAIR(3));
            }
            else
            {
                charToAdd = " ";
                wattron(gameWindow, COLOR_PAIR(1));
                mvwprintw(gameWindow, i, j, charToAdd);
                wattroff(gameWindow, COLOR_PAIR(1));
            }
        }
    }

    //position cursor and refresh window
    wmove(gameWindow, cursorY, cursorX);
    wrefresh(gameWindow);

    //start keyboard capture
    nodelay(gameWindow, TRUE);
    keypad(gameWindow, true);
}

int fight_handler(WINDOW *gameWindow, int animal)
{
    int health = 10;
    char inputOption;

    switch (animal)
    {
    case 0:
        wprintw(gameWindow, "     ___    ___\n");
        wprintw(gameWindow, "    ( _<    >_ )\n");
        wprintw(gameWindow, "    //        \\\\\n");
        wprintw(gameWindow, "    \\\\___..___//\n");
        wprintw(gameWindow, "     `-(    )-'\n");
        wprintw(gameWindow, "       _|__|_\n");
        wprintw(gameWindow, "      /_|__|_\\\n");
        wprintw(gameWindow, "      /_|__|_\\\n");
        wprintw(gameWindow, "   '-\\__Y__/-'\n");
        wprintw(gameWindow, "     `---`\n");
        wprintw(gameWindow, "\n\nYou're being mauled by a bear! - Fight it off!\n");
        wrefresh(gameWindow);
        mvwprintw(gameWindow, LINES*0.6, COLS*0.25, "What would you like to use?");
        for (int i = 0; i < 3; i++)
        {
            mvwprintw(gameWindow, (LINES*0.6)-2, COLS*0.25, "Enemy health: %d  ", health);
            mvwprintw(gameWindow, (LINES*0.6)+1, COLS*0.25, "F1) Sword: %d (Deals 6 damage)", inventoryValues[2]);
            mvwprintw(gameWindow, (LINES*0.6)+2, COLS*0.25, "F2) Hands (+2 hunger) (Deals 3 damage)");
            wrefresh(gameWindow);
            int j = 0;
            while(j == 0)
            {
                int input;
                if((input = wgetch(gameWindow)) == ERR)
                {
                    //just loop again and see if we get an input in the buffer
                }
                else
                {
                    switch(input)
                    {
                        case KEY_F(1):
                            inputOption = 'a';
                            j++;
                            break;
                        case KEY_F(2):
                            inputOption = 'b';
                            j++;
                            break;
                        default:
                            break;
                    }
                }
            }
            //legacy from when using wscanw
            if (inputOption == 'a')
            {
                LOCK;
                health -= 5;
                inventoryValues[2] -= 1;
                UNLOCK;
            }
            else if (inputOption == 'b')
            {
                LOCK;
                health -= 2;
                UNLOCK;
            }
            if(health < 1)
            {
                werase(gameWindow);
                wrefresh(gameWindow);
                mvwprintw(gameWindow, (LINES*0.4), COLS*0.4, "You won, gained 2 stars!");
                wrefresh(gameWindow);
                LOCK;
                inventoryValues[3] += 2;
                UNLOCK;
                i=3;
            }
            sleep(1);
        }
        break;
    case 2:
        wprintw(gameWindow, " /\\ \\  / /\\\n");
        wprintw(gameWindow, "//\\\\ .. //\\\\\n");
        wprintw(gameWindow, "//\\((  ))/\\\\\n");
        wprintw(gameWindow, "/  < `' >  \\\n");
        wprintw(gameWindow, "\n\nA spider is about to bite you - kill it!\n");
        wrefresh(gameWindow);
        mvwprintw(gameWindow, LINES*0.6, COLS*0.25, "What would you like to use?");
        for (int i = 0; i < 3; i++)
        {
            mvwprintw(gameWindow, (LINES*0.6)-2, COLS*0.25, "Enemy health: %d  ", health);
            mvwprintw(gameWindow, (LINES*0.6)+1, COLS*0.25, "F1) Sword: %d (Deals 10 damage)", inventoryValues[2]);
            mvwprintw(gameWindow, (LINES*0.6)+2, COLS*0.25, "F2) Hands (+2 hunger) (Deals 7 damage)");
            wrefresh(gameWindow);
            int j = 0;
            while(j == 0)
            {
                int input;
                if((input = wgetch(gameWindow)) == ERR)
                {
                    //just loop again and see if we get an input in the buffer
                }
                else
                {
                    switch(input)
                    {
                        case KEY_F(1):
                            inputOption = 'a';
                            j++;
                            break;
                        case KEY_F(2):
                            inputOption = 'b';
                            j++;
                            break;
                        default:
                            break;
                    }
                }
            }
            //legacy from when using wscanw
            if (inputOption == 'a')
            {
                LOCK;
                health -= 10;
                inventoryValues[2] -= 1;
                UNLOCK;
            }
            else if (inputOption == 'b')
            {
                LOCK;
                health -= 7;
                UNLOCK;
            }
            if(health < 1)
            {
                werase(gameWindow);
                wrefresh(gameWindow);
                mvwprintw(gameWindow, (LINES*0.4), COLS*0.4, "You won, gained 1 star!");
                wrefresh(gameWindow);
                LOCK;
                inventoryValues[3] += 1;
                UNLOCK;
                i=3;
            }
            sleep(1);
        }
        break;
    case 3:
        wprintw(gameWindow, "        _\n");
        wprintw(gameWindow, "       / \\      _-'\n");
        wprintw(gameWindow, "     _/|  \\-''- _ /\n");
        wprintw(gameWindow, "__-' { |          \\\\\n");
        wprintw(gameWindow, "    /             \\\\\n");
        wprintw(gameWindow, "    /       'o.  |o }\n");
        wprintw(gameWindow, "    |            \\ ;\n");
        wprintw(gameWindow, "                  ',\n");
        wprintw(gameWindow, "       \\_         __\\\n");
        wprintw(gameWindow, "         ''-_    \\.//\n");
        wprintw(gameWindow, "           / '-____'\n");
        wprintw(gameWindow, "          /\n");
        wprintw(gameWindow, "        _'\n");
        wprintw(gameWindow, "     _-'\n");
        wprintw(gameWindow, "\n\nA pack of wolves is hunting you - escape quickly\n");
        wrefresh(gameWindow);
        mvwprintw(gameWindow, LINES*0.6, COLS*0.25, "What would you like to use?");
        for (int i = 0; i < 3; i++)
        {
            mvwprintw(gameWindow, (LINES*0.6)-2, COLS*0.25, "Enemy health: %d  ", health);
            mvwprintw(gameWindow, (LINES*0.6)+1, COLS*0.25, "F1) Sword: %d (Deals 5 damage)", inventoryValues[2]);
            mvwprintw(gameWindow, (LINES*0.6)+2, COLS*0.25, "F2) Hands (+2 hunger) (Deals 2 damage)");
            wrefresh(gameWindow);
            int j = 0;
            while(j == 0)
            {
                int input;
                if((input = wgetch(gameWindow)) == ERR)
                {
                    //just loop again and see if we get an input in the buffer
                }
                else
                {
                    switch(input)
                    {
                        case KEY_F(1):
                            inputOption = 'a';
                            j++;
                            break;
                        case KEY_F(2):
                            inputOption = 'b';
                            j++;
                            break;
                        default:
                            break;
                    }
                }
            }
            //legacy from when using wscanw
            if (inputOption == 'a')
            {
                LOCK;
                health -= 5;
                inventoryValues[2] -= 1;
                UNLOCK;
            }
            else if (inputOption == 'b')
            {
                LOCK;
                health -= 2;
                UNLOCK;
            }
            if(health < 1)
            {
                werase(gameWindow);
                wrefresh(gameWindow);
                mvwprintw(gameWindow, (LINES*0.4), COLS*0.4, "You won, gained 3 stars!");
                wrefresh(gameWindow);
                LOCK;
                inventoryValues[3] += 3;
                UNLOCK;
                i=3;
            }
            sleep(1);
        }
        break;
    }
    sleep(5);
    start_game_window(gameWindow, 10, 10);
    return 0;
}

int can_move(int diffX, int diffY, char worldItems[5], WINDOW *gameWindow)
{
    //check if we're on the enemy, if so launch the fight window
    if (worldItems[4] == 'X')
    {
        werase(gameWindow);
        wrefresh(gameWindow);
        srand(time(NULL));
        int r = rand() % 4;
        fight_handler(gameWindow, r);
    }

    //hunger related damage
    if (inventoryValues[1] >= 25)
    {
        //cap hunger at 10
        inventoryValues[1] = 25;
        inventoryValues[0]--;
    }

    //while we're at it, sanity check the health value :)
    if (inventoryValues[0] >= 10)
    {
        inventoryValues[0] = 10;
    }
    else if (inventoryValues[0] < 0)
    {
        inventoryValues[0] = 0;
    }




    //diffX is the change in X, same idea with diffY
    //worldItems array is the set of items 0:up 1:down 2:left 3:right 4:underneath the moving object
    int i;
    switch (diffX)
    {
        case 1:
            i = 3;
            break;
        case -1:
            i = 2;
            break;
        default:
            break;
    }
    switch (diffY)
    {
        case 1:
            i = 1;
            break;
        case -1:
            i = 0;
            break;
        default:
            break;
    }

    switch (worldItems[i])
    {
        case 'T':
            return FALSE;
            break;
        case 'W':
            //one liner for nanosleep - water should slow you down a bit
            nanosleep((const struct timespec[]){{0, 500000000L}}, NULL);
            return TRUE;
            break;
        //all other types can be moved onto without modification
        default:
            return TRUE;
            break;
    }

}

int open_settings()
{
    //open the settings file and read each line
    FILE *fp;
    char buffer[1024];
    fp = fopen("settings.json", "r");
    // if (fp == NULL)
    // {
    //     //oops looks like the file hasn't been created yet... ask the user to open main.c
    //     printf("There was an error opening the settings file, please ensure that settings.json exists\n");
    //     printf("Running the menu screen again should solve this issue.\n");
    //     exit(1);
    // }

    // fread(buffer, 1024, 1, fp);
    // fclose(fp);

    // json_object *parsedJSON = json_tokener_parse(buffer)

    // json



    json_object *parsedSettings = parse_json_file(fp);
    if (!parsedSettings)
        return -1;
    printf("hello");
   
    difficulty = atoi(json_object_get_string(json_object_object_get(parsedSettings, "Difficulty")));
    seed = atoi(json_object_get_string(json_object_object_get(parsedSettings, "Seed")));

    if(strcmp(json_object_get_string(json_object_object_get(parsedSettings, "Multiplayer")), "true") == 0)
    {
        multiplayer = TRUE;
    }
    else
    {
        multiplayer = FALSE;
    }
}

void *stats_handler(void *p)
{
    //allow us to stop the thread from another function
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

    //start the ncurses window
    WINDOW *statsWindow;
    LOCK;
    statsWindow = create_newwin(LINES*0.2, COLS, LINES*0.8, 0);
    UNLOCK;

    for(;;)
    {
        if(recentInput == 0)
        {
            //run the hunger stats: every 5 moves, the player hunger increases
            if (movements >= 5)
            {
                LOCK;
                inventoryValues[1]++;
                movements =- 5;
                UNLOCK;
            }

            //TODO: deal damage if the player is above 8 hunger
            /*if (inventoryValues[1] >= 8)
            {
                inventoryValues[0]--;
            }*/


        }
        else
        {
            //legacy from when stats was handling multiple input types
            switch (recentInput)
            {
            case 10:
                if(charUnderCursor == 'B')
                {
                    int item = rand() % 2;
                    if (item == 0)
                    {
                        LOCK;
                        inventoryValues[2]++;
                        UNLOCK;
                        mvwprintw(statsWindow, 1, 1,"Picked up sword");
                    }
                    else
                    {
                        LOCK;
                        inventoryValues[1] -= 3;
                        UNLOCK;
                        mvwprintw(statsWindow, 1, 1,"Picked up food ");
                    }


                }
                else
                {
                    mvwprintw(statsWindow, 1, 1,"No item here!  ");
                }
                LOCK;
                recentInput = 0;
                charUnderCursor = '\0';
                UNLOCK;
                break;
            }

            //comfort sleep
            nanosleep((const struct timespec[]){{0, 250000000L}}, NULL);

            //get inventory and print
            for (int i = 0; i < 4; i++)
            {
                mvwprintw(statsWindow, (i+1), (COLS*0.5), "%s: %d", inventoryItems[i], inventoryValues[i]);
            }
            LOCK;
            wrefresh(statsWindow);
            UNLOCK;
        }
    }
}

void *game_handler(void *p)
{
    //allow us to stop the thread from another function
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

    //start the ncurses window
    WINDOW *gameWindow;
    LOCK;
    gameWindow = create_newwin(LINES*0.8, COLS, 0, 0);
    UNLOCK;

    //set cursor to the centre of the gameWindow
    int cursorX = COLS/2;
    int cursorY = LINES*0.4;
    position[0] = cursorX;
    position[1] = cursorY;
    start_game_window(gameWindow, cursorX, cursorY);

    for(;;)
    {
        int input;
        if((input = wgetch(gameWindow)) == ERR)
        {
            //TODO: yield thread? - not really useful since we're only a few threads here so probably rescheduled immediately
        }
        else
        {
            //look around the character so we can tell whether to move or not.
            char worldItems[5];
            worldItems[0] = mvwinch(gameWindow, cursorY - 1, cursorX);
            worldItems[1] = mvwinch(gameWindow, cursorY + 1, cursorX);
            worldItems[2] = mvwinch(gameWindow, cursorY, cursorX - 1);
            worldItems[3] = mvwinch(gameWindow, cursorY, cursorX + 1);
            worldItems[4] = mvwinch(gameWindow, cursorY, cursorX);
            wmove(gameWindow, cursorY, cursorX);

            //this also handles input for the stats screen using global variable recentInput
            switch(input)
            {
                case KEY_UP:
                    //character movements are a weird routine which should really be in a function but i couldn't work that out in time
                    if (can_move(0,-1, (char *)worldItems, gameWindow) == TRUE)
                    {
                        cursorY--;
                        wmove(gameWindow, cursorY, cursorX);
                        LOCK;
                        movements++;
                        UNLOCK;
                    }
                    break;
                case KEY_DOWN:
                    if (can_move(0, 1, (char *)worldItems, gameWindow) == TRUE)
                    {
                        cursorY++;
                        wmove(gameWindow, cursorY, cursorX);
                        LOCK;
                        movements++;
                        UNLOCK;
                    }
                    break;
                case KEY_LEFT:
                    if (can_move(-1, 0, (char *)worldItems, gameWindow) == TRUE)
                    {
                        cursorX--;
                        wmove(gameWindow, cursorY, cursorX);
                        LOCK;
                        movements++;
                        UNLOCK;
                    }
                    break;
                case KEY_RIGHT:
                    if (can_move(1, 0, (char *)worldItems, gameWindow) == TRUE)
                    {
                        cursorX++;
                        wmove(gameWindow, cursorY, cursorX);
                        LOCK;
                        movements++;
                        UNLOCK;
                    }
                    break;
                case 10:
                    //offload to stats handler
                    LOCK;
                    recentInput = 10;
                    charUnderCursor = winch(gameWindow);
                    UNLOCK;
                    break;

            }
            position[0] = cursorX;
            position[1] = cursorY;
            if(multiplayer = TRUE){
                send_movements();
                drawPositions(gameWindow, position);
            }
            
            wrefresh(gameWindow);
        }


    }
}

int main()
{
    open_settings();

    if(multiplayer = TRUE) {
        pthread_create(&networkThread, NULL, connect_to_server, NULL);
    }

    //initialise screen
    initscr();
    cbreak();
    noecho();

    //create window threads
    pthread_create(&statsThread, NULL, stats_handler, NULL);
    pthread_create(&gameThread, NULL, game_handler, NULL);

    //create win_death checker thread
    int result;
    pthread_create(&checkThread, NULL, check_win_death, &result);

    //wait for thread completion
    void *endType = 0;
    pthread_join(statsThread, NULL);
    pthread_join(gameThread, NULL);
    pthread_join(checkThread, &endType);
    if(multiplayer = TRUE) {
        pthread_join(&networkThread, NULL);
    }
    endwin();

    if (endType != 0)
    {
        printf("Congratulations, You won the game!\n");
    }
    else
    {
        printf("Oops, you died...\n");
    }

    return(0);
}
