#include "si4463_driver.h"
#include "radio_config_Si4463.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// STM32 HAL handles
static SPI_HandleTypeDef *hspi_si4463 = NULL;
static GPIO_TypeDef* SDN_PORT = NULL;
static uint16_t SDN_PIN = 0;
static GPIO_TypeDef* NIRQ_PORT = NULL;
static uint16_t NIRQ_PIN = 0;
static GPIO_TypeDef* CS_PORT = NULL;
static uint16_t CS_PIN = 0;

// Buffer for debug messages
#define DEBUG_BUFLEN 128
static char debugBuff[DEBUG_BUFLEN];

// Debug print functions for SI4463 driver
static void si4463_print(const char* message) {
    // You might want to use a different UART for debug messages
    // For now, using huart1 - adjust if needed
    extern UART_HandleTypeDef huart1;
    HAL_UART_Transmit(&huart1, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
}

static void si4463_printf(const char* format, ...) {
    extern UART_HandleTypeDef huart1;
    va_list args;
    va_start(args, format);
    int len = vsnprintf(debugBuff, DEBUG_BUFLEN, format, args);
    va_end(args);

    if (len > 0 && len < DEBUG_BUFLEN) {
        HAL_UART_Transmit(&huart1, (uint8_t*)debugBuff, len, HAL_MAX_DELAY);
    }
}

// Initialize SI4463 for STM32
void Si4463_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef* sdn_port, uint16_t sdn_pin,
                 GPIO_TypeDef* nirq_port, uint16_t nirq_pin,
                 GPIO_TypeDef* cs_port, uint16_t cs_pin) {
    hspi_si4463 = hspi;
    SDN_PORT = sdn_port;
    SDN_PIN = sdn_pin;
    NIRQ_PORT = nirq_port;
    NIRQ_PIN = nirq_pin;
    CS_PORT = cs_port;
    CS_PIN = cs_pin;

    // Configure CS pin as output high
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);

    // Reset and configure
    Si4463_Reset();
    HAL_Delay(100);
    Si4463_Configure();

    si4463_print("SI4463: Initialized\r\n");
}

// Reset SI4463
void Si4463_Reset(void) {
    HAL_GPIO_WritePin(SDN_PORT, SDN_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(SDN_PORT, SDN_PIN, GPIO_PIN_RESET);
    HAL_Delay(100); // Wait for boot
}

// Configure SI4463 with radio config
void Si4463_Configure(void) {
    const uint8_t *config = RADIO_CONFIGURATION_DATA_ARRAY;
    uint8_t len;
    uint8_t cmd[16];

    while (*config != 0x00) {
        len = *config++;
        memcpy(cmd, config, len);
        config += len;
        Si4463_Write(cmd, len);

        // Wait for CTS
        uint8_t cts = 0;
        while (cts != 0xFF) {
            Si4463_Read((uint8_t[]){0x44}, 1, &cts, 1);
            HAL_Delay(1);
        }
    }
    si4463_print("SI4463: Basic configuration complete\r\n");
}

// Configure specifically for POCSAG transmission
void Si4463_ConfigureForPOCSAG(void) {
    // Set up for 1200 baud FSK modulation typical for POCSAG
    Si4463_SetPOCSAGDataRate(1200);

    // Set deviation ~4.5 kHz for POCSAG
    uint8_t dev_cfg[] = {
        0x11, 0x20, 0x01, 0x0A, 0x52, // ~4.5 kHz deviation
    };
    Si4463_Write(dev_cfg, sizeof(dev_cfg));

    // Configure for FSK modulation
    uint8_t modem_cfg[] = {
        0x11, 0x20, 0x07, 0x00, // MODEM_MOD_TYPE
        0x03,                   // FSK mode
        0x00, 0x00, 0x00, 0x00, 0x00
    };
    Si4463_Write(modem_cfg, sizeof(modem_cfg));

    si4463_print("SI4463: Configured for POCSAG\r\n");
}

// Set POCSAG data rate (512 or 1200 baud)
void Si4463_SetPOCSAGDataRate(uint16_t baudRate) {
    uint32_t dataRateReg;

    if (baudRate == 512) {
        dataRateReg = 0x800000; // 512 bps
    } else {
        dataRateReg = 0x1DCD65; // 1200 bps (default)
    }

    uint8_t rate_cmd[] = {
        0x11, 0x20, 0x04, 0x03, // SET_PROPERTY, MODEM group, DATA_RATE
        (uint8_t)((dataRateReg >> 24) & 0xFF),
        (uint8_t)((dataRateReg >> 16) & 0xFF),
        (uint8_t)((dataRateReg >> 8) & 0xFF),
        (uint8_t)(dataRateReg & 0xFF)
    };
    Si4463_Write(rate_cmd, sizeof(rate_cmd));

    si4463_printf("SI4463: Data rate set to %d bps\r\n", baudRate);
}

// Write to SI4463
void Si4463_Write(uint8_t *data, uint8_t len) {
    if (!hspi_si4463 || !data || len == 0) return;

    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi_si4463, data, len, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
}

// Read from SI4463
void Si4463_Read(uint8_t *cmd, uint8_t cmdLen, uint8_t *data, uint8_t dataLen) {
    if (!hspi_si4463 || !cmd || !data || cmdLen == 0 || dataLen == 0) return;

    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);

    // Send command
    HAL_SPI_Transmit(hspi_si4463, cmd, cmdLen, HAL_MAX_DELAY);

    // Read response
    HAL_SPI_Receive(hspi_si4463, data, dataLen, HAL_MAX_DELAY);

    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
}

