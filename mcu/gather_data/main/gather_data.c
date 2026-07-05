#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_adc/adc_continuous.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#define TAG "JOYSTICK_BLE"

// ADC Configuration
#define ADC_SAMPLE_RATE     20 * 1000 // 20kHz minimum for continuous mode
#define ADC_READ_LEN        256
#define JOYSTICK_X_CHANNEL  ADC_CHANNEL_4 // GPIO32 on ESP32
#define JOYSTICK_Y_CHANNEL  ADC_CHANNEL_5 // GPIO39 on ESP32

// BLE Configuration
#define DEVICE_NAME         "ESP32_Joystick"
static uint16_t joystick_chr_val_handle;

// Structure to hold our 50Hz data payload
typedef struct __attribute__((packed)) {
    uint16_t x;
    uint16_t y;
} joystick_data_t;

static joystick_data_t joystick_data = {0, 0};
static adc_continuous_handle_t adc_handle = NULL;
static uint8_t ble_addr_type;

// Forward Declarations
static int ble_gatt_svr_chr_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

// BLE GATT Server Definition
// Service: 12345678-1234-5678-1234-567812345678
// Characteristic: 87654321-4321-8765-4321-876543210987
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(0x78, 0x56, 0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0x56, 0x78),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID128_DECLARE(0x87, 0x09, 0x21, 0x43, 0x21, 0x87, 0x65, 0x43, 0x21, 0x43, 0x65, 0x87, 0x21, 0x43, 0x65, 0x87),
                .access_cb = ble_gatt_svr_chr_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &joystick_chr_val_handle,
            },
            {0} // No more characteristics
        },
    },
    {0} // No more services
};

// GATT Characteristic Callback
static int ble_gatt_svr_chr_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (attr_handle == joystick_chr_val_handle) {
        os_mbuf_append(ctxt->om, &joystick_data, sizeof(joystick_data));
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// Initialize ADC Continuous Driver
static void init_adc(void) {
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = ADC_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = ADC_SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1, // Using ADC1
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    adc_digi_pattern_config_t pattern[2] = {
        {
            .atten = ADC_ATTEN_DB_12, // For 0-3.3V range
            .channel = JOYSTICK_X_CHANNEL,
            .unit = ADC_UNIT_1,
            .bit_width = ADC_BITWIDTH_12,
        },
        {
            .atten = ADC_ATTEN_DB_12,
            .channel = JOYSTICK_Y_CHANNEL,
            .unit = ADC_UNIT_1,
            .bit_width = ADC_BITWIDTH_12,
        }
    };

    dig_cfg.pattern_num = 2;
    dig_cfg.adc_pattern = pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

// 50Hz Processing Task
static void joystick_sampler_task(void *pvParameters) {
    uint8_t result_buffer[ADC_READ_LEN] = {0};
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(20); // 20ms = 50Hz

    while (1) {
        uint32_t ret_num = 0;
        uint32_t sum_x = 0, count_x = 0;
        uint32_t sum_y = 0, count_y = 0;

        // Read available samples from driver buffer
        esp_err_t ret = adc_continuous_read(adc_handle, result_buffer, ADC_READ_LEN, &ret_num, 0);
        if (ret == ESP_OK || ret == ESP_ERR_TIMEOUT) {
            for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result_buffer[i];
                uint32_t chan = p->type1.channel;
                uint32_t data = p->type1.data;

                if (chan == JOYSTICK_X_CHANNEL) {
                    sum_x += data;
                    count_x++;
                } else if (chan == JOYSTICK_Y_CHANNEL) {
                    sum_y += data;
                    count_y++;
                }
            }

            // Average the data collected in this window to reduce noise
            if (count_x > 0) joystick_data.x = sum_x / count_x;
            if (count_y > 0) joystick_data.y = sum_y / count_y;

            // Notify connected BLE clients
            struct os_mbuf *om = ble_hs_mbuf_from_flat(&joystick_data, sizeof(joystick_data));
            if (om) {
                ble_gatts_notify_custom(BLE_HS_CONN_HANDLE_NONE, joystick_chr_val_handle, om);
            }
        }

        // Strict 50Hz execution timing
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

// BLE Advertising Function
static void ble_app_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
}

static void ble_app_on_sync(void) {
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_app_advertise();
}

static void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void) {
    // 1. Initialize Storage and ADC
    init_adc();

    // 2. Initialize NimBLE
    ESP_ERROR_CHECK(nimble_port_init());
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    // 3. Start Tasks
    nimble_port_freertos_init(ble_host_task);
    xTaskCreate(joystick_sampler_task, "joystick_task", 4096, NULL, 5, NULL);
}