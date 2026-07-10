#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_adc/adc_continuous.h>
#include "hal/uart_ll.h"
#include "soc/uart_struct.h"

#define TAG "JOYSTICK_BLE"

// ADC Configuration
#define ADC_SAMPLE_RATE     20 * 1000 // 20kHz minimum for continuous mode
#define ADC_READ_LEN        256
#define JOYSTICK_X_CHANNEL  ADC_CHANNEL_4 // GPIO32 on ESP32
#define JOYSTICK_Y_CHANNEL  ADC_CHANNEL_5 // GPIO39 on ESP32

#define USB_UART_PORT UART_NUM_0

// Structure to hold our 50Hz data payload
typedef struct __attribute__((packed)) {
    uint16_t x;
    uint16_t y;
} joystick_data_t;

static joystick_data_t joystick_data = {0, 0};
static adc_continuous_handle_t adc_handle = NULL;





// Select your target hardware port (UART_NUM_0, UART_NUM_1, etc.)


void uart_write_raw_no_overhead(uint8_t *data, size_t len) {
    // Get a direct pointer to the hardware registers for this UART port
    uart_dev_t *hw = UART_LL_GET_HW(USB_UART_PORT);


	while (uart_ll_get_txfifo_len(hw) == 0) {
		// Spin-lock loop: strict hardware poll with zero software overhead
	}
	// Shove the byte directly into the FIFO register
	uart_ll_write_txfifo(hw, data, len);
    
}







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

	uint8_t data[11] = {'@', 1, 'R', 'x', 'x', 'y', 'y', 'e', 'n', 'd', '\n'};

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
        
			data[3] = joystick_data.x % 256;
			data[4] = joystick_data.x / 256;
			data[5] = joystick_data.y % 256;
			data[6] = joystick_data.y / 256;

			uart_write_raw_no_overhead(data, sizeof(data));
		}


        // Strict 50Hz execution timing
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

void app_main(void) {
    // 1. Initialize Storage and ADC
    init_adc();

    // 2. Initialize NimBLE
    xTaskCreate(joystick_sampler_task, "joystick_task", 4096, NULL, 5, NULL);
}