// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_
#include <math.h>
// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);

//builds the data packet return the length of the packet
int build_Data_Packet(unsigned char *buf,int bufSize,unsigned char *packet);          

//extracts the data packet info from the packet return the lenght of the data
int get_Data_Packet_Info(unsigned char *packet,unsigned char *buf);

//builds the control packet return the length of the packet
int build_Control_Packet(unsigned char C_value,const char *filename,long int file_len ,unsigned char *packet);

//extracts the control packet info from the packet return the lenght of the packet
int get_Control_Packet_Info(unsigned char *packet,unsigned char *C_value,char *filename,long int *file_len);

#endif // _APPLICATION_LAYER_H_
