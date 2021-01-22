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
    plant initPlant = {66, "dude", 300, 0, 0, 50, STATUS_OK, UNKNOWN, 0, {0}};
    plant initPlant1 = {55, "yoyo", 400, 5, 0, 40, STATUS_OK, UNKNOWN, 0, {0}};
    //savePlantToStorage(initPlant);
    //savePlantToStorage(initPlant1);

    //getPlantFromStorage(66);
    //getPlantFromStorage(55);
    //getPlantFromStorage(77);

    FF_DIR dir;
    FILINFO fileInformation;
    f_opendir(&dir, "/");
    f_readdir(&dir, &fileInformation);
    while(fileInformation.fsize!=0){
        ESP_LOGI(TAG, "Filename: %s, fileSize: %u, ", fileInformation.fname, fileInformation.fsize);
        getPlantFromStorage(0, fileInformation.fname ,false);
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
    ESP_LOGI(TAG, "Reading file");
    f = fopen(filename, "rb");

    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return p;
    }
    char line[128];
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

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