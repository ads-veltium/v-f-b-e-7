#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := VBLE2

include $(IDF_PATH)/make/project.mk

#cargar archivos al spiffs
SPIFFS_IMAGE_FLASH_IN_PROJECT := 1
#$(eval $(call spiffs_create_partition_image,spiffs,data))
$(eval $(call spiffs_create_partition_image,WebServer,data/WebServer))
#$(eval $(call spiffs_create_partition_image,PSOC5,data/PSOC5))
#$(eval $(call spiffs_create_partition_image,ESP32,data))
#$(eval $(call spiffs_create_partition_image,CONFIG,data/config))

