/**
 * @file esphome_proto.c
 * @brief Minimal protobuf encoding/decoding for ESPHome Native API
 */

#include "include/esphome_proto.h"
#include <string.h>
#include <stdio.h>

/* -----------------------------------------------------------------
 * Buffer management
 * ----------------------------------------------------------------- */

void pb_buffer_init_write(pb_buffer_t *buf, uint8_t *data, size_t size) {
    buf->data = data;
    buf->size = size;
    buf->pos = 0;
    buf->error = false;
}

void pb_buffer_init_read(pb_buffer_t *buf, const uint8_t *data, size_t size) {
    buf->data = (uint8_t *)data;
    buf->size = size;
    buf->pos = 0;
    buf->error = false;
}

/* -----------------------------------------------------------------
 * Varint encoding/decoding
 * ----------------------------------------------------------------- */

bool pb_encode_varint(pb_buffer_t *buf, uint64_t value) {
    while (value > 0x7F) {
        if (buf->pos >= buf->size) {
            buf->error = true;
            return false;
        }
        buf->data[buf->pos++] = (uint8_t)((value & 0x7F) | 0x80);
        value >>= 7;
    }

    if (buf->pos >= buf->size) {
        buf->error = true;
        return false;
    }

    buf->data[buf->pos++] = (uint8_t)(value & 0x7F);
    return true;
}

bool pb_decode_varint(pb_buffer_t *buf, uint64_t *value) {
    *value = 0;
    uint32_t shift = 0;

    while (buf->pos < buf->size) {
        uint8_t byte = buf->data[buf->pos++];
        *value |= ((uint64_t)(byte & 0x7F)) << shift;

        if ((byte & 0x80) == 0) {
            return true;
        }

        shift += 7;
        if (shift > 63) {
            buf->error = true;
            return false;
        }
    }

    buf->error = true;
    return false;
}

/* -----------------------------------------------------------------
 * Field encoding
 * ----------------------------------------------------------------- */

bool pb_encode_string(pb_buffer_t *buf, uint32_t field_num, const char *str) {
    if (!str || str[0] == '\0') {
        return true; /* Optional field, skip if empty */
    }

    size_t len = strlen(str);
    if (len == 0) {
        return true;
    }

    /* Encode tag */
    if (!pb_encode_varint(buf, PB_FIELD_TAG(field_num, PB_WIRE_TYPE_LENGTH))) {
        return false;
    }

    /* Encode length */
    if (!pb_encode_varint(buf, len)) {
        return false;
    }

    /* Encode string data */
    if (buf->pos + len > buf->size) {
        buf->error = true;
        return false;
    }

    memcpy(buf->data + buf->pos, str, len);
    buf->pos += len;
    return true;
}

bool pb_encode_bool(pb_buffer_t *buf, uint32_t field_num, bool value) {
    if (!pb_encode_varint(buf, PB_FIELD_TAG(field_num, PB_WIRE_TYPE_VARINT))) {
        return false;
    }
    return pb_encode_varint(buf, value ? 1 : 0);
}

bool pb_encode_uint32(pb_buffer_t *buf, uint32_t field_num, uint32_t value) {
    if (!pb_encode_varint(buf, PB_FIELD_TAG(field_num, PB_WIRE_TYPE_VARINT))) {
        return false;
    }
    return pb_encode_varint(buf, value);
}

bool pb_encode_fixed64(pb_buffer_t *buf, uint32_t field_num, uint64_t value) {
    if (!pb_encode_varint(buf, PB_FIELD_TAG(field_num, PB_WIRE_TYPE_64BIT))) {
        return false;
    }

    if (buf->pos + 8 > buf->size) {
        buf->error = true;
        return false;
    }

    /* Little-endian encoding */
    buf->data[buf->pos++] = (uint8_t)(value);
    buf->data[buf->pos++] = (uint8_t)(value >> 8);
    buf->data[buf->pos++] = (uint8_t)(value >> 16);
    buf->data[buf->pos++] = (uint8_t)(value >> 24);
    buf->data[buf->pos++] = (uint8_t)(value >> 32);
    buf->data[buf->pos++] = (uint8_t)(value >> 40);
    buf->data[buf->pos++] = (uint8_t)(value >> 48);
    buf->data[buf->pos++] = (uint8_t)(value >> 56);
    return true;
}

