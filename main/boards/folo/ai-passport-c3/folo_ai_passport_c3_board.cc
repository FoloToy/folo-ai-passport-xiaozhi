#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "display/lcd_display.h"
#include "wifi_board.h"

#include <button_adc.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstdint>
#include <string>

#define TAG "FoloAiPassportC3"

namespace {

enum AdcButtonIndex {
    kVolumeUpButton,
    kVolumeDownButton,
    kConfirmButton,
    kAdcButtonCount,
};

struct St7789InitCommand {
    uint8_t command;
    uint8_t data[16];
    uint8_t data_length;
    uint16_t delay_ms;
};

// Panel-specific power, porch and gamma settings from the original badge firmware.
constexpr St7789InitCommand kSt7789P3InitCommands[] = {
    {0xB2, {0x05, 0x05, 0x00, 0x33, 0x33}, 5, 0},
    {0xB7, {0x35}, 1, 0},
    {0xBB, {0x21}, 1, 0},
    {0xC0, {0x2C}, 1, 0},
    {0xC2, {0x01}, 1, 0},
    {0xC3, {0x0B}, 1, 0},
    {0xC4, {0x20}, 1, 0},
    {0xC6, {0x0F}, 1, 0},
    {0xD0, {0xA7, 0xA1}, 2, 0},
    {0xD0, {0xA4, 0xA1}, 2, 0},
    {0xD6, {0xA1}, 1, 0},
    {0xE0,
     {0xD0, 0x04, 0x08, 0x0A, 0x09, 0x05, 0x2D, 0x43, 0x49, 0x09, 0x16, 0x15, 0x26, 0x2B},
     14,
     0},
    {0xE1,
     {0xD0, 0x03, 0x09, 0x0A, 0x0A, 0x06, 0x2E, 0x44, 0x40, 0x3A, 0x15, 0x15, 0x26, 0x2A},
     14,
     10},
};

}  // namespace

class FoloAiPassportC3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    adc_oneshot_unit_handle_t button_adc_handle_ = nullptr;
    AdcButton* adc_buttons_[kAdcButtonCount] = {};
    LcdDisplay* display_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        bus_config.miso_io_num = GPIO_NUM_NC;
        bus_config.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        bus_config.max_transfer_sz = DISPLAY_WIDTH * 80 * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));
    }

    void ChangeVolume(int delta) {
        auto codec = GetAudioCodec();
        int volume = std::clamp(codec->output_volume() + delta, 0, 100);
        codec->SetOutputVolume(volume);
        GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
    }

    void ToggleChatState() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateStarting) {
            EnterWifiConfigMode();
            return;
        }
        app.ToggleChatState();
    }

    void InitializeButtons() {
        const adc_oneshot_unit_init_cfg_t unit_config = {
            .unit_id = BUTTON_ADC_UNIT,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &button_adc_handle_));

        // 3.3 V -- 10 kOhm pull-up -- GPIO0 -- 0/1 kOhm/2.2 kOhm -- GND.
        // Voltage windows are copied from the validated original badge driver.
        const uint16_t voltage_windows[kAdcButtonCount][2] = {
            {0, 150},
            {150, 447},
            {447, 1900},
        };

        for (int index = 0; index < kAdcButtonCount; ++index) {
            button_adc_config_t adc_config = {};
            adc_config.adc_handle = &button_adc_handle_;
            adc_config.unit_id = BUTTON_ADC_UNIT;
            adc_config.adc_channel = BUTTON_ADC_CHANNEL;
            adc_config.button_index = index;
            adc_config.min = voltage_windows[index][0];
            adc_config.max = voltage_windows[index][1];
            adc_buttons_[index] = new AdcButton(adc_config);
        }

        adc_buttons_[kVolumeUpButton]->OnClick([]() {
            Application::GetInstance().Schedule([]() {
                static_cast<FoloAiPassportC3Board&>(Board::GetInstance()).ChangeVolume(10);
            });
        });
        adc_buttons_[kVolumeDownButton]->OnClick([]() {
            Application::GetInstance().Schedule([]() {
                static_cast<FoloAiPassportC3Board&>(Board::GetInstance()).ChangeVolume(-10);
            });
        });
        adc_buttons_[kConfirmButton]->OnClick([]() {
            Application::GetInstance().Schedule([]() {
                static_cast<FoloAiPassportC3Board&>(Board::GetInstance()).ToggleChatState();
            });
        });
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(
            esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(DISPLAY_SPI_HOST),
                                     &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RESET_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        for (const auto& init_command : kSt7789P3InitCommands) {
            ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(panel_io, init_command.command,
                                                      init_command.data,
                                                      init_command.data_length));
            if (init_command.delay_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(init_command.delay_ms));
            }
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR));
        ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y));

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    FoloAiPassportC3Board() {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(FoloAiPassportC3Board);
