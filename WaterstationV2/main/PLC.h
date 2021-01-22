#include <stdio.h>

#define LOCKCOMMAND 10
#define READSTATUS 20 //+Unlock
#define OPENVALVE 30
#define CLOSEVALVE 50
#define BROADCASTVALVEADDRESS 80

#define ACK 'A'
#define NACK 'N'
#define RES 'R'
#define NOANSWER 'E'

#define STATE_LOCKINGVALVES 0
#define STATE_OPENINGVALVE 1
#define STATE_WATERING 2
#define STATE_CLOSINGVAVLE 3
#define STATE_NOWATERING 4

typedef struct wateringJob{
    uint8_t plantAddress;
    uint16_t waterAmount;
    uint16_t fertilizerAmount;
}wateringJob;

void initializePLCTask();

void addWateringJob(uint8_t plantAddress, uint16_t waterAmount, uint8_t fertilizerAmount);