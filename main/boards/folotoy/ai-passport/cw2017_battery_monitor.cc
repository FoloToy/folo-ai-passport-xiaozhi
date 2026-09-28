#include "cw2017_battery_monitor.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "Cw2017Battery"

namespace {

// Register map and 520 mAh CellWise profile from the AI Passport BSP.
constexpr uint8_t kRegVersion = 0x00;
constexpr uint8_t kRegVcellH = 0x02;
constexpr uint8_t kRegSocH = 0x04;
constexpr uint8_t kRegConfig = 0x08;
constexpr uint8_t kRegSocAlert = 0x0B;
constexpr uint8_t kRegProfile = 0x10;
constexpr uint8_t kConfigActive = 0x00;
constexpr uint8_t kConfigRestart = 0x30;
constexpr uint8_t kConfigSleep = 0xF0;
constexpr uint8_t kUpdateFlag = 0x80;
constexpr size_t kProfileSize = 80;

constexpr uint8_t kBatteryProfile[kProfileSize] = {
    0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAD, 0xC7, 0xC8, 0xCA, 0xBD, 0xB1, 0xC1, 0x94,
    0x88, 0xD1, 0xBD, 0x97, 0x88, 0x66, 0x56, 0x4A,
    0x3F, 0x33, 0x26, 0x5C, 0x37, 0xD1, 0x27, 0xD8,
    0xCC, 0xB7, 0xCF, 0xB3, 0xB2, 0xAE, 0xA6, 0x9E,
    0x99, 0x97, 0x9B, 0x86, 0x47, 0x1E, 0x17, 0x26,
    0x49, 0x96, 0xD9, 0xE1, 0xDD, 0xDC, 0xD4, 0x59,
    0x00, 0x00, 0x90, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C,
};

static_assert(sizeof(kBatteryProfile) == kProfileSize);

}  // namespace

Cw2017BatteryMonitor::Cw2017BatteryMonitor(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : i2c_bus_(i2c_bus), i2c_device_(nullptr), device_address_(addr) {
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        // CW2017 is a slow 100 kHz part on the Passport shared bus.
        .scl_speed_hz = 100 * 1000,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };
    if (i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device_) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register CW2017 device 0x%02X", addr);
        i2c_device_ = nullptr;
    }
}

Cw2017BatteryMonitor::~Cw2017BatteryMonitor() {
    if (i2c_device_) {
        i2c_master_bus_rm_device(i2c_device_);
        i2c_device_ = nullptr;
    }
}

int Cw2017BatteryMonitor::ReadReg(uint8_t reg, uint8_t* data, size_t length) {
    if (!i2c_device_) {
        return -1;
    }
    if (i2c_master_transmit_receive(i2c_device_, &reg, 1, data, length, 100) != ESP_OK) {
        return -1;
    }
    return 0;
}

int Cw2017BatteryMonitor::WriteReg(uint8_t reg, uint8_t value) {
    if (!i2c_device_) {
        return -1;
    }
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(i2c_device_, data, sizeof(data), 100) == ESP_OK ? 0 : -1;
}

int Cw2017BatteryMonitor::ReadReg16(uint8_t reg, uint16_t* value) {
    uint8_t data[2] = {0, 0};
    if (ReadReg(reg, data, sizeof(data)) != 0) {
        return -1;
    }
    *value = ((uint16_t)data[0] << 8) | data[1];
    return 0;
}

