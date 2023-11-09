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

#include <time.h>


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

//tester frame
unsigned char frame_test[20000];

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

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
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
void fun_sleep(){
       struct timespec sleep_time, remaining_time;
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 50000000;  // 10 milliseconds

    if (nanosleep(&sleep_time, &remaining_time) == -1) {
        perror("nanosleep");
    }
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
    unsigned char ret='\0';

    while(alarmTrigger!=TRUE && state!=STOP_RCV){
        if(read(fd,&byte,1)==1){
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

    //fun_sleep();
    int res = write(fd,buf,5);

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
    //srand(time(0));
    alarmCount=0;
    connectionParameters_global = connectionParameters;

    fd = connect(connectionParameters);

    if (fd < 0) {
        printf("Error connecting to serial port");
        return -1;
        }
    if(connectionParameters.role == LlTx){

        (void)signal(SIGALRM, alarmHandler);
        
        while(connectionParameters.nRetransmissions!=alarmCount){
            

            sendSupFrame(A_RECEIVER,C_SET);
            alarmTrigger = FALSE;
            alarm(connectionParameters.timeout);
            state = START;
            unsigned char byte='\0';
            while(!alarmTrigger){ 
                if(read(fd,&byte,1)==1){
                    switch(state){
                        case START:

                            if(byte == FLAG){

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
                            if(byte == (A_SENDER^C_UA)){

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


    alarmCount=0;
    int cur_transmissions=0;
    int accepted = 0;

    while(cur_transmissions<connectionParameters_global.nRetransmissions){

        cur_transmissions++;


        alarmTrigger = FALSE;
        alarm(connectionParameters_global.timeout);

        

        while(alarmTrigger == FALSE && accepted!=1){

            //memcpy(frame_test,frame,bufSize + 6 + extra_buffing_bytes);
            //if(rand()%100 < 10){
            //    frame_test[10]=0x99;
            //}
            //printf("sending frame: 0x%x  %d\n",frame_test[7],frame_number);
            //fun_sleep();
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
                printf("rejected: 0x%x  %d\n",frame_test[7],frame_number);
                accepted=-1;
                
            }else if(result== C_RR0 && frame_number==1){
                accepted=1;
                //printf("accepted: 0x%x  %d\n",frame_test[7],frame_number);
                frame_number=(1+frame_number)%2;
                free(frame);
                return (bufSize +6 +extra_buffing_bytes); 
            }else if(result== C_RR1 && frame_number==0){
                accepted=1;
                //printf("accepted: 0x%x  %d\n",frame_test[7],frame_number);
                frame_number=(1+frame_number)%2;
                free(frame);
                return (bufSize +6 +extra_buffing_bytes); 
            }else if((result== C_RR1 && frame_number==1) || (result== C_RR0 && frame_number==0)){
                printf("resending: 0x%x  %d\n",frame_test[7],frame_number);
                accepted=-1;
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




    while(I_state!=STOP_I){    
        if(read(fd,&byte,1)>0) {
            switch(I_state){
                case START_I:

                    if(byte == FLAG){

                        I_state = FLAG_I_RCV;
                    }
                    break;
                case FLAG_I_RCV:

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
                    if(byte == C_Info0 && frame_number==1){
                        I_state = START_I;
                        sendSupFrame(A_SENDER,C_RR1);

                    }
                    else if(byte == C_Info1 && frame_number==0){
                        I_state = START_I;
                        sendSupFrame(A_SENDER,C_RR0);
                    }else if(byte == C_Info0 || byte == C_Info1){

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

                            sendSupFrame(A_SENDER,frame_number==0?C_RR1:C_RR0);
                            frame_number=(1+frame_number)%2;
                            //printf("Frame accepted value : 0x%x  %d\n",packet[3],frame_number);
                            return i;
                        }else {

                            sendSupFrame(A_SENDER,frame_number==0?C_REJ0:C_REJ1);
                            printf("Frame rejected: 0x%x  %d\n",packet[3],frame_number);
                            i=0;
                            I_state=FLAG_I_RCV;
                        }


                    
                    }else if(i>=MAX_PAYLOAD_SIZE){

                        bcc2 =byte;
                    }else{
                        packet[i++]=byte;

                    }
                    break;
                case ESC_on_DATA:

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
                        printf("Frame rejected on esc\n");
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

                    if(byte == FLAG){
                        if(field_C==C_DISC){
                            

                            alarmCount=0;
                            
                            (void)signal(SIGALRM, alarmHandler);
                            state=START;
                            while(connectionParameters_global.nRetransmissions!=alarmCount && state!=STOP_RCV ){
                                

                                sendSupFrame(A_SENDER,C_DISC);
                                alarmTrigger = FALSE;
                                alarm(connectionParameters_global.timeout);
                                state = START;
                                unsigned char byte='\0';
                                while(state!=STOP_RCV && !alarmTrigger){ 
                                    if(read(fd,&byte,1)>0){
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
                                        }}
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

    alarmCount=0;
    (void)signal(SIGALRM, alarmHandler);
    state=START;    
    while(connectionParameters_global.nRetransmissions!=alarmCount && state!=STOP_RCV ){
        

        sendSupFrame(A_RECEIVER,C_DISC);
        alarmTrigger = FALSE;
        alarm(connectionParameters_global.timeout);
        state = START;
        unsigned char byte='\0';
        while(state!=STOP_RCV && !alarmTrigger){ 
            if(read(fd,&byte,1)>0){
                switch(state){
                    case START:
                        if(byte == FLAG){

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
                }}
            byte='\0';
        }
    }

    sendSupFrame(A_RECEIVER,C_UA);
    printf("Connection closed \n");
    if(state==STOP_RCV) return disconnect();
    return -1;
}


