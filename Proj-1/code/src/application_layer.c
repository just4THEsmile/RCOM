// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>




int build_Data_Packet(unsigned char *buf,int bufSize,unsigned char *packet){
    packet[0]=0x01;
    packet[1]=0x0;
    packet[2]=0x0;
    packet[1]=ceil(bufSize/(8*256));
    packet[2]=bufSize%(8*256);
    memcpy(&packet[3],buf,bufSize);
    return bufSize+3;
}

int get_Data_Packet_Info(unsigned char *packet,unsigned char *buf){
    if(packet[0]!=0x01) return -1;
    int i=1;
    int bufSize=packet[i++]*256;
    bufSize+=packet[i++];
    memcpy(buf,&packet[i],bufSize);
    return bufSize;
} 



//returns the length of the packet
int build_Control_Packet(unsigned char C_value,const char *filename,long int file_len ,unsigned char *packet){
    

    int packet_len=5+strlen(filename) + ceil((double)(file_len/8.0));

    packet[0]=C_value;


    packet[1]=0x00;                                                     //Type
    packet[2]=ceil((double)(file_len/8.0));                             //Length
    printf("File Length: %ld\n",file_len);
    packet[3]=file_len;                                                 //Content
    int i=3+packet[2];

    packet[i++]=0x01;
    packet[i++]=strlen(filename);
    memcpy(&packet[i],filename,strlen((char*)filename));
    printf("Filename: %s\n",filename);



    
    return packet_len;

}

int get_Control_Packet_Info(unsigned char *packet,unsigned char *C_value,char *filename,long int *file_len){
    *C_value=packet[0];
    *file_len=0;
    int i=2;
    int file_len_len=packet[i++];
    printf("File Length Length: %d\n\n\n",file_len_len);
    memcpy(file_len,&packet[i],file_len_len);
    file_len[file_len_len]='\0';
    printf("Filename Length: %ld\n\n\n",*file_len);
    i+=file_len_len;
    i++;
    int filename_len=packet[i++];
    memcpy(filename,&packet[i],filename_len);
    filename[filename_len]='\0';
    i+=filename_len+1;

    return i;
}


void applicationLayer(const char *serialPort, const char *role, int baudRate,int nTries, int timeout, const char *filename)
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

        unsigned char packet[MAX_PAYLOAD_SIZE];
        packet[0]='\0';

        int packet_len=build_Control_Packet(0x02,filename,5,packet);
        
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
                fread(buf, 1, MAX_PAYLO        get_Control_Packet_Info(packet,)AD_SIZE, file);
                if(llwrite(buf, MAX_PAYLOAD_SIZE)<0) printf("ERROR on WRITE");
                bytes_left-=MAX_PAYLOAD_SIZE;
            }

        }
        fclose(file);
        llclose(1);
    
    */

    }else{


        unsigned char packet[MAX_PAYLOAD_SIZE];
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

        



        packet[0]='\0';

        unsigned char buff[MAX_PAYLOAD_SIZE];
        buff[0]='\0';
        get_Data_Packet_Info(packet,buff);

        llread(packet);
    }
}