// Set Frequency for POCSAG
void Si4463_SetFrequency(float freq) {
    // SI4463 frequency calculation (assumes 30 MHz crystal)
    uint32_t freq_val = (uint32_t)(freq * 1000000.0f / 30.0f * 524288.0f);

    uint8_t cmd[8] = {
        0x11, // SET_PROPERTY
        0x20, // FREQ_CONTROL group
        0x04, // Number of bytes
        0x00, // FREQ_CONTROL_INTE
        (uint8_t)((freq_val >> 16) & 0xFF), // FREQ_CONTROL_FRAC_2
        (uint8_t)((freq_val >> 8) & 0xFF),  // FREQ_CONTROL_FRAC_1
        (uint8_t)(freq_val & 0xFF),         // FREQ_CONTROL_FRAC_0
        0x00  // Channel step size
    };

    Si4463_Write(cmd, 8);

    // Wait for CTS
    uint8_t cts = 0;
    while (cts != 0xFF) {
        Si4463_Read((uint8_t[]){0x44}, 1, &cts, 1);
        HAL_Delay(1);
    }

    si4463_printf("SI4463: Frequency set to %.3f MHz\r\n", freq);
}

// Transmit POCSAG data
void Si4463_TransmitPOCSAG(uint8_t *data, uint16_t len) {
    if (!data || len == 0) {
        si4463_print("SI4463: No data to transmit\r\n");
        return;
    }

    // Clear TX FIFO
    uint8_t clear_fifo[] = {0x15, 0x01}; // FIFO_INFO with clear TX
    Si4463_Write(clear_fifo, sizeof(clear_fifo));

    // Write data to TX FIFO
    uint8_t fifo_cmd = 0x66; // WRITE_TX_FIFO
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(hspi_si4463, &fifo_cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi_si4463, data, len, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);

    // Start transmission
    uint8_t tx_cmd[] = {0x31, 0x00, 0x00, 0x00, 0x00}; // START_TX command
    Si4463_Write(tx_cmd, sizeof(tx_cmd));

    // Calculate transmission time (bits * 1000 / baudRate) + margin
    uint32_t transmit_time_ms = (len * 8 * 1000 / 1200) + 100;
    si4463_printf("SI4463: Transmitting %d bytes (~%lu ms)\r\n", len, transmit_time_ms);

    // Wait for transmission to complete
    HAL_Delay(transmit_time_ms);

    si4463_print("SI4463: Transmission complete\r\n");
}

// Check if NIRQ is active (for RX applications)
bool Si4463_IsNIRQActive(void) {
    return (HAL_GPIO_ReadPin(NIRQ_PORT, NIRQ_PIN) == GPIO_PIN_RESET);
}

// Other functions for RX capability (unchanged)
void Si4463_StartRx(void) {
    uint8_t cmd[] = {0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    Si4463_Write(cmd, sizeof(cmd));
}

bool Si4463_ReadRxFifo(uint8_t *data, uint8_t len) {
    if (!data || len == 0) return false;
    uint8_t cmd[2] = {0x77, 0x00};
    Si4463_Read(cmd, 2, data, len);
    return true;
}

void Si4463_GetIntStatus(uint8_t *intStatus) {
    Si4463_Read((uint8_t[]){0x20}, 1, intStatus, 8);
}

void Si4463_ClearInt(void) {
    uint8_t dummy[8];
    Si4463_Read((uint8_t[]){0x20}, 1, dummy, 8);
}

uint8_t Si4463_GetFifoInfo(void) {
    uint8_t fifoInfo[2];
    Si4463_Read((uint8_t[]){0x15}, 1, fifoInfo, 2);
    return fifoInfo[0]; // Return RX bytes available
}
