/*
 * Minimalistic implementation of the xmodem-1k and ymodem sender protocol.
 * https://en.wikipedia.org/wiki/XMODEM
 * https://en.wikipedia.org/wiki/YMODEM
 *
 * SPDX-License-Identifier: GPL-2.0-or-later OR MIT-0
 *
 */

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <poll.h>
#include "xymodem.h"
#include "print.h"
#include "misc.h"

#define SOH 0x01
#define STX 0x02
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define EOT 0x04

#define SOH_STR "\001"
#define ACK_STR "\006"
#define NAK_STR "\025"
#define CAN_STR "\030"
#define EOT_STR "\004"

#define OK  0
#define ERR (-1)
#define ERR_FATAL (-2)
#define ERR_TMO   (-3)
#define ERR_USER_CAN  (-5)

#define RX_IGNORE 5

#define min(a, b)       ((a) < (b) ? (a) : (b))

struct xpkt_hdr
{
    uint8_t  type;
    uint8_t  seq;
    uint8_t  nseq;
} __attribute__((packed));

struct xpkt_ftr_crc
{
    uint8_t  crc_hi;
    uint8_t  crc_lo;
} __attribute__((packed));

struct xpkt_ftr_sum
{
    uint8_t  cksum;
} __attribute__((packed));

struct xpacket_1k
{
    struct xpkt_hdr hdr;
    uint8_t  data[1024];
    struct xpkt_ftr_crc ftr;
} __attribute__((packed));

struct xpacket_128b_crc
{
    struct xpkt_hdr hdr;
    uint8_t  data[128];
    struct xpkt_ftr_crc ftr;
} __attribute__((packed));

struct xpacket_128b_sum
{
    struct xpkt_hdr hdr;
    uint8_t  data[128];
    struct xpkt_ftr_sum ftr;
} __attribute__((packed));

/* See https://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks */
static uint16_t calculate_crc16(const uint8_t *data, uint16_t size)
{
    uint16_t crc, s;

    for (crc = 0; size > 0; size--)
    {
        s = *data++ ^ (crc >> 8);
        s ^= (s >> 4);
        crc = (crc << 8) ^ s ^ (s << 5) ^ (s << 12);
    }
    return crc;
}

static uint16_t update_crc16(uint16_t crc, char data_char)
{
    uint8_t data = data_char;

    crc = crc ^ ((uint16_t)data << 8);
    for (int ix = 0; (ix < 8); ix++)
    {
        if (crc & 0x8000)
        {
            crc = (crc << 1) ^ 0x1021;
        }
        else
        {
            crc <<= 1;
        }
    }
    return crc;
}

static uint8_t calculate_cksum8(const uint8_t *data, uint16_t size)
{
    uint8_t sum;
    for (sum = 0; size > 0; size--) {
        sum += *data++;
    }
    return sum;
}

/*
 * Drain pending characters from serial line. Insist on the
 * last drained character being initial character 'C':CRC,1K
 */
static int xmsend_initial_handshake(int sio, char init_ch)
{
    int rc;
    char resp = 0;

    /* Wait for initial character */
    while (true)
    {
        if (key_hit)
            return ERR_USER_CAN;

        rc = read_poll(sio, &resp, 1, 50);
        if (rc == 0)
        {
            /* timeout 50 ms
               resp has last received character beacuse read_poll() doesn't
               destroy resp value in this case. */
            if (resp == init_ch) break;
            if (resp == CAN) return ERR;
            continue;
        }
        else if (rc < 0)
        {
            tio_error_print("Read sync from serial failed");
            return ERR;
        }
    }
    return OK;
}

/*
 * Read receiver response, timeout 1 s
 */
static int xmsend_wait_response(int sio, char *resp, char tmo_resp)
{
    int rc;

    for (int n = 0; n < 20; n++)
    {
        if (key_hit)
            return ERR_USER_CAN;

        rc = read_poll(sio, resp, 1, 50);
        if (rc < 0)
        {
            tio_error_print("Read ack/nak from serial failed");
            return ERR;
        }
        else if (rc > 0)
        {
            /* response received */
            return OK;
        }
    }
    /* no response time-out */
    *resp = tmo_resp;
    return OK;
}

