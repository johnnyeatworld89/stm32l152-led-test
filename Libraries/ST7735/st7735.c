#include "st7735.h"


/* -------------------------------------------------------------------------- */
/* Display pins                                                               */
/* -------------------------------------------------------------------------- */

/* CS */
#define ST7735_CS_PORT     GPIOA
#define ST7735_CS_PIN      GPIO_PIN_4

/* DC */
#define ST7735_DC_PORT     GPIOB
#define ST7735_DC_PIN      GPIO_PIN_0

/* RESET */
#define ST7735_RST_PORT    GPIOB
#define ST7735_RST_PIN     GPIO_PIN_1


/* -------------------------------------------------------------------------- */
/* Internal state                                                             */
/* -------------------------------------------------------------------------- */

/*
 * Logical display dimensions.
 *
 * These are changed by ST7735_SetRotation().
 *
 * The physical panel is 128 x 160 pixels.
 */
static uint16_t st7735_width  = ST7735_WIDTH;
static uint16_t st7735_height = ST7735_HEIGHT;


/* -------------------------------------------------------------------------- */
/* Low-level control                                                          */
/* -------------------------------------------------------------------------- */

static void ST7735_Select(void)
{
    HAL_GPIO_WritePin(
        ST7735_CS_PORT,
        ST7735_CS_PIN,
        GPIO_PIN_RESET
    );
}


static void ST7735_Unselect(void)
{
    HAL_GPIO_WritePin(
        ST7735_CS_PORT,
        ST7735_CS_PIN,
        GPIO_PIN_SET
    );
}


static void ST7735_DC_Command(void)
{
    HAL_GPIO_WritePin(
        ST7735_DC_PORT,
        ST7735_DC_PIN,
        GPIO_PIN_RESET
    );
}


static void ST7735_DC_Data(void)
{
    HAL_GPIO_WritePin(
        ST7735_DC_PORT,
        ST7735_DC_PIN,
        GPIO_PIN_SET
    );
}


/* -------------------------------------------------------------------------- */
/* SPI communication                                                          */
/* -------------------------------------------------------------------------- */

static void ST7735_WriteCommand(uint8_t cmd)
{
    ST7735_Select();

    ST7735_DC_Command();

    HAL_SPI_Transmit(
        &hspi1,
        &cmd,
        1,
        HAL_MAX_DELAY
    );

    ST7735_Unselect();
}


static void ST7735_WriteData(
    uint8_t *data,
    uint16_t size
)
{
    ST7735_Select();

    ST7735_DC_Data();

    HAL_SPI_Transmit(
        &hspi1,
        data,
        size,
        HAL_MAX_DELAY
    );

    ST7735_Unselect();
}


/* -------------------------------------------------------------------------- */
/* Hardware reset                                                             */
/* -------------------------------------------------------------------------- */

static void ST7735_Reset(void)
{
    HAL_GPIO_WritePin(
        ST7735_RST_PORT,
        ST7735_RST_PIN,
        GPIO_PIN_RESET
    );

    HAL_Delay(20);

    HAL_GPIO_WritePin(
        ST7735_RST_PORT,
        ST7735_RST_PIN,
        GPIO_PIN_SET
    );

    HAL_Delay(150);
}


/* -------------------------------------------------------------------------- */
/* Initialization                                                             */
/* -------------------------------------------------------------------------- */

void ST7735_Init(void)
{
    ST7735_Reset();


    /* Software reset */
    ST7735_WriteCommand(0x01);

    HAL_Delay(150);


    /* Sleep out */
    ST7735_WriteCommand(0x11);

    HAL_Delay(150);


    /* 16-bit RGB565 */
    ST7735_WriteCommand(0x3A);

    uint8_t colorMode = 0x05;

    ST7735_WriteData(
        &colorMode,
        1
    );


    /* Display on */
    ST7735_WriteCommand(0x29);

    HAL_Delay(50);


    /* Default orientation */
    st7735_width  = ST7735_WIDTH;
    st7735_height = ST7735_HEIGHT;
}


/* -------------------------------------------------------------------------- */
/* Rotation                                                                   */
/* -------------------------------------------------------------------------- */

void ST7735_SetRotation(uint8_t rotation)
{
    uint8_t madctl;

    rotation &= 3;


    switch (rotation)
    {
        case 0:
            madctl = 0x00;
            st7735_width  = 128;
            st7735_height = 160;
            break;

        case 1:
            madctl = 0x60;
            st7735_width  = 160;
            st7735_height = 128;
            break;

        case 2:
            madctl = 0xC0;
            st7735_width  = 128;
            st7735_height = 160;
            break;

        case 3:
            madctl = 0xA0;
            st7735_width  = 160;
            st7735_height = 128;
            break;

        default:
            madctl = 0x00;
            st7735_width  = 128;
            st7735_height = 160;
            break;
    }


    ST7735_WriteCommand(0x36);

    ST7735_WriteData(
        &madctl,
        1
    );
}


/* -------------------------------------------------------------------------- */
/* Address window                                                             */
/* -------------------------------------------------------------------------- */

static void ST7735_SetAddressWindow(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1
)
{
    uint8_t data[4];


    /* Apply panel offsets */
    x0 += ST7735_XSTART;
    x1 += ST7735_XSTART;

    y0 += ST7735_YSTART;
    y1 += ST7735_YSTART;


    /* Column address */
    ST7735_WriteCommand(0x2A);

    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;

    ST7735_WriteData(
        data,
        4
    );


    /* Row address */
    ST7735_WriteCommand(0x2B);

    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;

    ST7735_WriteData(
        data,
        4
    );


    /* Start memory write */
    ST7735_WriteCommand(0x2C);
}


