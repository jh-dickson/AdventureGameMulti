#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

//shared memory writer for the game process - see https://www.geeksforgeeks.org/posix-shared-memory-api/
int main()
{
    const int memSize = 16; //<-plenty of memory for us to use
    const char* memName = "fight";//<- relatively unused, i reckon

    int memFileDescriptor;
    void* memPointer;

    //Create the shared memory object with our name and appropriate permissions
    memFileDescriptor = shm_open(memName, O_CREAT | O_RDWR, 0666);
    ftruncate(memFileDescriptor, memSize);
    memPointer = mmap(0, memSize, PROT_WRITE, MAP_SHARED, memFileDescriptor, 0);

    //write the result of the match to the shared memory
    sprintf(memPointer, "%d", 1);
    printf("Sent to client");

}