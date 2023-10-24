// Link layer protocol implementation

#include "link_layer.h"

//imports
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


//alarm variables
int alarmTrigger = FALSE;
int alarmCount = 0;
int timeout=0;

//port config
struct termios oldtio;
struct termios newtio;

//port number
int fd;

//serial portName
const char *serialPortName;

LinkLayer connectionParameters_global;

//frame state
enum frame_Sup_state {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP_RCV}; 
enum frame_Sup_state state;

enum frame_I_state  {START_I, FLAG_I_RCV, A_I_RCV, C_I_RCV,READING_DATA,STOP_I,ESC_on_DATA,LOST_SUP_FRAMES_BBC_OK,LOST_SUP_FRAMES_C_RCV}; 


//frame number
unsigned char frame_number = 0;



//Alarm Handler
void alarmHandler(int signal){
    alarmTrigger= TRUE;
    alarmCount++;
    printf("Alarm trying #%d\n", alarmCount);
    }


//connect to serial port
int connect(LinkLayer connectionParameters){
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    //checking if port is open
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 0 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    
    return fd;
} 

int disconnect(){
    // Close serial port
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }
    close(fd);
    return 0;
}

unsigned char Read_Frame_control(int fd){
    state = START;
    unsigned char byte='\0';
    unsigned char ret;

    while(alarmTrigger!=TRUE && state!=STOP_RCV){
        if(read(fd,&byte,1)==1)
            switch(state){
                case START:
                    if(byte == FLAG){
                        ret=0x00;
                        state = FLAG_RCV;
                    }
                    break;
                case FLAG_RCV:
                    if(byte == A_SENDER){
                        state = A_RCV;
                    }
                    else if(byte == FLAG){
                        state = FLAG_RCV;
                    }else{
                        state = START;
                    }
                    break;
                case A_RCV:
                    if(byte == C_RR0 || byte == C_RR1 || byte == C_REJ0 || byte == C_REJ1){
                        state = C_RCV;
                        ret = byte;
                    }
                    else if(byte == FLAG){
                        state = FLAG_RCV;
                    }
                    else{
                        state = START;
                    }
                    break;
                case C_RCV:
                    if(byte == (A_SENDER^ret)){
                        state = BCC_OK;
                    }
                    else if(byte == FLAG){
                        state = FLAG_RCV;
                    }
                    else{
                        state = START;
                    }
                    break;
                case BCC_OK:
                    if(byte == FLAG){
                        state = STOP_RCV;
                    }
                    else{
                        state = START;
                    }
                    break;
                case STOP_RCV:
                    break;
            }
    }
    return ret;
}


