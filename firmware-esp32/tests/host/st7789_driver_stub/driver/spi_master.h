#ifndef QW_ST7789_DRIVER_STUB_SPI_MASTER_H
#define QW_ST7789_DRIVER_STUB_SPI_MASTER_H

#include <stddef.h>

#include "esp_err.h"

typedef int spi_host_device_t;
typedef void *spi_device_handle_t;

#define SPI2_HOST 2
#define SPI_DMA_CH_AUTO 1

typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int max_transfer_sz;
} spi_bus_config_t;

typedef struct {
    int clock_speed_hz;
    int mode;
    int spics_io_num;
    int queue_size;
} spi_device_interface_config_t;

esp_err_t spi_bus_initialize(spi_host_device_t host,
                              const spi_bus_config_t *config, int dma_chan);
esp_err_t spi_bus_add_device(spi_host_device_t host,
                              const spi_device_interface_config_t *config,
                              spi_device_handle_t *device);
esp_err_t spi_bus_remove_device(spi_device_handle_t device);
esp_err_t spi_bus_free(spi_host_device_t host);

#endif