bool pb_encode_uint64(pb_buffer_t *buf, uint32_t field_num, uint64_t value) {
    if (!pb_encode_varint(buf, PB_FIELD_TAG(field_num, PB_WIRE_TYPE_VARINT))) {
        return false;
    }
    return pb_encode_varint(buf, value);
}

bool pb_encode_sint32(pb_buffer_t *buf, uint32_t field_num, int32_t value) {
    /* ZigZag encoding */
    uint32_t zigzag = (uint32_t)((value << 1) ^ (value >> 31));
    return pb_encode_uint32(buf, field_num, zigzag);
}

bool pb_encode_bytes(pb_buffer_t *buf, uint32_t field_num, const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return true; /* Optional field */
    }

    /* Encode tag */
    if (!pb_encode_varint(buf, PB_FIELD_TAG(field_num, PB_WIRE_TYPE_LENGTH))) {
        return false;
    }

    /* Encode length */
    if (!pb_encode_varint(buf, len)) {
        return false;
    }

    /* Encode bytes */
    if (buf->pos + len > buf->size) {
        buf->error = true;
        return false;
    }

    memcpy(buf->data + buf->pos, data, len);
    buf->pos += len;
    return true;
}

/* -----------------------------------------------------------------
 * Field decoding
 * ----------------------------------------------------------------- */

bool pb_decode_string(pb_buffer_t *buf, char *str, size_t max_len) {
    uint64_t len;
    if (!pb_decode_varint(buf, &len)) {
        return false;
    }

    if (len >= max_len) {
        buf->error = true;
        return false;
    }

    if (buf->pos + len > buf->size) {
        buf->error = true;
        return false;
    }

    memcpy(str, buf->data + buf->pos, len);
    str[len] = '\0';
    buf->pos += len;
    return true;
}

bool pb_decode_uint32(pb_buffer_t *buf, uint32_t *value) {
    uint64_t val64;
    if (!pb_decode_varint(buf, &val64)) {
        return false;
    }
    *value = (uint32_t)val64;
    return true;
}

bool pb_skip_field(pb_buffer_t *buf, uint8_t wire_type) {
    switch (wire_type) {
        case PB_WIRE_TYPE_VARINT: {
            uint64_t dummy;
            return pb_decode_varint(buf, &dummy);
        }
        case PB_WIRE_TYPE_64BIT:
            if (buf->pos + 8 > buf->size) {
                buf->error = true;
                return false;
            }
            buf->pos += 8;
            return true;
        case PB_WIRE_TYPE_LENGTH: {
            uint64_t len;
            if (!pb_decode_varint(buf, &len)) {
                return false;
            }
            if (buf->pos + len > buf->size) {
                buf->error = true;
                return false;
            }
            buf->pos += len;
            return true;
        }
        case PB_WIRE_TYPE_32BIT:
            if (buf->pos + 4 > buf->size) {
                buf->error = true;
                return false;
            }
            buf->pos += 4;
            return true;
        default:
            buf->error = true;
            return false;
    }
}

/* -----------------------------------------------------------------
 * ESPHome message encoding
 * ----------------------------------------------------------------- */

size_t esphome_encode_hello_response(uint8_t *buf, size_t size,
                                      const esphome_hello_response_t *msg) {
    pb_buffer_t pb;
    pb_buffer_init_write(&pb, buf, size);

    pb_encode_uint32(&pb, 1, msg->api_version_major);  /* Field 1: uint32 */
    pb_encode_uint32(&pb, 2, msg->api_version_minor);  /* Field 2: uint32 */
    pb_encode_string(&pb, 3, msg->server_info);
    pb_encode_string(&pb, 4, msg->name);

    return pb.error ? 0 : pb.pos;
}

