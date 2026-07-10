#include <stdio.h>
#include "esp_log.h"
#include "esp_ieee802154.h"
#include "ieee802154.h"
#include "protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/message_buffer.h"
#include <esp_adc/adc_continuous.h>

static const char *TAG = "DEVICE";


// ADC Configuration
#define ADC_SAMPLE_RATE     20 * 1000 // 20kHz minimum for continuous mode
#define ADC_READ_LEN        256
#define JOYSTICK_X_CHANNEL  ADC_CHANNEL_0
#define JOYSTICK_Y_CHANNEL  ADC_CHANNEL_1

// Structure to hold our 50Hz data payload


static data_t data = { 0 };
static adc_continuous_handle_t adc_handle = NULL;



//Functions required by the esp_ieee802154 component
void esp_ieee802154_receive_done(uint8_t* frame, esp_ieee802154_frame_info_t* frame_info) {}
// void esp_ieee802154_receive_failed(uint16_t error) {}
// void esp_ieee802154_receive_sfd_done() {}
// void esp_ieee802154_transmit_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_frame_info) {}
// void esp_ieee802154_transmit_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error) {}
// void esp_ieee802154_transmit_sfd_done(uint8_t *frame) {}


void esp_ieee802154_receive_failed(uint16_t error) { ESP_EARLY_LOGI(TAG, "rx failed, error %d", error); }

void esp_ieee802154_receive_sfd_done(void) { ESP_EARLY_LOGI(TAG, "rx sfd done, Radio state: %d", esp_ieee802154_get_state()); }

void esp_ieee802154_transmit_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_frame_info) { ESP_EARLY_LOGI(TAG, "tx OK, sent %d bytes, ack %d", frame[0], ack != NULL); }

void esp_ieee802154_transmit_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error) { ESP_EARLY_LOGI(TAG, "tx failed, error %d", error); }

void esp_ieee802154_transmit_sfd_done(uint8_t *frame) { ESP_EARLY_LOGI(TAG, "tx sfd done"); esp_ieee802154_receive_handle_done(frame); }

// Forward Declarations
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
    uint8_t result_buffer[ADC_READ_LEN * SOC_ADC_DIGI_RESULT_BYTES] = {0};
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
                uint32_t adc_chan = p->type2.channel;
                uint32_t adc_data = p->type2.data;

                if (adc_chan == JOYSTICK_X_CHANNEL) {
                    sum_x += adc_data;
                    count_x++;
                } else if (adc_chan == JOYSTICK_Y_CHANNEL) {
                    sum_y += adc_data;
                    count_y++;
                }
            }

            // Average the data collected in this window to reduce noise
            if (count_x > 0) data.x = sum_x / count_x;
            if (count_y > 0) data.y = sum_y / count_y;

			data.timestamp = esp_log_timestamp();

			ESP_LOGI(TAG, "X: %d, Y: %d", data.x, data.y);
			send_esp(0xFFFF, (uint8_t *)&data, sizeof(data));
		}


        // Strict 50Hz execution timing
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

void app_main(void) {
    // 1. Initialize ADC
    init_adc();

    // 2. Initialize ieee802.154
	ieee802154_init(false);

    xTaskCreate(joystick_sampler_task, "joystick_task", 4096, NULL, 5, NULL);
}

