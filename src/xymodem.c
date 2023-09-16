/*
 * Minimalistic implementation of the xmodem-1k and ymodem sender protocol.
 * https://en.wikipedia.org/wiki/XMODEM
 * https://en.wikipedia.org/wiki/YMODEM
 *
 * SPDX-License-Identifier: GPL-2.0-or-later OR MIT-0
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <termios.h>
#include "misc.h"

#define STX 0x02
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define EOT "\004"

#define OK  0
#define ERR (-1)

#define min(a, b)       ((a) < (b) ? (a) : (b))

struct xpacket {
    uint8_t  type;
    uint8_t  seq;
    uint8_t  nseq;
    uint8_t  data[1024];
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

static int xmodem(int sio, const void *data, size_t len, int seq)
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
        if (read(sio, &resp, 1) < 0) {
            if (errno == EWOULDBLOCK) {
                if (resp == 'C') break;
                if (resp == CAN) return ERR;
                usleep(50000);
                continue;
            }
            perror("Read sync from serial failed");
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
                perror("Write packet to serial failed");
                return ERR;
            }
            from += rc;
            sz   -= rc;
        }

        /* 'lrzsz' does not ACK ymodem's fin packet */
        if (seq == 0 && packet.data[0] == 0) resp = ACK;

        /* Read receiver response, timeout 1 s */
        for(int n=0; n < 20; n++) {
            if (key_hit)
                return ERR;
            if (read(sio, &resp, 1) < 0) {
                if (errno ==  EWOULDBLOCK) {
                    usleep(50000);
                    continue;
                }
                perror("Read ack/nak from serial failed");
                return ERR;
            }
            break;
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
        if (write(sio, EOT, 1) < 0) {
            perror("Write EOT to serial failed");
            return ERR;
        }
        write(STDOUT_FILENO, "|", 1);
        usleep(1000000); /* 1 s timeout*/
        if (read(sio, &resp, 1) < 0) {
            if (errno == EWOULDBLOCK) continue;
            perror("Read from serial failed");
            return ERR;
        }
        if (resp == ACK || resp == CAN) {
            write(STDOUT_FILENO, "\r\n", 2);
            return (resp == ACK) ? OK : ERR;
        }
    }
    return 0; /* not reached */
}

int xymodem_send(int sio, const char *filename, char mode)
{
    size_t         len;
    int            rc, fd;
    struct stat    stat;
    const uint8_t *buf;

    /* Open file, map into memory */
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Could not open file");
        return ERR;
    }
    fstat(fd, &stat);
    len = stat.st_size;
    buf = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!buf) {
        close(fd);
        perror("Could not mmap file");
        return ERR;
    }

    /* Do transfer */
    key_hit = 0;
    if (mode == 'x') {
        rc = xmodem(sio, buf, len, 1);
    }
    else {
        /* Ymodem: hdr + file + fin */
        while(1) {
            char hdr[128], *p;
            p  = stpcpy(hdr, filename) + 1;
            p += sprintf(p, "%ld %lo %o", len, stat.st_mtime, stat.st_mode);

            rc = -1;
            if (xmodem(sio, hdr, p - hdr, 0) < 0) break; /* hdr with metadata */
            if (xmodem(sio, buf, len,     1) < 0) break; /* xmodem file */
            if (xmodem(sio, "",  1,       0) < 0) break; /* empty hdr = fin */
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
