#include "mcp23s17.h"


/* -------------------------------------------------------------------------- */
/* MCP23S17 register map                                                      */
/*                                                                            */
/* IOCON.BANK = 0                                                             */
/* -------------------------------------------------------------------------- */

#define MCP23S17_REG_IODIRA       0x00U
#define MCP23S17_REG_IODIRB       0x01U

#define MCP23S17_REG_IPOLA        0x02U
#define MCP23S17_REG_IPOLB        0x03U

#define MCP23S17_REG_GPINTENA     0x04U
#define MCP23S17_REG_GPINTENB     0x05U

#define MCP23S17_REG_DEFVALA      0x06U
#define MCP23S17_REG_DEFVALB      0x07U

#define MCP23S17_REG_INTCONA      0x08U
#define MCP23S17_REG_INTCONB      0x09U

#define MCP23S17_REG_IOCONA       0x0AU
#define MCP23S17_REG_IOCONB       0x0BU

#define MCP23S17_REG_GPPUA        0x0CU
#define MCP23S17_REG_GPPUB        0x0DU

#define MCP23S17_REG_INTFA        0x0EU
#define MCP23S17_REG_INTFB        0x0FU

#define MCP23S17_REG_INTCAPA      0x10U
#define MCP23S17_REG_INTCAPB      0x11U

#define MCP23S17_REG_GPIOA        0x12U
#define MCP23S17_REG_GPIOB        0x13U

#define MCP23S17_REG_OLATA        0x14U
#define MCP23S17_REG_OLATB        0x15U


/* -------------------------------------------------------------------------- */
/* IOCON bits                                                                 */
/* -------------------------------------------------------------------------- */

#define MCP23S17_IOCON_INTPOL      (1U << 1)
#define MCP23S17_IOCON_ODR         (1U << 2)
#define MCP23S17_IOCON_HAEN        (1U << 3)
#define MCP23S17_IOCON_DISSLW      (1U << 4)
#define MCP23S17_IOCON_SEQOP       (1U << 5)
#define MCP23S17_IOCON_MIRROR      (1U << 6)
#define MCP23S17_IOCON_BANK        (1U << 7)


/* -------------------------------------------------------------------------- */
/* SPI control byte                                                           */
/* -------------------------------------------------------------------------- */

/*
 * MCP23S17 control byte:
 *
 * bit 7..4: 0100
 * bit 3..1: hardware address A2:A0
 * bit 0:    0 = write
 *           1 = read
 */
#define MCP23S17_OPCODE_BASE       0x40U
#define MCP23S17_OPCODE_WRITE      0x00U
#define MCP23S17_OPCODE_READ       0x01U


/* -------------------------------------------------------------------------- */
/* Driver configuration                                                       */
/* -------------------------------------------------------------------------- */

#define MCP23S17_SPI_TIMEOUT_MS     100U


/* -------------------------------------------------------------------------- */
/* Internal helper functions                                                  */
/* -------------------------------------------------------------------------- */

static uint8_t MCP23S17_IsDeviceValid(
    const MCP23S17_HandleTypeDef *device)
{
    if (device == NULL)
    {
        return 0U;
    }

    if (device->hspi == NULL)
    {
        return 0U;
    }

    if (device->csPort == NULL)
    {
        return 0U;
    }

    if (device->hardwareAddress >
        MCP23S17_ADDRESS_MAX)
    {
        return 0U;
    }

    return 1U;
}


static uint8_t MCP23S17_IsPortValid(
    MCP23S17_Port port)
{
    return
        (port == MCP23S17_PORT_A ||
         port == MCP23S17_PORT_B) ?
        1U :
        0U;
}


static uint8_t MCP23S17_IsPinValid(
    uint8_t pin)
{
    return
        (pin <
         MCP23S17_PIN_COUNT_PER_PORT) ?
        1U :
        0U;
}


static uint8_t MCP23S17_CreateOpcode(
    const MCP23S17_HandleTypeDef *device,
    uint8_t readOperation)
{
    uint8_t addressBits =
        (uint8_t)(
            (device->hardwareAddress & 0x07U)
            << 1
        );

    return
        (uint8_t)(
            MCP23S17_OPCODE_BASE |
            addressBits |
            (readOperation ?
                MCP23S17_OPCODE_READ :
                MCP23S17_OPCODE_WRITE)
        );
}


