#pragma once

#include <stdbool.h>

/* Streaming decoder for Consistent Overhead Byte Stuffing (COBS).
 *
 * COBS encodes an arbitrary payload into bytes that contain no zero, so a
 * single 0x00 byte can frame packet boundaries on the wire. This decoder is
 * fed the bytes of a packet one at a time (including the trailing framing
 * zero) and yields the recovered payload bytes one at a time. State is kept in
 * cobs_decoder_t so packets may arrive split across read() calls and several
 * packets may be concatenated in one buffer. See
 * https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing
 */

typedef struct
{
    unsigned char code;      /* code byte of the block being consumed */
    unsigned char remaining; /* data bytes left before the next code byte */
    bool started;            /* first code byte of a packet seen */
} cobs_decoder_t;

/* Reset decoder state so it is ready to receive a new packet. */
void cobs_decoder_reset(cobs_decoder_t *decoder);

/* Feed one COBS-encoded byte to the decoder.
 *
 * If a decoded payload byte results, write it to *out and return true. Return
 * false if the byte was framing overhead (a code byte or the framing zero) and
 * produced no output. A framing zero that arrives while no packet is in
 * progress is ignored. At most one decoded byte is produced per call. */
bool cobs_decode_put_byte(cobs_decoder_t *decoder, unsigned char byte,
                          unsigned char *out);
