/*
 * pocsag.c
 *
 *  Created on: Oct 21, 2025
 *      Author: peter
 */
#include "pocsag.h"
#include <string.h>

// Private function prototypes
static void replaceline(Pocsag_t* pocsag, int line, uint32_t val);
static uint8_t flip7charbitorder(uint8_t c_in);
static uint32_t createcrc(uint32_t in);

// Initialize POCSAG structure
void Pocsag_Init(Pocsag_t* pocsag) {
    if (pocsag == NULL) return;

    pocsag->state = 0;
    pocsag->size = 0;
    pocsag->error = POCSAGRC_UNDETERMINED;
    memset(&pocsag->Pocsagmsg, 0, sizeof(Pocsagmsg_s));
}

// Get State
int Pocsag_GetState(Pocsag_t* pocsag) {
    if (pocsag == NULL) return 0;
    return pocsag->state;
}

// Get Size
int Pocsag_GetSize(Pocsag_t* pocsag) {
    if (pocsag == NULL) return 0;
    return pocsag->size;
}

// Get Error
int Pocsag_GetError(Pocsag_t* pocsag) {
    if (pocsag == NULL) return 0;
    return pocsag->error;
}

// Get Message Pointer
void* Pocsag_GetMsgPointer(Pocsag_t* pocsag) {
    if (pocsag == NULL) return NULL;
    return (void*)&pocsag->Pocsagmsg;
}