static void MCP23S17_Select(
    const MCP23S17_HandleTypeDef *device)
{
    HAL_GPIO_WritePin(
        device->csPort,
        device->csPin,
        GPIO_PIN_RESET
    );
}


static void MCP23S17_Deselect(
    const MCP23S17_HandleTypeDef *device)
{
    HAL_GPIO_WritePin(
        device->csPort,
        device->csPin,
        GPIO_PIN_SET
    );
}


static uint8_t MCP23S17_GetDirectionRegister(
    MCP23S17_Port port)
{
    return
        (port == MCP23S17_PORT_A) ?
        MCP23S17_REG_IODIRA :
        MCP23S17_REG_IODIRB;
}


static uint8_t MCP23S17_GetPolarityRegister(
    MCP23S17_Port port)
{
    return
        (port == MCP23S17_PORT_A) ?
        MCP23S17_REG_IPOLA :
        MCP23S17_REG_IPOLB;
}


static uint8_t MCP23S17_GetPullUpRegister(
    MCP23S17_Port port)
{
    return
        (port == MCP23S17_PORT_A) ?
        MCP23S17_REG_GPPUA :
        MCP23S17_REG_GPPUB;
}


static uint8_t MCP23S17_GetGPIORegister(
    MCP23S17_Port port)
{
    return
        (port == MCP23S17_PORT_A) ?
        MCP23S17_REG_GPIOA :
        MCP23S17_REG_GPIOB;
}


static uint8_t MCP23S17_GetOutputLatchRegister(
    MCP23S17_Port port)
{
    return
        (port == MCP23S17_PORT_A) ?
        MCP23S17_REG_OLATA :
        MCP23S17_REG_OLATB;
}


/*
 * Verändert ein einzelnes Bit eines Registers,
 * ohne die übrigen Bits zu verändern.
 */
static HAL_StatusTypeDef MCP23S17_UpdateRegisterBit(
    MCP23S17_HandleTypeDef *device,
    uint8_t registerAddress,
    uint8_t pin,
    uint8_t bitValue)
{
    uint8_t registerValue = 0U;

    HAL_StatusTypeDef status =
        MCP23S17_ReadRegister(
            device,
            registerAddress,
            &registerValue
        );

    if (status != HAL_OK)
    {
        return status;
    }


    uint8_t bitMask =
        (uint8_t)(1U << pin);


    if (bitValue)
    {
        registerValue |= bitMask;
    }
    else
    {
        registerValue &=
            (uint8_t)(~bitMask);
    }


    return MCP23S17_WriteRegister(
        device,
        registerAddress,
        registerValue
    );
}


/* -------------------------------------------------------------------------- */
/* Low-level register access                                                  */
/* -------------------------------------------------------------------------- */

HAL_StatusTypeDef MCP23S17_WriteRegister(
    MCP23S17_HandleTypeDef *device,
    uint8_t registerAddress,
    uint8_t value)
{
    if (!MCP23S17_IsDeviceValid(device))
    {
        return HAL_ERROR;
    }


    uint8_t transmitData[3];

    transmitData[0] =
        MCP23S17_CreateOpcode(
            device,
            0U
        );

    transmitData[1] =
        registerAddress;

    transmitData[2] =
        value;


    MCP23S17_Select(device);


    HAL_StatusTypeDef status =
        HAL_SPI_Transmit(
            device->hspi,
            transmitData,
            sizeof(transmitData),
            MCP23S17_SPI_TIMEOUT_MS
        );


    MCP23S17_Deselect(device);


    return status;
}


