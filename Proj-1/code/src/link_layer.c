// Link layer protocol implementation

#include "link_layer.h"

// imports
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

// defining flags
#define FLAG 0x7E
#define A_RECEIVER 0x03
#define A_SENDER 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define C_RR0 0x05
#define C_RR1 0x85
#define C_REJ0 0x01
#define C_REJ1 0x81
#define C_Info0 0x00
#define C_Info1 0x40

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

// alarm variables
int alarmTrigger = FALSE;
int alarmCount = 0;
int timeout = 0;

// port config
struct termios oldtio;
struct termios newtio;

// port number
int fd;

// serial portName
const char *serialPortName;

// frame state
enum frame_state
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP_RCV
};
enum frame_state state;

// Alarm Handler
void alarmHandler(int signal)
{
    alarmTrigger = TRUE;
    alarmCount++;
    printf("Alarm trying #%d\n", alarmCount);
}

// connect to serial port
int connect(LinkLayer connectionParameters)
{
    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    // checking if port is open
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

// send supervisory frame
int sendSupFrame(unsigned char A, unsigned char C)
{
    unsigned char buf[5] = {0};
    buf[0] = FLAG;
    buf[1] = A;
    buf[2] = C;
    buf[3] = A ^ C;
    buf[4] = FLAG;
    int res = write(fd, buf, 5);
    printf(" send sup frame   Sent %d bytes\n", res);
    if (res < 0)
    {
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

    int fd = connect(connectionParameters);
    printf("llopen with fd: %d\n", fd);
    if (fd < 0)
    {
        printf("Error connecting to serial port");
        return -1;
    }
    if (connectionParameters.role == LlTx)
    {

        (void)signal(SIGALRM, alarmHandler);

        while (connectionParameters.nRetransmissions != alarmCount && state != STOP_RCV)
        {

            printf("Sending SET\n");
            sendSupFrame(A_SENDER, C_SET);
            alarmTrigger = FALSE;
            alarm(connectionParameters.timeout);
            state = START;
            unsigned char byte = '\0';
            while (state != STOP_RCV && !alarmTrigger)
            {
                read(fd, &byte, 1);
                switch (state)
                {
                case START:
                    if (byte == FLAG)
                    {
                        state = FLAG_RCV;
                    }
                    break;
                case FLAG_RCV:
                    if (byte == A_RECEIVER)
                    {
                        state = A_RCV;
                    }
                    else if (byte == FLAG)
                    {
                        state = FLAG_RCV;
                    }
                    else
                    {
                        state = START;
                    }
                    break;
                case A_RCV:
                    if (byte == C_UA)
                    {
                        state = C_RCV;
                    }
                    else if (byte == FLAG)
                    {
                        state = FLAG_RCV;
                    }
                    else
                    {
                        state = START;
                    }
                    break;
                case C_RCV:
                    if (byte == (A_RECEIVER ^ C_UA))
                    {
                        state = BCC_OK;
                    }
                    else if (byte == FLAG)
                    {
                        state = FLAG_RCV;
                    }
                    else
                    {
                        state = START;
                    }
                    break;
                case BCC_OK:
                    if (byte == FLAG)
                    {
                        state = STOP_RCV;
                    }
                    else
                    {
                        state = START;
                    }
                    break;
                case STOP_RCV:
                    break;
                }
            }
        }
    }
    else if (connectionParameters.role == LlRx)
    {
        state = START;
        unsigned char byte;

        while (state != STOP_RCV)
        {
            read(fd, &byte, 1);
            switch (state)
            {
            case START:
                if (byte == FLAG)
                {
                    state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if (byte == A_SENDER)
                {
                    state = A_RCV;
                }
                else if (byte == FLAG)
                {
                    state = FLAG_RCV;
                }
                else
                {
                    state = START;
                }
                break;
            case A_RCV:
                if (byte == C_SET)
                {
                    state = C_RCV;
                }
                else if (byte == FLAG)
                {
                    state = FLAG_RCV;
                }
                else
                {
                    state = START;
                }
                break;
            case C_RCV:
                if (byte == (A_SENDER ^ C_SET))
                {
                    state = BCC_OK;
                }
                else if (byte == FLAG)
                {
                    state = FLAG_RCV;
                }
                else
                {
                    state = START;
                }
                break;
            case BCC_OK:
                if (byte == FLAG)
                {
                    state = STOP_RCV;
                }
                else
                {
                    state = START;
                }
                break;
            case STOP_RCV:
                break;
            }
        }
        sendSupFrame(A_RECEIVER, C_UA);
    }
    else
        return -1;

    return EXIT_SUCCESS;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // Function to close the connection
    int closeConnection(int showStatistics)
    {
        // Check if it's a transmitter or receiver
        if (connectionParameters_cpy.role == LlTx)
        {
            printf("Transmitter Closing...\n");
            unsigned char tx_cmd[5] = {FLAG, ADDR_ER, DISC, BCC(ADDR_ER, DISC), FLAG};

            if (sendAndWaitMessage(fd, tx_cmd, 5) < 0)
            {
                printf("Transmission of DISC command failed\n");
                return -1;
            }

            printf("Transmitter sending final UA to close\n");
            unsigned char ua_cmd[5] = {FLAG, ADDR_ER, UA, BCC(ADDR_ER, UA), FLAG};
            write(fd, ua_cmd, 5);
            sleep(1);
        }
    }
}
