#include <c_types.h>
#include "knopa_flash.h"

//#define SPI_FLASH_SEC_SIZE      4096

#define ESP_PARAM_START_SEC   0x3C

#define ESP_PARAM_SAVE_0    1
#define ESP_PARAM_SAVE_1    2
#define ESP_PARAM_FLAG      3

struct esp_platform_sec_flag_param {
    uint8 flag;
    uint8 pad[3];
};

/******************************************************************************
 * FunctionName : user_esp_platform_load_param
 * Description  : load parameter from flash, toggle use two sector by flag value.
 * Parameters   : param--the parame point which write the flash
 * Returns      : none
*******************************************************************************/
bool FLASH_CODE Storage_ReadParams (void *param, uint16 len)
{
/*
  struct esp_platform_sec_flag_param flag;

  spi_flash_read((ESP_PARAM_START_SEC + ESP_PARAM_FLAG) * SPI_FLASH_SEC_SIZE,
                 (uint32 *)&flag, sizeof(struct esp_platform_sec_flag_param));

  if (flag.flag == 0) {
    spi_flash_read((ESP_PARAM_START_SEC + ESP_PARAM_SAVE_0) * SPI_FLASH_SEC_SIZE,
                   (uint32 *)param, len);
  } else {
    spi_flash_read((ESP_PARAM_START_SEC + ESP_PARAM_SAVE_1) * SPI_FLASH_SEC_SIZE,
                   (uint32 *)param, len);
  }
*/
  return true;
}

/******************************************************************************
 * FunctionName : user_esp_platform_save_param
 * Description  : toggle save param to two sector by flag value,
 *              : protect write and erase data while power off.
 * Parameters   : param -- the parame point which write the flash
 * Returns      : none
*******************************************************************************/
bool FLASH_CODE Storage_WriteParams (void *param, uint16 len)
{
/*
  struct esp_platform_sec_flag_param flag;

  int res, nerr = 0;

  res = spi_flash_read((ESP_PARAM_START_SEC + ESP_PARAM_FLAG) * SPI_FLASH_SEC_SIZE,
                       (uint32 *)&flag, sizeof(struct esp_platform_sec_flag_param));
  if (res!=SPI_FLASH_RESULT_OK) nerr++;

  if (flag.flag == 0) {
    res = spi_flash_erase_sector(ESP_PARAM_START_SEC + ESP_PARAM_SAVE_1);
    if (res!=SPI_FLASH_RESULT_OK) nerr++;
    res = spi_flash_write((ESP_PARAM_START_SEC + ESP_PARAM_SAVE_1) * SPI_FLASH_SEC_SIZE,
                          (uint32 *)param, len);
    if (res!=SPI_FLASH_RESULT_OK) nerr++;

    flag.flag = 1;
    res = spi_flash_erase_sector(ESP_PARAM_START_SEC + ESP_PARAM_FLAG);
    if (res!=SPI_FLASH_RESULT_OK) nerr++;

    res = spi_flash_write((ESP_PARAM_START_SEC + ESP_PARAM_FLAG) * SPI_FLASH_SEC_SIZE,
                    (uint32 *)&flag, sizeof(struct esp_platform_sec_flag_param));
    if (res!=SPI_FLASH_RESULT_OK) nerr++;
  } else {
    res = spi_flash_erase_sector(ESP_PARAM_START_SEC + ESP_PARAM_SAVE_0);
    if (res!=SPI_FLASH_RESULT_OK) nerr++;

    res = spi_flash_write((ESP_PARAM_START_SEC + ESP_PARAM_SAVE_0) * SPI_FLASH_SEC_SIZE,
                    (uint32 *)param, len);
    if (res!=SPI_FLASH_RESULT_OK) nerr++;

    flag.flag = 0;
    res = spi_flash_erase_sector(ESP_PARAM_START_SEC + ESP_PARAM_FLAG);
    if (res!=SPI_FLASH_RESULT_OK) nerr++;

    res = spi_flash_write((ESP_PARAM_START_SEC + ESP_PARAM_FLAG) * SPI_FLASH_SEC_SIZE,
                    (uint32 *)&flag, sizeof(struct esp_platform_sec_flag_param));
    if (res!=SPI_FLASH_RESULT_OK) nerr++;
  }
  return nerr==0;
*/
  return true;
}
