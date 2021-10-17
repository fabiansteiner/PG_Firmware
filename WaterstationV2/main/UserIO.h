
#ifndef USERIO_H
#define USERIO_H

#define SUBJECT_WATERING 10
#define SUBJECT_CONNECTED 20

void initializeUserIO();

void changeUserIOState(uint8_t subject, bool state);

void switchButtonDetection(bool on);

#endif