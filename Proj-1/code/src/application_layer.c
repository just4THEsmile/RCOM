// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <unistd.h>

// fills packet array with data. Returns buffsize +3
int build_Data_Packet(unsigned char *buf, int bufSize, unsigned char *packet)
{
    packet[0] = 0x01;
    packet[1] = 0x00;
    packet[2] = 0x00;
    packet[1] = ceil(bufSize / (256));
    packet[2] = bufSize % (256);
    memcpy(&packet[3], buf, bufSize);
    return bufSize + 3;
}

// returns lenght of bufsize
// uses second index to determine len = 256 * packet[1]
// adds remaining bytes which is storedneeded and fills buf from packet
int get_Data_Packet_Info(unsigned char *packet, unsigned char *buf)
{
    int i = 1;
    int bufSize = ((int)packet[i++]) * 256;
    bufSize += packet[i++];
    memcpy(buf, &packet[i], bufSize);
    return bufSize;
}

// returns the length of the packet
// determins how many bytes (octets) is needed to represent the file
// storying them from least to most significant 0001 0001 , 0001 0011 -> 0001 0011,  0001 0001
// storing filename type (1), len (how many letters), then the byte for each letter in each following index)
int build_Control_Packet(unsigned char C_value, const char *filename, long int file_len, unsigned char *packet)
{

    int packet_len = 5 + strlen(filename) + ceil(log2f((float)file_len) / 8.0);

    packet[0] = C_value;

    packet[1] = 0x00;                             // Type
    packet[2] = ceil(log2f((float)file_len) / 8); // Length
    printf("File Length: %ld\n", file_len);
    printf(" File Length len: %d\n", packet[2]);

    for (int j = 0; j < packet[2]; j++)
    {
        // printf(" Logic  0x%lx ",0xFF & file_len);
        packet[2 - j + packet[2]] = 0xFF & file_len;
        file_len = file_len >> 8;
    } // Content
    int i = 3 + packet[2];

    packet[i++] = 0x01;
    packet[i++] = strlen(filename);
    memcpy(&packet[i], filename, strlen((char *)filename));
    printf("Filename: %s\n", filename);

    return packet_len;
}

int get_Control_Packet_Info(unsigned char *packet, unsigned char *C_value, char *filename, long int *file_len)
{
    *C_value = packet[0];
    *file_len = 0;
    int i = 2;
    int file_len_len = packet[i++];
    printf("File Length Length: %d\n\n\n", file_len_len);

    for (int j = 0; j < file_len_len; j++)
    {
        // printf(" Logic  0x%f ",packet[2+file_len_len-j]*pow(256,j));
        *file_len += packet[2 + file_len_len - j] * pow(256, j);
    }
    printf("File Length: %ld\n", *file_len);

    i += file_len_len;
    i++;
    int filename_len = packet[i++];
    memcpy(filename, &packet[i], filename_len);
    filename[filename_len] = '\0';
    i += filename_len + 1;

    return i;
}
// applicationLayer(const char *serialPort, const char *role, int baudRate,int nTries, int timeout, const char *filename)
// Setting up all parameters used (baudrate, ntries, timeout, role) and allocating memory two arrays: packet and buf
// opening connection

// if tx:
// opening file.
// determining file length
// setting up variables to find where the file starts and calculating lenght of file
// setting up a pointer in the file (fseek) which will help keeping track of the "not sent data"
// which will be used to know when the file has been fully sent
// building and sending control package
//(in a while loop) using max_payload size to segment file into datapackages
// then closing the file and the connection

