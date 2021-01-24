#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_log.h"


//Reihenfolge important
#include "variablepool.h"
#include "FAT_storage.h"

static const char *TAG = "FAT_Storage";
char filename[20] = {0};

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// Mount path for the partition
const char *base_path = "/spiflash";

FILE *f;

void deleteAllFiles();
plant getPlantFromStorage(uint8_t index, char * name, bool useIndex);

void initFATStorage(){
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    // To mount device we need name of device partition, define base_path
    // and allow format partition in case if it is new one and was not formated before
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "storage", &mount_config, &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }

    //deleteAllFiles();

    //Read all existing Files from FAT Storage - 1 File = 1 Plant
    FF_DIR dir;
    FILINFO fileInformation;
    f_opendir(&dir, "/");
    f_readdir(&dir, &fileInformation);
    while(strcmp(fileInformation.fname, "")!=0){
        ESP_LOGI(TAG, "Filename: %s, fileSize: %u, ", fileInformation.fname, fileInformation.fsize);
        plant storedPlant = getPlantFromStorage(0, fileInformation.fname ,false);
        if(storedPlant.address != NONEXISTINGADDRESS)
            changePlant(storedPlant, CHANGE_ADD);
        fileInformation.fsize = 0;
        f_readdir(&dir, &fileInformation);
    }
}

plant getPlantFromStorage(uint8_t index, char * name, bool useIndex){
    if(useIndex){
        sprintf(filename, "/spiflash/%u.txt", index);
    }else{
        sprintf(filename, "/spiflash/%s", name);
    }
    // Open file for reading
    plant p = {0};
    p.address = NONEXISTINGADDRESS;
    ESP_LOGI(TAG, "Reading file");
    f = fopen(filename, "rb");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return p;
    }
    char line[128];
    fgets(line, sizeof(line), f);
    fclose(f);

    ESP_LOGI(TAG, "Read from file: '%s'", line);

    //Extract Information from file and safe it into the plant struct
    int counter = 0;
    char * token = strtok(line, ",");
    while( token != NULL ) {
        ESP_LOGI(TAG, "%s", token);
        switch (counter)
        {
        case 0: p.address = atoi(token);
            break;
        case 1: strcpy(p.name, token);
            break;
        case 2: p.waterAmount = atoi(token);
            break;
        case 3: p.fertilizerAmount = atoi(token);
            break;
        case 4: p.threshold = atoi(token);
            break;
        case 5: p.autoWatering = atoi(token);
            break;
        }
        token = strtok(NULL, ",");
        counter++;
    }

    return p;
}



void savePlantToStorage(plant plantToSave){
    
    sprintf(filename, "/spiflash/%u.txt", plantToSave.address);

    ESP_LOGI(TAG, "Opening file");
    f = fopen(filename, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fprintf(f, "%u,%s,%i,%u,%u,%u", plantToSave.address, plantToSave.name, plantToSave.waterAmount, plantToSave.fertilizerAmount, plantToSave.threshold, plantToSave.autoWatering);
    fclose(f);
    ESP_LOGI(TAG, "File written");

}

void removePlantFromStorage(plant plantToRemove){
    sprintf(filename, "%u.txt", plantToRemove.address);
    int result = f_unlink(filename);
    
    if(result == FR_OK){
        ESP_LOGI(TAG, "Plant %u deleted", plantToRemove.address);
    }else{
        ESP_LOGI(TAG, "Could not delete plant, FAT ERROR CODE: %i", plantToRemove.address);
    }
    
}

void deleteAllFiles(){

    FF_DIR dir;
    FILINFO fileInformation;
    f_opendir(&dir, "/");
    f_readdir(&dir, &fileInformation);
    while(strcmp(fileInformation.fname, "")!=0){
        ESP_LOGI(TAG, "Filename: %s, fileSize: %u, ", fileInformation.fname, fileInformation.fsize);
        f_unlink(fileInformation.fname);
        fileInformation.fsize = 0;
        f_readdir(&dir, &fileInformation);
    }
    f_closedir(&dir);
}