HAL_StatusTypeDef MCP23S17_ReadRegister(
    MCP23S17_HandleTypeDef *device,
    uint8_t registerAddress,
    uint8_t *value)
{
    if (!MCP23S17_IsDeviceValid(device) ||
        value == NULL)
    {
        return HAL_ERROR;
    }


    uint8_t transmitData[3];

    uint8_t receiveData[3] =
    {
        0U,
        0U,
        0U
    };


    transmitData[0] =
        MCP23S17_CreateOpcode(
            device,
            1U
        );

    transmitData[1] =
        registerAddress;

    /*
     * Dummy byte, während dessen das Register
     * vom MCP23S17 ausgegeben wird.
     */
    transmitData[2] = 0xFFU;


    MCP23S17_Select(device);


    HAL_StatusTypeDef status =
        HAL_SPI_TransmitReceive(
            device->hspi,
            transmitData,
            receiveData,
            sizeof(transmitData),
            MCP23S17_SPI_TIMEOUT_MS
        );


    MCP23S17_Deselect(device);


    if (status != HAL_OK)
    {
        return status;
    }


    *value =
        receiveData[2];


    return HAL_OK;
}


/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

HAL_StatusTypeDef MCP23S17_Init(
    MCP23S17_HandleTypeDef *device,
    SPI_HandleTypeDef *hspi,
    GPIO_TypeDef *csPort,
    uint16_t csPin,
    uint8_t hardwareAddress)
{
    if (device == NULL ||
        hspi == NULL ||
        csPort == NULL ||
        hardwareAddress >
            MCP23S17_ADDRESS_MAX)
    {
        return HAL_ERROR;
    }


    device->hspi =
        hspi;

    device->csPort =
        csPort;

    device->csPin =
        csPin;

    device->hardwareAddress =
        hardwareAddress;


    /*
     * Der Baustein darf beim Start nicht
     * ausgewählt sein.
     */
    MCP23S17_Deselect(device);


    /*
     * HAEN zunächst über Hardwareadresse 0
     * aktivieren.
     *
     * Nach Reset ist die Hardwareadressierung
     * noch nicht aktiviert. Deshalb wird für
     * diesen ersten IOCON-Zugriff temporär
     * Adresse 0 verwendet.
     */
    uint8_t requestedAddress =
        device->hardwareAddress;

    device->hardwareAddress = 0U;


    HAL_StatusTypeDef status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_IOCONA,
            MCP23S17_IOCON_HAEN
        );


    if (status != HAL_OK)
    {
        device->hardwareAddress =
            requestedAddress;

        return status;
    }


    /*
     * Nach Aktivierung von HAEN darf die
     * eigentliche Hardwareadresse verwendet werden.
     */
    device->hardwareAddress =
        requestedAddress;


    /*
     * IOCON für die adressierte Instanz ebenfalls
     * schreiben.
     *
     * BANK bleibt 0.
     * Sequential Operation bleibt aktiviert.
     * Interruptausgänge bleiben unabhängig,
     * aktiv-low und push-pull.
     */
    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_IOCONA,
            MCP23S17_IOCON_HAEN
        );

    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Beide Ports zunächst vollständig als
     * Eingänge konfigurieren.
     */
    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_IODIRA,
            0xFFU
        );

    if (status != HAL_OK)
    {
        return status;
    }


    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_IODIRB,
            0xFFU
        );

    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Eingangspolarität zunächst nicht invertieren.
     */
    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_IPOLA,
            0x00U
        );

    if (status != HAL_OK)
    {
        return status;
    }


    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_IPOLB,
            0x00U
        );

    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Interrupt-on-change zunächst deaktivieren.
     */
    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_GPINTENA,
            0x00U
        );

    if (status != HAL_OK)
    {
        return status;
    }


    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_GPINTENB,
            0x00U
        );

    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Interne Pull-ups zunächst deaktivieren.
     *
     * Die Shift-Taste aktiviert ihren Pull-up
     * anschließend gezielt nur für GPA0.
     */
    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_GPPUA,
            0x00U
        );

    if (status != HAL_OK)
    {
        return status;
    }


    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_GPPUB,
            0x00U
        );

    if (status != HAL_OK)
    {
        return status;
    }


    /*
     * Ausgangslatches in einen definierten
     * LOW-Zustand versetzen.
     *
     * Solange die Pins als Eingänge konfiguriert
     * sind, beeinflusst dies die Pegel nicht.
     */
    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_OLATA,
            0x00U
        );

    if (status != HAL_OK)
    {
        return status;
    }


    status =
        MCP23S17_WriteRegister(
            device,
            MCP23S17_REG_OLATB,
            0x00U
        );


    return status;
}


/* -------------------------------------------------------------------------- */
/* Pin configuration                                                          */
/* -------------------------------------------------------------------------- */

