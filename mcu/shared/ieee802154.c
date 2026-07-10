//
// Created by Hugo Trippaers on 02/06/2023.
//

#define ESP


#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#ifdef ESP
#include "esp_mac.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "ieee802154.h"
#endif



#define CHECK_BUFFER_LENGTH_INSERT(element)         if (buffer_length - position < sizeof(element)) return 0;
#define INSERT_INTO_BUFFER(element)          CHECK_BUFFER_LENGTH_INSERT(element); memcpy(&buffer[position], &element, sizeof(element));         position += sizeof(element);
#define INSERT_INTO_BUFFER_REVERSE(element)  CHECK_BUFFER_LENGTH_INSERT(element); reverse_memcpy(&buffer[position], &element, sizeof(element)); position += sizeof(element);

#define polynomial 0x1021 // CRC-16-CCITT X^16+X^12+X^5+X^0 (1 0001 0000 0010 0001)



#ifndef DEBUG
#define LOG(...) ;
#else
#ifdef ESP
#define TAG "ieee802154"
#define LOG(...) LOG(__VA_ARGS);
#endif
#endif

static uint16_t crc_1byte(unsigned char in, uint16_t crc)
{
	uint16_t a;

	a = ((in << 8) ^ crc) & 0xFF00;
	for (int bit = 8; bit != 0; bit--)
	{
		if (0 == (a & 0x8000)) a = (a << 1);
		else a = polynomial ^ (a << 1);
	}
	crc = a ^ (crc << 8);
	return crc;
}

uint16_t ieee802154_checksum(uint8_t * packet, size_t packet_length) {
	uint16_t crc = 0x0;
	for (int i=0;i<packet_length;i++)
	{
		crc = crc_1byte(packet[i], crc);
	}
	return crc;
}


static void reverse_memcpy(uint8_t *restrict dst, const uint8_t *restrict src, size_t n)
{
	for (size_t i = 0; i < n; ++i)
	{
		dst[n - 1 - i] = src[i];
	}
}

#define CHECK_BUFFER_LENGTH_READ(element)         if (buffer_length - position < sizeof(element)) return;
#define READ_FROM_BUFFER(element, type) CHECK_BUFFER_LENGTH_READ(type); element = *((type *) &buffer[position]); ieee802154_packet->element = element; position += sizeof(type);

