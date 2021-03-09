#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    //this is mostly documented in the fight.c comments, only real difference is we're reading not writing
    const int memSize = 16; 
    const char* memName = "fight"; 
    int memFileDescriptor; 
    void* memPointer; 
    int i;
    memFileDescriptor = shm_open(memName, O_RDONLY, 0666);

    memPointer = mmap(0, memSize, PROT_READ, MAP_SHARED, memFileDescriptor, 0); 

    
    
    printf("Success: %s", (char *)memPointer);
    //we've got the result of the fight process so go ahead and kill it
    shm_unlink(memName);
    return 0; 
}
