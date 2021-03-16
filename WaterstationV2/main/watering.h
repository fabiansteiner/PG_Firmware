#include <stdio.h>
#include "PLC.h"

#ifndef WATERING_H
#define WATERING_H

#define STEPSPERROTATION 800        //Quater-Step Mode
#define MLFERTILIZERPERROTATION 0.242968f

void initializeWateringComponents(bool test);

void startWateringTask(wateringJob job);


#endif