/* -------------------------------------------------------------------------- */
/* Fill screen                                                                */
/* -------------------------------------------------------------------------- */

void ST7735_FillScreen(uint16_t color)
{
    ST7735_FillRect(
        0,
        0,
        st7735_width,
        st7735_height,
        color
    );
}


/* -------------------------------------------------------------------------- */
/* Pixel                                                                      */
/* -------------------------------------------------------------------------- */

void ST7735_DrawPixel(
    uint16_t x,
    uint16_t y,
    uint16_t color
)
{
    if (x >= st7735_width ||
        y >= st7735_height)
    {
        return;
    }


    ST7735_SetAddressWindow(
        x,
        y,
        x,
        y
    );


    uint8_t pixel[2];

    pixel[0] = color >> 8;
    pixel[1] = color & 0xFF;


    ST7735_Select();

    ST7735_DC_Data();

    HAL_SPI_Transmit(
        &hspi1,
        pixel,
        2,
        HAL_MAX_DELAY
    );

    ST7735_Unselect();
}


/* -------------------------------------------------------------------------- */
/* Line                                                                       */
/* -------------------------------------------------------------------------- */

void ST7735_DrawLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color
)
{
    int16_t dx = x1 - x0;
    int16_t sx = (dx >= 0) ? 1 : -1;

    int16_t dy = y1 - y0;
    int16_t sy = (dy >= 0) ? 1 : -1;

    if (dy < 0)
    {
        dy = -dy;
    }

    if (dx < 0)
    {
        dx = -dx;
    }

    int16_t err = dx - dy;


    while (1)
    {
        if (x0 >= 0 &&
            y0 >= 0 &&
            x0 < st7735_width &&
            y0 < st7735_height)
        {
            ST7735_DrawPixel(
                x0,
                y0,
                color
            );
        }


        if (x0 == x1 &&
            y0 == y1)
        {
            break;
        }


        int16_t e2 = 2 * err;


        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }


        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}


/* -------------------------------------------------------------------------- */
/* Rectangle outline                                                          */
/* -------------------------------------------------------------------------- */

void ST7735_DrawRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color
)
{
    if (width == 0 || height == 0)
    {
        return;
    }


    ST7735_DrawLine(
        x,
        y,
        x + width - 1,
        y,
        color
    );


    ST7735_DrawLine(
        x,
        y + height - 1,
        x + width - 1,
        y + height - 1,
        color
    );


    ST7735_DrawLine(
        x,
        y,
        x,
        y + height - 1,
        color
    );


    ST7735_DrawLine(
        x + width - 1,
        y,
        x + width - 1,
        y + height - 1,
        color
    );
}


/* -------------------------------------------------------------------------- */
/* Filled rectangle                                                           */
/* -------------------------------------------------------------------------- */

void ST7735_FillRect(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color
)
{
    if (width == 0 || height == 0)
    {
        return;
    }


    /*
     * Clip rectangle to display boundaries.
     */

    if (x >= st7735_width ||
        y >= st7735_height)
    {
        return;
    }


    if (x + width > st7735_width)
    {
        width = st7735_width - x;
    }


    if (y + height > st7735_height)
    {
        height = st7735_height - y;
    }


    ST7735_SetAddressWindow(
        x,
        y,
        x + width - 1,
        y + height - 1
    );


    uint8_t pixel[2];

    pixel[0] = color >> 8;
    pixel[1] = color & 0xFF;


    ST7735_Select();

    ST7735_DC_Data();


    uint32_t pixels =
        (uint32_t)width *
        (uint32_t)height;


    for (uint32_t i = 0;
         i < pixels;
         i++)
    {
        HAL_SPI_Transmit(
            &hspi1,
            pixel,
            2,
            HAL_MAX_DELAY
        );
    }


    ST7735_Unselect();
}


/* -------------------------------------------------------------------------- */
/* Arrow                                                                      */
/* -------------------------------------------------------------------------- */

void ST7735_DrawArrow(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color
)
{
    /*
     * Draw main line.
     */
    ST7735_DrawLine(
        x0,
        y0,
        x1,
        y1,
        color
    );


    /*
     * Calculate arrowhead.
     *
     * The arrowhead consists of two lines at approximately
     * 30 degrees to the main line.
     */

    float dx = x1 - x0;
    float dy = y1 - y0;

    float length = 0.0f;


    if (dx != 0.0f ||
        dy != 0.0f)
    {
        /*
         * Avoid requiring a math library here.
         *
         * We use a simple normalized approximation below.
         */
        float ax = dx;
        float ay = dy;


        if (ax < 0)
        {
            ax = -ax;
        }

        if (ay < 0)
        {
            ay = -ay;
        }


        length = ax + ay;


        if (length < 1.0f)
        {
            length = 1.0f;
        }


        dx /= length;
        dy /= length;
    }


    const float arrowSize = 6.0f;


    /*
     * Rotate the direction vector by ±30 degrees.
     *
     * cos(30°) ≈ 0.866
     * sin(30°) ≈ 0.5
     */

    float leftX =
        x1 -
        arrowSize *
        (dx * 0.866f + dy * 0.5f);

    float leftY =
        y1 -
        arrowSize *
        (dy * 0.866f - dx * 0.5f);


    float rightX =
        x1 -
        arrowSize *
        (dx * 0.866f - dy * 0.5f);

    float rightY =
        y1 -
        arrowSize *
        (dy * 0.866f + dx * 0.5f);


    ST7735_DrawLine(
        x1,
        y1,
        (int16_t)leftX,
        (int16_t)leftY,
        color
    );


    ST7735_DrawLine(
        x1,
        y1,
        (int16_t)rightX,
        (int16_t)rightY,
        color
    );
}
