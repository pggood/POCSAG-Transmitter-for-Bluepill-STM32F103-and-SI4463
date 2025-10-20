/*
 * Pocsag.h
 *
 *  Created on: Oct 21, 2025
 *      Author: peter
 */

#ifndef POCSAG_H
#define POCSAG_H

#include <stdint.h>
#include <stdbool.h>

// Return codes
#define POCSAG_FAILED 0
#define POCSAG_SUCCESS 1

// Error codes
typedef enum {
    POCSAGRC_UNDETERMINED = 0,
    POCSAGRC_INVALIDADDRESS,
    POCSAGRC_INVALIDSOURCE,
    POCSAGRC_INVALIDBATCH2OPT,
    POCSAGRC_INVALIDINVERTOPT
} Pocsag_error;

// POCSAG message structure
typedef struct {
    uint8_t sync[72];
    uint8_t synccw1[4];
    uint32_t batch1[16];
    uint8_t synccw2[4];
    uint32_t batch2[16];
} Pocsagmsg_s;

// POCSAG context structure
typedef struct {
    Pocsagmsg_s Pocsagmsg;
    int state;
    int size;
    int error;
} Pocsag_t;

// Function prototypes
void Pocsag_Init(Pocsag_t* pocsag);
int Pocsag_GetState(Pocsag_t* pocsag);
int Pocsag_GetSize(Pocsag_t* pocsag);
int Pocsag_GetError(Pocsag_t* pocsag);
void* Pocsag_GetMsgPointer(Pocsag_t* pocsag);
int Pocsag_CreatePocsag(Pocsag_t* pocsag, long int address, int source, char* text, int option_batch2, int option_invert);

#endif // POCSAG_H
