#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

/*GLOBAL VARS*/
void *memPointer;

//shared memory writer for the game process - see https://www.geeksforgeeks.org/posix-shared-memory-api/
int mmap_writer_setup()
{
    const int memSize = 16; //<-plenty of memory for us to use
    const char* memName = "fight";//<- relatively unused, i reckon

    int memFileDescriptor;

    //Create the shared memory object with our name and appropriate permissions
    memFileDescriptor = shm_open(memName, O_CREAT | O_RDWR, 0666);
    ftruncate(memFileDescriptor, memSize);
    memPointer = mmap(0, memSize, PROT_WRITE, MAP_SHARED, memFileDescriptor, 0);
}

int main()
{
    mmap_writer_setup(); //<- we need this up before the reader can read from it, otherwise it gets unhappy...
    srand(time(NULL));
    int r = rand() % 4;

    switch (r)
    {
    case 0:
        printf("     ___    ___\n");
        printf("    ( _<    >_ )\n");
        printf("    //        \\\\\n");
        printf("    \\\\___..___//\n");
        printf("     `-(    )-'\n");
        printf("       _|__|_\n");
        printf("      /_|__|_\\\n");
        printf("      /_|__|_\\\n");
        printf("      /_\\__/_\\\n");
        printf("       \\ || /  _\n");
        printf("         ||   ( )\n");
        printf("         \\\\___//\n");
        printf("          `---'\n");
        printf("\n\nYou just stood on a scorpion! - You've lost 2 health points!\n");
        break;
    case 1:
        printf(" __         __\n");
        printf("/  \\.-***-./  \\\n");
        printf("\\    -   -    /\n");
        printf(" |   o   o   |\n");
        printf(" \\  .-'''-.  /\n");
        printf("  '-\\__Y__/-'\n");
        printf("     `---`\n");
        printf("\n\nYou're being mauled by a bear! - Fight it off!\n");
        break;
    case 2:
        printf(" /\\ \\  / /\\\n");
        printf("//\\\\ .. //\\\\\n");
        printf("//\\((  ))/\\\\\n");
        printf("/  < `' >  \\\n");
        printf("\n\nA spider is about to bite you - kill it!\n");
        break;
    case 3:
        printf("        _\n");
        printf("       / \\      _-'\n");
        printf("     _/|  \\-''- _ /\n");
        printf("__-' { |          \\\\\n");
        printf("    /             \\\\\n");
        printf("    /       'o.  |o }\n");
        printf("    |            \\ ;\n");
        printf("                  ',\n");
        printf("       \\_         __\\\n");
        printf("         ''-_    \\.//\n");
        printf("           / '-____'\n");
        printf("          /\n");
        printf("        _'\n");
        printf("     _-'\n");
        printf("\n\nA pack of wolves is hunting you - escape quickly\n");
        break;
    }
    //write the result of the match to the shared memory
    sprintf(memPointer, "[RESULT HERE]");
}