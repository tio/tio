#include "cobs.h"

void cobs_decoder_reset(cobs_decoder_t *decoder)
{
    decoder->code = 0;
    decoder->remaining = 0;
    decoder->started = false;
}

bool cobs_decode_put_byte(cobs_decoder_t *decoder, unsigned char byte,
                          unsigned char *out)
{
    /* The framing zero ends the current packet. The final implicit zero
       promised by the last block is never part of the decoded payload - it is
       COBS overhead pointing at end-of-packet - so nothing is emitted here.
       A stray framing byte that arrives while no packet is in progress is
       ignored. */
    if (byte == 0)
    {
        decoder->started = false;
        return false;
    }

    if (!decoder->started)
    {
        /* First code byte of a packet. A code of 0xFF means a 254-byte block
           with no trailing zero; any other code N means N-1 data bytes
           followed by one implicit zero. No payload byte is emitted yet. */
        decoder->code = byte;
        decoder->remaining = (byte == 0xFF) ? 254 : (unsigned char)(byte - 1);
        decoder->started = true;
        return false;
    }

    if (decoder->remaining == 0)
    {
        /* Block boundary: the previous code promised an implicit zero unless
           it was 0xFF. Emit that implicit zero if owed, then consume this
           byte as the next code byte. At most one decoded byte is produced
           (the implicit zero); the input byte itself is overhead. */
        bool implicit_zero_owed = (decoder->code != 0xFF);
        if (implicit_zero_owed)
        {
            *out = 0;
        }
        decoder->code = byte;
        decoder->remaining = (byte == 0xFF) ? 254 : (unsigned char)(byte - 1);
        return implicit_zero_owed;
    }

    /* Plain data byte within the current block. */
    *out = byte;
    decoder->remaining--;
    return true;
}
