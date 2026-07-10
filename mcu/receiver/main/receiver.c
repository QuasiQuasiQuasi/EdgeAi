#include <stdio.h>
#include "esp_log.h"
#include "esp_ieee802154.h"
#include "ieee802154.h"
#include "protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/message_buffer.h"
#include "hal/usb_serial_jtag_ll.h"

static const char *TAG = "DEVICE";
#define USB_UART_PORT UART_NUM_0

StreamBufferHandle_t xMessageBuffer = NULL;

void esp_ieee802154_receive_done(uint8_t* frame, esp_ieee802154_frame_info_t* frame_info)
{	
	ieee802154_packet_t packet = {0};

	// ESP_EARLY_LOGI(TAG, "rx OK, received %d bytes", frame[0]);
	xMessageBufferSendFromISR(xMessageBuffer, frame, frame[0], NULL);
	


	ESP_ERROR_CHECK(esp_ieee802154_receive());
	esp_ieee802154_receive_handle_done(frame);
}

void esp_ieee802154_receive_failed(uint16_t error) {}
void esp_ieee802154_receive_sfd_done() {}
void esp_ieee802154_transmit_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_frame_info) {}
void esp_ieee802154_transmit_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error) {}
void esp_ieee802154_transmit_sfd_done(uint8_t *frame) { esp_ieee802154_receive_handle_done(frame); }


// void esp_ieee802154_receive_failed(uint16_t error) { ESP_EARLY_LOGI(TAG, "rx failed, error %d", error); }

// void esp_ieee802154_receive_sfd_done(void) { ESP_EARLY_LOGI(TAG, "rx sfd done, Radio state: %d", esp_ieee802154_get_state()); }

// void esp_ieee802154_transmit_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_frame_info) { ESP_EARLY_LOGI(TAG, "tx OK, sent %d bytes, ack %d", frame[0], ack != NULL); }

// void esp_ieee802154_transmit_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error) { ESP_EARLY_LOGI(TAG, "tx failed, error %d", error); }

// void esp_ieee802154_transmit_sfd_done(uint8_t *frame) { ESP_EARLY_LOGI(TAG, "tx sfd done"); esp_ieee802154_receive_handle_done(frame); }

void uart_write_raw_no_overhead(uint8_t *data, size_t len) {
   
	// Spin-lock until the native USB hardware buffer is ready to accept data
	while (!usb_serial_jtag_ll_txfifo_writable()) {
		// Hardware poll
	}
	// Write directly to the USB Serial hardware buffer
	usb_serial_jtag_ll_write_txfifo(data, len);
	
	// Flush the USB buffer to send it over the wire immediately
	usb_serial_jtag_ll_txfifo_flush();
	
}





static void receive_packet_task(void *pvParameters)
{
	uint8_t buffer[1024];
	ieee802154_packet_t packet = {0};
	size_t readBytes;
	

	uint8_t uart_buffer[sizeof(uint8_t) + sizeof(ieee802154_short_addr_t) + sizeof(data_t) + sizeof(uint8_t) * 4];
	uart_buffer[0] = '@';
	memcpy(&uart_buffer[sizeof(uart_buffer) - sizeof(uint8_t) * 4], "end\n", 4);

	uart_write_raw_no_overhead((uint8_t*)"test\n", 5);

	while (true)
	{
		readBytes = xMessageBufferReceive(xMessageBuffer, buffer, sizeof(buffer), portMAX_DELAY);
		if (readBytes == 0)
		{
			vTaskDelay(10);
			continue;
		}

		ieee802154_analysis_packet(&buffer[1], buffer[0], &packet);
		if (packet.data_length != sizeof(data_t))
		{
			vTaskDelay(10);
			continue;
		}

		
		
		memcpy(&uart_buffer[sizeof(uint8_t)], &packet.short_src_addr, sizeof(ieee802154_short_addr_t));
		memcpy(&uart_buffer[sizeof(uint8_t) + sizeof(ieee802154_short_addr_t)], packet.data, sizeof(data_t));
		
		uart_write_raw_no_overhead(uart_buffer, sizeof(uart_buffer));

		// ESP_LOGI(TAG, "X: %d, Y: %d", ((data_t*)packet.data)->x, ((data_t*)packet.data)->y);
	}
}


void app_main(void)
{


	xMessageBuffer = xMessageBufferCreate(1024);
	configASSERT(xMessageBuffer);



	ieee802154_init(true);


	xTaskCreate(&receive_packet_task, "RX", 1024*5, NULL, 5, NULL);


	
	while (true)
	{
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
