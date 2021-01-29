/*
*   library for storing and reading plants from/to FAT partition
*   plant is saved as follows:
*   1 plant = 1 pile
*   filename = [plant.address].txt (i.E.: 55.txt)
*   Content = [plantToSave.address],[plantToSave.name],[plantToSave.waterAmount],[plantToSave.fertilizerAmount],[plantToSave.threshold],[plantToSave.autoWatering]
*   
*/

#include <stdio.h>

#ifndef FATSTORAGE_H
#define FATSTORAGE_H


/**
*   Mount fat partition
*   Get all saved plants and load the into ram
*/
void initFATStorage();


/**
*   Safe new plant to FAT partition OR Change existing plant, if it exists
*   @param plantToSave - copy of plant object, that has to be saved
*/
void savePlantToStorage(plant plantToSave);

plant getPlantFromStorage(uint8_t index, char * name, bool useIndex);

/**
*   remove plant from FAT partition
*   @param plantToRemove - copy of plant object, that should be removed
*/
void removePlantFromStorage(plant plantToRemove);

#endif