size_t esphome_encode_connect_response(uint8_t *buf, size_t size,
                                        const esphome_connect_response_t *msg) {
    pb_buffer_t pb;
    pb_buffer_init_write(&pb, buf, size);

    pb_encode_bool(&pb, 1, msg->invalid_password);

    return pb.error ? 0 : pb.pos;
}

size_t esphome_encode_device_info_response(uint8_t *buf, size_t size,
                                            const esphome_device_info_response_t *msg) {
    pb_buffer_t pb;
    pb_buffer_init_write(&pb, buf, size);

    /* Encode fields in order (matching api.proto DeviceInfoResponse) */
    pb_encode_bool(&pb, 1, msg->uses_password);           /* Field 1 */
    pb_encode_string(&pb, 2, msg->name);                  /* Field 2 */
    pb_encode_string(&pb, 3, msg->mac_address);           /* Field 3 */
    pb_encode_string(&pb, 4, msg->esphome_version);       /* Field 4 */
    pb_encode_string(&pb, 5, msg->compilation_time);      /* Field 5 */
    pb_encode_string(&pb, 6, msg->model);                 /* Field 6 */
    pb_encode_bool(&pb, 7, msg->has_deep_sleep);          /* Field 7 */

    /* Field 8: project_name (optional) */
    if (msg->project_name[0] != '\0') {
        pb_encode_string(&pb, 8, msg->project_name);
    }

    /* Field 9: project_version (optional) */
    if (msg->project_version[0] != '\0') {
        pb_encode_string(&pb, 9, msg->project_version);
    }

    /* Field 10: webserver_port (optional, only if non-zero) */
    if (msg->webserver_port != 0) {
        pb_encode_uint32(&pb, 10, msg->webserver_port);
    }

    pb_encode_string(&pb, 12, msg->manufacturer);         /* Field 12 */
    pb_encode_string(&pb, 13, msg->friendly_name);        /* Field 13 */

    /* Field 15: Bluetooth proxy feature flags (optional) */
    if (msg->bluetooth_proxy_feature_flags != 0) {
        pb_encode_uint32(&pb, 15, msg->bluetooth_proxy_feature_flags);
    }

    pb_encode_string(&pb, 16, msg->suggested_area);       /* Field 16 */

    /* Field 17: Voice assistant feature flags (optional) */
    if (msg->voice_assistant_feature_flags != 0) {
        pb_encode_uint32(&pb, 17, msg->voice_assistant_feature_flags);
    }

    /* Field 18: Bluetooth MAC address */
    pb_encode_string(&pb, 18, msg->bluetooth_mac_address);

    /* Field 19: API encryption supported (optional) */
    if (msg->api_encryption_supported) {
        pb_encode_bool(&pb, 19, msg->api_encryption_supported);
    }

    /* Fields 20-22: repeated/nested messages - not yet implemented */

    /* Field 23: Z-Wave proxy feature flags (optional) */
    if (msg->zwave_proxy_feature_flags != 0) {
        pb_encode_uint32(&pb, 23, msg->zwave_proxy_feature_flags);
    }

    /* Field 24: Z-Wave home ID (optional) */
    if (msg->zwave_home_id != 0) {
        pb_encode_uint32(&pb, 24, msg->zwave_home_id);
    }

    return pb.error ? 0 : pb.pos;
}

size_t esphome_encode_list_entities_done(uint8_t *buf, size_t size) {
    (void)buf;
    (void)size;
    return 0; /* Empty message */
}

