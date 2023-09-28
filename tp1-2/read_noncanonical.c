// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

volatile int STOP = FALSE;

enum frame_state {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP_RCV}; 

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

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
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

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

    printf("New termios structure set\n");

    // Loop for input
    unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char

    enum frame_state State = START;

    int bytes;
    while (STOP == FALSE)
    {
        switch(State){

            case(START):{
                // Returns after 5 chars have been input
                bytes = read(fd, buf, BUF_SIZE);
                buf[bytes] = '\0'; 
                if (buf[0] == 0x7E){
                    State = FLAG_RCV;
                }

                break;
            }
            case(FLAG_RCV):{

                printf("flag_rcv buf[1]: %02X\n", buf[1]);
                if (buf[1] == 0x03)
                    State = A_RCV;
                else if (buf[1] == 0x7E)
                    State = FLAG_RCV;
                else
                    State = START;
                break;
            }
            case(A_RCV):{
                printf("A_rcv buf[2]: %02X\n", buf[2]);  
                if (buf[2] == 0x03)
                    State = C_RCV;
                else if (buf[2] == 0x7E)
                    State = FLAG_RCV;
                else
                    State = START;
                break;
            }
            case(C_RCV):{
                printf("C_rcv buf[3]: %02X\n", buf[3]);
                if (buf[3] == 0x00)
                    State = BCC_OK;
                else if (buf[3] == 0x7E)
                    State = FLAG_RCV;
                else
                    State = START;
                break;
            
            }
            case(BCC_OK):{
                printf("BCC_OK buf[4]: %02X\n", buf[4]);
                if (buf[4] == 0x7E)
                    State = STOP_RCV;
                else
                    State = START;
                break;
            }
            case(STOP_RCV):{
                STOP = TRUE;
                printf("contents: 0x%02X %02X %02X %02X %02X :%d\n", buf[0],buf[1],buf[2],buf[3],buf[4], bytes);
                break;
                }

        };
        
                    

    }

    buf[0]='\0';


    buf[0] = 0x7E;
    buf[1] = 0x03;
    buf[2] = 0x07;
    buf[3] = 0x06;
    buf[4] = 0x7E;

    bytes = write(fd, buf, 5);
    printf("%d bytes written\n", bytes);

    // Wait until all bytes have been written to the serial port
    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide
    sleep(1);
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}