/*
 * Send EOT at 1 Hz until ACK or CAN received
 */
static int xmsend_repeat_eot_and_wait_response(int sio)
{
    int rc;
    char resp;

    while (true)
    {
        if (key_hit)
            return ERR_USER_CAN;

        if (write(sio, EOT_STR, 1) < 0)
        {
            tio_error_print("Write EOT to serial failed");
            return ERR;
        }
        write(STDOUT_FILENO, "|", 1);
        /* 1s timeout */
        rc = read_poll(sio, &resp, 1, 1000);
        if (rc < 0)
        {
            tio_error_print("Read from serial failed");
            return ERR;
        }
        else if (rc == 0)
        {
            continue;
        }
        if (resp == ACK || resp == CAN)
        {
            write(STDOUT_FILENO, "\r\n", 2);
            return (resp == ACK) ? OK : ERR;
        }
    }
    return OK; /* not reached */
}

static int xmodem_send_1k(int sio, const void *data, size_t len, int seq)
{
    struct xpacket_1k packet;
    const uint8_t  *buf = data;
    char            resp = 0, tmo_resp;
    int             rc, crc, err;

    /* Drain pending characters from serial line.
       Insist on the last drained character being 'C' */
    err = xmsend_initial_handshake(sio, 'C');
    if (err != OK)
    {
        return err;
    }

    /* Always work with 1K packets */
    packet.hdr.seq  = seq;
    packet.hdr.type = STX;

    while (len)
    {
        size_t  sz, z = 0;
        char   *from, status;

        /* Build next packet, pad with 0 to full seq */
        z = min(len, sizeof(packet.data));
        memcpy(packet.data, buf, z);
        memset(packet.data + z, 0, sizeof(packet.data) - z);
        crc = calculate_crc16(packet.data, sizeof(packet.data));
        packet.ftr.crc_hi = crc >> 8;
        packet.ftr.crc_lo = crc;
        packet.hdr.nseq = 0xff - packet.hdr.seq;

        /* Send packet */
        from = (char *) &packet;
        sz =  sizeof(packet);
        while (sz)
        {
            if (key_hit)
                return ERR_USER_CAN;

            if ((rc = write(sio, from, sz)) < 0 )
            {
                if (errno ==  EWOULDBLOCK)
                {
                    usleep(1000);
                    continue;
                }
                tio_error_print("Write packet to serial failed");
                return ERR;
            }
            from += rc;
            sz   -= rc;
        }

        /* Clear response */
        tmo_resp = 0;

        /* 'lrzsz' does not ACK ymodem's fin packet */
        if (seq == 0 && packet.data[0] == 0) tmo_resp = ACK;

        /* Read receiver response, timeout 1 s */
        err = xmsend_wait_response(sio, &resp, tmo_resp);
        if (err != OK)
        {
            return err;
        }

        /* Update "progress bar" */
        switch (resp)
        {
            case NAK: status = 'N'; break;
            case ACK: status = '.'; break;
            case 'C': status = 'C'; break;
            case CAN: status = '!'; return ERR;
            default:  status = '?';
        }
        write(STDOUT_FILENO, &status, 1);

        /* Move to next block after ACK */
        if (resp == ACK)
        {
            packet.hdr.seq++;
            len -= z;
            buf += z;
        }
    }

    if (seq != 0)
    {
        /* Send EOT at 1 Hz until ACK or CAN received */
        err = xmsend_repeat_eot_and_wait_response(sio);
        if (err != OK) {
            return err;
        }
    }

    return OK;
}