// creates pocsag message in pocsagmsg structure
int Pocsag_CreatePocsag(Pocsag_t* pocsag, long int address, int source, char* text, int option_batch2, int option_invert) {
    if (pocsag == NULL || text == NULL) {
        return POCSAG_FAILED;
    }

    int txtlen;
    uint8_t c; // temporary var for character being processed
    int stop; // temp var

    char lastchar; // memorize last char of input text
    uint8_t txtcoded[56]; // encoded text can be up to 56 octets
    int txtcodedlen;

    // local vars to encode address line
    int currentframe;
    uint32_t addressline;

    // counters to encode text
    int bitcount_in, bitcount_out, bytecount_in, bytecount_out;

    // table to convert size to n-times 1 bit mask
    const uint8_t size2mask[7] = {0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f};

    // some sanity checks
    txtlen = strlen(text);
    if (txtlen > 40) {
        // messages can be up to 40 chars (+ terminating EOT)
        txtlen = 40;
    }

    // reinit state to 0 (no message)
    pocsag->state = 0;
    pocsag->error = POCSAGRC_UNDETERMINED;
    pocsag->size = 0;

    // some sanity checks for the address
    // addresses are 21 bits
    if ((address > 0x1FFFFF) || (address <= 0)) {
        pocsag->error = POCSAGRC_INVALIDADDRESS;
        return POCSAG_FAILED;
    }

    // source is 2 bits
    if ((source < 0) || (source > 3)) {
        pocsag->error = POCSAGRC_INVALIDSOURCE;
        return POCSAG_FAILED;
    }

    // option "batch2" goes from 0 to 2
    if ((option_batch2 < 0) || (option_batch2 > 2)) {
        pocsag->error = POCSAGRC_INVALIDBATCH2OPT;
        return POCSAG_FAILED;
    }

    // option "invert" should be 0 or 1
    if ((option_invert < 0) || (option_invert > 1)) {
        pocsag->error = POCSAGRC_INVALIDINVERTOPT;
        return POCSAG_FAILED;
    }

    // replace terminating \0 by EOT
    lastchar = text[txtlen];
    text[txtlen] = 0x04; // EOT (end of transmission)
    txtlen++; // increase size by 1 (for EOT)

    // now we know everything is OK. Set state to 1 (message)
    pocsag->state = 1;

    // create packet

    // part 0.1: frame synchronization pattern
    if (option_invert == 0) {
        memset(pocsag->Pocsagmsg.sync, 0xaa, 72);
    } else {
        memset(pocsag->Pocsagmsg.sync, 0x55, 72); // pattern 0x55 is inverse of 0xaa
    }

    // part 0.2: batch synchronization
    // a batch begins with a sync codeword
    // 0111 1100 1101 0010 0001 0101 1101 1000
    pocsag->Pocsagmsg.synccw1[0] = 0x7c;  pocsag->Pocsagmsg.synccw1[1] = 0xd2;
    pocsag->Pocsagmsg.synccw1[2] = 0x15;  pocsag->Pocsagmsg.synccw1[3] = 0xd8;

    pocsag->Pocsagmsg.synccw2[0] = 0x7c;  pocsag->Pocsagmsg.synccw2[1] = 0xd2;
    pocsag->Pocsagmsg.synccw2[2] = 0x15;  pocsag->Pocsagmsg.synccw2[3] = 0xd8;

    // invert bits if needed
    if (option_invert == 1) {
        pocsag->Pocsagmsg.synccw1[0] ^= 0xff; pocsag->Pocsagmsg.synccw1[1] ^= 0xff;
        pocsag->Pocsagmsg.synccw1[2] ^= 0xff; pocsag->Pocsagmsg.synccw1[3] ^= 0xff;

        pocsag->Pocsagmsg.synccw2[0] ^= 0xff; pocsag->Pocsagmsg.synccw2[1] ^= 0xff;
        pocsag->Pocsagmsg.synccw2[2] ^= 0xff; pocsag->Pocsagmsg.synccw2[3] ^= 0xff;
    }

    // part 0.3: init batches with all "idle-pattern"
    for (int l = 0; l < 16; l++) {
        pocsag->Pocsagmsg.batch1[l] = 0x7a89c197;
        pocsag->Pocsagmsg.batch2[l] = 0x7a89c197;
    }

    // part 1: address line
    currentframe = ((address & 0x7) << 1);
    addressline = address >> 3;

    // add address source
    addressline <<= 2;
    addressline += source;

    // replace address line
    replaceline(pocsag, currentframe, createcrc(addressline << 11));

    // part 2.1: convert text to pocsag format
    // init vars
    memset(txtcoded, 0x00, 56); // 56 octets, all "0x00"

    bitcount_out = 7; // 1 char can contain up to 7 bits
    bytecount_out = 0;

    bitcount_in = 7;
    bytecount_in = 0;
    c = flip7charbitorder(text[0]); // start with first char

    txtcodedlen = 0;
    txtcoded[0] = 0x80; // output, initialize as empty with leftmost bit="1"

    // loop for all chars
    stop = 0;

    while (!stop) {
        int bits2copy;
        uint8_t t; // temporary var bits to be copied

        // how many bits to copy?
        // minimum of "bits available for input" and "bits that can be stored"
        if (bitcount_in > bitcount_out) {
            bits2copy = bitcount_out;
        } else {
            bits2copy = bitcount_in;
        }

        // copy "bits2copy" bits, shifted the left if needed
        t = c & (size2mask[bits2copy-1] << (bitcount_in - bits2copy));

        // where to place ?
        // move left to write if needed
        if (bitcount_in > bitcount_out) {
            // move to right and export
            t >>= (bitcount_in - bitcount_out);
        } else if (bitcount_in < bitcount_out) {
            // move to left
            t <<= (bitcount_out - bitcount_in);
        }

        // now copy bits
        txtcoded[txtcodedlen] |= t;

        // decrease bit counters
        bitcount_in -= bits2copy;
        bitcount_out -= bits2copy;

        // outbound queue is full
        if (bitcount_out == 0) {
            // go to next position in txtcoded
            bytecount_out++;
            txtcodedlen++;

            // data is stored in codeblocks, each containing 20 bits of useful data
            if (bytecount_out == 1) {
                // 2nd char of codeword:
                // all 8 bits used, init with all 0
                txtcoded[txtcodedlen] = 0x00;
                bitcount_out = 8;
            } else if (bytecount_out == 2) {
                // 3rd char of codeword:
                // only 5 bits used, init with all 0
                txtcoded[txtcodedlen] = 0x00;
                bitcount_out = 5;
            } else if (bytecount_out >= 3) {
                // 1st char of codeword (wrap around of "bytecount_out" value)
                // init with leftmost bit as "1"
                txtcoded[txtcodedlen] = 0x80;
                bitcount_out = 7;

                // wrap around "bytecount_out" value
                bytecount_out = 0;
            }
        }

        // ingress queue is empty
        if (bitcount_in == 0) {
            bytecount_in++;

            // any more text ?
            if (bytecount_in < txtlen) {
                // reinit vars
                c = flip7charbitorder(text[bytecount_in]);
                bitcount_in = 7; // only use 7 bits of every char
            } else {
                // no more text
                // done, so ... stop!!
                stop = 1;
                continue;
            }
        }
    }

    // increase txtcodedlen. Was used as index (0 to ...) above. Add one to
    // make it indicate a length
    txtcodedlen++;

    // Part 2.2: copy coded message into pocsag message
    // now insert text message
    for (int l = 0; l < txtcodedlen; l += 3) {
        uint32_t t;

        // move up to next frame
        currentframe++;

        // copying is done octet per octet to be architecture / endian independent
        t = (uint32_t)txtcoded[l] << 24; // copy octet to bits 24 to 31 in 32 bit struct
        t |= (uint32_t)txtcoded[l+1] << 16; // copy octet to bits 16 to 23 in 32 bit struct
        t |= (uint32_t)txtcoded[l+2] << 11; // copy octet to bits 11 to 15 in 32 bit struct

        replaceline(pocsag, currentframe, createcrc(t));

        // note: move up 3 chars at a time (see "for" above)
    }

    // invert bits if needed
    if (option_invert) {
        for (int l = 0; l < 16; l++) {
            pocsag->Pocsagmsg.batch1[l] ^= 0xffffffff;
        }

        for (int l = 0; l < 16; l++) {
            pocsag->Pocsagmsg.batch2[l] ^= 0xffffffff;
        }
    }

    // convert to make endian/architecture independent
    for (int l = 0; l < 16; l++) {
        int32_t t1;

        // structure to convert int32 to architecture / endian independent 4-char array
        union {
            int32_t i;
            uint8_t c[4];
        } t2;

        // batch 1
        t1 = pocsag->Pocsagmsg.batch1[l];

        // left most octet
        t2.c[0] = (t1 >> 24) & 0xff; t2.c[1] = (t1 >> 16) & 0xff;
        t2.c[2] = (t1 >> 8) & 0xff; t2.c[3] = t1 & 0xff;

        // copy back
        pocsag->Pocsagmsg.batch1[l] = t2.i;

        // batch 2
        t1 = pocsag->Pocsagmsg.batch2[l];

        // left most octet
        t2.c[0] = (t1 >> 24) & 0xff; t2.c[1] = (t1 >> 16) & 0xff;
        t2.c[2] = (t1 >> 8) & 0xff; t2.c[3] = t1 & 0xff;

        // copy back
        pocsag->Pocsagmsg.batch2[l] = t2.i;
    }

    // done
    // return

    // reset last char in input char (was overwritten at the beginning of the function)
    text[txtlen] = lastchar;

    // If only one single batch used
    if (currentframe < 16) {
        // batch2 option:
        // 0: truncate to one batch
        // 1: copy batch1 to batch2
        // 2: leave batch2 as "idle"

        if (option_batch2 == 0) {
            // done. set length to one single batch (140 octets)
            pocsag->size = 140;
            return POCSAG_SUCCESS;
        } else if (option_batch2 == 1) {
            memcpy(pocsag->Pocsagmsg.batch2, pocsag->Pocsagmsg.batch1, 64); // 16 codewords of 32 bits
        }

        // return for (option_batch2 == 1) or (option_batch2 == 2)
        // set length to 2 batches (208 octets)
        pocsag->size = 208;
        return POCSAG_SUCCESS;
    }

    // more than one batch found
    // length = 2 batches (208 octets)
    pocsag->size = 208;
    return POCSAG_SUCCESS;
}