// if rx
// reading first byte if its not a (start) control package abort
// extract information about filename length of file
// creating new file in directory
//(while) llread > 0 : if its not a controlpackage extract info from get_data_info and write it to newly created file
// close file
void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{
    LinkLayer App_info;
    App_info.baudRate = baudRate;
    App_info.nRetransmissions = nTries;
    App_info.timeout = timeout;
    strcpy(App_info.serialPort, serialPort);
    App_info.role = strcmp(role, "rx") == 0 ? LlRx : LlTx;

    int fd = llopen(App_info);

    if (fd < 0)
    {
        printf("Error opening connection\n");
        return;
    }

    unsigned char *packet = (unsigned char *)malloc(sizeof(unsigned char) * MAX_PAYLOAD_SIZE);

    memset(packet, 0, MAX_PAYLOAD_SIZE);

    unsigned char *buf = (unsigned char *)malloc(sizeof(unsigned char) * MAX_PAYLOAD_SIZE);

    if (App_info.role == LlTx)
    {

        /*
        int packet_len=build_Control_Packet(0x02,filename,1000,packet);

        printf("Control Packet Length: %d\n",packet_len);
        for(int i=0;i<packet_len;i++){
            printf("0x%x ",packet[i]);
        }
        printf("\n");


        llwrite(packet,packet_len);

        unsigned char buf[MAX_PAYLOAD_SIZE];

        buf[0]=0x01;
        buf[1]=0x02;
        buf[2]=0x03;
        buf[3]=0x04;
        buf[4]=0x05;



        packet_len=build_Data_Packet(buf,5,packet);
        printf("Data Packet Length: %d\n",packet_len);
        for(int i=0;i<packet_len;i++){
            printf("0x%x ",packet[i]);
        }

        llwrite(packet,packet_len);

        llclose(1);*/

        //-----------------------------
        long int len;
        system("ls");

        FILE *file = fopen(filename, "rb+");
        if (file == NULL)
        {
            printf("Error opening file\n");
            return;
        }
        printf("Filename: %s\n", filename);

        int begin = ftell(file);

        fseek(file, 0, SEEK_END);

        len = ftell(file);

        fseek(file, begin, SEEK_SET);
        long int bytes_left = len;

        printf("File Length: %ld\n", bytes_left);

        int packet_len = build_Control_Packet(0x02, filename, len, packet);
        llwrite(packet, packet_len);
        printf("Control Packet Length: %d\n", packet_len);

        int written;
        while (bytes_left > 0)
        {
            memset(buf, 0, MAX_PAYLOAD_SIZE);
            if (bytes_left < (MAX_PAYLOAD_SIZE - 3))
            {
                fread(buf, 1, bytes_left, file);
                printf("bytes_left: %ld\n", bytes_left);

                packet_len = build_Data_Packet(buf, bytes_left, packet);
                printf("Data Packet Length: %d\n", packet_len);

                if ((written = llwrite(packet, packet_len)) < 0)
                    printf("ERROR on WRITE");

                printf("Written: %d\n", written);
                bytes_left = 0;
                printf("bytes_left: %ld\n", bytes_left);
            }
            else
            {
                fread(buf, 1, MAX_PAYLOAD_SIZE - 3, file);

                packet_len = build_Data_Packet(buf, (MAX_PAYLOAD_SIZE - 3), packet);

                printf("Data Packet Length: %d\n", packet_len);
                for (int i = 0; i < packet_len; i++)
                {
                    printf("0x%x ", packet[i]);
                }
                printf("\n");

                if ((written = llwrite(packet, packet_len)) < 0)
                    printf("ERROR on WRITE");
                printf("Written: %d\n", written);

                bytes_left -= (MAX_PAYLOAD_SIZE - 3);
                printf("bytes_left: %ld\n", bytes_left);
            }
        }
        fclose(file);
        llclose(1);

        //-----------------------------
        // if rx
        // reading first byte if its not a (start) control package abort
        // extract information about filename length of file
        // creating new file in directory
        //(while) llread > 0 : if its not a controlpackage extract info from get_packet_info and write it to newly created file
        // close file
    }
    else
    {
        unsigned char packet[MAX_PAYLOAD_SIZE];
        memset(packet, 0, MAX_PAYLOAD_SIZE);

        llread(packet);

        if (packet[0] == 0x02)
        {
            printf("Control Packet\n");
        }
        else
        {
            printf("Not a Control Packet\n");
            return;
        }

        unsigned char C_fag;
        char name[256];
        long int file_len;

        get_Control_Packet_Info(packet, &C_fag, name, &file_len);

        memset(packet, 0, MAX_PAYLOAD_SIZE);

        char *path = (char *)malloc(50);

        strcat(path, "new/");
        strcat(path, filename);

        printf("path:%s  \n", path);

        FILE *new_file = fopen(path, "wb+");
        int packet_len;
        int data_len;

        unsigned char buf[MAX_PAYLOAD_SIZE];
        memset(buf, 0, MAX_PAYLOAD_SIZE);
        int readed;

        // if its a controlpackage (cflag 2 or 3) jump to the next iteration
        while ((readed = packet_len = llread(packet)) > 0)
        {
            printf("\n\n readed: %d\n\n", readed);
            if (packet[0] == 0x03)
                continue;
            else if (packet[0] == 0x02)
                continue;

            data_len = get_Data_Packet_Info(packet, buf);
            printf("data len : %d\n", data_len);
            fwrite(buf, sizeof(unsigned char), data_len, new_file);
            memset(packet, 0, MAX_PAYLOAD_SIZE);
        }
        fclose(new_file);

        /*
        int packet_len=llread(packet);
        printf("1 ------- Packet Length: %d\n",packet_len);
        if(packet[0]==0x02){
            printf("Control Packet\n");
        }else{
            printf("Not a Control Packet\n");
        }
        for(int i=0;i<packet_len;i++){
            printf("0x%x ",packet[i]);
        }
        printf("\n");


        unsigned char C_fag;
        char name[200];
        long int file_len;

        get_Control_Packet_Info(packet,&C_fag,name,&file_len);

        printf("c_value 0x%x\n",C_fag);
        printf("name: %s \n",name);
        printf("file len: %ld \n",file_len);




        packet_len=llread(packet);
        packet[packet_len]='\0';
        printf("\n\n\n2 ------ Control Packet Length: %d\n\n\n",packet_len);
        for(int i=0;i<packet_len;i++){
            printf("0x%x ",packet[i]);
        }
        printf("\n");


        printf("what\n");

        unsigned char buff[MAX_PAYLOAD_SIZE];

         packet_len=get_Data_Packet_Info(packet,buff);

        buff[packet_len]='\0';
        for(int i=0;i<packet_len;i++){
            printf("0x%x ",buff[i]);
        }
        printf("what packet_len %d\n",packet_len);


        llread(packet);*/
    }
}
