#pragma once

#include "esp_err.h"

#define SD_BASE_PATH "/sd"

esp_err_t odroid_sdcard_open();
esp_err_t odroid_sdcard_close();
size_t odroid_sdcard_get_filesize(const char* path);
size_t odroid_sdcard_copy_file_to_memory(const char* path, void* buf, size_t buf_size);
size_t odroid_sdcard_unzip_file_to_memory(const char* path, void* buf, size_t buf_size);
int odroid_sdcard_mkdir(char *dir);

const char* odroid_sdcard_get_filename(const char* path);
const char* odroid_sdcard_get_extension(const char* path);