static int xmodem_send_128b(int sio, const void *data, size_t len, bool use_crc)
{
    union
    {
        struct xpacket_128b_crc c;
        struct xpacket_128b_sum s;
    } packet;
    const uint8_t  *buf = data;
    char            resp = 0, tmo_resp;
    int             rc, crc, err;

    /* Drain pending characters from serial line.
       Insist on the last drained character being 'C' */
    if (use_crc)
    {
        err = xmsend_initial_handshake(sio, 'C');
    }
    else
    {
        err = xmsend_initial_handshake(sio, NAK);
    }
    if (err != OK)
        return err;

    /* Always work with 128b packets */
    if (use_crc)
    {
        packet.c.hdr.seq  = 1;
        packet.c.hdr.type = SOH;
    }
    else
    {
        packet.s.hdr.seq  = 1;
        packet.s.hdr.type = SOH;
    }

    while (len)
    {
        size_t  sz, z = 0;
        char   *from, status;

        if (use_crc)
        {
            /* Build next packet, pad with 0 to full seq */
            z = min(len, sizeof(packet.c.data));
            memcpy(packet.c.data, buf, z);
            memset(packet.c.data + z, 0, sizeof(packet.c.data) - z);
            crc = calculate_crc16(packet.c.data, sizeof(packet.c.data));
            packet.c.ftr.crc_hi = crc >> 8;
            packet.c.ftr.crc_lo = crc;
            packet.c.hdr.nseq = 0xff - packet.c.hdr.seq;

            from = (char *) &packet.c;
            sz = sizeof(packet.c);
        }
        else
        {
            /* Build next packet, pad with 0 to full seq */
            z = min(len, sizeof(packet.s.data));
            memcpy(packet.s.data, buf, z);
            memset(packet.s.data + z, 0, sizeof(packet.s.data) - z);
            packet.s.ftr.cksum = calculate_cksum8(packet.s.data, sizeof(packet.s.data));
            packet.s.hdr.nseq = 0xff - packet.s.hdr.seq;

            from = (char *) &packet.s;
            sz = sizeof(packet.s);
        }

        /* Send packet */
        while (sz)
        {
            if (key_hit)
                return ERR_USER_CAN;

            if ((rc = write(sio, from, sz)) < 0 )
            {
                if (errno ==  EWOULDBLOCK)
                {
                    usleep(1000);
                    continue;
                }
                tio_error_print("Write packet to serial failed");
                return ERR;
            }
            from += rc;
            sz   -= rc;
        }

        /* Clear response */
        tmo_resp = 0;

        /* Read receiver response, timeout 1 s */
        err = xmsend_wait_response(sio, &resp, tmo_resp);
        if (err != OK)
        {
            return err;
        }

        /* Update "progress bar" */
        switch (resp)
        {
            case NAK: status = 'N'; break;
            case ACK: status = '.'; break;
            case 'C': status = 'C'; break;
            case CAN: status = '!'; return ERR;
            default:  status = '?';
        }
        write(STDOUT_FILENO, &status, 1);

        /* Move to next block after ACK */
        if (resp == ACK)
        {
            if (use_crc)
                packet.c.hdr.seq++;
            else
                packet.s.hdr.seq++;

            len -= z;
            buf += z;
        }
    }

    /* Send EOT at 1 Hz until ACK or CAN received */
    err = xmsend_repeat_eot_and_wait_response(sio);

    return err;
}

/*
 * Ymodem: hdr + file + fin
 */
static int ymodem_send_1k(int sio, const void *data, size_t len, const char *filename, struct stat *stat)
{
    int err;

    while (1)
    {
        char hdr[1024], *p;

        err = ERR;
        if (strlen(filename) > 977) break; /* hdr block overrun */
        p  = stpncpy(hdr, filename, 1024) + 1;
        p += sprintf(p, "%ld %lo %o", len, stat->st_mtime, stat->st_mode);

        if (xmodem_send_1k(sio, hdr,  p - hdr, 0) < 0) break; /* hdr with metadata */
        if (xmodem_send_1k(sio, data, len,     1) < 0) break; /* xmodem file */
        if (xmodem_send_1k(sio, "",   1,       0) < 0) break; /* empty hdr = fin */
        err = OK;                                      break;
    }
    return err;
}

/*
 * Drain pending characters from serial line.
 */
static int xmrecv_drain_pending_chars(int sio)
{
    int rc;
    char resp;

    while (true)
    {
        if (key_hit)
            return ERR_USER_CAN;

        rc = read_poll(sio, &resp, 1, 50);
        if (rc == 0)
        {
            break;
        }
        else if (rc < 0)
        {
            tio_error_print("Read sync from serial failed");
            return ERR;
        }
        if (resp == CAN)
        {
            return ERR;
        }
    }
    return OK;
}