//send supervisory frame
int sendSupFrame(unsigned char A,unsigned char C){
    unsigned char buf[5]={0};
    buf[0] = FLAG;
    buf[1] = A;
    buf[2] = C;
    buf[3] = A^C;
    buf[4] = FLAG;
    //printf("Sending frame on port  %d\n",fd);
    int res = write(fd,buf,5);
    //printf(" send sup frame   Sent %d bytes\n",res);
    if(res<0){
        perror("Error writing to serial port");
        return -1;
    }   
    return 0;    

}   

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{   
    alarmCount=0;
    connectionParameters_global = connectionParameters;

    fd = connect(connectionParameters);
    //printf("llopen with fd: %d\n",fd);
    if (fd < 0) {
        printf("Error connecting to serial port");
        return -1;
        }
    if(connectionParameters.role == LlTx){

        (void)signal(SIGALRM, alarmHandler);
        
        while(connectionParameters.nRetransmissions!=alarmCount){
            
            //printf("Sending SET\n\n\n\n");
            sendSupFrame(A_RECEIVER,C_SET);
            alarmTrigger = FALSE;
            alarm(connectionParameters.timeout);
            state = START;
            unsigned char byte='\0';
            while(!alarmTrigger){ 
                if(read(fd,&byte,1)==1)
                    switch(state){
                        case START:
                        //printf("START: 0x%x\n",byte);
                            if(byte == FLAG){
                            //    printf("FLAG\n");
                                state = FLAG_RCV;
                            }
                            break;
                        case FLAG_RCV:
                            if(byte == A_SENDER){
                            //    printf("A_SENDER\n");
                                state = A_RCV;
                            }
                            else if(byte == FLAG){
                                state = FLAG_RCV;
                            }else{
                                state = START;
                            }
                            break;
                        case A_RCV:
                            if(byte == C_UA){
                            //    printf("C_UA\n");
                                state = C_RCV;
                            }
                            else if(byte == FLAG){

                                state = FLAG_RCV;
                            }
                            else{
                                state = START;
                            }
                            break;
                        case C_RCV:
                            if(byte == (A_SENDER^C_UA)){
                            //    printf("BCC_OK\n");
                                state = BCC_OK;
                            }
                            else if(byte == FLAG){
                                state = FLAG_RCV;
                            }
                            else{
                                state = START;
                            }
                            break;
                        case BCC_OK:
                            if(byte == FLAG){
                            //    printf("STOP_RCV\n");
                                state = STOP_RCV;
                                return fd;
                            }
                            else{
                                state = START;
                            }
                            break;
                        case STOP_RCV:{
                            return fd;
                            }

                        }
                    }
            }

        printf("timout rx\n");
        return -1;

    }else if(connectionParameters.role == LlRx){
        state = START;
        unsigned char byte;

        while(state!=STOP_RCV){
            if(read(fd,&byte,1)>0){
                switch(state){
                    case START:
                        if(byte == FLAG){
                            //printf("FLAG\n");
                            state = FLAG_RCV;
                        }
                        break;
                    case FLAG_RCV:
                        //printf("FLAG_RCV 0x%x\n",byte);
                        if(byte == A_RECEIVER){
                            
                            state = A_RCV;
                        }
                        else if(byte == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                        break;
                    case A_RCV:
                        //printf("A_RCV 0x%x\n",byte);
                        if(byte == C_SET){
                            state = C_RCV;
                        }
                        else if(byte == FLAG){
                            state = FLAG_RCV;
                        }
                        else{
                            state = START;
                        }
                        break;
                    case C_RCV:
                        //printf("C_RCV 0x%x\n",byte);
                        if(byte == (A_RECEIVER^C_SET)){
                            state = BCC_OK;
                        }
                        else if(byte == FLAG){
                            state = FLAG_RCV;
                        }
                        else{
                            state = START;
                        }
                        break;
                    case BCC_OK:
                        //printf("BCC_OK 0x%x\n",byte);
                        if(byte == FLAG){
                            state = STOP_RCV;
                        }
                        else{
                            state = START;
                        }
                        break;
                    case STOP_RCV:
                        break;
                }
            }
 
        }
        
        sendSupFrame(A_SENDER,C_UA);

    }else {
        printf("\nerror\n");
        return -1;
        }

    printf("Connection established\n");
    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{   
    //printf("llwrite\n");
    unsigned char *frame=malloc(bufSize+6);

    unsigned char frame_number_aux=frame_number==0?C_Info0:C_Info1;
    frame[0]=FLAG;
    frame[1]=A_RECEIVER;
    frame[2]=frame_number_aux;
    frame[3]=A_RECEIVER^frame_number_aux;

    unsigned char bcc2=buf[0];

    u_int32_t extra_buffing_bytes=0;

    for(int i=1;i<bufSize;i++){
        bcc2^=buf[i];
    }

    int j=4;
    for(int i=0;i<bufSize;i++){
        if(buf[i]==FLAG){

            extra_buffing_bytes++;
            
            frame=realloc(frame,bufSize + extra_buffing_bytes +6);
            
            frame[i + j ]=0x7D;

            j++;

            frame[i+j]=0x5E;


        }else if(buf[i]==0x7D){

            extra_buffing_bytes++;

            frame=realloc(frame,bufSize + extra_buffing_bytes +6);

            
            frame[i + j ]=0x7D;
            j++;
            frame[i + j ]=0x5D;
        }else{
            frame[i+j]=buf[i];
        }
    }
    if(bcc2==0x7E){
            extra_buffing_bytes++;
            
            frame=realloc(frame,bufSize + extra_buffing_bytes +6);
            
            frame[bufSize+4+ extra_buffing_bytes-1]=0x7D;


            frame[bufSize+4+ extra_buffing_bytes]=0x5E;

    }else if(bcc2==0x7D){

            extra_buffing_bytes++;

            frame=realloc(frame,bufSize + extra_buffing_bytes +6);

            
            frame[bufSize +4+ extra_buffing_bytes - 1 ]=0x7D;
            j++;
            frame[bufSize +4+ extra_buffing_bytes ]=0x5D;
        }else{
            frame[bufSize+4+ extra_buffing_bytes]=bcc2;
        }

    frame[bufSize+5+ extra_buffing_bytes]=FLAG;
    //printf("Frame:      ");
    //for(int i=0;i<bufSize+6+extra_buffing_bytes;i++){
        //printf("0x%x ",frame[i]);
    //}
    //printf("\n");

    alarmCount=0;
    int cur_transmissions=0;
    int accepted = 0;

    while(cur_transmissions<connectionParameters_global.nRetransmissions){

        cur_transmissions++;


        alarmTrigger = FALSE;
        alarm(connectionParameters_global.timeout);

        

        while(alarmTrigger == FALSE && accepted!=1){
            //printf("Sending frame\n");
            int res = write(fd, frame, bufSize + 6 + extra_buffing_bytes);

            if(res<0){
                perror("Error writing to serial port");
                return -1;
            }

            unsigned char result=Read_Frame_control(fd); 


            if(alarmTrigger==TRUE){
                printf("Timeout\n");
                continue;
            }else if(result== C_REJ0 || result == C_REJ1){
                printf("Frame rejected\n");
                accepted=-1;
                
            }else if(result== C_RR0 || result == C_RR1){
                //printf("Frame accepted\n");
                accepted=1;
                frame_number=(1+frame_number)%2;
            }else continue;
        }
        if(accepted==1){
            break;
        }

    
    }

    free(frame);
    if(accepted==1){
        return (bufSize +6 +extra_buffing_bytes); 
    }else{
        return -1;
    }

}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    enum frame_I_state I_state= START_I;
    unsigned char byte='\0';
    unsigned char field_C;

    unsigned char bcc2;

    unsigned char bcc2_check;

    int i=0;


    
    //printf("llread \n\n");

    while(I_state!=STOP_I){    
        if(read(fd,&byte,1)>0) 
            switch(I_state){
                case START_I:
                    //printf("START_I 0x%x\n",byte);
                    if(byte == FLAG){
                        //printf("FLAG\n");
                        I_state = FLAG_I_RCV;
                    }
                    break;
                case FLAG_I_RCV:
                //printf("FLAG_I_RCV 0x%x\n",byte);
                    if(byte == A_RECEIVER){
                        I_state = A_I_RCV;
                    }
                    else if(byte == FLAG){
                        I_state = FLAG_I_RCV;
                    }else{
                        I_state = START_I;
                    }
                    break;
                case A_I_RCV:
                    //printf("A_I_RCV 0x%x\n",byte);
                    if(byte == C_Info0 || byte == C_Info1){
                        //printf("Info0\n");
                        I_state = C_I_RCV;
                        field_C=byte;
                    }else if(byte == FLAG){
                        I_state = FLAG_I_RCV;
                    }else if(byte == C_DISC || byte == C_SET){
                        I_state = LOST_SUP_FRAMES_C_RCV;
                        field_C=byte;
                    }   
                    else{
                        I_state = START_I;
                    }
                    break;
                case C_I_RCV:
                    //printf("C_I_RCV 0x%x\n",byte);
                    if(byte == (A_RECEIVER^field_C)){
                        I_state = READING_DATA;
                    }
                    else if(byte == FLAG){
                        I_state = FLAG_I_RCV;
                    }
                    else{
                        I_state = START_I;
                    }
                    break;
            

                case READING_DATA:
                    if (byte==0x7D){
                        I_state=ESC_on_DATA;
                    }
                    else if(byte==FLAG ){
                        if(i>=MAX_PAYLOAD_SIZE) {
                            //printf("max size\n");

                        }else{
                            
                            i--;
                            bcc2=packet[i];
                        }
                        packet[i]='\0';

                        bcc2_check=packet[0];
                        for(int j=1;j<i;j++){
                            bcc2_check^=packet[j];
                        }

                        if(bcc2_check==bcc2){
                            //printf("BCC2 OK\n");
                            sendSupFrame(A_SENDER,frame_number==0?C_RR0:C_RR1);
                            frame_number=(1+frame_number)%2;
                            return i;
                        }else {
                            //printf("BCC2 error %d\n",i);
                            //printf("BCC2 error 0x%x ::: 0x%x\n",bcc2_check,bcc2);
                            sendSupFrame(A_SENDER,frame_number==0?C_REJ0:C_REJ1);
                            i=0;
                            I_state=FLAG_I_RCV;
                        }


                    
                    }else if(i>=MAX_PAYLOAD_SIZE){
                        //printf("max --size\n");
                        bcc2 =byte;
                    }else{
                        packet[i++]=byte;

                    }
                    break;
                case ESC_on_DATA:
                    //printf("ESC on DATA 0x%x\n",byte);
                    if(i>=MAX_PAYLOAD_SIZE){
                        if(byte==0x5E){
                            bcc2=0x7E;
                            I_state=READING_DATA;
                        }else if(byte==0x5D){
                            bcc2=0x7D;
                            I_state=READING_DATA;
                        }else{
                            I_state=START_I;
                            sendSupFrame(A_SENDER,frame_number==0?C_REJ0:C_REJ1);
                            i=0;
                        }

                    }else if(byte==0x5E){
                        packet[i++]=0x7E;
                        I_state=READING_DATA;
                    }else if(byte==0x5D){
                        packet[i++]=0x7D;
                        I_state=READING_DATA;
                    }else{
                        I_state=START_I;
                        sendSupFrame(A_SENDER,frame_number==0?C_REJ0:C_REJ1);
                        i=0;
                    }
                    
                    
                break;
                case LOST_SUP_FRAMES_C_RCV:
                    if(byte == (field_C^A_RECEIVER)){
                        I_state = LOST_SUP_FRAMES_BBC_OK;
                    }
                    else if(byte == FLAG){
                        I_state = FLAG_RCV;
                    }
                    else{
                        I_state = START;
                    }

                
                break;

                case LOST_SUP_FRAMES_BBC_OK:
                    //printf("LOST_SUP_FRAMES_BBC_OK 0x%x\n",byte);
                    if(byte == FLAG){
                        if(field_C==C_DISC){
                            
                            //sending Disc back and waiting for response
                            alarmCount=0;
                            
                            (void)signal(SIGALRM, alarmHandler);
                            state=START;
                            while(connectionParameters_global.nRetransmissions!=alarmCount && state!=STOP_RCV ){
                                
                                //printf("Sending DISC\n");
                                sendSupFrame(A_SENDER,C_DISC);
                                alarmTrigger = FALSE;
                                alarm(connectionParameters_global.timeout);
                                state = START;
                                unsigned char byte='\0';
                                while(state!=STOP_RCV && !alarmTrigger){ 
                                    if(read(fd,&byte,1)>0)
                                        switch(state){
                                            case START:
                                                if(byte == FLAG){
                                                    state = FLAG_RCV;
                                                }
                                                break;
                                            case FLAG_RCV:
                                                if(byte == A_RECEIVER){
                                                    state = A_RCV;
                                                }
                                                else if(byte == FLAG){
                                                    state = FLAG_RCV;
                                                }else{
                                                    state = START;
                                                }
                                                break;
                                            case A_RCV:
                                                if(byte == C_UA){
                                                    state = C_RCV;
                                                }
                                                else if(byte == FLAG){
                                                    state = FLAG_RCV;
                                                }
                                                else{
                                                    state = START;
                                                }
                                                break;
                                            case C_RCV:
                                                if(byte == (A_RECEIVER^C_UA)){
                                                    state = BCC_OK;
                                                }
                                                else if(byte == FLAG){
                                                    state = FLAG_RCV;
                                                }
                                                else{
                                                    state = START;
                                                }
                                                break;
                                            case BCC_OK:
                                                if(byte == FLAG){
                                                    state = STOP_RCV;
                                                }
                                                else{
                                                    state = START;
                                                }
                                                break;
                                            case STOP_RCV:
                                                break;
                                        }
                                    byte='\0';
                                }
                            }
                            printf("Connection closed\n");
                        if(state==STOP_RCV) return disconnect();

                        I_state = START_I;
                    }
                    else if(field_C==C_SET){
                        sendSupFrame(A_SENDER,C_UA);
                        I_state = START;
                    }
                    else{
                        I_state = START;
                    }

                }
                else{
                    I_state = START;
                }
                break;

                case STOP_I:
                    break;
                
                

            }
        byte='\0';
    }    

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{   
    //printf("llclose\n\n\n\n");
    alarmCount=0;
    (void)signal(SIGALRM, alarmHandler);
    state=START;    
    while(connectionParameters_global.nRetransmissions!=alarmCount && state!=STOP_RCV ){
        
        //printf("Sending DISC\n");
        sendSupFrame(A_RECEIVER,C_DISC);
        alarmTrigger = FALSE;
        alarm(connectionParameters_global.timeout);
        state = START;
        unsigned char byte='\0';
        while(state!=STOP_RCV && !alarmTrigger){ 
            if(read(fd,&byte,1)>0)
                switch(state){
                    case START:
                        if(byte == FLAG){
                        //    printf("FLAG\n");
                            state = FLAG_RCV;
                        }
                        break;
                    case FLAG_RCV:
                        //printf("FLAG_RCV 0x%x\n",byte);
                        if(byte == A_SENDER){
                            state = A_RCV;
                        }
                        else if(byte == FLAG){
                            state = FLAG_RCV;
                        }else{
                            state = START;
                        }
                        break;
                    case A_RCV:
                        //printf("A_RCV 0x%x\n",byte);
                        if(byte == C_DISC){
                            state = C_RCV;
                        }
                        else if(byte == FLAG){
                            state = FLAG_RCV;
                        }
                        else{
                            state = START;
                        }
                        break;
                    case C_RCV:
                        //printf("C_RCV 0x%x\n",byte);
                        if(byte == (A_SENDER^C_DISC)){
                            state = BCC_OK;
                        }
                        else if(byte == FLAG){
                            state = FLAG_RCV;
                        }
                        else{
                            state = START;
                        }
                        break;
                    case BCC_OK:
                        if(byte == FLAG){
                            state = STOP_RCV;
                        }
                        else{
                            state = START;
                        }
                        break;
                    case STOP_RCV:
                        break;
                }
            byte='\0';
        }
    }
    //printf("Sending UA\n");
    sendSupFrame(A_RECEIVER,C_UA);
    printf("Connection closed %d\n",state);
    if(state==STOP_RCV) return disconnect();
    return -1;
}


