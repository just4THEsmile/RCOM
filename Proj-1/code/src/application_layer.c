// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

// returns the length of the packet
int build_Control_Packet(unsigned char C_value, const char *filename, long int file_len, unsigned char *packet)
{

    printf("Building Control Packet\n");
    int packet_len = 5 + strlen(filename);
    printf("Packet Length: %d\n", packet_len);
    packet[0] = C_value;
    printf("C_value: %d\n", C_value);

    packet[1] = 0x00;                                         // Type
    packet[2] = ceil((double)log2f((float)(file_len / 8.0))); // Length
    printf("File Length: %ld\n", file_len);
    packet[3] = file_len; // Content
    int i = 4 + packet[2];

    packet[i++] = 0x01;
    packet[i++] = (unsigned char)strlen((char *)filename);
    printf("Filename Length: %ld\n", strlen((char *)filename));
    memcpy(&packet[i], filename, strlen((char *)filename));
    printf("Filename: %s\n", filename);

    return packet_len;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{
    LinkLayer App_info;
    App_info.baudRate = baudRate;
    App_info.nRetransmissions = nTries;
    App_info.timeout = timeout;
    strcpy(App_info.serialPort, serialPort);
    App_info.role = strcmp(role, "rx") == 0 ? LlRx : LlTx;

    printf("Serial Check: %s\n", App_info.serialPort);
    int fd = llopen(App_info);
    printf("Serial Check: %s\n", App_info.serialPort);
    if (fd < 0)
    {
        printf("Error opening connection\n");
        return;
    }

    if (App_info.role == LlTx)
    {

        unsigned char packet[MAX_PAYLOAD_SIZE];
        packet[0] = 0x01;
        packet[1] = 0x02;
        packet[2] = 0x03;
        packet[3] = 0x04;
        packet[4] = 0x05;

        llclose(1);

        /*
        long int len;

        FILE *file = fopen(filename, "rb");

        fseek(file, 0, SEEK_END);
        len = ftell(file);
        long int bytes_left=len;

        int packet_len=build_Control_Packet(0x02,filename,len,packet);



        while(bytes_left>0){
            unsigned char buf[MAX_PAYLOAD_SIZE];
            if(bytes_left<MAX_PAYLOAD_SIZE){
                fread(buf, 1, bytes_left, file);
                if(llwrite(buf, bytes_left)<0) printf("ERROR on WRITE");
                bytes_left=0;
            }
            else{
                fread(buf, 1, MAX_PAYLOAD_SIZE, file);
                if(llwrite(buf, MAX_PAYLOAD_SIZE)<0) printf("ERROR on WRITE");
                bytes_left-=MAX_PAYLOAD_SIZE;
            }

        }
        fclose(file);
        llclose(1);

    */
    }
    else
    {

        unsigned char packet[MAX_PAYLOAD_SIZE];
        int packet_len = llread(packet);
        printf("Packet Length: %d\n", packet_len);
        if (packet[0] == 0x01)
        {
            printf("Control Packet\n");
        }
        else
        {
            printf("Not a Control Packet\n");
        }
        for (int i = 0; i < packet_len; i++)
        {
            printf("0x%x ", packet[i]);
        }
    }
}
