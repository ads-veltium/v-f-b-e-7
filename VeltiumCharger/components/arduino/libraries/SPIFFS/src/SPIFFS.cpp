#include "vfs_api.h"

extern "C" {
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_spiffs.h"
}

#include "SPIFFS.h"

using namespace fs;

class SPIFFSImpl : public VFSImpl
{
public:
    SPIFFSImpl();
    virtual ~SPIFFSImpl() { }
    virtual bool exists(const char* path);
};

SPIFFSImpl::SPIFFSImpl()
{
}

bool SPIFFSImpl::exists(const char* path)
{
    File f = open(path, "r");
    return (f == true) && !f.isDirectory();
}

SPIFFSFS::SPIFFSFS() : FS(FSImplPtr(new SPIFFSImpl())), partitionLabel_(NULL)
{

}

SPIFFSFS::~SPIFFSFS()
{
    if (partitionLabel_){
        free(partitionLabel_);
        partitionLabel_ = NULL;
    }
}

bool SPIFFSFS::begin(bool formatOnFail, const char * basePath, uint8_t maxOpenFiles, const char * partitionLabel)
{
    if (partitionLabel_){
        free(partitionLabel_);
        partitionLabel_ = NULL;
    }

    if (partitionLabel){
        partitionLabel_ = strdup(partitionLabel);
    }

    if(esp_spiffs_mounted(partitionLabel_)){
        log_w("SPIFFS Already Mounted!");
        return true;
    }

    esp_vfs_spiffs_conf_t conf = {
      .base_path = basePath,
      .partition_label = partitionLabel_,
      .max_files = maxOpenFiles,
      .format_if_mount_failed = false
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if(err == ESP_FAIL && formatOnFail){
        if(format()){
            err = esp_vfs_spiffs_register(&conf);
        }
    }
    if(err != ESP_OK){
        log_e("Mounting SPIFFS failed! Error: %d", err);
        return false;
    }
    _impl->mountpoint(basePath);
    return true;
}

void SPIFFSFS::end()
{
    if(esp_spiffs_mounted(partitionLabel_)){
        esp_err_t err = esp_vfs_spiffs_unregister(partitionLabel_);
        if(err){
            log_e("Unmounting SPIFFS failed! Error: %d", err);
            return;
        }
        _impl->mountpoint(NULL);
    }
}

bool SPIFFSFS::format()
{
    disableCore0WDT();
    esp_err_t err = esp_spiffs_format(partitionLabel_);
    enableCore0WDT();
    if(err){
        log_e("Formatting SPIFFS failed! Error: %d", err);
        return false;
    }
    return true;
}

size_t SPIFFSFS::totalBytes()
{
    size_t total,used;
    if(esp_spiffs_info(partitionLabel_, &total, &used)){
        return 0;
    }
    return total;
}

size_t SPIFFSFS::usedBytes()
{
    size_t total,used;
    if(esp_spiffs_info(partitionLabel_, &total, &used)){
        return 0;
    }
    return used;
}

SPIFFSFS SPIFFS;