void ieee802154_analysis_packet(uint8_t* buffer, size_t buffer_length, ieee802154_packet_t *ieee802154_packet)
{
	uint8_t position = 0;

	frame_header_t frame_header;
	READ_FROM_BUFFER(frame_header, frame_header_t);

	LOG("-----------------------");
	// ESP_LOG_BUFFER_HEXDUMP(buffer, buffer_length, ESP_LOG_INFO);
	// ESP_LOG_BUFFER_HEXDUMP(&frame_header, sizeof(uint16_t), ESP_LOG_DEBUG);

	LOG("Frame type:                   %x", frame_header.frameType);
	LOG("Security Enabled:             %s", frame_header.secure ? "True" : "False");
	LOG("Frame pending:                %s", frame_header.framePending ? "True" : "False");
	LOG("Acknowledge request:          %s", frame_header.ackReqd ? "True" : "False");
	LOG("PAN ID Compression:           %s", frame_header.panIdCompressed ? "True" : "False");
	LOG("Reserved:                     %s", frame_header.rfu1 ? "True" : "False");
	LOG("Sequence Number Suppression:  %s", frame_header.sequenceNumberSuppression ? "True" : "False");
	LOG("Information Elements Present: %s", frame_header.informationElementsPresent ? "True" : "False");
	LOG("Destination addressing mode:  %x", frame_header.destAddrType);
	LOG("Frame version:                %x", frame_header.frameVer);
	LOG("Source addressing mode:       %x", frame_header.srcAddrType);

	if (frame_header.rfu1) {
		LOG("Reserved field 1 is set, ignoring packet");
		return;
	}

	LOG("frame_header->frameType=%d", frame_header.frameType);
	switch (frame_header.frameType) {
	case FRAME_TYPE_DATA:
	{
		uint8_t sequence_number;
		READ_FROM_BUFFER(sequence_number, uint8_t);
		LOG("sequence_number (%x)", sequence_number);

		uint16_t pan_id			= 0;
		uint8_t  dst_addr[8]	= {0};
		uint8_t  src_addr[8]	= {0};
		uint16_t short_dst_addr = 0;
		uint16_t short_src_addr = 0;
		bool	 broadcast		= false;

		//Destination Addresse
		LOG("frame_header->destAddrType=%d", frame_header.destAddrType);
		switch (frame_header.destAddrType) {
		case ADDR_MODE_NONE:
		{
			LOG("Without PAN ID or address field");
			break;
		}
		case ADDR_MODE_SHORT:
		{
			ieee802154_packet->mode = ADDR_MODE_SHORT;
			READ_FROM_BUFFER(pan_id, uint16_t);
			READ_FROM_BUFFER(short_dst_addr, uint16_t);

			//Broadcast
			if (pan_id == 0xFFFF && short_dst_addr == 0xFFFF) {
				broadcast = true;
				pan_id	  = *((uint16_t*) &buffer[position]);  // srcPan
				position += sizeof(uint16_t);
				LOG("Broadcast on PAN %04x", pan_id);
			} else {
				LOG("On PAN 0x%04x to short address 0x%04x", pan_id, short_dst_addr);
			}
			break;
		}
		case ADDR_MODE_LONG:
		{
			ieee802154_packet->mode = ADDR_MODE_LONG;
			READ_FROM_BUFFER(pan_id, uint16_t);
			CHECK_BUFFER_LENGTH_READ(dst_addr);

			reverse_memcpy(dst_addr, &buffer[position], sizeof(dst_addr));
			position += sizeof(dst_addr);

			memcpy(ieee802154_packet->long_dst_addr, dst_addr, sizeof(dst_addr));
			
			LOG("On PAN 0x%04x to long address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", pan_id, dst_addr[0],
						dst_addr[1], dst_addr[2], dst_addr[3], dst_addr[4], dst_addr[5], dst_addr[6], dst_addr[7]);
			break;
		}
		default:
		{
			LOG("With reserved destination address type, ignoring packet");
			return;
		}
		}

		//Source Addresse
		LOG("frame_header->srcAddrType=%d", frame_header.srcAddrType);
		switch (frame_header.srcAddrType) {
		case ADDR_MODE_NONE:
		{
			LOG("Originating from the PAN coordinator");
			break;
		}
		case ADDR_MODE_SHORT:
		{
			READ_FROM_BUFFER(short_src_addr, uint16_t);
			LOG("Originating from short address 0x%04x", short_src_addr);
			break;
		}
		case ADDR_MODE_LONG:
		{
			CHECK_BUFFER_LENGTH_READ(src_addr);

			reverse_memcpy(src_addr, &buffer[position], sizeof(src_addr));
			position += sizeof(src_addr);

			memcpy(ieee802154_packet->long_src_addr, src_addr, sizeof(src_addr));
			LOG("Originating from long address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", src_addr[0], src_addr[1],
						src_addr[2], src_addr[3], src_addr[4], src_addr[5], src_addr[6], src_addr[7]);
			break;
		}
		default:
		{
			LOG("With reserved source address type, ignoring packet");
			return;
		}
		}

		//Data
		uint8_t* header		   = &buffer[0];
		uint8_t  header_length = position;
		uint8_t* data		   = &buffer[position];
		uint8_t  data_length   = buffer_length - position - sizeof(uint16_t);



		ieee802154_packet->data_length = data_length;
		memcpy(ieee802154_packet->data, data, data_length);
		// ESP_LOG_BUFFER_HEXDUMP(header, header_length, ESP_LOG_DEBUG);
		// ESP_LOG_BUFFER_HEXDUMP(data, data_length, ESP_LOG_DEBUG);

		LOG("Data length: %u", data_length);
		for (int i=0;i<data_length;i++) {
			LOG("Data[%d]=0x%x(%c)", i, data[i], data[i]);
		}

		//Checksum
		uint16_t checksum = *((uint16_t*) &buffer[buffer_length - sizeof(uint16_t)]);
		LOG("checksum: 0x%04x", checksum);

		LOG("PAN %04x S %04x %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X to %04x %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X %s", pan_id,
					short_src_addr, src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5], src_addr[6], src_addr[7],
					short_dst_addr, dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3], dst_addr[4], dst_addr[5], dst_addr[6], dst_addr[7],
					broadcast ? "BROADCAST" : "");
		break;
	}
	default:
	{
		LOG("Packet ignored because of frame type (%u)", frame_header.frameType);
		break;
	}
	}
	LOG("-----------------------");
}