size_t esphome_encode_ble_advertisements(uint8_t *buf, size_t size,
                                          const esphome_ble_advertisements_response_t *msg) {
    pb_buffer_t pb;
    pb_buffer_init_write(&pb, buf, size);

    for (size_t i = 0; i < msg->count && i < ESPHOME_MAX_ADV_BATCH; i++) {
        const esphome_ble_advertisement_t *adv = &msg->advertisements[i];

        printf("[esphome-proto] Encoding advertisement %zu: address=%02X:%02X:%02X:%02X:%02X:%02X, rssi=%d, type=%u, data_len=%zu\n",
               i,
               (uint8_t)(adv->address >> 40), (uint8_t)(adv->address >> 32),
               (uint8_t)(adv->address >> 24), (uint8_t)(adv->address >> 16),
               (uint8_t)(adv->address >> 8), (uint8_t)(adv->address),
               adv->rssi, adv->address_type, adv->data_len);

        /* Encode repeated message: tag + length + fields */
        uint8_t adv_buf[256];
        pb_buffer_t adv_pb;
        pb_buffer_init_write(&adv_pb, adv_buf, sizeof(adv_buf));

        /* Field 1: address (uint64 varint) */
        if (!pb_encode_uint64(&adv_pb, 1, adv->address)) {
            printf("[esphome-proto] ERROR: Failed to encode address field\n");
            return 0;
        }
        printf("[esphome-proto] After encoding address: pos=%zu\n", adv_pb.pos);

        /* Field 2: rssi (sint32) */
        if (!pb_encode_sint32(&adv_pb, 2, adv->rssi)) {
            printf("[esphome-proto] ERROR: Failed to encode RSSI field\n");
            return 0;
        }
        printf("[esphome-proto] After encoding RSSI: pos=%zu\n", adv_pb.pos);

        /* Field 3: address_type (uint32) */
        if (!pb_encode_uint32(&adv_pb, 3, adv->address_type)) {
            printf("[esphome-proto] ERROR: Failed to encode address_type field\n");
            return 0;
        }
        printf("[esphome-proto] After encoding address_type: pos=%zu\n", adv_pb.pos);

        /* Field 4: data (bytes) */
        if (!pb_encode_bytes(&adv_pb, 4, adv->data, adv->data_len)) {
            printf("[esphome-proto] ERROR: Failed to encode data field\n");
            return 0;
        }
        printf("[esphome-proto] After encoding data: pos=%zu\n", adv_pb.pos);

        if (adv_pb.error) {
            printf("[esphome-proto] ERROR: Buffer error flag set\n");
            return 0;
        }

        /* Hex dump of encoded advertisement */
        printf("[esphome-proto] Encoded advertisement (%zu bytes): ", adv_pb.pos);
        for (size_t j = 0; j < adv_pb.pos && j < 50; j++) {
            printf("%02x ", adv_buf[j]);
        }
        if (adv_pb.pos > 50) printf("...");
        printf("\n");

        /* Encode the advertisement as a repeated message (field 1) */
        pb_encode_varint(&pb, PB_FIELD_TAG(1, PB_WIRE_TYPE_LENGTH));
        pb_encode_varint(&pb, adv_pb.pos);

        if (pb.pos + adv_pb.pos > size) {
            pb.error = true;
            return 0;
        }

        memcpy(buf + pb.pos, adv_buf, adv_pb.pos);
        pb.pos += adv_pb.pos;
    }

    return pb.error ? 0 : pb.pos;
}

/* -----------------------------------------------------------------
 * ESPHome message decoding
 * ----------------------------------------------------------------- */

bool esphome_decode_hello_request(const uint8_t *buf, size_t size,
                                   esphome_hello_request_t *msg) {
    pb_buffer_t pb;
    pb_buffer_init_read(&pb, buf, size);

    memset(msg, 0, sizeof(*msg));

    while (pb.pos < pb.size && !pb.error) {
        uint64_t tag;
        if (!pb_decode_varint(&pb, &tag)) {
            break;
        }

        uint32_t field_num = tag >> 3;
        uint8_t wire_type = tag & 0x7;

        if (field_num == 1 && wire_type == PB_WIRE_TYPE_LENGTH) {
            pb_decode_string(&pb, msg->client, sizeof(msg->client));
        } else {
            pb_skip_field(&pb, wire_type);
        }
    }

    return !pb.error;
}