int Cw2017BatteryMonitor::EnterSleep() {
    if (WriteReg(kRegConfig, kConfigRestart) != 0) {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    if (WriteReg(kRegConfig, kConfigSleep) != 0) {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return 0;
}

int Cw2017BatteryMonitor::EnterActive() {
    if (WriteReg(kRegConfig, kConfigRestart) != 0) {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    if (WriteReg(kRegConfig, kConfigActive) != 0) {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return 0;
}

int Cw2017BatteryMonitor::ProfileMatches(bool* matches) {
    uint8_t value = 0;
    *matches = false;
    if (ReadReg(kRegSocAlert, &value, 1) != 0) {
        return -1;
    }
    if ((value & kUpdateFlag) == 0) {
        return 0;
    }
    for (size_t i = 0; i < kProfileSize; ++i) {
        if (ReadReg(static_cast<uint8_t>(kRegProfile + i), &value, 1) != 0) {
            return -1;
        }
        if (value != kBatteryProfile[i]) {
            return 0;
        }
    }
    *matches = true;
    return 0;
}

int Cw2017BatteryMonitor::UpdateProfile() {
    if (EnterSleep() != 0) {
        return -1;
    }
    for (size_t i = 0; i < kProfileSize; ++i) {
        if (WriteReg(static_cast<uint8_t>(kRegProfile + i), kBatteryProfile[i]) != 0) {
            ESP_LOGE(TAG, "CW2017 profile write failed at index %u", static_cast<unsigned>(i));
            return -1;
        }
    }
    uint8_t value = 0;
    for (size_t i = 0; i < kProfileSize; ++i) {
        if (ReadReg(static_cast<uint8_t>(kRegProfile + i), &value, 1) != 0 ||
            value != kBatteryProfile[i]) {
            ESP_LOGE(TAG, "CW2017 profile verification failed at index %u",
                     static_cast<unsigned>(i));
            return -1;
        }
    }
    if (ReadReg(kRegSocAlert, &value, 1) != 0 || WriteReg(kRegSocAlert, value | kUpdateFlag) != 0) {
        return -1;
    }
    return EnterActive();
}

int Cw2017BatteryMonitor::WaitForSocReady() {
    for (int retry = 0; retry < 50; ++retry) {
        uint8_t soc = 0;
        vTaskDelay(pdMS_TO_TICKS(100));
        if (ReadReg(kRegSocH, &soc, 1) == 0 && soc <= 100) {
            return 0;
        }
    }
    return -1;
}

bool Cw2017BatteryMonitor::Initialize() {
    if (present_) {
        return true;
    }
    if (!i2c_device_) {
        return false;
    }

    uint8_t version = 0;
    if (ReadReg(kRegVersion, &version, 1) != 0) {
        ESP_LOGW(TAG, "CW2017 did not answer at 0x%02X - battery disabled", device_address_);
        return false;
    }
    ESP_LOGI(TAG, "CW2017 found (version 0x%02X)", version);

    bool profile_matches = false;
    if (ProfileMatches(&profile_matches) != 0) {
        ESP_LOGW(TAG, "CW2017 profile read failed - battery disabled");
        return false;
    }
    if (!profile_matches) {
        ESP_LOGI(TAG, "Writing AI Passport 520mAh battery profile");
        if (UpdateProfile() != 0) {
            ESP_LOGW(TAG, "CW2017 profile update failed - battery disabled");
            return false;
        }
    } else {
        uint8_t config = 0;
        if (ReadReg(kRegConfig, &config, 1) != 0 ||
            (config != kConfigActive && EnterActive() != 0)) {
            ESP_LOGW(TAG, "CW2017 wake failed - battery disabled");
            return false;
        }
        ESP_LOGI(TAG, "AI Passport 520mAh battery profile matched");
    }

    if (WaitForSocReady() != 0) {
        ESP_LOGW(TAG, "CW2017 SOC not ready - battery disabled");
        return false;
    }

    present_ = true;
    return true;
}

int Cw2017BatteryMonitor::GetBatteryLevel() {
    uint16_t soc;
    if (!present_ || ReadReg16(kRegSocH, &soc) != 0) {
        return -1;
    }
    int percent = soc >> 8;  // high byte is the integer percentage
    if (percent > 100) {
        // Chip not ready yet may report 0xFF.
        return -1;
    }
    return percent;
}

int Cw2017BatteryMonitor::GetBatteryVoltageMv() {
    uint16_t raw;
    if (!present_ || ReadReg16(kRegVcellH, &raw) != 0) {
        return -1;
    }
    raw &= 0x3FFF;  // 14-bit voltage
    return (int)(((uint32_t)raw * 3125) / 10000);  // raw * 312.5 uV -> mV
}