/*
 * Start Receive
 */
static int xmrecv_start_receive(int sio, bool use_crc)
{
    int rc;
    struct pollfd fds;
    fds.events = POLLIN;
    fds.fd = sio;

    for (int n = 0; n < 20; n++)
    {
        /* Send the 'C' byte until the sender of the file responds with
           something.  The start character will be sent once a second for a number of
           seconds.  If nothing is received in that time then return false to indicate
           that the transfer did not start. */
        if (use_crc)
            rc = write(sio, "C", 1);
        else
            rc = write(sio, NAK_STR, 1);

        if (rc < 0)
        {
            if (errno ==  EWOULDBLOCK)
            {
                usleep(1000);
                continue;
            }
            tio_error_print("Write packet to serial failed");
            return ERR;
        }
        /* Wait until data is available */
        rc = poll(&fds, 1, 3000);
        if (rc < 0)
        {
            tio_error_print("%s", strerror(errno));
            return ERR;
        }
        else if (rc > 0)
        {
            if (fds.revents & POLLIN)
            {
                return OK;
            }
            else /* if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) */
            {
                return ERR;
            }
        }
        if (key_hit)
            return ERR_USER_CAN;
    }
    return ERR_TMO;
}

/*
 * Receive a packet
 */
static int xmrecv_receive_packet(int sio, struct xpacket_128b_crc *packet, int fd, bool use_crc)
{
    char rxSeq1, rxSeq2 = 0;
    char resp = 0;
    uint16_t calcCrc = 0;
    uint16_t rxCrc = 0;
    int rc;

    struct pollfd fds;
    fds.events = POLLIN;
    fds.fd = sio;

    /* Read seq bytes*/
    rc = read_poll(sio, &rxSeq1, 1, 3000);
    if (rc == 0)
    {
        tio_error_print("Timeout waiting for first seq byte");
        return ERR_TMO;
    }
    else if (rc < 0)
    {
        tio_error_print("Error reading first seq byte");
        return ERR_FATAL;
    }
    rc = read_poll(sio, &rxSeq2, 1, 3000);
    if (rc == 0)
    {
        tio_error_print("Timeout waiting for second seq byte");
        return ERR_TMO;
    }
    else if (rc < 0)
    {
        tio_error_print("Error reading second seq byte");
        return ERR_FATAL;
    }
    if (key_hit)
        return ERR_USER_CAN;

    /* Read packet Data */
    for (unsigned ix = 0; (ix < sizeof(packet->data)); ix++)
    {
        rc = read_poll(sio, &resp, 1, 3000);
        /* If the read times out or fails then fail this packet. */
        if (rc == 0)
        {
            tio_error_print("Timeout waiting for next packet char");
            rc = write(sio, CAN_STR, 1);
            if (rc < 0)
            {
                tio_error_print("Write cancel packet to serial failed");
                return ERR_FATAL;
            }
            return ERR_TMO;
        }
        else if (rc < 0)
        {
            tio_error_print("Error reading next packet char");
            rc = write(sio, CAN_STR, 1);
            if (rc < 0)
            {
                tio_error_print("Write cancel packet to serial failed");
            }
            return ERR_FATAL;
        }
        packet->data[ix] = (uint8_t) resp;
        if (use_crc)
            calcCrc = update_crc16(calcCrc, resp);
        else
            calcCrc = (calcCrc + (uint8_t)resp) & 0xff;

        if (key_hit)
            return ERR_USER_CAN;
    }

    if (use_crc)
    {
        /* Read CRC */
        rc = read_poll(sio, &resp, 1, 3000);
        if (rc == 0)
        {
            tio_error_print("Timeout waiting for first CRC byte");
            return ERR_TMO;
        }
        else if (rc < 0)
        {
            tio_error_print("Error reading first CRC byte");
            return ERR_FATAL;
        }

        uint8_t uresp = resp;
        uint16_t uresp16 = uresp;
        rxCrc  = uresp16 << 8;

        rc = read_poll(sio, &resp, 1, 3000);
        if (rc == 0)
        {
            tio_error_print("Timeout waiting for second CRC byte");
            return ERR_TMO;
        }
        else if (rc < 0)
        {
            tio_error_print("Error reading second CRC byte");
            return ERR_FATAL;
        }

        uresp = resp;
        uresp16 = uresp;
        rxCrc |= uresp16;
    }
    else
    {
        /* Read checksum */
        rc = read_poll(sio, &resp, 1, 3000);
        if (rc == 0)
        {
            tio_error_print("Timeout waiting for checksum byte");
            return ERR_TMO;
        }
        else if (rc < 0)
        {
            tio_error_print("Error reading checksum byte");
            return ERR_FATAL;
        }

        rxCrc = (uint8_t)resp;
    }

    if (key_hit)
        return ERR_USER_CAN;

    /* At this point in the code, there should not be anything in the receive buffer
       because the sender has just sent a complete packet and is waiting on a response. */
    rc = poll(&fds, 1, 10);
    if (rc < 0)
    {
        tio_error_print("%s", strerror(errno));
        tio_error_print("Poll check error after packet finish");
        rc = write(sio, CAN_STR, 1);
        if (rc < 0)
        {
            tio_error_print("Write cancel packet to serial failed");
        }
        return ERR_FATAL;
    }
    else if (rc > 0)
    {
        if (fds.revents & POLLIN)
        {
            tio_error_print("RX sync error");
            char dummy = 0;
            /* Drain buffer */
            while (read_poll(sio, &dummy, 1, 100) > 0) {}
            return ERR;
        }
        else /* if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) */
        {
            return ERR;
        }
    }

    uint8_t tester = 0xff;
    uint8_t seq1 = rxSeq1;
    uint8_t seq2 = rxSeq2;

    if ((calcCrc == rxCrc) && (seq1 == packet->hdr.seq - 1) && ((seq1 ^ seq2) == tester))
    {
        /* Resend of previously processed packet. */
        rc = write(sio, ACK_STR, 1);
        if (rc < 0)
        {
            tio_error_print("Write acknowlegdement packet to serial failed");
            return ERR_FATAL;
        }
        return RX_IGNORE;
    }
    else if ((calcCrc != rxCrc) || (seq1 != packet->hdr.seq) || ((seq1 ^ seq2) != tester))
    {
        /* Fail if the CRC or sequence number is not correct or if the two received
           sequence numbers are not the complement of one another. */
        tio_error_print("Bad CRC or sequence number");
        tio_debug_printf("CRC read: %u", rxCrc);
        tio_debug_printf("CRC calculated: %u", calcCrc);
        tio_debug_printf("Seq read: %hhu", rxSeq1);
        tio_debug_printf("Seq should be: %hhu", packet->hdr.seq);
        tio_debug_printf("inv seq: %hhu", rxSeq2);
        return ERR;
    }
    else
    {
        /* The data is good.  Process the packet then ACK it to the sender. */
        rc = write(fd, packet->data, sizeof(packet->data));
        if (rc < 0)
        {
            tio_error_print("Problem writing to file");
            rc = write(sio, CAN_STR, 1);
            if (rc < 0)
            {
                tio_error_print("Write cancel packet to serial failed");
            }
            return ERR_FATAL;
        }
        rc = write(sio, ACK_STR, 1);
        if (rc < 0)
        {
            tio_error_print("Write acknowlegdement packet to serial failed");
            return ERR_FATAL;
        }
    }

    return OK;
}

