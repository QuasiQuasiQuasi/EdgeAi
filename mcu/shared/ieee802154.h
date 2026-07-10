//
// Created by Hugo Trippaers on 02/06/2023.
//

#pragma once

#define FRAME_VERSION_STD_2003 0
#define FRAME_VERSION_STD_2006 1
#define FRAME_VERSION_STD_2015 2

#define FRAME_TYPE_BEACON       (0)
#define FRAME_TYPE_DATA         (1)
#define FRAME_TYPE_ACK          (2)
#define FRAME_TYPE_MAC_COMMAND  (3)
#define FRAME_TYPE_RESERVED     (4)
#define FRAME_TYPE_MULTIPURPOSE (5)
#define FRAME_TYPE_FRAGMENT     (6)
#define FRAME_TYPE_EXTENDED     (7)

#define ADDR_MODE_NONE     (0)  // PAN ID and address fields are not present
#define ADDR_MODE_RESERVED (1)  // Reseved
#define ADDR_MODE_SHORT    (2)  // Short address (16-bit)
#define ADDR_MODE_LONG     (3)  // Extended address (64-bit)


typedef uint16_t ieee802154_cheksum_t;
typedef uint8_t  ieee802154_package_size_t;
typedef uint16_t ieee802154_short_addr_t;

//Size 2 Bytes
typedef struct frame_header {
    uint8_t frameType                  : 3;
    uint8_t secure                     : 1;
    uint8_t framePending               : 1;
    uint8_t ackReqd                    : 1;
    uint8_t panIdCompressed            : 1;
    uint8_t rfu1                       : 1;
    uint8_t sequenceNumberSuppression  : 1;
    uint8_t informationElementsPresent : 1;
    uint8_t destAddrType               : 2;
    uint8_t frameVer                   : 2;
    uint8_t srcAddrType                : 2;
} frame_header_t;

typedef struct {
	uint8_t mode; // ADDR_MODE_NONE || ADDR_MODE_SHORT || ADDR_MODE_LONG
	union {
		ieee802154_short_addr_t short_address;
		uint8_t long_address[8];
	};
} ieee802154_address_t;

typedef struct {
	frame_header_t frame_header;
	uint8_t sequence_number;
	uint16_t pan_id;
	uint8_t mode;
	ieee802154_short_addr_t short_dst_addr;
	ieee802154_short_addr_t short_src_addr;
	uint8_t long_dst_addr[8];
	uint8_t long_src_addr[8];
	size_t data_length;
	uint8_t data[256];
} ieee802154_packet_t;





void ieee802154_analysis_packet(uint8_t* packet, size_t packet_length, ieee802154_packet_t *ieee802154_packet);
uint8_t ieee802154_header(const uint16_t pan_id, ieee802154_address_t *src, ieee802154_address_t *dst, uint8_t *buffer, size_t buffer_length);
void ieee802154_construct_packet_long(uint8_t dst_addr[8], uint8_t src_addr[8], uint16_t pan_id, uint8_t *payload, size_t payload_length, uint8_t buffer[256]);
void ieee802154_construct_packet_short(ieee802154_short_addr_t dst_addr, ieee802154_short_addr_t src_addr, uint16_t pan_id, uint8_t *payload, size_t payload_length, uint8_t buffer[256]);
uint16_t ieee802154_checksum(uint8_t * packet, size_t packet_length);

void send_esp(ieee802154_short_addr_t dst_addr, uint8_t *payload, size_t payload_length);

void ieee802154_init(bool receive);