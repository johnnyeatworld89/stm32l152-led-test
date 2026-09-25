#ifndef MCP23S17_H
#define MCP23S17_H


#include "stm32l1xx_hal.h"

#include 


/* -------------------------------------------------------------------------- */
/* Constants                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * Ein MCP23S17 besitzt zwei Ports mit jeweils
 * acht GPIOs.
 */
#define MCP23S17_PIN_COUNT_PER_PORT    8U

/*
 * Gültiger Bereich der Hardwareadresse A2:A0.
 */
#define MCP23S17_ADDRESS_MIN           0U
#define MCP23S17_ADDRESS_MAX           7U


/* -------------------------------------------------------------------------- */
/* Types                                                                      */
/* -------------------------------------------------------------------------- */

typedef enum
{
    MCP23S17_PORT_A = 0,
    MCP23S17_PORT_B = 1

} MCP23S17_Port;


/*
 * Beschreibt eine einzelne MCP23S17-Instanz.
 *
 * Mehrere Bausteine dürfen denselben SPI-Bus
 * verwenden. Jeder Baustein erhält entweder:
 *
 * - einen eigenen CS-Pin
 *
 * oder später zusätzlich:
 *
 * - eine eigene Hardwareadresse über A2:A0.
 */
typedef struct
{
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *csPort;
    uint16_t csPin;

    /*
     * Hardwareadresse entsprechend A2:A0:
     *
     * 0 bis 7
     */
    uint8_t hardwareAddress;

} MCP23S17_HandleTypeDef;


/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

/*
 * Initialisiert die Treiberinstanz und den
 * MCP23S17.
 *
 * Die Funktion:
 *
 * - speichert SPI-Handle, CS-Port und CS-Pin
 * - prüft die Hardwareadresse
 * - setzt CS auf HIGH
 * - aktiviert IOCON.HAEN
 * - setzt beide Ports zunächst als Eingänge
 * - deaktiviert Polaritätsinvertierung
 * - deaktiviert zunächst alle Interrupts
 *
 * Rückgabe:
 *
 * HAL_OK    Initialisierung erfolgreich
 * HAL_ERROR ungültige Parameter oder SPI-Fehler
 */
HAL_StatusTypeDef MCP23S17_Init(
    MCP23S17_HandleTypeDef *device,
    SPI_HandleTypeDef *hspi,
    GPIO_TypeDef *csPort,
    uint16_t csPin,
    uint8_t hardwareAddress
);


/* -------------------------------------------------------------------------- */
/* Pin configuration                                                          */
/* -------------------------------------------------------------------------- */

/*
 * Konfiguriert einen einzelnen GPIO als Eingang.
 */
HAL_StatusTypeDef MCP23S17_ConfigurePinAsInput(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t pin
);


/*
 * Konfiguriert einen einzelnen GPIO als Ausgang.
 */
HAL_StatusTypeDef MCP23S17_ConfigurePinAsOutput(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t pin
);


/*
 * Aktiviert oder deaktiviert den internen Pull-up
 * eines einzelnen Pins.
 *
 * enabled:
 *
 * 0 = Pull-up deaktiviert
 * 1 = Pull-up aktiviert
 */
HAL_StatusTypeDef MCP23S17_SetPullUp(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t pin,
    uint8_t enabled
);


/*
 * Aktiviert oder deaktiviert die logische
 * Eingangsinvertierung.
 *
 * inverted:
 *
 * 0 = Eingang nicht invertiert
 * 1 = Eingang invertiert
 */
HAL_StatusTypeDef