int xmodem_receive(int sio, int fd, bool use_crc)
{
    struct xpacket_128b_crc packet;
    char            resp = 0;
    int             rc, err;
    bool complete = false;
    char status;

    /* Drain pending characters from serial line.*/
    err = xmrecv_drain_pending_chars(sio);
    if (err != OK)
    {
        return err;
    }

    /* Always work with 128b packets */
    packet.hdr.seq  = 1;
    packet.hdr.type = SOH;

    /* Start Receive*/
    err = xmrecv_start_receive(sio, use_crc);
    if (err != OK)
    {
        if (err == ERR_TMO)
        {
            tio_error_print("Timeout waiting for transfer to start");
        }
        else
        {
            tio_error_print("Error starting XMODEM receive");
        }
        return err;
    }

    while (!complete)
    {
        /* Poll for 1 new byte for 3 seconds */
        rc = read_poll(sio, &resp, 1, 3000);
        if (rc == 0)
        {
            tio_error_print("Timeout waiting for start of next packet");
            return ERR_TMO;
        }
        else if (rc < 0)
        {
            tio_error_print("Error reading start of next packet");
            return ERR;
        }
        if (key_hit)
            return ERR_USER_CAN;

        switch (resp)
        {
            case SOH:
                /* Start of a packet */
                err = xmrecv_receive_packet(sio, &packet, fd, use_crc);
                if (err == OK)
                {
                    packet.hdr.seq++;
                    status = '.';
                }
                else if (err == ERR || err == ERR_TMO)
                {
                    rc = write(sio, NAK_STR, 1);
                    if (rc < 0)
                    {
                        tio_error_print("Writing not acknowledge packet to serial failed");
                        return ERR;
                    }
                    status = 'N';
                }
                else if (err == ERR_FATAL)
                {
                    tio_error_print("Receive cancelled due to fatal error");
                    return ERR;
                }
                else if (err == ERR_USER_CAN)
                {
                    rc = write(sio, CAN_STR, 1);
                    if (rc < 0)
                    {
                        tio_error_print("Writing cancel to serial failed");
                        return ERR;
                    }
                    return ERR_USER_CAN;
                }
                else if (err == RX_IGNORE)
                {
                    status = ':';
                }
                break;

            case EOT:
                /* End of Transfer */
                rc = write(sio, ACK_STR, 1);
                if (rc < 0)
                {
                    tio_error_print("Write acknowlegdement packet to serial failed");
                    return ERR;
                }
                complete = true;
                status = '\0';
                write(STDOUT_FILENO, "|\r\n", 3);
                break;

            case CAN:
                /* Cancel from sender */
                tio_error_print("Transmission cancelled from sender");
                return ERR;
                break;

            default:
                tio_error_print("Unexpected character received waiting for next packet");
                return ERR;
                break;
        }

        /* Update "progress bar" */
        write(STDOUT_FILENO, &status, 1);
    }
    return OK;
}

