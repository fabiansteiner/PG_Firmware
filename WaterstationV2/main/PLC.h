#include <stdio.h>

#define LOCKCOMMAND 10
#define READSTATUS 20 //+Unlock
#define OPENVALVE 30
#define CLOSEVALVE 50
#define RECEIVEDVALVEADDRESS 80

#define ACK 'A'
#define NACK 'N'
#define RES 'R'

void initializePLCTask();