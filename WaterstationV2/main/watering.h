#include <stdio.h>

#include "PLC.h"

#ifndef WATERING_H
#define WATERING_H

void initializeWateringComponents(bool test);

void startWateringTask(wateringJob job);


#endif