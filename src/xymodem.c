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
#define USER_CAN  (-5)

#define RX_IGNORE 5

#define min(a, b)       ((a) < (b) ? (a) : (b))

struct xpacket_1k {
    uint8_t  type;
    uint8_t  seq;
    uint8_t  nseq;
    uint8_t  data[1024];
    uint8_t  crc_hi;
    uint8_t  crc_lo;
} __attribute__((packed));

struct xpacket {
    uint8_t  type;
    uint8_t  seq;
    uint8_t  nseq;
    uint8_t  data[128];
    uint8_t  crc_hi;
    uint8_t  crc_lo;
} __attribute__((packed));

/* See https://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks */
static uint16_t crc16(const uint8_t *data, uint16_t size)
{
    uint16_t crc, s;

    for (crc = 0; size > 0; size--) {
        s = *data++ ^ (crc >> 8);
        s ^= (s >> 4);
        crc = (crc << 8) ^ s ^ (s << 5) ^ (s << 12);
    }
    return crc;
}

static int xmodem_1k(int sio, const void *data, size_t len, int seq)
{
    struct xpacket_1k  packet;
    const uint8_t  *buf = data;
    char            resp = 0;
    int             rc, crc;

    /* Drain pending characters from serial line. Insist on the
     * last drained character being 'C'.
     */
    while(1) {
        if (key_hit)
            return -1;
        rc = read_poll(sio, &resp, 1, 50);
        if (rc == 0) {
            if (resp == 'C') break;
            if (resp == CAN) return ERR;
            continue;
        }
        else if (rc < 0) {
            tio_error_print("Read sync from serial failed");
            return ERR;
        }
    }

    /* Always work with 1K packets */
    packet.seq  = seq;
    packet.type = STX;

    while (len) {
        size_t  sz, z = 0;
        char   *from, status;

        /* Build next packet, pad with 0 to full seq */
        z = min(len, sizeof(packet.data));
        memcpy(packet.data, buf, z);
        memset(packet.data + z, 0, sizeof(packet.data) - z);
        crc = crc16(packet.data, sizeof(packet.data));
        packet.crc_hi = crc >> 8;
        packet.crc_lo = crc;
        packet.nseq = 0xff - packet.seq;

        /* Send packet */
        from = (char *) &packet;
        sz =  sizeof(packet);
        while (sz) {
            if (key_hit)
                return ERR;
            if ((rc = write(sio, from, sz)) < 0 ) {
                if (errno ==  EWOULDBLOCK) {
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
        resp = 0;

        /* 'lrzsz' does not ACK ymodem's fin packet */
        if (seq == 0 && packet.data[0] == 0) resp = ACK;

        /* Read receiver response, timeout 1 s */
        for(int n=0; n < 20; n++) {
            if (key_hit)
                return ERR;
            rc = read_poll(sio, &resp, 1, 50);
            if (rc < 0) {
                tio_error_print("Read ack/nak from serial failed");
                return ERR;
            } else if(rc > 0) {
                break;
            }
        }

        /* Update "progress bar" */
        switch (resp) {
        case NAK: status = 'N'; break;
        case ACK: status = '.'; break;
        case 'C': status = 'C'; break;
        case CAN: status = '!'; return ERR;
        default:  status = '?';
        }
        write(STDOUT_FILENO, &status, 1);

        /* Move to next block after ACK */
        if (resp == ACK) {
            packet.seq++;
            len -= z;
            buf += z;
        }
    }

    /* Send EOT at 1 Hz until ACK or CAN received */
    while (seq) {
        if (key_hit)
            return ERR;
        if (write(sio, EOT_STR, 1) < 0) {
            tio_error_print("Write EOT to serial failed");
            return ERR;
        }
        write(STDOUT_FILENO, "|", 1);
        /* 1s timeout */
        rc = read_poll(sio, &resp, 1, 1000);
        if (rc < 0) {
            tio_error_print("Read from serial failed");
            return ERR;
        } else if(rc == 0) {
            continue;
        }
        if (resp == ACK || resp == CAN) {
            write(STDOUT_FILENO, "\r\n", 2);
            return (resp == ACK) ? OK : ERR;
        }
    }
    return 0; /* not reached */
}

static int xmodem(int sio, const void *data, size_t len)
{
    struct xpacket  packet;
    const uint8_t  *buf = data;
    char            resp = 0;
    int             rc, crc;

    /* Drain pending characters from serial line. Insist on the
     * last drained character being 'C'.
     */
    while(1) {
        if (key_hit)
            return -1;
        rc = read_poll(sio, &resp, 1, 50);
        if (rc == 0) {
            if (resp == 'C') break;
            if (resp == CAN) return ERR;
            continue;
        }
        else if (rc < 0) {
            tio_error_print("Read sync from serial failed");
            return ERR;
        }
    }

    /* Always work with 128b packets */
    packet.seq  = 1;
    packet.type = SOH;

    while (len) {
        size_t  sz, z = 0;
        char   *from, status;

        /* Build next packet, pad with 0 to full seq */
        z = min(len, sizeof(packet.data));
        memcpy(packet.data, buf, z);
        memset(packet.data + z, 0, sizeof(packet.data) - z);
        crc = crc16(packet.data, sizeof(packet.data));
        packet.crc_hi = crc >> 8;
        packet.crc_lo = crc;
        packet.nseq = 0xff - packet.seq;

        /* Send packet */
        from = (char *) &packet;
        sz =  sizeof(packet);
        while (sz) {
            if (key_hit)
                return ERR;
            if ((rc = write(sio, from, sz)) < 0 ) {
                if (errno ==  EWOULDBLOCK) {
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
        resp = 0;

        /* Read receiver response, timeout 1 s */
        for(int n=0; n < 20; n++) {
            if (key_hit)
                return ERR;
            rc = read_poll(sio, &resp, 1, 50);
            if (rc < 0) {
                tio_error_print("Read ack/nak from serial failed");
                return ERR;
            } else if(rc > 0) {
                break;
            }
        }

        /* Update "progress bar" */
        switch (resp) {
        case NAK: status = 'N'; break;
        case ACK: status = '.'; break;
        case 'C': status = 'C'; break;
        case CAN: status = '!'; return ERR;
        default:  status = '?';
        }
        write(STDOUT_FILENO, &status, 1);

        /* Move to next block after ACK */
        if (resp == ACK) {
            packet.seq++;
            len -= z;
            buf += z;
        }
    }

    /* Send EOT at 1 Hz until ACK or CAN received */
    while (1) {
        if (key_hit)
            return ERR;
        if (write(sio, EOT_STR, 1) < 0) {
            tio_error_print("Write EOT to serial failed");
            return ERR;
        }
        write(STDOUT_FILENO, "|", 1);
        /* 1s timeout */
        rc = read_poll(sio, &resp, 1, 1000);
        if (rc < 0) {
            tio_error_print("Read from serial failed");
            return ERR;
        } else if(rc == 0) {
            continue;
        }
        if (resp == ACK || resp == CAN) {
            write(STDOUT_FILENO, "\r\n", 2);
            return (resp == ACK) ? OK : ERR;
        }
    }
    return 0; /* not reached */
}

int start_receive(int sio)
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
        rc = write(sio, "C", 1);
        if (rc < 0) {
            if (errno ==  EWOULDBLOCK) {
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
            return rc;
        }
        else if (rc > 0)
        {
            if (fds.revents & POLLIN)
            {
                return rc;
            }
        }
        if (key_hit)
            return USER_CAN;
    }
    return rc;
}

uint16_t update_CRC(uint16_t crc, char data_char) 
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

int receive_packet(int sio, struct xpacket packet, int fd)
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
    if (rc == 0) {
        tio_error_print("Timeout waiting for first seq byte");
        return ERR;
    } else if (rc < 0) {
        tio_error_print("Error reading first seq byte")
        return ERR_FATAL;
    }
    rc = read_poll(sio, &rxSeq2, 1, 3000);
    if (rc == 0) {
        tio_error_print("Timeout waiting for second seq byte");
        return ERR;
    } else if (rc < 0) {
        tio_error_print("Error reading second seq byte")
        return ERR_FATAL;
    }
    if (key_hit)
            return USER_CAN;

    /* Read packet Data */
    for (unsigned ix = 0; (ix < sizeof(packet.data)); ix++)
    {
        rc = read_poll(sio, &resp, 1, 3000);
        /* If the read times out or fails then fail this packet. */ 
        if (rc == 0)
        {
            tio_error_print("Timeout waiting for next packet char");
            rc = write(sio, CAN_STR, 1);
            if (rc < 0) {
                tio_error_print("Write cancel packet to serial failed");
                return ERR_FATAL;
            }
            return ERR;
        } else if (rc < 0) {
            tio_error_print("Error reading next packet char")
            rc = write(sio, CAN_STR, 1);
            if (rc < 0) {
                tio_error_print("Write cancel packet to serial failed");
            }
            return ERR_FATAL;
        }
        packet.data[ix] = (uint8_t) resp;
        calcCrc = update_CRC(calcCrc, resp);
        if (key_hit)
            return USER_CAN;
    }

    /* Read CRC */
    rc = read_poll(sio, &resp, 1, 3000);
    if (rc == 0) {
        tio_error_print("Timeout waiting for first CRC byte");
        return ERR;
    } else if (rc < 0) {
        tio_error_print("Error reading first CRC byte")
        return ERR_FATAL;
    }

    uint8_t uresp = resp;
    uint16_t uresp16 = uresp;
    rxCrc  = uresp16 << 8;

    rc = read_poll(sio, &resp, 1, 3000);
    if (rc == 0) {
        tio_error_print("Timeout waiting for second CRC byte");
        return ERR;
    } else if (rc < 0) {
        tio_error_print("Error reading second CRC byte")
        return ERR_FATAL;
    }
    
    uresp = resp;
    uresp16 = uresp;
    rxCrc |= uresp16;

    if (key_hit)
            return USER_CAN;

    /* At this point in the code, there should not be anything in the receive buffer
    because the sender has just sent a complete packet and is waiting on a response. */
    rc = poll(&fds, 1, 10);
    if (rc < 0)
    {
        tio_error_print("%s", strerror(errno));
        tio_error_print("Poll check error after packet finish");
        rc = write(sio, CAN_STR, 1);
        if (rc < 0) {
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
    }

    uint8_t tester = 0xff;
    uint8_t seq1 = rxSeq1;
    uint8_t seq2 = rxSeq2;

    if ((calcCrc == rxCrc) && (seq1 == packet.seq - 1) && ((seq1 ^ seq2) == tester))
    {
        /* Resend of previously processed packet. */
        rc = write(sio, ACK_STR, 1);
        if (rc < 0) {
            tio_error_print("Write acknowlegdement packet to serial failed");
            return ERR_FATAL;
        }
        return RX_IGNORE;
    }
    else if ((calcCrc != rxCrc) || (seq1 != packet.seq) || ((seq1 ^ seq2) != tester))
    {
        /* Fail if the CRC or sequence number is not correct or if the two received
           sequence numbers are not the complement of one another. */
        tio_error_print("Bad CRC or sequence number");
        tio_debug_printf("CRC read: %u", rxCrc);
        tio_debug_printf("CRC calculated: %u", calcCrc);
        tio_debug_printf("Seq read: %hhu", rxSeq1);
        tio_debug_printf("Seq should be: %hhu", packet.seq);
        tio_debug_printf("inv seq: %hhu", rxSeq2);
        return ERR;
    }
    else
    {
        /* The data is good.  Process the packet then ACK it to the sender. */
        rc = write(fd, packet.data, sizeof(packet.data));
        if (rc < 0)
        {
            tio_error_print("Problem writing to file");
            rc = write(sio, CAN_STR, 1);
            if (rc < 0) {
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

int xmodem_receive(int sio, int fd)
{
    struct xpacket  packet;
    char            resp = 0;
    int             rc;
    bool complete = false;
    char status;

    /* Drain pending characters from serial line.*/
    while(1) {
        if (key_hit)
            return -1;
        rc = read_poll(sio, &resp, 1, 50);
        if (rc == 0) {
            if (resp == CAN) return ERR;
            break;
        }
        else if (rc < 0) {
            if (rc != USER_CAN) {
                tio_error_print("Read sync from serial failed");
            }
            return ERR;
        }
    }

    /* Always work with 128b packets */
    packet.seq  = 1;
    packet.type = SOH;

    /* Start Receive*/
    rc = start_receive(sio);
    if (rc == 0)
    {
        tio_error_print("Timeout waiting for transfer to start");
        return ERR;
    } else if (rc < 0) {
        tio_error_print("Error starting XMODEM receive");
        return ERR;
    }

    while (!complete) {
        /* Poll for 1 new byte for 3 seconds */
        rc = read_poll(sio, &resp, 1, 3000);
        if (rc == 0) {
            tio_error_print("Timeout waiting for start of next packet");
            return ERR;
        } else if (rc < 0) {
            tio_error_print("Error reading start of next packet")
            return ERR;
        }
        if (key_hit)
            return USER_CAN;

        switch(resp)
        {
            case SOH:
            /* Start of a packet */
            rc = receive_packet(sio, packet, fd);
            if (rc == OK) {
                packet.seq++;
                status = '.';
            } else if (rc == ERR) {
                rc = write(sio, NAK_STR, 1);
                if (rc < 0) {
                    tio_error_print("Writing not acknowledge packet to serial failed");
                    return ERR;
                }
                status = 'N';
            } else if (rc == ERR_FATAL) {
                tio_error_print("Receive cancelled due to fatal error");
                return ERR;
            } else if (rc == USER_CAN) {
                rc = write(sio, CAN_STR, 1);
                if (rc < 0) {
                    tio_error_print("Writing cancel to serial failed");
                    return ERR;
                }
                return USER_CAN;
            } else if (rc == RX_IGNORE) {
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
    int            rc, fd;
    struct stat    stat;
    const uint8_t *buf;

    /* Open file, map into memory */
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        tio_error_print("Could not open file");
        return ERR;
    }
    fstat(fd, &stat);
    len = stat.st_size;
    buf = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!buf) {
        close(fd);
        tio_error_print("Could not mmap file");
        return ERR;
    }

    /* Do transfer */
    key_hit = 0;
    if (mode == XMODEM_1K) {
        rc = xmodem_1k(sio, buf, len, 1);
    }
    else if (mode == XMODEM_CRC) {
        rc = xmodem(sio, buf, len);
    }
    else {
        /* Ymodem: hdr + file + fin */
        while(1) {
            char hdr[1024], *p;

            rc = -1;
            if (strlen(filename) > 977) break; /* hdr block overrun */
            p  = stpncpy(hdr, filename, 1024) + 1;
            p += sprintf(p, "%ld %lo %o", len, stat.st_mtime, stat.st_mode);

            if (xmodem_1k(sio, hdr, p - hdr, 0) < 0) break; /* hdr with metadata */
            if (xmodem_1k(sio, buf, len,     1) < 0) break; /* xmodem file */
            if (xmodem_1k(sio, "",  1,       0) < 0) break; /* empty hdr = fin */
            rc = 0;                               break;
        }
    }
    key_hit = 0xff;

    /* Flush serial and release resources */
    tcflush(sio, TCIOFLUSH);
    munmap((void *)buf, len);
    close(fd);
    return rc;
}

int xymodem_receive(int sio, const char *filename, modem_mode_t mode)
{
    int            rc, fd;

    /* Create new file */
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (fd < 0) {
        tio_error_print("Could not open file");
        return ERR;
    }

    /* Do transfer */
    key_hit = 0;
    if (mode == XMODEM_1K) {
        tio_error_print("Not supported");
        rc = -1;
    }
    else if (mode == XMODEM_CRC) {
        rc = xmodem_receive(sio, fd);
    }
    else {
        tio_error_print("Not supported");
        rc = -1;
    }
    key_hit = 0xff;

    /* Flush serial and release resources */
    tcflush(sio, TCIOFLUSH);
    close(fd);
    return rc;
}