uint8_t ieee802154_header(const uint16_t pan_id, ieee802154_address_t *src, ieee802154_address_t *dst, uint8_t *buffer, size_t buffer_length)
{
	static uint8_t sequence_number = 0x00;
	frame_header_t frame_header =
	{
		.frameType = FRAME_TYPE_DATA,
		.secure = false,
		.framePending = false,
		.ackReqd = false,
		.panIdCompressed = true,
		.rfu1 = false,
		.sequenceNumberSuppression = false,
		.informationElementsPresent = false,
		.destAddrType = dst->mode,
		.frameVer = FRAME_VERSION_STD_2003,
		.srcAddrType = src->mode
	};
	uint8_t position = 0;


	//Frame header
	INSERT_INTO_BUFFER(frame_header);

	//Sequence number
	INSERT_INTO_BUFFER(sequence_number);

	//Pan Id
	INSERT_INTO_BUFFER(pan_id);


	if (frame_header.destAddrType == ADDR_MODE_SHORT)
	{
		INSERT_INTO_BUFFER(dst->short_address);
	}
	else if (frame_header.destAddrType == ADDR_MODE_LONG)
	{
		CHECK_BUFFER_LENGTH_INSERT(dst->long_address);
		reverse_memcpy(&buffer[position], (uint8_t *)&dst->long_address, sizeof(dst->long_address));
		position += sizeof(dst->long_address);
	}

	if (frame_header.srcAddrType == ADDR_MODE_SHORT)
	{
		INSERT_INTO_BUFFER(src->short_address);
	}
	else if (frame_header.srcAddrType == ADDR_MODE_LONG)
	{
		CHECK_BUFFER_LENGTH_INSERT(src->long_address);
		reverse_memcpy(&buffer[position], (uint8_t *)&src->long_address, sizeof(src->long_address));
		position += sizeof(src->long_address);
	}

	return position;
}




void ieee802154_construct_packet_long(uint8_t dst_addr[8], uint8_t src_addr[8], uint16_t pan_id, uint8_t *payload, size_t payload_length, uint8_t buffer[256])
{
	
	ieee802154_address_t src = {
			.mode = ADDR_MODE_LONG,
			.long_address = { src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5], src_addr[6], src_addr[7]}
	};

	ieee802154_address_t dst = {
			.mode = ADDR_MODE_LONG,
			.long_address = { dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3], dst_addr[4], dst_addr[5], dst_addr[6], dst_addr[7]}
	};

	//First byte reserved for packet length

	uint8_t header_length = ieee802154_header(pan_id, &src, &dst, &buffer[sizeof(ieee802154_package_size_t)], sizeof(uint8_t[256]) - 3);
	// ESP_LOG_BUFFER_HEXDUMP(&buffer[1], header_length, ESP_LOG_DEBUG);

	//Add the payload
	memcpy(&buffer[sizeof(ieee802154_package_size_t) + header_length], payload, payload_length);

	//Packet length
	uint8_t packet_length = header_length + payload_length;
	
	buffer[0] = packet_length + sizeof(ieee802154_cheksum_t);

	//Calculate checksum
	ieee802154_cheksum_t checksum = ieee802154_checksum(buffer, packet_length);
	memcpy(&buffer[sizeof(ieee802154_package_size_t) + packet_length], &checksum, sizeof(checksum));
	LOG("checksum=0x%x", checksum);


	

	// ESP_LOG_BUFFER_HEXDUMP(&buffer[1], buffer[0], ESP_LOG_DEBUG);


}