int xymodem_send(int sio, const char *filename, modem_mode_t mode)
{
    size_t         len;
    int            err, fd;
    struct stat    stat;
    const uint8_t *buf;

    /* Open file, map into memory */
    fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        tio_error_print("Could not open file");
        return ERR;
    }
    fstat(fd, &stat);
    len = stat.st_size;
    buf = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!buf)
    {
        close(fd);
        tio_error_print("Could not mmap file");
        return ERR;
    }

    /* Do transfer */
    key_hit = 0;
    if (mode == XMODEM_1K)
    {
        err = xmodem_send_1k(sio, buf, len, 1);
    }
    else if (mode == XMODEM_CRC)
    {
        err = xmodem_send_128b(sio, buf, len, /* use_crc= */ true);
    }
    else if (mode == XMODEM_SUM)
    {
        err = xmodem_send_128b(sio, buf, len, /* use_crc= */ false);
    }
    else /* if (mode == YMODEM) */
    {
        err = ymodem_send_1k(sio, buf, len, filename, &stat);
    }
    key_hit = 0xff;

    /* Flush serial and release resources */
    tcflush(sio, TCIOFLUSH);
    munmap((void *)buf, len);
    close(fd);
    return err;
}

int xymodem_receive(int sio, const char *filename, modem_mode_t mode)
{
    int            err, fd;

    /* Create new file */
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (fd < 0)
    {
        tio_error_print("Could not open file");
        return ERR;
    }

    /* Do transfer */
    key_hit = 0;
    if (mode == XMODEM_1K)
    {
        tio_error_print("Not supported");
        err = ERR;
    }
    else if (mode == XMODEM_CRC)
    {
        err = xmodem_receive(sio, fd, /* use_crc= */ true);
    }
    else if (mode == XMODEM_SUM)
    {
        err = xmodem_receive(sio, fd, /* use_crc= */ false);
    }
    else
    {
        tio_error_print("Not supported");
        err = ERR;
    }
    key_hit = 0xff;

    /* Flush serial and release resources */
    tcflush(sio, TCIOFLUSH);
    close(fd);
    return err;
}