// Private functions
static void replaceline(Pocsag_t* pocsag, int line, uint32_t val) {
    if (pocsag == NULL) return;

    // sanity checks
    if ((line < 0) || (line > 32)) {
        return;
    }

    if (line < 16) {
        pocsag->Pocsagmsg.batch1[line] = val;
    } else {
        pocsag->Pocsagmsg.batch2[line-16] = val;
    }
}

static uint8_t flip7charbitorder(uint8_t c_in) {

    uint8_t c_out;

    // init
    c_out = 0x00;

    // copy and shift 6 bits
    for (int l = 0; l < 6; l++) {
        if (c_in & 0x01) {
            c_out |= 1;
        }

        // shift data
        c_out <<= 1; c_in >>= 1;
    }

    // copy 7th bit, do not shift
    if (c_in & 0x01) {
        c_out |= 1;
    }

    return c_out;
}

static uint32_t createcrc(uint32_t in) {
    uint32_t cw; // codeword
    uint32_t local_cw = 0;
    uint32_t parity = 0;

    // init cw
    cw = in;

    // move up 11 bits to make place for crc + parity
    local_cw = in;  /* bch */

    // calculate crc
    for (int bit = 1; bit <= 21; bit++, cw <<= 1) {
        if (cw & 0x80000000) {
            cw ^= 0xED200000;
        }
    }

    local_cw |= (cw >> 21);

    // parity
    cw = local_cw;

    for (int bit = 1; bit <= 32; bit++, cw <<= 1) {
        if (cw & 0x80000000) {
            parity++;
        }
    }

    // make even parity
    if (parity % 2) {
        local_cw++;
    }

    // done
    return local_cw;
}