void ieee802154_construct_packet_short(ieee802154_short_addr_t dst_addr, ieee802154_short_addr_t src_addr, uint16_t pan_id, uint8_t *payload, size_t payload_length, uint8_t buffer[256])
{

	ieee802154_address_t src = {
		.mode = ADDR_MODE_SHORT,
		.short_address = src_addr
	};

	ieee802154_address_t dst = {
		.mode = ADDR_MODE_SHORT,
		.short_address = dst_addr
	};

	uint8_t header_length = ieee802154_header(pan_id, &src, &dst, &buffer[sizeof(ieee802154_package_size_t)], sizeof(uint8_t[256]) - 3);
	// ESP_LOG_BUFFER_HEXDUMP(&buffer[1], header_length, ESP_LOG_DEBUG);

	//Add the payload
	memcpy(&buffer[sizeof(ieee802154_package_size_t) + header_length], payload, payload_length);

	//Packet length
	uint8_t packet_length = header_length + payload_length;
	
	buffer[0] = packet_length + sizeof(ieee802154_cheksum_t);

	//Calculate checksum
	ieee802154_cheksum_t checksum = ieee802154_checksum(buffer, packet_length);
	memcpy(&buffer[sizeof(ieee802154_package_size_t) + packet_length], &checksum, sizeof(checksum));
	LOG("checksum=0x%x", checksum);

	

	// ESP_LOG_BUFFER_HEXDUMP(&buffer[1], buffer[0], ESP_LOG_DEBUG);
}


void send_esp(ieee802154_short_addr_t dst_addr, uint8_t *payload, size_t payload_length)
{
	uint16_t pan_id = esp_ieee802154_get_panid();
	esp_ieee802154_set_panid(pan_id);

	// uint8_t ext_addr[8];
	// esp_ieee802154_get_extended_address(ext_addr);

	uint8_t buffer[256];

	ieee802154_construct_packet_short(dst_addr, esp_ieee802154_get_short_address(), pan_id, payload, payload_length, buffer);


	esp_ieee802154_transmit(buffer, false);
}




void ieee802154_init(bool receive)
{
	//Checking if things are as they should be
	ESP_ERROR_CHECK(esp_ieee802154_enable());
	ESP_ERROR_CHECK(esp_ieee802154_set_promiscuous(true));
	ESP_ERROR_CHECK(esp_ieee802154_set_rx_when_idle(true));
	ESP_ERROR_CHECK(esp_ieee802154_set_panid(0x6767));
	ESP_ERROR_CHECK(esp_ieee802154_set_coordinator(false));
	ESP_ERROR_CHECK(esp_ieee802154_set_channel(12));

	// Set long address to the mac address
	uint8_t mac_addr[8] = {0};
	esp_read_mac(mac_addr, ESP_MAC_IEEE802154);
	uint8_t eui64_rev[8] = {0};
	for (int i=0; i<8; i++) {
		eui64_rev[7-i] = mac_addr[i];
		LOG("mac_addr[%d]=0x%x", i, mac_addr[i]);
	}
	esp_ieee802154_set_extended_address(eui64_rev);

	// Set short address
	esp_ieee802154_set_short_address(ieee802154_checksum(mac_addr, sizeof(mac_addr)));

	// Start receiver
	if (receive) ESP_ERROR_CHECK(esp_ieee802154_receive());


	LOG("Ready, panId=0x%04x, channel=%d, myAddr=0x%04x", esp_ieee802154_get_panid(), esp_ieee802154_get_channel(), esp_ieee802154_get_short_address());
}