HAL_StatusTypeDef MCP23S17_ConfigurePinAsInput(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t pin)
{
    if (!MCP23S17_IsDeviceValid(device) ||
        !MCP23S17_IsPortValid(port) ||
        !MCP23S17_IsPinValid(pin))
    {
        return HAL_ERROR;
    }


    uint8_t registerAddress =
        MCP23S17_GetDirectionRegister(
            port
        );


    /*
     * IODIR-Bit:
     *
     * 1 = Eingang
     * 0 = Ausgang
     */
    return MCP23S17_UpdateRegisterBit(
        device,
        registerAddress,
        pin,
        1U
    );
}


HAL_StatusTypeDef MCP23S17_ConfigurePinAsOutput(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t pin)
{
    if (!MCP23S17_IsDeviceValid(device) ||
        !MCP23S17_IsPortValid(port) ||
        !MCP23S17_IsPinValid(pin))
    {
        return HAL_ERROR;
    }


    uint8_t registerAddress =
        MCP23S17_GetDirectionRegister(
            port
        );


    return MCP23S17_UpdateRegisterBit(
        device,
        registerAddress,
        pin,
        0U
    );
}


HAL_StatusTypeDef MCP23S17_SetPullUp(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t pin,
    uint8_t enabled)
{
    if (!MCP23S17_IsDeviceValid(device) ||
        !MCP23S17_IsPortValid(port) ||
        !MCP23S17_IsPinValid(pin))
    {
        return HAL_ERROR;
    }


    uint8_t registerAddress =
        MCP23S17_GetPullUpRegister(
            port
        );


    return MCP23S17_UpdateRegisterBit(
        device,
        registerAddress,
        pin,
        enabled ? 1U : 0U
    );
}


HAL_StatusTypeDef MCP23S17_SetInputPolarity(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t pin,
    uint8_t inverted)
{
    if (!MCP23S17_IsDeviceValid(device) ||
        !MCP23S17_IsPortValid(port) ||
        !MCP23S17_IsPinValid(pin))
    {
        return HAL_ERROR;
    }


    uint8_t registerAddress =
        MCP23S17_GetPolarityRegister(
            port
        );


    return MCP23S17_UpdateRegisterBit(
        device,
        registerAddress,
        pin,
        inverted ? 1U : 0U
    );
}


/* -------------------------------------------------------------------------- */
/* GPIO input                                                                 */
/* -------------------------------------------------------------------------- */

HAL_StatusTypeDef MCP23S17_ReadPort(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t *value)
{
    if (!MCP23S17_IsDeviceValid(device) ||
        !MCP23S17_IsPortValid(port) ||
        value == NULL)
    {
        return HAL_ERROR;
    }


    uint8_t registerAddress =
        MCP23S17_GetGPIORegister(
            port
        );


    return MCP23S17_ReadRegister(
        device,
        registerAddress,
        value
    );
}


HAL_StatusTypeDef MCP23S17_ReadPin(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t pin,
    uint8_t *value)
{
    if (!MCP23S17_IsDeviceValid(device) ||
        !MCP23S17_IsPortValid(port) ||
        !MCP23S17_IsPinValid(pin) ||
        value == NULL)
    {
        return HAL_ERROR;
    }


    uint8_t portValue = 0U;


    HAL_StatusTypeDef status =
        MCP23S17_ReadPort(
            device,
            port,
            &portValue
        );


    if (status != HAL_OK)
    {
        return status;
    }


    *value =
        (portValue &
         (uint8_t)(1U << pin)) ?
        1U :
        0U;


    return HAL_OK;
}


/* -------------------------------------------------------------------------- */
/* GPIO output                                                                */
/* -------------------------------------------------------------------------- */

HAL_StatusTypeDef MCP23S17_WritePort(
    MCP23S17_HandleTypeDef *device,
    MCP23S17_Port port,
    uint8_t value)
{
    if (!MCP23S17_IsDeviceValid(device) ||
        !MCP23S17_IsPortValid(port))
    {
        return HAL_ERROR;
    }


    uint8_t registerAddress =
        MCP23S17_GetOutputLatchRegister(
            port
        );


    return MCP23S17_WriteRegister(
        device,
        registerAddress,
        value