bool esphome_decode_connect_request(const uint8_t *buf, size_t size,
                                     esphome_connect_request_t *msg) {
    pb_buffer_t pb;
    pb_buffer_init_read(&pb, buf, size);

    memset(msg, 0, sizeof(*msg));

    while (pb.pos < pb.size && !pb.error) {
        uint64_t tag;
        if (!pb_decode_varint(&pb, &tag)) {
            break;
        }

        uint32_t field_num = tag >> 3;
        uint8_t wire_type = tag & 0x7;

        if (field_num == 1 && wire_type == PB_WIRE_TYPE_LENGTH) {
            pb_decode_string(&pb, msg->password, sizeof(msg->password));
        } else {
            pb_skip_field(&pb, wire_type);
        }
    }

    return !pb.error;
}

bool esphome_decode_subscribe_ble_advertisements(const uint8_t *buf, size_t size,
                                                  esphome_subscribe_ble_advertisements_t *msg) {
    pb_buffer_t pb;
    pb_buffer_init_read(&pb, buf, size);

    memset(msg, 0, sizeof(*msg));

    while (pb.pos < pb.size && !pb.error) {
        uint64_t tag;
        if (!pb_decode_varint(&pb, &tag)) {
            break;
        }

        uint32_t field_num = tag >> 3;
        uint8_t wire_type = tag & 0x7;

        if (field_num == 1 && wire_type == PB_WIRE_TYPE_VARINT) {
            pb_decode_uint32(&pb, &msg->flags);
        } else {
            pb_skip_field(&pb, wire_type);
        }
    }

    return !pb.error;
}

/* -----------------------------------------------------------------
 * ESPHome message framing
 * ----------------------------------------------------------------- */

size_t esphome_frame_message(uint8_t *out_buf, size_t out_size,
                              uint16_t msg_type,
                              const uint8_t *payload, size_t payload_len) {
    pb_buffer_t pb;
    pb_buffer_init_write(&pb, out_buf, out_size);

    /* Write preamble byte (0x00) */
    if (out_size < 1) {
        return 0;
    }
    out_buf[pb.pos++] = 0x00;

    /* Encode message length as varint (payload length only, NOT including type) */
    if (!pb_encode_varint(&pb, payload_len)) {
        return 0;
    }

    /* Encode message type as varint */
    if (!pb_encode_varint(&pb, msg_type)) {
        return 0;
    }

    /* Copy payload */
    if (pb.pos + payload_len > out_size) {
        return 0;
    }
    memcpy(out_buf + pb.pos, payload, payload_len);
    pb.pos += payload_len;

    return pb.pos;
}

size_t esphome_decode_frame_header(const uint8_t *buf, size_t size,
                                    uint32_t *msg_len, uint16_t *msg_type) {
    pb_buffer_t pb;
    pb_buffer_init_read(&pb, buf, size);

    /* Check for preamble byte (0x00) */
    if (size < 1 || buf[0] != 0x00) {
        return 0; /* Invalid frame */
    }
    pb.pos = 1; /* Skip preamble */

    /* Decode message length (varint) - this is the payload length ONLY */
    uint64_t len64;
    if (!pb_decode_varint(&pb, &len64)) {
        return 0; /* Need more data */
    }

    *msg_len = (uint32_t)len64;

    /* Decode message type (varint) - must do this before checking total size */
    size_t pos_before_type = pb.pos;
    uint64_t type64;
    if (!pb_decode_varint(&pb, &type64)) {
        return 0; /* Need more data */
    }
    size_t type_varint_size = pb.pos - pos_before_type;

    *msg_type = (uint16_t)type64;

    /* Now check if we have enough data for the full message */
    /* Total needed = what we've read so far + payload length */
    if (pb.pos + *msg_len > size) {
        return 0; /* Need more data for complete payload */
    }

    return pb.pos; /* Return position where payload starts */
}
