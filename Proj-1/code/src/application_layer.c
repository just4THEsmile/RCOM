// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <math.h>
#include<unistd.h>

#include <sys/time.h>   // for gettimeofday()






int build_Data_Packet(unsigned char *buf,int bufSize,unsigned char *packet){
    packet[0]=0x01;
    packet[1]=0x00;
    packet[2]=0x00;
    packet[1]=ceil(bufSize/(256));
    packet[2]=bufSize%(256);
    memcpy(&packet[3],buf,bufSize);
    return bufSize+3;
}

int get_Data_Packet_Info(unsigned char *packet,unsigned char *buf){
    int i=1;
    int bufSize=((int)packet[i++])*256;
    bufSize+=packet[i++];
    memcpy(buf,&packet[i],bufSize);
    return bufSize;
} 



//returns the length of the packet
int build_Control_Packet(unsigned char C_value,const char *filename,long int file_len ,unsigned char *packet){
    

    int packet_len=5+strlen(filename) + ceil(log2f((float)file_len)/8.0);

    packet[0]=C_value;


    packet[1]=0x00;                                                             //Type
    packet[2]= (unsigned char) ceil((double) log2f((float) file_len )/8 );                                  //Length

    
    for(int j = 0;j<packet[2];j++){

        packet[2-j+packet[2]]=0xFF & file_len;
        file_len=file_len>>8;
    }                                                                           //Content
    int i=3+packet[2];

    packet[i++]=0x01;
    packet[i++]=strlen(filename);
    memcpy(&packet[i],filename,strlen((char*)filename));




    
    return packet_len;

}

int get_Control_Packet_Info(unsigned char *packet,unsigned char *C_value,char *filename,long int *file_len){
    *C_value=packet[0];
    *file_len=0;
    int i=2;
    int file_len_len=packet[i++];


    for(int j = 0;j<file_len_len;j++){

        *file_len+=packet[2+file_len_len-j]*pow(256,j);
    }
    printf("File Length: %ld\n",*file_len);

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

    printf("llopen status: %d\n",fd);

    if(fd<0){
        printf("Error opening connection\n");
        return;
    }

    unsigned char* packet = (unsigned char*) malloc(sizeof(unsigned char)*MAX_PAYLOAD_SIZE);

    memset(packet,0,MAX_PAYLOAD_SIZE);

    unsigned char* buf = (unsigned char*) malloc(sizeof(unsigned char)*MAX_PAYLOAD_SIZE);



    struct timeval t1, t2;
    double elapsedTime;

    // start timer
    gettimeofday(&t1, NULL);

    if(App_info.role==LlTx){




        long int len;
        long int bytes_left;
        FILE *file = fopen(filename, "rb");
        if(file==NULL){
            printf("Error opening file\n");
            return;
        }
        printf("Filename: %s\n",filename);

        int begin = ftell(file);

        fseek(file, 0, SEEK_END);

        len = ftell(file);

        fseek(file, begin, SEEK_SET);
        bytes_left=len;


        

        int packet_len=build_Control_Packet(0x02,filename,len,packet);
        llwrite(packet,packet_len);
        //printf("Control Packet Length: %d\n",packet_len);

        int written;



        while(bytes_left>0){   
            memset(buf,0,MAX_PAYLOAD_SIZE);
            if(bytes_left<(MAX_PAYLOAD_SIZE-3)){
                fread(buf, 1, bytes_left, file);


                packet_len=build_Data_Packet(buf, bytes_left, packet);


                if((written =llwrite(packet,packet_len))<0) printf("ERROR on WRITE");


                bytes_left=0;

            }else{
                fread(buf, 1, MAX_PAYLOAD_SIZE-3, file);

                packet_len=build_Data_Packet(buf, (MAX_PAYLOAD_SIZE-3), packet);



                
                if((written =llwrite(packet,packet_len))<0) printf("ERROR on WRITE");
                //printf("Written: %d\n",written);

                bytes_left-=(MAX_PAYLOAD_SIZE-3);

            }

        }
        packet_len=build_Control_Packet(0x03,filename,len,packet);
        llwrite(packet,packet_len);

        fclose(file);
        llclose(1);

    //-----------------------------

    }else{


        unsigned char packet[MAX_PAYLOAD_SIZE];
        memset(packet,0,MAX_PAYLOAD_SIZE);

        if(llread(packet)==0) return;

        if(packet[0]==0x02){
            //printf("Control Packet\n");
        }else{
            printf("Error\n");
            return;
        }

        unsigned char C_fag;
        char name[256];
        long int file_len;

        get_Control_Packet_Info(packet,&C_fag,name,&file_len);


        memset(packet,0,MAX_PAYLOAD_SIZE);



        FILE* new_file = fopen(filename,"wb");
        int packet_len;
        int data_len;

        unsigned char buf[MAX_PAYLOAD_SIZE];
        memset(buf,0,MAX_PAYLOAD_SIZE);




        while((packet_len=llread(packet))>0){

            if(packet[0]==0x03) {
                //printf("controll end \n");
                continue;
                }
            else if(packet[0]==0x02) {
               // printf("controll start");
                continue;
                }
            data_len=get_Data_Packet_Info(packet,buf);

            fwrite(buf,sizeof(unsigned char),data_len,new_file);
            memset(packet,0,MAX_PAYLOAD_SIZE);

        }
        fclose(new_file);



    }
    free(buf);
    free(packet);

    // stop timer
    gettimeofday(&t2, NULL);

    // compute and print the elapsed time in millisec
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
    printf("\n\n\n%f ms.\n", elapsedTime);
}
