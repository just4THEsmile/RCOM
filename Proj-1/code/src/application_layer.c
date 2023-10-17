// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer App_info;
    App_info.baudRate = baudRate;
    App_info.nRetransmissions = nTries;
    App_info.timeout = timeout;
    strcpy(App_info.serialPort, serialPort);
    App_info.role = strcmp(role, "rx") == 0 ? LlRx : LlTx;

    int fd=llopen(App_info);
    if(fd<0){
        printf("Error opening connection\n");
        return;
    }


    if(App_info.role==LlTx){

        FILE *file = fopen(filename, "rb");

        ftell(fp)
        
    }
    

}
