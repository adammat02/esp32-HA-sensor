#include "sht31.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "sht31";

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t dev;

void sht31_init(void)
{
  i2c_master_bus_config_t bus_cfg = {
      .i2c_port = CONFIG_SHT31_I2C_PORT,
      .sda_io_num = CONFIG_SHT31_I2C_SDA_GPIO,
      .scl_io_num = CONFIG_SHT31_I2C_SCL_GPIO,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = false,
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

  i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = CONFIG_SHT31_I2C_ADDR,
    .scl_speed_hz = CONFIG_SHT31_I2C_FREQ_HZ,
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));
}

void sht31_measure(float *temp, float *hum)
{
  uint8_t cmd[2] = {0x2C, 0x06};
  uint8_t data[6];

  i2c_master_transmit_receive(dev, cmd, 2, data, 6, 100 /* ms timeout */);

  uint16_t s_t  = (data[0] << 8) | data[1];
  uint16_t s_rh = (data[3] << 8) | data[4];

  *temp = -45.0f + 175.0f * s_t  / 65535.0f;
  *hum  =  100.0f * s_rh / 65535.0f;

  ESP_LOGI(TAG, "temp=%.2f C, hum=%.2f %%", *temp, *hum);
}