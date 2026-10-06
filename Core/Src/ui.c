#include "ui.h"
#include "st7735.h"
#include "font5x7.h"
#include "preset_store.h"

#define UI_COLOR_BACKGROUND          0x0000
#define UI_COLOR_TEXT_LIGHT          0xFFFF
#define UI_COLOR_TEXT_DARK           0x0000
#define UI_COLOR_BORDER_NORMAL       0xFFFF
#define UI_COLOR_BORDER_SELECTED     0x051F
#define UI_COLOR_BORDER_GRABBED      0xF81F
#define UI_COLOR_LOOP_OFF            0x0000
#define UI_COLOR_LOOP_CONFIRMED      0x07E0
#define UI_COLOR_LOOP_UNCONFIRMED    0xFFE0
#define UI_COLOR_CONNECTION          0xFFFF

#define UI_DISPLAY_WIDTH              160
#define UI_DISPLAY_HEIGHT             128
#define UI_VISIBLE_ELEMENT_COLUMNS    4
#define UI_VISIBLE_LANE_COUNT          3
#define UI_ELEMENT_WIDTH              24
#define UI_ELEMENT_HEIGHT             24
#define UI_CONNECTION_WIDTH           18
#define UI_VERTICAL_CONNECTION_HEIGHT 16
#define UI_GRID_CONTENT_WIDTH \
    ((UI_VISIBLE_ELEMENT_COLUMNS * UI_ELEMENT_WIDTH) + \
     ((UI_VISIBLE_ELEMENT_COLUMNS - 1) * UI_CONNECTION_WIDTH))
#define UI_GRID_CONTENT_HEIGHT \
    ((UI_VISIBLE_LANE_COUNT * UI_ELEMENT_HEIGHT) + \
     ((UI_VISIBLE_LANE_COUNT - 1) * UI_VERTICAL_CONNECTION_HEIGHT))
#define UI_GRID_LEFT ((UI_DISPLAY_WIDTH - UI_GRID_CONTENT_WIDTH) / 2)
#define UI_GRID_TOP 2
#define UI_FOOTER_TOP (UI_GRID_TOP + UI_GRID_CONTENT_HEIGHT + 2)
#define UI_FOOTER_HEIGHT (UI_DISPLAY_HEIGHT - UI_FOOTER_TOP)

#define UI_IO_CIRCLE_RADIUS             7
#define UI_IO_DIAGONAL_OFFSET           5
#define UI_AUTO_NODE_RADIUS             6
#define UI_AUTO_NODE_DIAGONAL_OFFSET    4
#define UI_MANUAL_NODE_RADIUS           6
#define UI_MANUAL_NODE_DIAGONAL_OFFSET  3
#define UI_LOOP_CORNER_RADIUS           2
#define UI_LOOP_DIAGONAL_INSET          1
#define UI_FOCUS_FRAME_GAP              2
#define UI_FOCUS_CORNER_RADIUS          3
#define UI_ARROW_LENGTH                 5
#define UI_ARROW_HALF_WIDTH             3
#define UI_ARROW_FIXED_SCALE            256

/* -------------------------------------------------------------------------- */
/* Shift debug indicator                                                      */
/* -------------------------------------------------------------------------- */

#define UI_SHIFT_INDICATOR_WIDTH       12U
#define UI_SHIFT_INDICATOR_HEIGHT      10U

#define UI_SHIFT_INDICATOR_X           \
    (UI_DISPLAY_WIDTH -                \
     UI_SHIFT_INDICATOR_WIDTH - 1U)

#define UI_SHIFT_INDICATOR_Y           \
    (UI_FOOTER_TOP +                   \
     ((UI_FOOTER_HEIGHT -              \
       UI_SHIFT_INDICATOR_HEIGHT) / 2U))

/* -------------------------------------------------------------------------- */
/* Menu overlay                                                               */
/* -------------------------------------------------------------------------- */

#define UI_MENU_X                    8U
#define UI_MENU_Y                    8U
#define UI_MENU_WIDTH              144U
#define UI_MENU_HEIGHT              80U
#define UI_MENU_ROW_HEIGHT          24U
#define UI_MENU_TEXT_X_OFFSET        8U
#define UI_MENU_TEXT_Y_OFFSET        8U
#define UI_MENU_GENERAL_ROW          0U
#define UI_MENU_ITEM_ROW             1U
#define UI_MENU_DELETE_NODE_ROW      2U

/* Anzahl der Zeilen, die das Menue-Overlay gleichzeitig anzeigen kann. */
#define UI_MENU_VISIBLE_ROWS         3U

#define UI_MENU_CONFIRM_TITLE_ROW    0U
#define UI_MENU_CONFIRM_CANCEL_ROW   1U
#define UI_MENU_CONFIRM_DELETE_ROW   2U

static UI_Item uiItems[] =
{
    {
        .id = 1,
        .type = UI_ITEM_INPUT,
        .order = 0,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "In1",
        .longName = "Input 1"
    },

    {
        .id = 2,
        .type = UI_ITEM_LOOP,
        .order = 1,
        .lane = 1,
        .loopStatus =
            UI_LOOP_STATUS_ACTIVE_CONFIRMED,
        .focus = UI_FOCUS_SELECTED,
        .shortName = "L01",
        .longName = "Loop 01"
    },

    {
        .id = 3,
        .type = UI_ITEM_LOOP,
        .order = 1,
        .lane = 0,
        .loopStatus =
            UI_LOOP_STATUS_ACTIVE_UNCONFIRMED,
        .focus = UI_FOCUS_NONE,
        .shortName = "L02",
        .longName = "Loop 02"
    },

    {
        .id = 4,
        .type = UI_ITEM_LOOP,
        .order = 2,
        .lane = 1,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "L03",
        .longName = "Loop 03"
    },

    {
        .id = 5,
        .type = UI_ITEM_LOOP,
        .order = 2,
        .lane = 0,
        .loopStatus =
            UI_LOOP_STATUS_ACTIVE_CONFIRMED,
        .focus = UI_FOCUS_NONE,
        .shortName = "L04",
        .longName = "Loop 04"
    },

    {
        .id = 6,
        .type = UI_ITEM_OUTPUT,
        .order = 3,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "Out1",
        .longName = "Output 1"
    },

{
        .id = 7,
        .type = UI_ITEM_LOOP,
        .order = 1,
        .lane = -1,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "L05",
        .longName = "LOOP 5"
},


{
        .id = 8,
        .type = UI_ITEM_LOOP,
        .order = 2,
        .lane = -1,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "L06",
        .longName = "LOOP 6"
    },

    /*
     * Reserve pool for dynamically created
     * manual nodes.
     *
     * order < 0 means inactive.
     */
    {
        .id = 100,
        .type = UI_ITEM_MANUAL_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "N10",
        .longName = "Node 10"
    },

    {
        .id = 101,
        .type = UI_ITEM_MANUAL_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "N11",
        .longName = "Node 11"
    },

    {
        .id = 102,
        .type = UI_ITEM_MANUAL_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "N12",
        .longName = "Node 12"
    },

    {
        .id = 103,
        .type = UI_ITEM_MANUAL_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "N13",
        .longName = "Node 13"
    },


    /*
     * Reserve pool for automatically generated nodes.
     *
     * Automatic nodes are derived routing elements. They remain
     * inactive while order == UI_INACTIVE_ORDER.
     */
    {
        .id = 200,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 201,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 202,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 203,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 204,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 205,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 206,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 207,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 208,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 209,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 210,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 211,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 212,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 213,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 214,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 215,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 216,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 217,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 218,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 219,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 220,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 221,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 222,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
    {
        .id = 223,
        .type = UI_ITEM_AUTO_NODE,
        .order = -100,
        .lane = 0,
        .loopStatus = UI_LOOP_STATUS_OFF,
        .focus = UI_FOCUS_NONE,
        .shortName = "",
        .longName = ""
    },
};


#define UI_ITEM_COUNT (sizeof(uiItems) / sizeof(uiItems[0]))
#define UI_MAX_CONNECTIONS 64U


/*
 * Beschreibbarer Verbindungspuffer.
 *
 * Die aktiven Einträge werden dynamisch durch
 * UI_RebuildCalculatedConnections() erzeugt.
 */
static UI_Connection uiConnections[
    UI_MAX_CONNECTIONS
];

#define UI_CONNECTION_CAPACITY \
    (sizeof(uiConnections) / sizeof(uiConnections[0]))

static uint16_t uiConnectionCount = 0U;


static UI_ItemGeometry uiGeometry[UI_ITEM_COUNT];

/*
 * Absolute order der ersten links sichtbaren
 * Bildschirmspalte.
 *
 * Beispiel:
 * uiFirstVisibleOrder = 2
 *
 * Sichtbar sind:
 * order 2, 3, 4 und 5.
 */
static int16_t uiFirstVisibleOrder = 0;

/*
 * Höchste aktuell sichtbare Lane.
 *
 * Bei drei sichtbaren Lanes und dem Wert +1 sind
 * die Lanes +1, 0 und -1 sichtbar.
 */
static int16_t uiHighestVisibleLane = 1;

/*
 * Temporärer Shift-Debugzustand.
 *
 * 0 = nicht gedrückt
 * 1 = gedrückt
 */
static uint8_t uiShiftDebugState = 0U;

/*
 * Sicherheitsgrenze für die Anzahl logischer
 * Spalten.
 *
 * Dies ist keine sichtbare Begrenzung.
 */
#define UI_MAX_ORDER_COUNT 32

/*
 * Automatische Routingberechnung.
 */
#define UI_MAX_ROUTING_ITERATIONS 64U
#define UI_INACTIVE_ORDER         (-100)

/* -------------------------------------------------------------------------- */
/* Internal function prototypes                                               */
/* -------------------------------------------------------------------------- */

static void UI_DrawConnections(void);


static void UI_DrawConnectionsForItem(
    int16_t itemIndex
);

static void UI_UpdateChangedStructureDisplay(void);

static void UI_DrawItem(
    const UI_Item *item,
    int16_t x,
    int16_t y
);



static void UI_DrawFooter(void);
static void UI_DrawMenu(void);
static void UI_CloseMenu(uint8_t restorePreview);
static void UI_MenuHandleEncoderStep(int8_t direction);
static void UI_MenuHandleEnter(void);
static void UI_MenuHandleReturn(void);
static uint8_t UI_MenuGetRowCount(void);
static uint8_t UI_MenuItemIsManualNode(void);
static void UI_MenuSavePreset(void);
static void UI_MenuManagePresets(void);
static void UI_DeleteSelectedManualNode(void);

/*
* Horizontal viewport.
*/
static void UI_ClampHorizontalViewport(void);

/*
* Structure boundaries.
*/
static int16_t UI_GetMinimumPermanentOrder(void);
static int16_t UI_GetMaximumPermanentOrder(void);

static int16_t UI_FindFirstSelectableIndex(void);

static int16_t UI_FindNextSelectableIndex(
    int16_t currentIndex
);

static void UI_ClearConnections(void);

static uint8_t UI_AddConnection(
    uint16_t sourceId,
    uint16_t targetId
);

static int8_t UI_CompareItemPositions(
    const UI_Item *itemA,
    const UI_Item *itemB
);

static int16_t UI_FindValidOutputTargetAt(
    int16_t order,
    int16_t lane
);

static int16_t UI_FindValidInputSourceAt(
    int16_t order,
    int16_t lane
);

static int16_t UI_FindNextRoutingItemIndex(
    int16_t previousIndex
);

static int16_t UI_FindActiveItemAt(
    int16_t order,
    int16_t lane
);

static int16_t UI_FindInactiveAutoNodeIndex(void);

static int16_t UI_ActivateAutoNodeAt(
    int16_t order,
    int16_t lane
);

static void UI_InsertRoutingOrderBefore(
    int16_t insertionOrder
);

static void UI_RemoveAllAutoNodes(void);

static uint8_t UI_RebuildCalculatedConnections(void);

static void UI_InsertOrderBefore(
    int16_t insertionOrder,
    int16_t excludedItemIndex
);

static int16_t UI_FindInactiveManualNodeIndex(void);

static uint8_t UI_CreateManualNodeRightOfSelection(void);

typedef struct
{
    int16_t x;
    int16_t y;
} UI_ConnectionPoint;

typedef struct
{
    int16_t order;
    int16_t lane;
    UI_FocusState focus;

} UI_ItemPositionState;


static UI_ItemPositionState previousStepState[
    UI_ITEM_COUNT
];

static UI_ItemGeometry previousStepGeometry[
    UI_ITEM_COUNT
];

static UI_Connection previousStepConnections[
    UI_MAX_CONNECTIONS
];

static uint16_t previousStepConnectionCount = 0U;

static int16_t previousStepFirstVisibleOrder = 0;
static int16_t previousStepHighestVisibleLane = 1;

/*
 * Vollständiger Zustand unmittelbar vor dem letzten
 * Greifen eines Items. Return stellt diesen Zustand
 * unabhängig von der Anzahl der Bewegungen wieder her.
 */
static UI_ItemPositionState grabStartState[
    UI_ITEM_COUNT
];

static int16_t grabStartFirstVisibleOrder = 0;
static int16_t grabStartHighestVisibleLane = 1;
static uint8_t grabStartStateValid = 0U;

typedef enum
{
    UI_MODE_STATE_VIEW = 0,
    UI_MODE_MENU

} UI_Mode;

typedef enum
{
    UI_MENU_CONTROL_NAVIGATION = 0,
    UI_MENU_CONTROL_ITEM_SELECTION

} UI_MenuControlMode;

typedef enum
{
    UI_MENU_PAGE_ROOT = 0,
    UI_MENU_PAGE_DELETE_CONFIRM,
    UI_MENU_PAGE_GENERAL,
    UI_MENU_PAGE_SAVE_PRESET,
    UI_MENU_PAGE_SAVE_NAME

} UI_MenuPage;

static UI_Mode uiMode =
    UI_MODE_STATE_VIEW;

static UI_MenuControlMode uiMenuControlMode =
    UI_MENU_CONTROL_NAVIGATION;

static UI_MenuPage uiMenuPage =
    UI_MENU_PAGE_ROOT;

static int16_t uiMenuSelectedRow =
    UI_MENU_ITEM_ROW;

static int16_t uiMenuItemIndex = -1;
static int16_t uiMenuPreviewItemIndex = -1;
static int16_t uiMenuItemSelectionStartIndex = -1;


/* -------------------------------------------------------------------------- */
/* Save Preset: Konstanten und Zustand                                        */
/* -------------------------------------------------------------------------- */

/*
 * Die Preset-Liste belegt den ganzen Bildschirm (160 x 128):
 *
 *   y   2 ..  13   Bank-Zeile (immer sichtbar)
 *   y  16 .. 111   8 sichtbare Preset-Zeilen (scrollen)
 *   y 114          Trennlinie
 *   y 115 .. 127   Statuszeile
 */
#define UI_SAVE_ROW_HEIGHT            12U
#define UI_SAVE_ROW_WIDTH            154U
#define UI_SAVE_TEXT_X_OFFSET          4U
#define UI_SAVE_TEXT_Y_OFFSET          2U
#define UI_SAVE_HEADER_Y               2U
#define UI_SAVE_LIST_Y                16U
#define UI_SAVE_VISIBLE_PRESETS        8U
#define UI_SAVE_SEPARATOR_Y          114U
#define UI_SAVE_STATUS_Y             118U
#define UI_SAVE_TEXT_CAPACITY         26U

#define UI_SAVE_SCROLLBAR_X          157U
#define UI_SAVE_SCROLLBAR_WIDTH        3U
#define UI_SAVE_COLOR_TRACK          0x2104U

/* Zeile 0 = Bank, Zeilen 1 bis 24 = Presets. */
#define UI_SAVE_LAST_ROW              ((int16_t)PRESET_SLOTS_PER_BANK)

typedef enum
{
    UI_SAVE_STATUS_NONE = 0,
    UI_SAVE_STATUS_SAVING,
    UI_SAVE_STATUS_SAVED,
    UI_SAVE_STATUS_MEMORY_FULL,
    UI_SAVE_STATUS_ERROR,
    UI_SAVE_STATUS_NAME_HINT

} UI_SaveStatus;

/* Aktuell gewaehlte Bank (0 bis 63), bleibt beim Schliessen erhalten. */
static uint8_t uiPresetBank = 0U;
static uint8_t uiPresetBankBackup = 0U;
static uint8_t uiPresetBankEditing = 0U;

/* Index des ersten sichtbaren Presets (0 bis 16). */
static uint8_t uiPresetScrollTop = 0U;

static UI_SaveStatus uiSaveStatus = UI_SAVE_STATUS_NONE;


/* -------------------------------------------------------------------------- */
/* Save Preset: Untermenue mit Namenseditor                                   */
/* -------------------------------------------------------------------------- */

/*
 * Zeilen des Untermenues (y in Pixeln):
 *
 *   y   2          Kopfzeile "Bank n Preset nn"
 *   y  20 ..  35   Zeile 0: Name (editierbar)
 *   y  38 ..  53   Zeile 1: Save
 *   y  56 ..  71   Zeile 2: Cancel
 */
#define UI_NAME_ROW_NAME               0U
#define UI_NAME_ROW_SAVE               1U
#define UI_NAME_ROW_CANCEL             2U
#define UI_NAME_ROW_COUNT              3U

#define UI_NAME_FIRST_ROW_Y           20U
#define UI_NAME_ROW_PITCH             18U
#define UI_NAME_ROW_HEIGHT            16U
#define UI_NAME_TEXT_Y_OFFSET          4U

#define UI_NAME_MAX_LENGTH            (PRESET_NAME_LENGTH - 1U)

/* Zeichen, die der Encoder im Namenseditor durchlaeuft. */
static const char uiNameAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789- ";

#define UI_NAME_ALPHABET_SIZE \
    ((int8_t)(sizeof(uiNameAlphabet) - 1U))

static uint8_t uiNameSlot = 0U;
static uint8_t uiNameRow = UI_NAME_ROW_SAVE;

/* Bereits gesetzte Zeichen (nullterminiert). */
static char uiNameBuffer[PRESET_NAME_LENGTH];
static uint8_t uiNameLength = 0U;

/* Name vor dem Editieren, wird bei leerem Ergebnis wiederhergestellt. */
static char uiNameOriginal[PRESET_NAME_LENGTH];

/* Gerade ausgewaehltes Zeichen (Index in uiNameAlphabet), -1 = keines. */
static int8_t uiNameCandidate = -1;
static uint8_t uiNameEditing = 0U;

static void UI_DrawCircle(int16_t cx, int16_t cy, int16_t radius,
                          uint16_t color)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t error = 1 - radius;

    while (x >= y)
    {
        ST7735_DrawPixel(cx + x, cy + y, color);
        ST7735_DrawPixel(cx + y, cy + x, color);
        ST7735_DrawPixel(cx - y, cy + x, color);
        ST7735_DrawPixel(cx - x, cy + y, color);
        ST7735_DrawPixel(cx - x, cy - y, color);
        ST7735_DrawPixel(cx - y, cy - x, color);
        ST7735_DrawPixel(cx + y, cy - x, color);
        ST7735_DrawPixel(cx + x, cy - y, color);
        y++;
        if (error < 0)
        {
            error += (2 * y) + 1;
        }
        else
        {
            x--;
            error += (2 * (y - x)) + 1;
        }
    }
}

static void UI_FillCircle(int16_t cx, int16_t cy, int16_t radius,
                          uint16_t color)
{
    for (int16_t y = -radius; y <= radius; y++)
    {
        for (int16_t x = -radius; x <= radius; x++)
        {
            if ((x * x) + (y * y) <= (radius * radius))
            {
                ST7735_DrawPixel(cx + x, cy + y, color);
            }
        }
    }
}

static void UI_FillRoundedRect(int16_t x, int16_t y, uint16_t width,
                               uint16_t height, uint16_t radius,
                               uint16_t color)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    if (radius == 0)
    {
        ST7735_FillRect((uint16_t)x, (uint16_t)y, width, height, color);
        return;
    }

    ST7735_FillRect((uint16_t)(x + radius), (uint16_t)y,
                    width - (2 * radius), height, color);
    ST7735_FillRect((uint16_t)x, (uint16_t)(y + radius),
                    radius, height - (2 * radius), color);
    ST7735_FillRect((uint16_t)(x + width - radius),
                    (uint16_t)(y + radius), radius,
                    height - (2 * radius), color);

    for (uint16_t row = 0; row < radius; row++)
    {
        uint16_t inset = radius - row - 1;
        uint16_t lineWidth = width - (2 * inset);
        ST7735_FillRect((uint16_t)(x + inset), (uint16_t)(y + row),
                        lineWidth, 1, color);
        ST7735_FillRect((uint16_t)(x + inset),
                        (uint16_t)(y + height - 1 - row),
                        lineWidth, 1, color);
    }
}

static void UI_DrawRoundedRect(int16_t x, int16_t y, uint16_t width,
                               uint16_t height, uint16_t radius,
                               uint16_t color)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ST7735_DrawLine(x + radius, y,
                    x + width - radius - 1, y, color);
    ST7735_DrawLine(x + radius, y + height - 1,
                    x + width - radius - 1, y + height - 1, color);
    ST7735_DrawLine(x, y + radius,
                    x, y + height - radius - 1, color);
    ST7735_DrawLine(x + width - 1, y + radius,
                    x + width - 1, y + height - radius - 1, color);

    ST7735_DrawPixel(x + 1, y + 1, color);
    ST7735_DrawPixel(x + width - 2, y + 1, color);
    ST7735_DrawPixel(x + 1, y + height - 2, color);
    ST7735_DrawPixel(x + width - 2, y + height - 2, color);
}

static int32_t UI_TriangleEdge(int16_t ax, int16_t ay,
                               int16_t bx, int16_t by,
                               int16_t px, int16_t py)
{
    return ((int32_t)(px - ax) * (int32_t)(by - ay)) -
           ((int32_t)(py - ay) * (int32_t)(bx - ax));
}

static void UI_FillTriangle(int16_t x0, int16_t y0,
                            int16_t x1, int16_t y1,
                            int16_t x2, int16_t y2,
                            uint16_t color)
{
    int16_t minX = x0;
    int16_t maxX = x0;
    int16_t minY = y0;
    int16_t maxY = y0;

    if (x1 < minX) minX = x1;
    if (x2 < minX) minX = x2;
    if (x1 > maxX) maxX = x1;
    if (x2 > maxX) maxX = x2;
    if (y1 < minY) minY = y1;
    if (y2 < minY) minY = y2;
    if (y1 > maxY) maxY = y1;
    if (y2 > maxY) maxY = y2;

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= UI_DISPLAY_WIDTH) maxX = UI_DISPLAY_WIDTH - 1;
    if (maxY >= UI_DISPLAY_HEIGHT) maxY = UI_DISPLAY_HEIGHT - 1;

    for (int16_t y = minY; y <= maxY; y++)
    {
        for (int16_t x = minX; x <= maxX; x++)
        {
            int32_t e0 = UI_TriangleEdge(x0, y0, x1, y1, x, y);
            int32_t e1 = UI_TriangleEdge(x1, y1, x2, y2, x, y);
            int32_t e2 = UI_TriangleEdge(x2, y2, x0, y0, x, y);
            uint8_t hasNegative = (e0 < 0) || (e1 < 0) || (e2 < 0);
            uint8_t hasPositive = (e0 > 0) || (e1 > 0) || (e2 > 0);

            if (!(hasNegative && hasPositive))
            {
                ST7735_DrawPixel(x, y, color);
            }
        }
    }
}

static uint16_t UI_GetFillColor(const UI_Item *item)
{
    if (item == NULL || item->type != UI_ITEM_LOOP)
    {
        return UI_COLOR_BACKGROUND;
    }

    switch (item->loopStatus)
    {
        case UI_LOOP_STATUS_ACTIVE_CONFIRMED:
            return UI_COLOR_LOOP_CONFIRMED;
        case UI_LOOP_STATUS_ACTIVE_UNCONFIRMED:
            return UI_COLOR_LOOP_UNCONFIRMED;
        case UI_LOOP_STATUS_OFF:
        default:
            return UI_COLOR_LOOP_OFF;
    }
}

static uint16_t UI_GetFocusColor(const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_COLOR_BORDER_NORMAL;
    }

    switch (item->focus)
    {
        case UI_FOCUS_SELECTED:
            return UI_COLOR_BORDER_SELECTED;
        case UI_FOCUS_GRABBED:
            return UI_COLOR_BORDER_GRABBED;
        case UI_FOCUS_NONE:
        default:
            return UI_COLOR_BORDER_NORMAL;
    }
}

static uint16_t UI_GetTextColor(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return UI_COLOR_TEXT_LIGHT;
    }

    if (item->type == UI_ITEM_LOOP)
    {
        if (item->loopStatus ==
                UI_LOOP_STATUS_ACTIVE_CONFIRMED ||
            item->loopStatus ==
                UI_LOOP_STATUS_ACTIVE_UNCONFIRMED)
        {
            return UI_COLOR_TEXT_DARK;
        }
    }

    return UI_COLOR_TEXT_LIGHT;
}

static void UI_DrawCenteredText(uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height,
                                const char *text, uint16_t foreground,
                                uint16_t background, uint8_t scale)
{
    if (text == NULL)
    {
        return;
    }

    uint16_t textWidth = Font5x7_GetStringWidth(text, scale);
    uint16_t textHeight = Font5x7_GetHeight(scale);
    uint16_t textX = x;
    uint16_t textY = y;

    if (textWidth < width)
    {
        textX = x + ((width - textWidth) / 2);
    }

    if (textHeight < height)
    {
        textY = y + ((height - textHeight) / 2);
    }

    Font5x7_DrawString(textX, textY, text,
                       foreground, background, scale);
}

static int16_t UI_GetElementX(int16_t visibleOrder)
{
    return UI_GRID_LEFT +
           (visibleOrder * (UI_ELEMENT_WIDTH + UI_CONNECTION_WIDTH));
}

static int16_t UI_GetLowestVisibleLane(void)
{
    return
        uiHighestVisibleLane -
        UI_VISIBLE_LANE_COUNT +
        1;
}


static uint8_t UI_IsLaneVisible(int16_t lane)
{
    return
        (lane <= uiHighestVisibleLane &&
         lane >= UI_GetLowestVisibleLane()) ?
        1U :
        0U;
}


static int16_t UI_GetLaneY(int16_t lane)
{
    int16_t visibleLaneIndex =
        uiHighestVisibleLane - lane;

    return
        UI_GRID_TOP +
        (visibleLaneIndex *
         (UI_ELEMENT_HEIGHT +
          UI_VERTICAL_CONNECTION_HEIGHT));
}


static uint8_t UI_EnsureItemVerticallyVisible(
    int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >=
            (int16_t)UI_ITEM_COUNT)
    {
        return 0U;
    }

    if (uiItems[itemIndex].order < 0)
    {
        return 0U;
    }

    int16_t previousHighestVisibleLane =
        uiHighestVisibleLane;

    int16_t itemLane =
        uiItems[itemIndex].lane;

    int16_t lowestVisibleLane =
        UI_GetLowestVisibleLane();

    /*
     * Item liegt oberhalb des Viewports:
     * am oberen Rand anzeigen.
     */
    if (itemLane >
        uiHighestVisibleLane)
    {
        uiHighestVisibleLane =
            itemLane;
    }

    /*
     * Item liegt unterhalb des Viewports:
     * am unteren Rand anzeigen.
     */
    else if (itemLane <
             lowestVisibleLane)
    {
        uiHighestVisibleLane =
            itemLane +
            UI_VISIBLE_LANE_COUNT -
            1;
    }

    return
        (uiHighestVisibleLane !=
         previousHighestVisibleLane) ?
        1U :
        0U;
}

static int16_t UI_GetMaximumActiveOrder(void)
{
    int16_t maximumOrder = -1;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        /*
         * Entfernte automatische Knoten besitzen
         * beispielsweise order = -100 und werden
         * deshalb ignoriert.
         */
        if (uiItems[i].order < 0)
        {
            continue;
        }


        if (uiItems[i].order >
            maximumOrder)
        {
            maximumOrder =
                uiItems[i].order;
        }
    }


    return maximumOrder;
}

static uint8_t UI_OrderContainsItemType(
    int16_t order,
    UI_ItemType itemType)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].order != order)
        {
            continue;
        }

        if (uiItems[i].type == itemType)
        {
            return 1;
        }
    }

    return 0;
}

static uint8_t UI_TryBlockedEdgeScroll(
    int16_t grabbedIndex,
    int8_t direction)
{
    if (grabbedIndex < 0 ||
        grabbedIndex >=
            (int16_t)UI_ITEM_COUNT)
    {
        return 0;
    }

    if (direction == 0)
    {
        return 0;
    }

    const UI_Item *grabbedItem =
        &uiItems[grabbedIndex];

    if (grabbedItem->focus !=
        UI_FOCUS_GRABBED)
    {
        return 0;
    }

    if (grabbedItem->order < 0)
    {
        return 0;
    }

    int16_t visibleOrder =
        grabbedItem->order -
        uiFirstVisibleOrder;

    int16_t maximumOrder =
        UI_GetMaximumActiveOrder();


    /*
     * Sonderfall rechts:
     *
     * Das gegriffene Item steht im letzten
     * sichtbaren Slot. In der nächsten Order
     * befindet sich die blockierende OUT-Spalte.
     */
    if (direction > 0)
    {
        if (visibleOrder !=
            UI_VISIBLE_ELEMENT_COLUMNS - 1)
        {
            return 0;
        }

        int16_t blockingOrder =
            grabbedItem->order + 1;

        /*
         * Die nächste Order muss existieren und
         * die äußerste rechte Order sein.
         */
        if (blockingOrder != maximumOrder)
        {
            return 0;
        }

        if (!UI_OrderContainsItemType(
                blockingOrder,
                UI_ITEM_OUTPUT))
        {
            return 0;
        }

        int16_t oldViewportStart =
            uiFirstVisibleOrder;

        uiFirstVisibleOrder++;

        UI_ClampHorizontalViewport();

        if (uiFirstVisibleOrder ==
            oldViewportStart)
        {
            return 0;
        }

        /*
         * Nur die Ansicht hat sich verändert.
         * Itempositionen und Verbindungen bleiben
         * logisch unverändert.
         */
        UI_Draw();

        return 1;
    }


    /*
     * Sonderfall links:
     *
     * Das gegriffene Item steht im ersten
     * sichtbaren Slot. In der vorherigen Order
     * befindet sich die blockierende IN-Spalte.
     */
    if (visibleOrder != 0)
    {
        return 0;
    }

    int16_t blockingOrder =
        grabbedItem->order - 1;

    if (blockingOrder < 0)
    {
        return 0;
    }

    /*
     * Die blockierende IN-Spalte muss die
     * äußerste linke Order sein.
     */
    int16_t minimumOrder =
        UI_GetMinimumPermanentOrder();

    if (blockingOrder != minimumOrder)
    {
        return 0;
    }

    if (!UI_OrderContainsItemType(
            blockingOrder,
            UI_ITEM_INPUT))
    {
        return 0;
    }

    int16_t oldViewportStart =
        uiFirstVisibleOrder;

    uiFirstVisibleOrder--;

    UI_ClampHorizontalViewport();

    if (uiFirstVisibleOrder ==
        oldViewportStart)
    {
        return 0;
    }

    UI_Draw();

    return 1;
}

static void UI_ClampHorizontalViewport(void)
{
    int16_t maximumOrder =
        UI_GetMaximumActiveOrder();


    if (maximumOrder < 0)
    {
        uiFirstVisibleOrder = 0;
        return;
    }


    int16_t maximumFirstVisibleOrder =
        maximumOrder -
        UI_VISIBLE_ELEMENT_COLUMNS +
        1;


    if (maximumFirstVisibleOrder < 0)
    {
        maximumFirstVisibleOrder = 0;
    }


    if (uiFirstVisibleOrder < 0)
    {
        uiFirstVisibleOrder = 0;
    }


    if (uiFirstVisibleOrder >
        maximumFirstVisibleOrder)
    {
        uiFirstVisibleOrder =
            maximumFirstVisibleOrder;
    }
}

static uint8_t UI_EnsureItemVisible(
    int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >=
            (int16_t)UI_ITEM_COUNT)
    {
        return 0;
    }


    if (uiItems[itemIndex].order < 0)
    {
        return 0;
    }


    int16_t previousViewportStart =
        uiFirstVisibleOrder;

    int16_t itemOrder =
        uiItems[itemIndex].order;


    /*
     * Item liegt links außerhalb des Viewports.
     *
     * Das Item wird in den ganz linken Slot
     * gescrollt.
     */
    if (itemOrder <
        uiFirstVisibleOrder)
    {
        uiFirstVisibleOrder =
            itemOrder;
    }


    /*
     * Item liegt rechts außerhalb des Viewports.
     *
     * Das Item wird in den ganz rechten sichtbaren
     * Slot gescrollt.
     */
    else if (itemOrder >=
             uiFirstVisibleOrder +
             UI_VISIBLE_ELEMENT_COLUMNS)
    {
        uiFirstVisibleOrder =
            itemOrder -
            UI_VISIBLE_ELEMENT_COLUMNS +
            1;
    }


    UI_ClampHorizontalViewport();


    return
        (uiFirstVisibleOrder !=
         previousViewportStart) ?
        1 :
        0;
}

static void UI_CalculateGeometry(void)
{
    UI_ClampHorizontalViewport();


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uiGeometry[i].itemId =
            uiItems[i].id;

        uiGeometry[i].width =
            UI_ELEMENT_WIDTH;

        uiGeometry[i].height =
            UI_ELEMENT_HEIGHT;

        uiGeometry[i].visible = 0;


        if (uiItems[i].order < 0 ||
            !UI_IsLaneVisible(
                uiItems[i].lane))
        {
            continue;
        }


        /*
         * Absolute logische order in einen
         * sichtbaren Slot umrechnen.
         */
        int16_t visibleOrder =
            uiItems[i].order -
            uiFirstVisibleOrder;


        if (visibleOrder < 0 ||
            visibleOrder >=
                UI_VISIBLE_ELEMENT_COLUMNS)
        {
            continue;
        }


        uiGeometry[i].x =
            UI_GetElementX(
                visibleOrder
            );

        uiGeometry[i].y =
            UI_GetLaneY(
                uiItems[i].lane
            );

        uiGeometry[i].visible = 1;
    }
}

static int16_t UI_FindItemIndexById(uint16_t itemId)
{
    for (uint16_t i = 0; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].id == itemId)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


/* -------------------------------------------------------------------------- */
/* Clear one item area                                                        */
/* -------------------------------------------------------------------------- */

static void UI_EraseFocusFrame(
    int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return;
    }


    if (!uiGeometry[itemIndex].visible)
    {
        return;
    }


    const UI_Item *item =
        &uiItems[itemIndex];

    const UI_ItemGeometry *geometry =
        &uiGeometry[itemIndex];


    int16_t centerX =
        geometry->x +
        (geometry->width / 2);

    int16_t centerY =
        geometry->y +
        (geometry->height / 2);


    switch (item->type)
    {
        case UI_ITEM_LOOP:
        {
            /*
             * Nur den äußeren Fokusrahmen löschen.
             *
             * Der eigentliche Loopkasten bleibt
             * vollständig unangetastet.
             */
            UI_DrawRoundedRect(
                geometry->x -
                    UI_FOCUS_FRAME_GAP,

                geometry->y -
                    UI_FOCUS_FRAME_GAP,

                geometry->width +
                    (2 * UI_FOCUS_FRAME_GAP),

                geometry->height +
                    (2 * UI_FOCUS_FRAME_GAP),

                UI_FOCUS_CORNER_RADIUS,

                UI_COLOR_BACKGROUND
            );

            break;
        }


        case UI_ITEM_MANUAL_NODE:
        {
            /*
             * Äußeren Fokusdiamanten löschen.
             */
            int16_t radius =
                UI_MANUAL_NODE_RADIUS +
                UI_FOCUS_FRAME_GAP;


            ST7735_DrawLine(
                centerX,
                centerY - radius,
                centerX + radius,
                centerY,
                UI_COLOR_BACKGROUND
            );

            ST7735_DrawLine(
                centerX + radius,
                centerY,
                centerX,
                centerY + radius,
                UI_COLOR_BACKGROUND
            );

            ST7735_DrawLine(
                centerX,
                centerY + radius,
                centerX - radius,
                centerY,
                UI_COLOR_BACKGROUND
            );

            ST7735_DrawLine(
                centerX - radius,
                centerY,
                centerX,
                centerY - radius,
                UI_COLOR_BACKGROUND
            );

            break;
        }


        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
        {
            /*
             * IN und OUT besitzen keinen separaten Rahmen.
             *
             * Der Kreis wird später mit der normalen
             * weißen Farbe neu gezeichnet.
             */
            break;
        }


        case UI_ITEM_AUTO_NODE:
        default:
            break;
    }
}

static void UI_ClearGeometryArea(
    const UI_ItemGeometry *geometry)
{
    if (geometry == NULL ||
        !geometry->visible)
    {
        return;
    }

    int16_t x =
        geometry->x -
        UI_FOCUS_FRAME_GAP -
        1;

    int16_t y =
        geometry->y -
        UI_FOCUS_FRAME_GAP -
        1;

    int16_t width =
        geometry->width +
        (2 * UI_FOCUS_FRAME_GAP) +
        2;

    int16_t height =
        geometry->height +
        (2 * UI_FOCUS_FRAME_GAP) +
        2;


    if (x < 0)
    {
        width += x;
        x = 0;
    }

    if (y < 0)
    {
        height += y;
        y = 0;
    }

    if ((x + width) >
        UI_DISPLAY_WIDTH)
    {
        width =
            UI_DISPLAY_WIDTH - x;
    }

    if ((y + height) >
        UI_FOOTER_TOP)
    {
        height =
            UI_FOOTER_TOP - y;
    }

    if (width <= 0 ||
        height <= 0)
    {
        return;
    }


    ST7735_FillRect(
        (uint16_t)x,
        (uint16_t)y,
        (uint16_t)width,
        (uint16_t)height,
        UI_COLOR_BACKGROUND
    );
}

/* -------------------------------------------------------------------------- */
/* Redraw one item                                                            */
/* -------------------------------------------------------------------------- */



static void UI_RedrawItem(int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return;
    }


    if (!uiGeometry[itemIndex].visible)
    {
        return;
    }


    UI_DrawItem(
        &uiItems[itemIndex],
        uiGeometry[itemIndex].x,
        uiGeometry[itemIndex].y
    );
}

/* -------------------------------------------------------------------------- */
/* Partial selection update                                                   */
/* -------------------------------------------------------------------------- */

static void UI_UpdateSelectionDisplay(
    int16_t oldIndex,
    int16_t newIndex)
{
    /*
     * Alten äußeren Auswahl- oder Greifrahmen
     * gezielt entfernen.
     */
    UI_EraseFocusFrame(oldIndex);


    /*
     * Verbindungen des alten Elements können durch
     * das Entfernen des Fokusrahmens teilweise
     * überzeichnet worden sein.
     */
    UI_DrawConnectionsForItem(
        oldIndex
    );


    /*
     * Verbindungen des neuen Elements vorsorglich
     * vor dem Item neu zeichnen.
     */
    if (newIndex != oldIndex)
    {
        UI_DrawConnectionsForItem(
            newIndex
        );
    }


    /*
     * Items werden über den Verbindungslinien
     * gezeichnet.
     */
    UI_RedrawItem(
        oldIndex
    );


    if (newIndex != oldIndex)
    {
        UI_RedrawItem(
            newIndex
        );
    }


    /*
     * Footerbereich aktualisieren.
     */
    UI_DrawFooter();
}


/* -------------------------------------------------------------------------- */
/* Selection                                                                  */
/* -------------------------------------------------------------------------- */

static uint8_t UI_IsSelectable(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }

if (item->order < 0)
{
    return 0;
}
    
    switch (item->type)
    {
        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
        case UI_ITEM_LOOP:
        case UI_ITEM_MANUAL_NODE:
            return 1;

        case UI_ITEM_AUTO_NODE:
        default:
            return 0;
    }
}

static uint8_t UI_IsPermanentItem(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }

if (item->order < 0)
{
    return 0;
}

    
    switch (item->type)
    {
        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
        case UI_ITEM_LOOP:
        case UI_ITEM_MANUAL_NODE:
            return 1;

        case UI_ITEM_AUTO_NODE:
        default:
            return 0;
    }
}

static void UI_ClearConnections(void)
{
    uiConnectionCount = 0U;
}

static uint8_t UI_AddConnection(
    uint16_t sourceId,
    uint16_t targetId)
{
    /*
     * Ungültige Selbstverbindung verhindern.
     */
    if (sourceId == targetId)
    {
        return 0;
    }


    /*
     * Verbindung nicht doppelt eintragen.
     */
    for (uint16_t i = 0;
         i < uiConnectionCount;
         i++)
    {
        if (uiConnections[i].sourceId ==
                sourceId &&
            uiConnections[i].targetId ==
                targetId)
        {
            return 1;
        }
    }


    /*
     * Puffergrenze prüfen.
     */
    if (uiConnectionCount >=
        UI_MAX_CONNECTIONS)
    {
        return 0;
    }

    if (uiConnectionCount >=
        UI_CONNECTION_CAPACITY)
    {
        return 0;
    }


    uiConnections[
        uiConnectionCount
    ].sourceId =
        sourceId;

    uiConnections[
        uiConnectionCount
    ].targetId =
        targetId;


    uiConnectionCount++;


    return 1;
}

static uint8_t UI_IsValidOutputTarget(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }


    /*
     * Ein Pfeil darf niemals an einem Input enden.
     */
    if (item->type == UI_ITEM_INPUT)
    {
        return 0;
    }


    /*
     * Permanente Items sind gültige Ziele.
     *
     * Automatische Knoten werden später ebenfalls
     * gültige Ziele. Momentan existieren nach einer
     * Neuberechnung jedoch keine Auto-Knoten.
     */
    if (UI_IsPermanentItem(item))
    {
        return 1;
    }


    if (item->type == UI_ITEM_AUTO_NODE &&
        item->order >= 0)
    {
        return 1;
    }


    return 0;
}

static uint8_t UI_IsValidInputSource(
    const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }


    /*
     * Ein Pfeil darf niemals an einem Output
     * beginnen.
     */
    if (item->type == UI_ITEM_OUTPUT)
    {
        return 0;
    }


    if (UI_IsPermanentItem(item))
    {
        return 1;
    }


    if (item->type == UI_ITEM_AUTO_NODE &&
        item->order >= 0)
    {
        return 1;
    }


    return 0;
}

static int16_t UI_FindValidOutputTargetAt(
    int16_t order,
    int16_t lane)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsValidOutputTarget(
                &uiItems[i]))
        {
            continue;
        }


        if (uiItems[i].order == order &&
            uiItems[i].lane == lane)
        {
            return (int16_t)i;
        }
    }


    return -1;
}

static int16_t UI_FindValidInputSourceAt(
    int16_t order,
    int16_t lane)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsValidInputSource(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order == order &&
            uiItems[i].lane == lane)
        {
            return (int16_t)i;
        }
    }

    return -1;
}

static uint8_t UI_HasIncomingConnection(
    uint16_t itemId)
{
    for (uint16_t i = 0;
         i < uiConnectionCount;
         i++)
    {
        if (uiConnections[i].targetId ==
            itemId)
        {
            return 1;
        }
    }


    return 0;
}

static int16_t UI_FindActiveItemAt(
    int16_t order,
    int16_t lane)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].order < 0)
        {
            continue;
        }

        if (uiItems[i].order == order &&
            uiItems[i].lane == lane)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


static int16_t UI_FindInactiveAutoNodeIndex(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].type !=
            UI_ITEM_AUTO_NODE)
        {
            continue;
        }

        if (uiItems[i].order < 0)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


static int16_t UI_ActivateAutoNodeAt(
    int16_t order,
    int16_t lane)
{
    int16_t existingIndex =
        UI_FindActiveItemAt(
            order,
            lane
        );

    if (existingIndex >= 0)
    {
        return
            (uiItems[existingIndex].type ==
             UI_ITEM_AUTO_NODE) ?
            existingIndex :
            -1;
    }

    int16_t autoNodeIndex =
        UI_FindInactiveAutoNodeIndex();

    if (autoNodeIndex < 0)
    {
        return -1;
    }

    uiItems[autoNodeIndex].order = order;
    uiItems[autoNodeIndex].lane = lane;
    uiItems[autoNodeIndex].focus =
        UI_FOCUS_NONE;
    uiItems[autoNodeIndex].loopStatus =
        UI_LOOP_STATUS_OFF;

    return autoNodeIndex;
}


static void UI_InsertRoutingOrderBefore(
    int16_t insertionOrder)
{
    /*
     * Unlike UI_InsertOrderBefore(), this function also moves
     * active automatic nodes. It is used only while the derived
     * routing structure is being built.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].order < 0)
        {
            continue;
        }

        if (uiItems[i].order >=
            insertionOrder)
        {
            uiItems[i].order++;
        }
    }
}


static uint8_t UI_FindNearestOutputDirections(
    uint16_t sourceIndex,
    int8_t directions[2],
    uint8_t *directionCount)
{
    const UI_Item *sourceItem =
        &uiItems[sourceIndex];

    int16_t bestOrderDistance = 32767;
    int16_t bestLaneDistance = 32767;
    uint8_t foundPositive = 0;
    uint8_t foundNegative = 0;
    uint8_t foundSameLane = 0;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsValidOutputTarget(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order <=
            sourceItem->order)
        {
            continue;
        }

        int16_t orderDistance =
            uiItems[i].order -
            sourceItem->order;

        int16_t laneDelta =
            uiItems[i].lane -
            sourceItem->lane;

        int16_t laneDistance =
            (laneDelta >= 0) ?
            laneDelta :
            -laneDelta;

        if (orderDistance < bestOrderDistance ||
            (orderDistance == bestOrderDistance &&
             laneDistance < bestLaneDistance))
        {
            bestOrderDistance = orderDistance;
            bestLaneDistance = laneDistance;
            foundPositive = 0;
            foundNegative = 0;
            foundSameLane = 0;
        }

        if (orderDistance != bestOrderDistance ||
            laneDistance != bestLaneDistance)
        {
            continue;
        }

        if (laneDelta > 0)
        {
            foundPositive = 1;
        }
        else if (laneDelta < 0)
        {
            foundNegative = 1;
        }
        else
        {
            foundSameLane = 1;
        }
    }

    *directionCount = 0;

    if (bestOrderDistance == 32767)
    {
        return 1;
    }

    if (foundSameLane)
    {
        directions[0] = 0;
        *directionCount = 1;
        return 1;
    }

    if (foundPositive && foundNegative)
    {
        if (sourceItem->lane > 0)
        {
            directions[0] = -1;
            *directionCount = 1;
        }
        else if (sourceItem->lane < 0)
        {
            directions[0] = 1;
            *directionCount = 1;
        }
        else
        {
            directions[0] = 1;
            directions[1] = -1;
            *directionCount = 2;
        }

        return 1;
    }

    directions[0] =
        foundPositive ? 1 : -1;
    *directionCount = 1;

    return 1;
}


static uint8_t UI_FindNearestInputDirections(
    uint16_t targetIndex,
    int8_t directions[2],
    uint8_t *directionCount)
{
    const UI_Item *targetItem =
        &uiItems[targetIndex];

    int16_t bestOrderDistance = 32767;
    int16_t bestLaneDistance = 32767;
    uint8_t foundPositive = 0;
    uint8_t foundNegative = 0;
    uint8_t foundSameLane = 0;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsValidInputSource(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order >=
            targetItem->order)
        {
            continue;
        }

        int16_t orderDistance =
            targetItem->order -
            uiItems[i].order;

        int16_t laneDelta =
            uiItems[i].lane -
            targetItem->lane;

        int16_t laneDistance =
            (laneDelta >= 0) ?
            laneDelta :
            -laneDelta;

        if (orderDistance < bestOrderDistance ||
            (orderDistance == bestOrderDistance &&
             laneDistance < bestLaneDistance))
        {
            bestOrderDistance = orderDistance;
            bestLaneDistance = laneDistance;
            foundPositive = 0;
            foundNegative = 0;
            foundSameLane = 0;
        }

        if (orderDistance != bestOrderDistance ||
            laneDistance != bestLaneDistance)
        {
            continue;
        }

        if (laneDelta > 0)
        {
            foundPositive = 1;
        }
        else if (laneDelta < 0)
        {
            foundNegative = 1;
        }
        else
        {
            foundSameLane = 1;
        }
    }

    *directionCount = 0;

    if (bestOrderDistance == 32767)
    {
        return 1;
    }

    if (foundSameLane)
    {
        directions[0] = 0;
        *directionCount = 1;
        return 1;
    }

    if (foundPositive && foundNegative)
    {
        if (targetItem->lane > 0)
        {
            directions[0] = -1;
            *directionCount = 1;
        }
        else if (targetItem->lane < 0)
        {
            directions[0] = 1;
            *directionCount = 1;
        }
        else
        {
            directions[0] = 1;
            directions[1] = -1;
            *directionCount = 2;
        }

        return 1;
    }

    directions[0] =
        foundPositive ? 1 : -1;
    *directionCount = 1;

    return 1;
}


static uint8_t UI_ConnectOutputThroughAutoNode(
    uint16_t sourceIndex,
    int8_t direction,
    uint8_t *structureChanged)
{
    const UI_Item *sourceItem =
        &uiItems[sourceIndex];

    int16_t targetOrder =
        sourceItem->order + 1;

    if (targetOrder >=
        UI_MAX_ORDER_COUNT)
    {
        return 0;
    }

    /*
     * Sonderregel für die ganz rechte Order:
     *
     * Ein Auto-Node darf nicht in derselben äußersten
     * Order wie die Outputs entstehen. Auch bei freier
     * Zielposition hätte der Knoten rechts kein Ziel.
     *
     * Deshalb wird ausschließlich in diesem Randfall
     * eine neue Order direkt vor der Output-Order
     * eingefügt. Die Output-Order wandert nach rechts;
     * targetOrder bezeichnet danach die neue Order.
     */
    int16_t maximumPermanentOrder =
        UI_GetMaximumPermanentOrder();

    if (targetOrder ==
        maximumPermanentOrder)
    {
        if (maximumPermanentOrder + 1 >=
            UI_MAX_ORDER_COUNT)
        {
            return 0;
        }

        UI_InsertRoutingOrderBefore(
            targetOrder
        );

        *structureChanged = 1U;
    }

    int16_t candidateLanes[3];
    uint8_t candidateCount = 0;

    candidateLanes[candidateCount++] =
        sourceItem->lane + direction;

    if (sourceItem->lane !=
        candidateLanes[0])
    {
        candidateLanes[candidateCount++] =
            sourceItem->lane;
    }

    int16_t oppositeLane =
        sourceItem->lane - direction;

    uint8_t oppositeAlreadyPresent = 0;

    for (uint8_t i = 0;
         i < candidateCount;
         i++)
    {
        if (candidateLanes[i] ==
            oppositeLane)
        {
            oppositeAlreadyPresent = 1;
        }
    }

    if (!oppositeAlreadyPresent &&
        candidateCount < 3)
    {
        candidateLanes[candidateCount++] =
            oppositeLane;
    }

    for (uint8_t candidate = 0;
         candidate < candidateCount;
         candidate++)
    {
        int16_t itemIndex =
            UI_FindActiveItemAt(
                targetOrder,
                candidateLanes[candidate]
            );

        if (itemIndex < 0)
        {
            int16_t autoNodeIndex =
                UI_ActivateAutoNodeAt(
                    targetOrder,
                    candidateLanes[candidate]
                );

            if (autoNodeIndex < 0)
            {
                return 0;
            }

            *structureChanged = 1;

            return UI_AddConnection(
                sourceItem->id,
                uiItems[autoNodeIndex].id
            );
        }

        if (uiItems[itemIndex].type ==
            UI_ITEM_AUTO_NODE)
        {
            return UI_AddConnection(
                sourceItem->id,
                uiItems[itemIndex].id
            );
        }

        if (UI_IsValidOutputTarget(
                &uiItems[itemIndex]))
        {
            return UI_AddConnection(
                sourceItem->id,
                uiItems[itemIndex].id
            );
        }

        /*
         * An INPUT is invalid in the output direction.
         * Continue with the next candidate lane.
         */
    }

    /*
     * All candidate positions are blocked by invalid INPUTs.
     * Insert a new column before the original target order.
     */
    UI_InsertRoutingOrderBefore(
        targetOrder
    );

    int16_t autoNodeIndex =
        UI_ActivateAutoNodeAt(
            targetOrder,
            sourceItem->lane + direction
        );

    if (autoNodeIndex < 0)
    {
        return 0;
    }

    *structureChanged = 1;

    return UI_AddConnection(
        sourceItem->id,
        uiItems[autoNodeIndex].id
    );
}


static uint8_t UI_ConnectInputThroughAutoNode(
    uint16_t targetIndex,
    int8_t direction,
    uint8_t *structureChanged)
{
    UI_Item *targetItem =
        &uiItems[targetIndex];

    int16_t sourceOrder =
        targetItem->order - 1;

    if (sourceOrder < 0)
    {
        return 1;
    }

    /*
     * Sonderregel für die ganz linke Order:
     *
     * Ein Auto-Node darf nicht in derselben äußersten
     * Order wie die Inputs entstehen. Auch bei freier
     * Zielposition hätte der Knoten links keine Quelle.
     *
     * Deshalb wird ausschließlich in diesem Randfall
     * direkt vor dem Ziel eine neue Order eingefügt.
     * Die Input-Order bleibt links unverändert, das
     * Ziel wandert nach rechts und der Auto-Node wird
     * in der neuen Order dazwischen erzeugt.
     */
    int16_t minimumPermanentOrder =
        UI_GetMinimumPermanentOrder();

    if (sourceOrder ==
        minimumPermanentOrder)
    {
        if (UI_GetMaximumPermanentOrder() + 1 >=
            UI_MAX_ORDER_COUNT)
        {
            return 0;
        }

        int16_t insertionOrder =
            targetItem->order;

        UI_InsertRoutingOrderBefore(
            insertionOrder
        );

        /*
         * targetItem zeigt weiterhin auf dasselbe
         * Arrayelement. Dessen Order wurde durch das
         * Einfügen bereits um eins erhöht.
         */
        sourceOrder =
            targetItem->order - 1;

        *structureChanged = 1U;
    }

    int16_t candidateLanes[3];
    uint8_t candidateCount = 0;

    candidateLanes[candidateCount++] =
        targetItem->lane + direction;

    if (targetItem->lane !=
        candidateLanes[0])
    {
        candidateLanes[candidateCount++] =
            targetItem->lane;
    }

    int16_t oppositeLane =
        targetItem->lane - direction;

    uint8_t oppositeAlreadyPresent = 0;

    for (uint8_t i = 0;
         i < candidateCount;
         i++)
    {
        if (candidateLanes[i] ==
            oppositeLane)
        {
            oppositeAlreadyPresent = 1;
        }
    }

    if (!oppositeAlreadyPresent &&
        candidateCount < 3)
    {
        candidateLanes[candidateCount++] =
            oppositeLane;
    }

    for (uint8_t candidate = 0;
         candidate < candidateCount;
         candidate++)
    {
        int16_t itemIndex =
            UI_FindActiveItemAt(
                sourceOrder,
                candidateLanes[candidate]
            );

        if (itemIndex < 0)
        {
            int16_t autoNodeIndex =
                UI_ActivateAutoNodeAt(
                    sourceOrder,
                    candidateLanes[candidate]
                );

            if (autoNodeIndex < 0)
            {
                return 0;
            }

            *structureChanged = 1;

            return UI_AddConnection(
                uiItems[autoNodeIndex].id,
                targetItem->id
            );
        }

        if (uiItems[itemIndex].type ==
            UI_ITEM_AUTO_NODE)
        {
            return UI_AddConnection(
                uiItems[itemIndex].id,
                targetItem->id
            );
        }

        if (UI_IsValidInputSource(
                &uiItems[itemIndex]))
        {
            return UI_AddConnection(
                uiItems[itemIndex].id,
                targetItem->id
            );
        }

        /*
         * An OUTPUT is invalid in the input direction.
         * Continue with the next candidate lane.
         */
    }

    /*
     * All candidates are blocked by invalid OUTPUTs.
     * Insert a new column immediately before the target item.
     */
    int16_t insertionOrder =
        targetItem->order;

    UI_InsertRoutingOrderBefore(
        insertionOrder
    );

    int16_t autoNodeIndex =
        UI_ActivateAutoNodeAt(
            insertionOrder,
            targetItem->lane + direction
        );

    if (autoNodeIndex < 0)
    {
        return 0;
    }

    *structureChanged = 1;

    return UI_AddConnection(
        uiItems[autoNodeIndex].id,
        targetItem->id
    );
}


static uint8_t UI_CalculateOutputForItem(
    uint16_t sourceIndex,
    uint8_t *structureChanged)
{
    if (sourceIndex >= UI_ITEM_COUNT ||
        structureChanged == NULL)
    {
        return 0;
    }

    const UI_Item *sourceItem =
        &uiItems[sourceIndex];

    if (sourceItem->type ==
        UI_ITEM_OUTPUT)
    {
        return 1;
    }

    if (!UI_IsPermanentItem(sourceItem) &&
        sourceItem->type !=
            UI_ITEM_AUTO_NODE)
    {
        return 1;
    }

    if (sourceItem->order < 0)
    {
        return 1;
    }

    int16_t targetOrder =
        sourceItem->order + 1;

    int16_t sameLaneTargetIndex =
        UI_FindValidOutputTargetAt(
            targetOrder,
            sourceItem->lane
        );

    if (sameLaneTargetIndex >= 0)
    {
        return UI_AddConnection(
            sourceItem->id,
            uiItems[sameLaneTargetIndex].id
        );
    }

    int16_t upperTargetIndex =
        UI_FindValidOutputTargetAt(
            targetOrder,
            sourceItem->lane + 1
        );

    int16_t lowerTargetIndex =
        UI_FindValidOutputTargetAt(
            targetOrder,
            sourceItem->lane - 1
        );

    uint8_t directTargetFound = 0;

    if (upperTargetIndex >= 0)
    {
        directTargetFound = 1;

        if (!UI_AddConnection(
                sourceItem->id,
                uiItems[upperTargetIndex].id))
        {
            return 0;
        }
    }

    if (lowerTargetIndex >= 0)
    {
        directTargetFound = 1;

        if (!UI_AddConnection(
                sourceItem->id,
                uiItems[lowerTargetIndex].id))
        {
            return 0;
        }
    }

    if (directTargetFound)
    {
        return 1;
    }

    int8_t directions[2] = {0, 0};
    uint8_t directionCount = 0;

    if (!UI_FindNearestOutputDirections(
            sourceIndex,
            directions,
            &directionCount))
    {
        return 0;
    }

    for (uint8_t i = 0;
         i < directionCount;
         i++)
    {
        if (!UI_ConnectOutputThroughAutoNode(
                sourceIndex,
                directions[i],
                structureChanged))
        {
            return 0;
        }
    }

    return 1;
}


static uint8_t UI_CalculateMissingInputForItem(
    uint16_t targetIndex,
    uint8_t *structureChanged)
{
    if (targetIndex >= UI_ITEM_COUNT ||
        structureChanged == NULL)
    {
        return 0;
    }

    const UI_Item *targetItem =
        &uiItems[targetIndex];

    if (targetItem->type ==
        UI_ITEM_INPUT)
    {
        return 1;
    }

    if (!UI_IsPermanentItem(targetItem) &&
        targetItem->type !=
            UI_ITEM_AUTO_NODE)
    {
        return 1;
    }

    if (targetItem->order < 0)
    {
        return 1;
    }

    if (UI_HasIncomingConnection(
            targetItem->id))
    {
        return 1;
    }

    int16_t sourceOrder =
        targetItem->order - 1;

    if (sourceOrder < 0)
    {
        return 1;
    }

    int16_t upperSourceIndex =
        UI_FindValidInputSourceAt(
            sourceOrder,
            targetItem->lane + 1
        );

    int16_t lowerSourceIndex =
        UI_FindValidInputSourceAt(
            sourceOrder,
            targetItem->lane - 1
        );

    uint8_t directSourceFound = 0;

    if (upperSourceIndex >= 0)
    {
        directSourceFound = 1;

        if (!UI_AddConnection(
                uiItems[upperSourceIndex].id,
                targetItem->id))
        {
            return 0;
        }
    }

    if (lowerSourceIndex >= 0)
    {
        directSourceFound = 1;

        if (!UI_AddConnection(
                uiItems[lowerSourceIndex].id,
                targetItem->id))
        {
            return 0;
        }
    }

    if (directSourceFound)
    {
        return 1;
    }

    int8_t directions[2] = {0, 0};
    uint8_t directionCount = 0;

    if (!UI_FindNearestInputDirections(
            targetIndex,
            directions,
            &directionCount))
    {
        return 0;
    }

    for (uint8_t i = 0;
         i < directionCount;
         i++)
    {
        if (!UI_ConnectInputThroughAutoNode(
                targetIndex,
                directions[i],
                structureChanged))
        {
            return 0;
        }
    }

    return 1;
}


static int16_t UI_FindNextRoutingItemIndex(
    int16_t previousIndex)
{
    int16_t bestIndex = -1;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        const UI_Item *candidate =
            &uiItems[i];

        if (!UI_IsPermanentItem(candidate) &&
            candidate->type !=
                UI_ITEM_AUTO_NODE)
        {
            continue;
        }

        if (candidate->order < 0)
        {
            continue;
        }

        if (previousIndex >= 0)
        {
            if (UI_CompareItemPositions(
                    candidate,
                    &uiItems[previousIndex]) <= 0)
            {
                continue;
            }
        }

        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                candidate,
                &uiItems[bestIndex]) < 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}


static uint8_t UI_RebuildCalculatedConnections(void)
{
    /*
     * Rule 4.1: derived routing is rebuilt globally.
     */
    UI_RemoveAllAutoNodes();

    for (uint16_t iteration = 0;
         iteration < UI_MAX_ROUTING_ITERATIONS;
         iteration++)
    {
        uint8_t structureChanged = 0;

        UI_ClearConnections();

        /*
         * Phase 1: calculate outputs from left to right.
         */
        int16_t currentIndex = -1;

        while (1)
        {
            currentIndex =
                UI_FindNextRoutingItemIndex(
                    currentIndex
                );

            if (currentIndex < 0)
            {
                break;
            }

            if (!UI_CalculateOutputForItem(
                    (uint16_t)currentIndex,
                    &structureChanged))
            {
                UI_ClearConnections();
                return 0;
            }
        }

        /*
         * Phase 2: add only still-missing inputs.
         */
        currentIndex = -1;

        while (1)
        {
            currentIndex =
                UI_FindNextRoutingItemIndex(
                    currentIndex
                );

            if (currentIndex < 0)
            {
                break;
            }

            if (!UI_CalculateMissingInputForItem(
                    (uint16_t)currentIndex,
                    &structureChanged))
            {
                UI_ClearConnections();
                return 0;
            }
        }

        if (!structureChanged)
        {
            return 1;
        }
    }

    /*
     * Rule 4.5: routing did not converge.
     */
    UI_ClearConnections();
    return 0;
}






static int8_t UI_CompareItemPositions(
    const UI_Item *itemA,
    const UI_Item *itemB)
{
    if (itemA == NULL || itemB == NULL)
    {
        return 0;
    }

    /*
     * Kleinere order-Werte liegen weiter links
     * und kommen zuerst.
     */
    if (itemA->order < itemB->order)
    {
        return -1;
    }

    if (itemA->order > itemB->order)
    {
        return 1;
    }

    /*
     * Bei gleicher Spalte:
     * höhere Lane kommt zuerst.
     *
     * Beispiel:
     * lane +1
     * lane  0
     * lane -1
     */
    if (itemA->lane > itemB->lane)
    {
        return -1;
    }

    if (itemA->lane < itemB->lane)
    {
        return 1;
    }

    /*
     * Sicherheitsregel für unerwartet gleiche
     * Rasterpositionen.
     */
    if (itemA->id < itemB->id)
    {
        return -1;
    }

    if (itemA->id > itemB->id)
    {
        return 1;
    }

    return 0;
}

static int16_t UI_FindNextSelectableIndex(
    int16_t currentIndex)
{
    if (currentIndex < 0 ||
        currentIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return -1;
    }

    int16_t bestIndex = -1;

    const UI_Item *currentItem =
        &uiItems[currentIndex];

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if ((int16_t)i == currentIndex)
        {
            continue;
        }

        if (!UI_IsSelectable(&uiItems[i]))
        {
            continue;
        }

        /*
         * Der Kandidat muss hinter dem aktuellen
         * Item in der Rasterreihenfolge liegen.
         */
        if (UI_CompareItemPositions(
                &uiItems[i],
                currentItem) <= 0)
        {
            continue;
        }

        /*
         * Ersten passenden Kandidaten übernehmen
         * oder einen näheren Kandidaten finden.
         */
        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                &uiItems[i],
                &uiItems[bestIndex]) < 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}

static int16_t UI_FindPreviousSelectableIndex(
    int16_t currentIndex)
{
    if (currentIndex < 0 ||
        currentIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return -1;
    }

    int16_t bestIndex = -1;

    const UI_Item *currentItem =
        &uiItems[currentIndex];

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if ((int16_t)i == currentIndex)
        {
            continue;
        }

        if (!UI_IsSelectable(&uiItems[i]))
        {
            continue;
        }

        /*
         * Der Kandidat muss vor dem aktuellen Item
         * in der Rasterreihenfolge liegen.
         */
        if (UI_CompareItemPositions(
                &uiItems[i],
                currentItem) >= 0)
        {
            continue;
        }

        /*
         * Gesucht wird der größte Kandidat,
         * der noch vor dem aktuellen Item liegt.
         */
        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                &uiItems[i],
                &uiItems[bestIndex]) > 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}

static int16_t UI_FindFirstSelectableIndex(void)
{
    int16_t bestIndex = -1;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsSelectable(&uiItems[i]))
        {
            continue;
        }

        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                &uiItems[i],
                &uiItems[bestIndex]) < 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}

static int16_t UI_FindLastSelectableIndex(void)
{
    int16_t bestIndex = -1;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsSelectable(&uiItems[i]))
        {
            continue;
        }

        if (bestIndex < 0 ||
            UI_CompareItemPositions(
                &uiItems[i],
                &uiItems[bestIndex]) > 0)
        {
            bestIndex = (int16_t)i;
        }
    }

    return bestIndex;
}

static int16_t UI_FindPermanentItemAt(
    int16_t order,
    int16_t lane,
    int16_t excludedIndex)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if ((int16_t)i == excludedIndex)
        {
            continue;
        }

        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order == order &&
            uiItems[i].lane == lane)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


static void UI_RemoveAllAutoNodes(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].type !=
            UI_ITEM_AUTO_NODE)
        {
            continue;
        }

        uiItems[i].order =
            UI_INACTIVE_ORDER;

        uiItems[i].lane = 0;
        uiItems[i].focus = UI_FOCUS_NONE;
    }
}

static void UI_NormalizeOrders(void)
{
    int16_t oldOrders[UI_ITEM_COUNT];


    /*
     * Ursprüngliche order-Werte sichern.
     *
     * Die Sicherung ist notwendig, weil die neuen
     * Werte nicht während der Berechnung die noch
     * auszuwertenden alten Werte verändern dürfen.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        oldOrders[i] =
            uiItems[i].order;
    }


    for (uint16_t itemIndex = 0;
         itemIndex < UI_ITEM_COUNT;
         itemIndex++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[itemIndex]))
        {
            continue;
        }

        if (oldOrders[itemIndex] < 0)
        {
            continue;
        }


        int16_t normalizedOrder = 0;


        /*
         * Anzahl der unterschiedlichen belegten
         * Spalten links vom aktuellen Item bestimmen.
         */
        for (uint16_t candidateIndex = 0;
             candidateIndex < UI_ITEM_COUNT;
             candidateIndex++)
        {
            if (!UI_IsPermanentItem(
                    &uiItems[candidateIndex]))
            {
                continue;
            }

            int16_t candidateOrder =
                oldOrders[candidateIndex];


            if (candidateOrder < 0 ||
                candidateOrder >=
                    oldOrders[itemIndex])
            {
                continue;
            }


            /*
             * Prüfen, ob dieser kleinere order-Wert
             * bereits zuvor gezählt wurde.
             */
            uint8_t alreadyCounted = 0;


            for (uint16_t previousIndex = 0;
                 previousIndex <
                    candidateIndex;
                 previousIndex++)
            {
                if (!UI_IsPermanentItem(
                        &uiItems[previousIndex]))
                {
                    continue;
                }

                if (oldOrders[previousIndex] ==
                    candidateOrder)
                {
                    alreadyCounted = 1;
                    break;
                }
            }


            if (!alreadyCounted)
            {
                normalizedOrder++;
            }
        }


        uiItems[itemIndex].order =
            normalizedOrder;
    }
}

static int16_t UI_GetMinimumPermanentOrder(void)
{
    int16_t minimumOrder = 32767;
    uint8_t found = 0;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order < 0)
        {
            continue;
        }

        if (!found ||
            uiItems[i].order <
                minimumOrder)
        {
            minimumOrder =
                uiItems[i].order;

            found = 1;
        }
    }


    return found ? minimumOrder : -1;
}

static int16_t UI_GetMaximumPermanentOrder(void)
{
    int16_t maximumOrder = -32768;
    uint8_t found = 0;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order < 0)
        {
            continue;
        }

        if (!found ||
            uiItems[i].order >
                maximumOrder)
        {
            maximumOrder =
                uiItems[i].order;

            found = 1;
        }
    }


    return found ? maximumOrder : -1;
}

static uint8_t UI_ValidateEdgeRules(void)
{
    int16_t minimumOrder =
        UI_GetMinimumPermanentOrder();

    int16_t maximumOrder =
        UI_GetMaximumPermanentOrder();


    /*
     * Es muss mindestens eine permanente Spalte geben.
     */
    if (minimumOrder < 0 ||
        maximumOrder < 0)
    {
        return 0;
    }


    /*
     * Linke und rechte Randspalte müssen
     * voneinander verschieden sein.
     *
     * In einer gültigen Struktur existieren
     * mindestens eine IN- und eine OUT-Spalte.
     */
    if (minimumOrder >= maximumOrder)
    {
        return 0;
    }


    uint8_t inputAtLeftEdge = 0;
    uint8_t outputAtRightEdge = 0;


    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }


        /*
         * Äußerste linke Spalte:
         * Hier dürfen ausschließlich Inputs liegen.
         */
        if (uiItems[i].order ==
            minimumOrder)
        {
            if (uiItems[i].type !=
                UI_ITEM_INPUT)
            {
                return 0;
            }

            inputAtLeftEdge = 1;
        }


        /*
         * Äußerste rechte Spalte:
         * Hier dürfen ausschließlich Outputs liegen.
         */
        if (uiItems[i].order ==
            maximumOrder)
        {
            if (uiItems[i].type !=
                UI_ITEM_OUTPUT)
            {
                return 0;
            }

            outputAtRightEdge = 1;
        }
    }


    /*
     * Zusätzlich muss tatsächlich mindestens
     * ein Input links und ein Output rechts liegen.
     */
    if (!inputAtLeftEdge ||
        !outputAtRightEdge)
    {
        return 0;
    }


    return 1;
}

static void UI_EnsureEdgeColumns(void)
{
    int16_t minimumOrder =
        UI_GetMinimumPermanentOrder();

    int16_t maximumOrder =
        UI_GetMaximumPermanentOrder();


    if (minimumOrder < 0 ||
        maximumOrder < 0)
    {
        return;
    }


    uint8_t invalidLeftEdge = 0;
    uint8_t invalidRightEdge = 0;


    /*
     * Prüfen, ob die äußersten Spalten noch
     * unerlaubte Itemtypen enthalten.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }


        if (uiItems[i].order ==
                minimumOrder &&
            uiItems[i].type !=
                UI_ITEM_INPUT)
        {
            invalidLeftEdge = 1;
        }


        if (uiItems[i].order ==
                maximumOrder &&
            uiItems[i].type !=
                UI_ITEM_OUTPUT)
        {
            invalidRightEdge = 1;
        }
    }


    /*
     * Ungültige linke Randspalte:
     *
     * Alle Nicht-Inputs werden um eine Spalte
     * nach rechts verschoben.
     *
     * Inputs bleiben in order 0.
     */
    if (invalidLeftEdge)
    {
        for (uint16_t i = 0;
             i < UI_ITEM_COUNT;
             i++)
        {
            if (!UI_IsPermanentItem(
                    &uiItems[i]))
            {
                continue;
            }


            if (uiItems[i].type ==
                UI_ITEM_INPUT)
            {
                uiItems[i].order = 0;
            }
            else
            {
                uiItems[i].order++;
            }
        }
    }


    /*
     * Nach einer möglichen linken Erweiterung
     * die äußerste Nicht-Output-Spalte neu bestimmen.
     */
    if (invalidRightEdge)
    {
        int16_t maximumNonOutputOrder = -1;


        for (uint16_t i = 0;
             i < UI_ITEM_COUNT;
             i++)
        {
            if (!UI_IsPermanentItem(
                    &uiItems[i]))
            {
                continue;
            }


            if (uiItems[i].type ==
                UI_ITEM_OUTPUT)
            {
                continue;
            }


            if (uiItems[i].order >
                maximumNonOutputOrder)
            {
                maximumNonOutputOrder =
                    uiItems[i].order;
            }
        }


        if (maximumNonOutputOrder >= 0)
        {
            int16_t newOutputOrder =
                maximumNonOutputOrder + 1;


            for (uint16_t i = 0;
                 i < UI_ITEM_COUNT;
                 i++)
            {
                if (uiItems[i].type ==
                    UI_ITEM_OUTPUT)
                {
                    uiItems[i].order =
                        newOutputOrder;
                }
            }
        }
    }


    /*
     * Vollständig leere Spalten schließen.
     */
    UI_NormalizeOrders();
}

static int16_t UI_GetFocusedIndex(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].focus == UI_FOCUS_SELECTED ||
            uiItems[i].focus == UI_FOCUS_GRABBED)
        {
            return (int16_t)i;
        }
    }

    return -1;
}

static uint8_t UI_IsItemGrabbed(void)
{
    int16_t focusedIndex =
        UI_GetFocusedIndex();

    if (focusedIndex < 0)
    {
        return 0;
    }

    return
        (uiItems[focusedIndex].focus ==
         UI_FOCUS_GRABBED) ?
        1 :
        0;
}


uint8_t UI_IsGrabbed(void)
{
    return UI_IsItemGrabbed();
}

static void UI_SetSelectedIndex(
    int16_t newIndex)
{
    if (newIndex < 0 ||
        newIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return;
    }

    if (!UI_IsSelectable(
            &uiItems[newIndex]))
    {
        return;
    }

    int16_t oldIndex =
        UI_GetFocusedIndex();

    /*
     * Solange ein Item gegriffen ist, darf der
     * Fokus nicht auf ein anderes Item wechseln.
     */
    if (oldIndex >= 0 &&
        uiItems[oldIndex].focus ==
            UI_FOCUS_GRABBED)
    {
        return;
    }

    if (oldIndex == newIndex)
    {
        return;
    }

    /*
     * Invariante:
     * Es darf immer nur genau ein Item ausgewählt
     * oder gegriffen sein.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uiItems[i].focus =
            UI_FOCUS_NONE;
    }

    uiItems[newIndex].focus =
        UI_FOCUS_SELECTED;


    /*
     * Nach jedem Auswahlwechsel beide Viewports prüfen.
     * Das gilt sowohl innerhalb derselben Order als auch
     * beim Wechsel in eine andere Order.
     */
    uint8_t horizontalViewportChanged =
        UI_EnsureItemVisible(
            newIndex
        );

    uint8_t verticalViewportChanged =
        UI_EnsureItemVerticallyVisible(
            newIndex
        );


    if (horizontalViewportChanged ||
        verticalViewportChanged)
    {
        /*
         * Mindestens ein Viewport wurde verschoben.
         * Dadurch ändern sich mehrere Item- und
         * Pfeilpositionen gleichzeitig.
         */
        UI_Draw();
    }
    else
    {
        UI_UpdateSelectionDisplay(
            oldIndex,
            newIndex
        );
    }
}



void UI_SelectNext(void)
{
    /*
     * Im Grab-Modus darf der Fokus nicht wandern.
     */
    if (UI_IsItemGrabbed())
    {
        return;
    }

    int16_t currentIndex =
        UI_GetFocusedIndex();

    /*
     * Falls noch kein Fokus existiert,
     * linkestes/oberstes Item auswählen.
     */
    if (currentIndex < 0)
    {
        int16_t firstIndex =
            UI_FindFirstSelectableIndex();

        if (firstIndex >= 0)
        {
            UI_SetSelectedIndex(firstIndex);
        }

        return;
    }

    int16_t nextIndex =
        UI_FindNextSelectableIndex(
            currentIndex
        );

    /*
     * Kein Wrap-around.
     * Am letzten Item bleibt die Auswahl stehen.
     */
    if (nextIndex < 0)
    {
        return;
    }

    UI_SetSelectedIndex(nextIndex);
}



void UI_SelectPrevious(void)
{
    /*
     * Im Grab-Modus darf der Fokus nicht wandern.
     */
    if (UI_IsItemGrabbed())
    {
        return;
    }

    int16_t currentIndex =
        UI_GetFocusedIndex();

    /*
     * Falls noch kein Fokus existiert,
     * rechtest/unterstes Item auswählen.
     */
    if (currentIndex < 0)
    {
        int16_t lastIndex =
            UI_FindLastSelectableIndex();

        if (lastIndex >= 0)
        {
            UI_SetSelectedIndex(lastIndex);
        }

        return;
    }

    int16_t previousIndex =
        UI_FindPreviousSelectableIndex(
            currentIndex
        );

    /*
     * Kein Wrap-around.
     */
    if (previousIndex < 0)
    {
        return;
    }

    UI_SetSelectedIndex(previousIndex);
}

static void UI_SaveGrabStartState(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        grabStartState[i].order =
            uiItems[i].order;

        grabStartState[i].lane =
            uiItems[i].lane;

        grabStartState[i].focus =
            uiItems[i].focus;
    }

    grabStartFirstVisibleOrder =
        uiFirstVisibleOrder;

    grabStartHighestVisibleLane =
        uiHighestVisibleLane;

    grabStartStateValid = 1U;
}


static uint8_t UI_RestoreGrabStartState(void)
{
    if (!grabStartStateValid)
    {
        return 0U;
    }

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uiItems[i].order =
            grabStartState[i].order;

        uiItems[i].lane =
            grabStartState[i].lane;

        uiItems[i].focus =
            grabStartState[i].focus;
    }

    uiFirstVisibleOrder =
        grabStartFirstVisibleOrder;

    uiHighestVisibleLane =
        grabStartHighestVisibleLane;

    /*
     * Auto-Nodes und Verbindungen sind abgeleitete
     * Daten und werden aus dem wiederhergestellten
     * permanenten Zustand vollständig neu aufgebaut.
     */
    if (!UI_RebuildCalculatedConnections())
    {
        UI_ClearConnections();
    }

    grabStartStateValid = 0U;

    UI_Draw();

    return 1U;
}


static void UI_MenuSetConfirmedItem(
    int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT ||
        !UI_IsSelectable(&uiItems[itemIndex]))
    {
        return;
    }

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uiItems[i].focus =
            UI_FOCUS_NONE;
    }

    uiItems[itemIndex].focus =
        UI_FOCUS_SELECTED;

    uiMenuItemIndex = itemIndex;

    /*
     * Der Statusbildschirm wird erst beim Schließen
     * des Menüs gezeichnet. Die Viewports werden aber
     * bereits auf das bestätigte Item vorbereitet.
     */
    (void)UI_EnsureItemVisible(itemIndex);
    (void)UI_EnsureItemVerticallyVisible(itemIndex);
}


static const char *UI_MenuGetItemName(void)
{
    int16_t itemIndex =
        (uiMenuControlMode ==
         UI_MENU_CONTROL_ITEM_SELECTION) ?
        uiMenuPreviewItemIndex :
        uiMenuItemIndex;

    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return "No item";
    }

    if (uiItems[itemIndex].longName[0] != '\0')
    {
        return uiItems[itemIndex].longName;
    }

    return uiItems[itemIndex].shortName;
}


/* -------------------------------------------------------------------------- */
/* General Menu: Eintraege                                                    */
/* -------------------------------------------------------------------------- */

typedef struct
{
    const char *label;
    void (*action)(void);

} UI_MenuEntry;

static const UI_MenuEntry uiGeneralMenuEntries[] =
{
    { "Save Preset",     UI_MenuSavePreset     },
    { "Manage Presets",  UI_MenuManagePresets  }
};

#define UI_GENERAL_MENU_ENTRY_COUNT \
    ((uint8_t)(sizeof(uiGeneralMenuEntries) / \
               sizeof(uiGeneralMenuEntries[0])))

/*
 * Das Overlay zeigt aktuell nur UI_MENU_VISIBLE_ROWS Zeilen und
 * scrollt nicht. Bei mehr Eintraegen muss zuerst ein Scroll-Offset
 * eingebaut werden.
 */
_Static_assert(
    (sizeof(uiGeneralMenuEntries) /
     sizeof(uiGeneralMenuEntries[0])) <= UI_MENU_VISIBLE_ROWS,
    "General Menu hat mehr Eintraege als Zeilen im Overlay");


/* -------------------------------------------------------------------------- */
/* Save Preset: Text-Hilfsfunktionen                                          */
/* -------------------------------------------------------------------------- */

static uint8_t UI_SaveAppendText(
    char *buffer,
    uint8_t position,
    uint8_t capacity,
    const char *text)
{
    while (*text != '\0' &&
           (uint8_t)(position + 1U) < capacity)
    {
        buffer[position++] = *text++;
    }

    buffer[position] = '\0';

    return position;
}


static uint8_t UI_SaveAppendUInt(
    char *buffer,
    uint8_t position,
    uint8_t capacity,
    uint8_t value)
{
    char digits[3];
    uint8_t count = 0U;

    do
    {
        digits[count++] = (char)('0' + (value % 10U));
        value = (uint8_t)(value / 10U);
    }
    while (value != 0U && count < sizeof(digits));

    while (count > 0U)
    {
        count--;

        if ((uint8_t)(position + 1U) < capacity)
        {
            buffer[position++] = digits[count];
        }
    }

    buffer[position] = '\0';

    return position;
}


/* Zweistellig mit fuehrender Null (0 bis 99). */
static uint8_t UI_SaveAppendUInt2(
    char *buffer,
    uint8_t position,
    uint8_t capacity,
    uint8_t value)
{
    if (value < 10U)
    {
        position = UI_SaveAppendText(
            buffer, position, capacity, "0");
    }

    return UI_SaveAppendUInt(
        buffer, position, capacity, value);
}


/*
 * Zeile 0:  "<bank> <bankname>"      z.B. "3 bank 3"
 * Zeile n:  "<nn>: <name>"           z.B. "05: empty"
 */
static void UI_SaveBuildRowText(
    uint8_t row,
    char *text)
{
    uint8_t position = 0U;

    text[0] = '\0';

    if (row == 0U)
    {
        char bankName[PRESET_BANK_NAME_LENGTH];

        PresetStore_GetBankName(uiPresetBank, bankName);

        position = UI_SaveAppendUInt(
            text, position, UI_SAVE_TEXT_CAPACITY, uiPresetBank);
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, " ");
        UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, bankName);

        return;
    }

    char presetName[PRESET_NAME_LENGTH];

    uint8_t used = PresetStore_GetSlotName(
        uiPresetBank, (uint8_t)(row - 1U), presetName);

    position = UI_SaveAppendUInt2(
        text, position, UI_SAVE_TEXT_CAPACITY, row);
    position = UI_SaveAppendText(
        text, position, UI_SAVE_TEXT_CAPACITY, ": ");
    UI_SaveAppendText(
        text,
        position,
        UI_SAVE_TEXT_CAPACITY,
        used ? presetName : "empty");
}


/* -------------------------------------------------------------------------- */
/* Save Preset: Zeichnen der Liste                                            */
/* -------------------------------------------------------------------------- */

static void UI_SaveDrawRow(uint8_t row)
{
    uint16_t y;

    if (row == 0U)
    {
        y = UI_SAVE_HEADER_Y;
    }
    else
    {
        int16_t position =
            (int16_t)(row - 1U) - (int16_t)uiPresetScrollTop;

        if (position < 0 ||
            position >= (int16_t)UI_SAVE_VISIBLE_PRESETS)
        {
            return;
        }

        y = (uint16_t)(UI_SAVE_LIST_Y +
            ((uint16_t)position * UI_SAVE_ROW_HEIGHT));
    }

    uint16_t background = UI_COLOR_BACKGROUND;

    if ((int16_t)row == uiMenuSelectedRow)
    {
        background =
            (row == 0U && uiPresetBankEditing) ?
            UI_COLOR_BORDER_GRABBED :
            UI_COLOR_BORDER_SELECTED;
    }

    char text[UI_SAVE_TEXT_CAPACITY];

    UI_SaveBuildRowText(row, text);

    ST7735_FillRect(
        0U,
        y,
        UI_SAVE_ROW_WIDTH,
        UI_SAVE_ROW_HEIGHT,
        background
    );

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        y + UI_SAVE_TEXT_Y_OFFSET,
        text,
        UI_COLOR_TEXT_LIGHT,
        background,
        1U
    );
}


static void UI_SaveDrawScrollbar(void)
{
    uint16_t trackHeight =
        UI_SAVE_VISIBLE_PRESETS * UI_SAVE_ROW_HEIGHT;

    uint16_t thumbHeight =
        (uint16_t)((trackHeight * UI_SAVE_VISIBLE_PRESETS) /
                   PRESET_SLOTS_PER_BANK);

    uint16_t thumbY =
        (uint16_t)(UI_SAVE_LIST_Y +
            (((uint32_t)trackHeight * uiPresetScrollTop) /
             PRESET_SLOTS_PER_BANK));

    ST7735_FillRect(
        UI_SAVE_SCROLLBAR_X,
        UI_SAVE_LIST_Y,
        UI_SAVE_SCROLLBAR_WIDTH,
        trackHeight,
        UI_SAVE_COLOR_TRACK
    );

    ST7735_FillRect(
        UI_SAVE_SCROLLBAR_X,
        thumbY,
        UI_SAVE_SCROLLBAR_WIDTH,
        thumbHeight,
        UI_COLOR_TEXT_LIGHT
    );
}


static void UI_SaveDrawList(void)
{
    for (uint8_t i = 0U; i < UI_SAVE_VISIBLE_PRESETS; i++)
    {
        UI_SaveDrawRow(
            (uint8_t)(uiPresetScrollTop + i + 1U));
    }

    UI_SaveDrawScrollbar();
}


/*
 * Statuszeile unter der Trennlinie. Wird von der Preset-Liste und vom
 * Untermenue gemeinsam benutzt.
 */
static void UI_SaveDrawStatus(void)
{
    char text[UI_SAVE_TEXT_CAPACITY];
    uint8_t position = 0U;
    uint16_t color = UI_COLOR_TEXT_LIGHT;

    text[0] = '\0';

    switch (uiSaveStatus)
    {
        case UI_SAVE_STATUS_SAVING:
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Saving...");
            break;

        case UI_SAVE_STATUS_MEMORY_FULL:
            color = UI_COLOR_LOOP_UNCONFIRMED;
            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Memory full ");
            position = UI_SaveAppendUInt(
                text, position, UI_SAVE_TEXT_CAPACITY,
                PresetStore_GetUsedCount());
            position = UI_SaveAppendText(
                text, position, UI_SAVE_TEXT_CAPACITY, "/");
            UI_SaveAppendUInt(
                text, position, UI_SAVE_TEXT_CAPACITY,
                PRESET_MAX_RECORDS);
            break;

        case UI_SAVE_STATUS_ERROR:
            color = UI_COLOR_LOOP_UNCONFIRMED;
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Save failed");
            break;

        case UI_SAVE_STATUS_NAME_HINT:
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY,
                "Enter set  Return del");
            break;

        case UI_SAVE_STATUS_SAVED:
            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Saved. Used ");
            position = UI_SaveAppendUInt(
                text, position, UI_SAVE_TEXT_CAPACITY,
                PresetStore_GetUsedCount());
            position = UI_SaveAppendText(
                text, position, UI_SAVE_TEXT_CAPACITY, "/");
            UI_SaveAppendUInt(
                text, position, UI_SAVE_TEXT_CAPACITY,
                PRESET_MAX_RECORDS);
            break;

        case UI_SAVE_STATUS_NONE:
        default:
            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Used ");
            position = UI_SaveAppendUInt(
                text, position, UI_SAVE_TEXT_CAPACITY,
                PresetStore_GetUsedCount());
            position = UI_SaveAppendText(
                text, position, UI_SAVE_TEXT_CAPACITY, "/");
            UI_SaveAppendUInt(
                text, position, UI_SAVE_TEXT_CAPACITY,
                PRESET_MAX_RECORDS);
            break;
    }

    ST7735_FillRect(
        0U,
        UI_SAVE_SEPARATOR_Y + 1U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT - (UI_SAVE_SEPARATOR_Y + 1U),
        UI_COLOR_BACKGROUND
    );

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        UI_SAVE_STATUS_Y,
        text,
        color,
        UI_COLOR_BACKGROUND,
        1U
    );
}


/* Zeichnet die komplette Liste neu (mit Bildschirm loeschen). */
static void UI_SaveDrawPage(void)
{
    ST7735_FillRect(
        0U,
        0U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT,
        UI_COLOR_BACKGROUND
    );

    UI_SaveDrawRow(0U);
    UI_SaveDrawList();

    ST7735_DrawLine(
        0U,
        UI_SAVE_SEPARATOR_Y,
        UI_DISPLAY_WIDTH - 1U,
        UI_SAVE_SEPARATOR_Y,
        UI_COLOR_BORDER_NORMAL
    );

    UI_SaveDrawStatus();
}


/* -------------------------------------------------------------------------- */
/* Save Preset: Layout serialisieren und speichern                            */
/* -------------------------------------------------------------------------- */

/*
 * Schreibt die Position aller aktiven Items (ohne Auto-Nodes, die beim
 * Laden neu berechnet werden) in den Puffer:
 *
 *   [0]  Anzahl der Eintraege
 *   danach je Eintrag: id (uint8), order (int8), lane (int8)
 *
 * Rueckgabe: Laenge in Bytes, 0 bei Fehler.
 */
static uint8_t UI_PresetSerializeLayout(
    uint8_t *buffer,
    uint8_t capacity)
{
    uint8_t count = 0U;
    uint8_t position = 1U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        const UI_Item *item = &uiItems[i];

        if (item->type == UI_ITEM_AUTO_NODE ||
            item->order < 0)
        {
            continue;
        }

        if (item->id > 255U ||
            item->order > 127 ||
            item->lane < -128 ||
            item->lane > 127 ||
            (uint16_t)(position + 3U) > capacity)
        {
            return 0U;
        }

        buffer[position++] = (uint8_t)item->id;
        buffer[position++] = (uint8_t)(int8_t)item->order;
        buffer[position++] = (uint8_t)(int8_t)item->lane;
        count++;
    }

    buffer[0] = count;

    return position;
}


/*
 * Speichert das aktuelle Layout unter dem Namen in den Slot der
 * gewaehlten Bank und kehrt danach zur Preset-Liste zurueck, die das
 * Ergebnis in der Statuszeile zeigt.
 */
static void UI_SaveExecute(
    uint8_t slot,
    const char *name)
{
    uint8_t layout[PRESET_LAYOUT_MAX_BYTES];

    uint8_t length =
        UI_PresetSerializeLayout(layout, sizeof(layout));

    if (length == 0U)
    {
        uiSaveStatus = UI_SAVE_STATUS_ERROR;
    }
    else
    {
        /*
         * Das Schreiben blockiert, deshalb vorher anzeigen.
         */
        uiSaveStatus = UI_SAVE_STATUS_SAVING;
        UI_SaveDrawStatus();

        PresetStoreStatus result = PresetStore_SavePreset(
            uiPresetBank, slot, name, layout, length);

        if (result == PRESET_STORE_OK)
        {
            uiSaveStatus = UI_SAVE_STATUS_SAVED;
        }
        else if (result == PRESET_STORE_ERR_FULL)
        {
            uiSaveStatus = UI_SAVE_STATUS_MEMORY_FULL;
        }
        else
        {
            uiSaveStatus = UI_SAVE_STATUS_ERROR;
        }
    }

    uiNameEditing = 0U;
    uiMenuPage = UI_MENU_PAGE_SAVE_PRESET;
    UI_SaveDrawPage();
}


/* -------------------------------------------------------------------------- */
/* Save Preset: Untermenue mit Namenseditor                                   */
/* -------------------------------------------------------------------------- */

static void UI_NameCopy(
    char *destination,
    const char *source)
{
    uint8_t i = 0U;

    while (source[i] != '\0' && i < UI_NAME_MAX_LENGTH)
    {
        destination[i] = source[i];
        i++;
    }

    destination[i] = '\0';
}


static uint8_t UI_NameLengthOf(const char *text)
{
    uint8_t length = 0U;

    while (text[length] != '\0' && length < UI_NAME_MAX_LENGTH)
    {
        length++;
    }

    return length;
}


static int8_t UI_NameAlphabetIndex(char c)
{
    for (int8_t i = 0; i < UI_NAME_ALPHABET_SIZE; i++)
    {
        if (uiNameAlphabet[i] == c)
        {
            return i;
        }
    }

    return -1;
}


/*
 * X-Position des Zeichens, das hinter den bereits gesetzten Zeichen
 * folgt. Die Breite wird mit der Schrift gemessen: Breite von
 * "<gesetzt>A" minus Breite von "A".
 */
static uint16_t UI_NameNextCharX(void)
{
    if (uiNameLength == 0U)
    {
        return UI_SAVE_TEXT_X_OFFSET;
    }

    char text[PRESET_NAME_LENGTH + 1U];
    char single[2] = { 'A', '\0' };

    uint8_t i = 0U;

    for (; i < uiNameLength; i++)
    {
        text[i] = uiNameBuffer[i];
    }

    text[i++] = 'A';
    text[i] = '\0';

    uint16_t fullWidth = Font5x7_GetStringWidth(text, 1U);
    uint16_t singleWidth = Font5x7_GetStringWidth(single, 1U);

    return (uint16_t)(UI_SAVE_TEXT_X_OFFSET +
                      fullWidth - singleWidth);
}


static void UI_NameDrawRow(uint8_t row)
{
    uint16_t y = (uint16_t)(UI_NAME_FIRST_ROW_Y +
                 ((uint16_t)row * UI_NAME_ROW_PITCH));
    uint16_t textY = (uint16_t)(y + UI_NAME_TEXT_Y_OFFSET);
    uint16_t background = UI_COLOR_BACKGROUND;

    if (row == uiNameRow)
    {
        background =
            (row == UI_NAME_ROW_NAME && uiNameEditing) ?
            UI_COLOR_BORDER_GRABBED :
            UI_COLOR_BORDER_SELECTED;
    }

    ST7735_FillRect(
        0U,
        y,
        UI_SAVE_ROW_WIDTH,
        UI_NAME_ROW_HEIGHT,
        background
    );

    if (row == UI_NAME_ROW_SAVE)
    {
        Font5x7_DrawString(
            UI_SAVE_TEXT_X_OFFSET, textY, "Save",
            UI_COLOR_TEXT_LIGHT, background, 1U);
        return;
    }

    if (row == UI_NAME_ROW_CANCEL)
    {
        Font5x7_DrawString(
            UI_SAVE_TEXT_X_OFFSET, textY, "Cancel",
            UI_COLOR_TEXT_LIGHT, background, 1U);
        return;
    }

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET, textY, uiNameBuffer,
        UI_COLOR_TEXT_LIGHT, background, 1U);

    if (!uiNameEditing ||
        uiNameLength >= UI_NAME_MAX_LENGTH)
    {
        return;
    }

    /*
     * Naechstes Zeichen: gelb hinterlegt, solange noch keines
     * ausgewaehlt ist als gelber Strich.
     */
    uint16_t x = UI_NameNextCharX();

    if (uiNameCandidate >= 0)
    {
        char single[2] =
            { uiNameAlphabet[uiNameCandidate], '\0' };

        ST7735_FillRect(
            x - 1U, textY - 1U, 7U, 9U,
            UI_COLOR_LOOP_UNCONFIRMED);

        Font5x7_DrawString(
            x, textY, single,
            UI_COLOR_TEXT_DARK, UI_COLOR_LOOP_UNCONFIRMED, 1U);
    }
    else
    {
        ST7735_DrawLine(
            x, textY + 8U, x + 4U, textY + 8U,
            UI_COLOR_LOOP_UNCONFIRMED);
    }
}


/* Zeichnet das Untermenue komplett neu (mit Bildschirm loeschen). */
static void UI_NameDrawPage(void)
{
    char header[UI_SAVE_TEXT_CAPACITY];
    uint8_t position = 0U;

    ST7735_FillRect(
        0U,
        0U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT,
        UI_COLOR_BACKGROUND
    );

    position = UI_SaveAppendText(
        header, 0U, UI_SAVE_TEXT_CAPACITY, "Bank ");
    position = UI_SaveAppendUInt(
        header, position, UI_SAVE_TEXT_CAPACITY, uiPresetBank);
    position = UI_SaveAppendText(
        header, position, UI_SAVE_TEXT_CAPACITY, " Preset ");
    UI_SaveAppendUInt2(
        header, position, UI_SAVE_TEXT_CAPACITY,
        (uint8_t)(uiNameSlot + 1U));

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        UI_SAVE_HEADER_Y,
        header,
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND,
        1U
    );

    for (uint8_t row = 0U; row < UI_NAME_ROW_COUNT; row++)
    {
        UI_NameDrawRow(row);
    }

    ST7735_DrawLine(
        0U,
        UI_SAVE_SEPARATOR_Y,
        UI_DISPLAY_WIDTH - 1U,
        UI_SAVE_SEPARATOR_Y,
        UI_COLOR_BORDER_NORMAL
    );

    UI_SaveDrawStatus();
}


/*
 * Oeffnet das Untermenue fuer einen Slot. Der Name ist bei einem
 * belegten Preset der vorhandene, bei einem leeren der Vorschlag
 * "preset nn". Der Cursor steht auf "Save".
 */
static void UI_NameOpen(uint8_t slot)
{
    char name[PRESET_NAME_LENGTH];

    if (!PresetStore_GetSlotName(uiPresetBank, slot, name))
    {
        uint8_t position = UI_SaveAppendText(
            name, 0U, sizeof(name), "preset ");

        UI_SaveAppendUInt2(
            name, position, sizeof(name), (uint8_t)(slot + 1U));
    }

    uiNameSlot = slot;
    UI_NameCopy(uiNameBuffer, name);
    UI_NameCopy(uiNameOriginal, name);
    uiNameLength = UI_NameLengthOf(uiNameBuffer);

    uiNameRow = UI_NAME_ROW_SAVE;
    uiNameCandidate = -1;
    uiNameEditing = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;

    uiMenuPage = UI_MENU_PAGE_SAVE_NAME;
    UI_DrawMenu();
}


static void UI_NameBackToList(void)
{
    uiNameEditing = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;
    uiMenuPage = UI_MENU_PAGE_SAVE_PRESET;
    UI_SaveDrawPage();
}


/*
 * Click auf die Namenszeile: Der Vorschlag wird geloescht und die
 * Eingabe beginnt mit dem ersten Zeichen.
 */
static void UI_NameStartEditing(void)
{
    uiNameBuffer[0] = '\0';
    uiNameLength = 0U;
    uiNameCandidate = -1;
    uiNameEditing = 1U;
    uiSaveStatus = UI_SAVE_STATUS_NAME_HINT;

    UI_NameDrawRow(UI_NAME_ROW_NAME);
    UI_SaveDrawStatus();
}


/*
 * Beendet den Editor. Leerzeichen am Ende werden entfernt. Bleibt der
 * Name leer, gilt wieder der Name von vor dem Editieren.
 */
static void UI_NameFinishEditing(void)
{
    while (uiNameLength > 0U &&
           uiNameBuffer[uiNameLength - 1U] == ' ')
    {
        uiNameLength--;
    }

    uiNameBuffer[uiNameLength] = '\0';

    if (uiNameLength == 0U)
    {
        UI_NameCopy(uiNameBuffer, uiNameOriginal);
        uiNameLength = UI_NameLengthOf(uiNameBuffer);
    }

    uiNameCandidate = -1;
    uiNameEditing = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;

    UI_NameDrawRow(UI_NAME_ROW_NAME);
    UI_SaveDrawStatus();
}


static void UI_NameHandleEncoder(int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    if (uiNameEditing)
    {
        /*
         * Ohne ausgewaehltes Zeichen: im Uhrzeigersinn "A", dagegen
         * das letzte Zeichen. Danach durchlaeuft der Encoder den
         * Zeichensatz mit Umlauf.
         */
        if (uiNameCandidate < 0)
        {
            uiNameCandidate =
                (direction > 0) ?
                0 :
                (int8_t)(UI_NAME_ALPHABET_SIZE - 1);
        }
        else if (direction > 0)
        {
            uiNameCandidate = (int8_t)
                ((uiNameCandidate + 1) % UI_NAME_ALPHABET_SIZE);
        }
        else
        {
            uiNameCandidate = (int8_t)
                ((uiNameCandidate + UI_NAME_ALPHABET_SIZE - 1) %
                 UI_NAME_ALPHABET_SIZE);
        }

        UI_NameDrawRow(UI_NAME_ROW_NAME);
        return;
    }

    int16_t oldRow = uiNameRow;
    int16_t newRow = oldRow + direction;

    if (newRow < 0)
    {
        newRow = 0;
    }

    if (newRow > (int16_t)(UI_NAME_ROW_COUNT - 1U))
    {
        newRow = (int16_t)(UI_NAME_ROW_COUNT - 1U);
    }

    if (newRow == oldRow)
    {
        return;
    }

    uiNameRow = (uint8_t)newRow;

    UI_NameDrawRow((uint8_t)oldRow);
    UI_NameDrawRow((uint8_t)newRow);
}


static void UI_NameHandleEnter(void)
{
    if (uiNameEditing)
    {
        if (uiNameCandidate < 0)
        {
            /*
             * Enter ohne ausgewaehltes Zeichen verlaesst den Editor.
             */
            UI_NameFinishEditing();
            return;
        }

        uiNameBuffer[uiNameLength++] =
            uiNameAlphabet[uiNameCandidate];
        uiNameBuffer[uiNameLength] = '\0';
        uiNameCandidate = -1;

        if (uiNameLength >= UI_NAME_MAX_LENGTH)
        {
            /*
             * Name ist voll.
             */
            UI_NameFinishEditing();
            return;
        }

        UI_NameDrawRow(UI_NAME_ROW_NAME);
        return;
    }

    if (uiNameRow == UI_NAME_ROW_NAME)
    {
        UI_NameStartEditing();
    }
    else if (uiNameRow == UI_NAME_ROW_SAVE)
    {
        UI_SaveExecute(uiNameSlot, uiNameBuffer);
    }
    else
    {
        UI_NameBackToList();
    }
}


static void UI_NameHandleReturn(void)
{
    if (!uiNameEditing)
    {
        UI_NameBackToList();
        return;
    }

    if (uiNameLength == 0U)
    {
        if (uiNameCandidate >= 0)
        {
            /*
             * Das erste Zeichen wieder verwerfen.
             */
            uiNameCandidate = -1;
            UI_NameDrawRow(UI_NAME_ROW_NAME);
        }
        else
        {
            /*
             * Nichts mehr zu loeschen: Editor verlassen.
             */
            UI_NameFinishEditing();
        }

        return;
    }

    /*
     * Das aktuelle Zeichen wird verworfen, das vorherige wieder
     * editierbar.
     */
    uiNameLength--;
    uiNameCandidate = UI_NameAlphabetIndex(uiNameBuffer[uiNameLength]);
    uiNameBuffer[uiNameLength] = '\0';

    UI_NameDrawRow(UI_NAME_ROW_NAME);
}


/* -------------------------------------------------------------------------- */
/* Save Preset: Bedienung der Preset-Liste                                    */
/* -------------------------------------------------------------------------- */

static void UI_SaveEnsureSelectionVisible(void)
{
    if (uiMenuSelectedRow <= 0)
    {
        return;
    }

    int16_t preset = uiMenuSelectedRow - 1;

    if (preset < (int16_t)uiPresetScrollTop)
    {
        uiPresetScrollTop = (uint8_t)preset;
    }
    else if (preset >=
             (int16_t)(uiPresetScrollTop + UI_SAVE_VISIBLE_PRESETS))
    {
        uiPresetScrollTop =
            (uint8_t)(preset - UI_SAVE_VISIBLE_PRESETS + 1);
    }
}


static void UI_SaveHandleEncoder(int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    /*
     * Bank-Zeile ist gegriffen: der Encoder aendert die Bank (0 bis 63,
     * mit Umlauf).
     */
    if (uiPresetBankEditing)
    {
        if (direction > 0)
        {
            uiPresetBank = (uint8_t)
                ((uiPresetBank + 1U) % PRESET_BANK_COUNT);
        }
        else
        {
            uiPresetBank = (uint8_t)
                ((uiPresetBank + PRESET_BANK_COUNT - 1U) %
                 PRESET_BANK_COUNT);
        }

        uiSaveStatus = UI_SAVE_STATUS_NONE;

        UI_SaveDrawRow(0U);
        UI_SaveDrawList();
        UI_SaveDrawStatus();
        return;
    }

    int16_t oldRow = uiMenuSelectedRow;
    int16_t newRow = oldRow + direction;
    uint8_t oldTop = uiPresetScrollTop;

    if (newRow < 0)
    {
        newRow = 0;
    }

    if (newRow > UI_SAVE_LAST_ROW)
    {
        newRow = UI_SAVE_LAST_ROW;
    }

    if (newRow == oldRow)
    {
        return;
    }

    uiMenuSelectedRow = newRow;
    UI_SaveEnsureSelectionVisible();

    if (uiPresetScrollTop != oldTop)
    {
        UI_SaveDrawRow(0U);
        UI_SaveDrawList();
    }
    else
    {
        UI_SaveDrawRow((uint8_t)oldRow);
        UI_SaveDrawRow((uint8_t)newRow);
    }

    /*
     * Eine Meldung vom letzten Speichern verschwindet beim Weiterdrehen.
     */
    if (uiSaveStatus != UI_SAVE_STATUS_NONE)
    {
        uiSaveStatus = UI_SAVE_STATUS_NONE;
        UI_SaveDrawStatus();
    }
}


static void UI_SaveHandleEnter(void)
{
    if (uiMenuSelectedRow == 0)
    {
        /*
         * Enter auf der Bank-Zeile: Bank aendern starten / bestaetigen.
         */
        if (uiPresetBankEditing)
        {
            uiPresetBankEditing = 0U;
        }
        else
        {
            uiPresetBankBackup = uiPresetBank;
            uiPresetBankEditing = 1U;
        }

        uiSaveStatus = UI_SAVE_STATUS_NONE;

        UI_SaveDrawRow(0U);
        UI_SaveDrawStatus();
        return;
    }

    /*
     * Enter auf einem Preset: Untermenue mit Namenseditor oeffnen.
     */
    UI_NameOpen((uint8_t)(uiMenuSelectedRow - 1));
}


static void UI_SaveHandleReturn(void)
{
    if (uiPresetBankEditing)
    {
        /*
         * Bank-Aenderung verwerfen.
         */
        uiPresetBank = uiPresetBankBackup;
        uiPresetBankEditing = 0U;

        UI_SaveDrawRow(0U);
        UI_SaveDrawList();
        return;
    }

    /*
     * Zurueck zum General Menu: erst den Statusbildschirm darunter
     * neu zeichnen, dann das Overlay.
     */
    uiMenuPage = UI_MENU_PAGE_GENERAL;
    uiMenuSelectedRow = 0;

    UI_Draw();
    UI_DrawMenu();
}


static void UI_MenuSavePreset(void)
{
    if (uiPresetBankEditing)
    {
        uiPresetBank = uiPresetBankBackup;
    }

    uiPresetBankEditing = 0U;
    uiNameEditing = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;
    uiPresetScrollTop = 0U;

    uiMenuPage = UI_MENU_PAGE_SAVE_PRESET;
    uiMenuSelectedRow = 0;

    UI_DrawMenu();
}


static void UI_MenuManagePresets(void)
{
    /*
     * Platzhalter: wird im naechsten Schritt implementiert.
     */
}


static void UI_DrawMenuRow(
    uint8_t row,
    const char *text)
{
    uint16_t rowX = UI_MENU_X + 2U;
    uint16_t rowY =
        UI_MENU_Y + 2U +
        ((uint16_t)row * UI_MENU_ROW_HEIGHT);

    uint16_t rowWidth =
        UI_MENU_WIDTH - 4U;

    uint16_t background =
        ((int16_t)row ==
         uiMenuSelectedRow) ?
        UI_COLOR_BORDER_SELECTED :
        UI_COLOR_BACKGROUND;

    ST7735_FillRect(
        rowX,
        rowY,
        rowWidth,
        UI_MENU_ROW_HEIGHT,
        background
    );

    Font5x7_DrawString(
        rowX + UI_MENU_TEXT_X_OFFSET,
        rowY + UI_MENU_TEXT_Y_OFFSET,
        text,
        UI_COLOR_TEXT_LIGHT,
        background,
        1U
    );

    /*
     * Aktiver Item-Auswahlmodus: zusätzlicher
     * magentafarbener Innenrahmen. Damit werden keine
     * nicht unterstützten Schriftzeichen benötigt.
     */
    if (row == UI_MENU_ITEM_ROW &&
        uiMenuControlMode ==
            UI_MENU_CONTROL_ITEM_SELECTION)
    {
        ST7735_DrawLine(
            rowX + 1U,
            rowY + 1U,
            rowX + rowWidth - 2U,
            rowY + 1U,
            UI_COLOR_BORDER_GRABBED
        );

        ST7735_DrawLine(
            rowX + 1U,
            rowY + UI_MENU_ROW_HEIGHT - 2U,
            rowX + rowWidth - 2U,
            rowY + UI_MENU_ROW_HEIGHT - 2U,
            UI_COLOR_BORDER_GRABBED
        );

        ST7735_DrawLine(
            rowX + 1U,
            rowY + 1U,
            rowX + 1U,
            rowY + UI_MENU_ROW_HEIGHT - 2U,
            UI_COLOR_BORDER_GRABBED
        );

        ST7735_DrawLine(
            rowX + rowWidth - 2U,
            rowY + 1U,
            rowX + rowWidth - 2U,
            rowY + UI_MENU_ROW_HEIGHT - 2U,
            UI_COLOR_BORDER_GRABBED
        );
    }
}


static uint8_t UI_MenuItemIsManualNode(void)
{
    return
        (uiMenuItemIndex >= 0 &&
         uiMenuItemIndex < (int16_t)UI_ITEM_COUNT &&
         uiItems[uiMenuItemIndex].order >= 0 &&
         uiItems[uiMenuItemIndex].type ==
             UI_ITEM_MANUAL_NODE) ?
        1U :
        0U;
}


static uint8_t UI_MenuGetRowCount(void)
{
    if (uiMenuPage ==
        UI_MENU_PAGE_DELETE_CONFIRM)
    {
        return 3U;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_GENERAL)
    {
        return UI_GENERAL_MENU_ENTRY_COUNT;
    }

    return UI_MenuItemIsManualNode() ?
        3U :
        2U;
}


static void UI_DeleteSelectedManualNode(void)
{
    if (!UI_MenuItemIsManualNode())
    {
        return;
    }

    int16_t deletedIndex =
        uiMenuItemIndex;

    int16_t replacementIndex =
        UI_FindPreviousSelectableIndex(
            deletedIndex
        );

    if (replacementIndex < 0)
    {
        replacementIndex =
            UI_FindNextSelectableIndex(
                deletedIndex
            );
    }

    uiItems[deletedIndex].order =
        UI_INACTIVE_ORDER;
    uiItems[deletedIndex].lane = 0;
    uiItems[deletedIndex].focus =
        UI_FOCUS_NONE;
    uiItems[deletedIndex].loopStatus =
        UI_LOOP_STATUS_OFF;

    UI_RemoveAllAutoNodes();
    UI_NormalizeOrders();
    UI_EnsureEdgeColumns();

    if (!UI_RebuildCalculatedConnections())
    {
        UI_ClearConnections();
    }

    if (replacementIndex < 0 ||
        !UI_IsSelectable(
            &uiItems[replacementIndex]))
    {
        replacementIndex =
            UI_FindFirstSelectableIndex();
    }

    if (replacementIndex >= 0)
    {
        UI_MenuSetConfirmedItem(
            replacementIndex
        );
    }

    uiMenuPage = UI_MENU_PAGE_ROOT;
    uiMenuControlMode =
        UI_MENU_CONTROL_NAVIGATION;
    uiMode = UI_MODE_STATE_VIEW;

    UI_Draw();
}


static void UI_DrawMenu(void)
{
    if (uiMenuPage ==
        UI_MENU_PAGE_SAVE_PRESET)
    {
        UI_SaveDrawPage();
        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_SAVE_NAME)
    {
        UI_NameDrawPage();
        return;
    }

    ST7735_FillRect(
        UI_MENU_X,
        UI_MENU_Y,
        UI_MENU_WIDTH,
        UI_MENU_HEIGHT,
        UI_COLOR_BACKGROUND
    );

    ST7735_DrawLine(
        UI_MENU_X,
        UI_MENU_Y,
        UI_MENU_X + UI_MENU_WIDTH - 1U,
        UI_MENU_Y,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        UI_MENU_X,
        UI_MENU_Y + UI_MENU_HEIGHT - 1U,
        UI_MENU_X + UI_MENU_WIDTH - 1U,
        UI_MENU_Y + UI_MENU_HEIGHT - 1U,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        UI_MENU_X,
        UI_MENU_Y,
        UI_MENU_X,
        UI_MENU_Y + UI_MENU_HEIGHT - 1U,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        UI_MENU_X + UI_MENU_WIDTH - 1U,
        UI_MENU_Y,
        UI_MENU_X + UI_MENU_WIDTH - 1U,
        UI_MENU_Y + UI_MENU_HEIGHT - 1U,
        UI_COLOR_BORDER_NORMAL
    );

    if (uiMenuPage ==
        UI_MENU_PAGE_DELETE_CONFIRM)
    {
        UI_DrawMenuRow(
            UI_MENU_CONFIRM_TITLE_ROW,
            "Delete Node?"
        );

        UI_DrawMenuRow(
            UI_MENU_CONFIRM_CANCEL_ROW,
            "Cancel"
        );

        UI_DrawMenuRow(
            UI_MENU_CONFIRM_DELETE_ROW,
            "Delete"
        );

        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_GENERAL)
    {
        for (uint8_t i = 0U;
             i < UI_GENERAL_MENU_ENTRY_COUNT;
             i++)
        {
            UI_DrawMenuRow(
                i,
                uiGeneralMenuEntries[i].label
            );
        }

        return;
    }

    UI_DrawMenuRow(
        UI_MENU_GENERAL_ROW,
        "General Menu"
    );

    UI_DrawMenuRow(
        UI_MENU_ITEM_ROW,
        UI_MenuGetItemName()
    );

    if (UI_MenuItemIsManualNode())
    {
        UI_DrawMenuRow(
            UI_MENU_DELETE_NODE_ROW,
            "Delete Node"
        );
    }
}


static void UI_OpenMenu(void)
{
    int16_t focusedIndex =
        UI_GetFocusedIndex();

    if (focusedIndex < 0 ||
        !UI_IsSelectable(&uiItems[focusedIndex]))
    {
        return;
    }

    /*
     * Ein eventuell gegriffenes Item bleibt an seiner
     * aktuellen Position, wird aber für die Menünutzung
     * wieder in den ausgewählten Zustand versetzt.
     */
    uiItems[focusedIndex].focus =
        UI_FOCUS_SELECTED;

    uiMode = UI_MODE_MENU;
    uiMenuPage = UI_MENU_PAGE_ROOT;
    uiMenuControlMode =
        UI_MENU_CONTROL_NAVIGATION;

    uiMenuSelectedRow =
        UI_MENU_ITEM_ROW;

    uiMenuItemIndex = focusedIndex;
    uiMenuPreviewItemIndex = focusedIndex;
    uiMenuItemSelectionStartIndex = focusedIndex;

    UI_DrawMenu();
}


static void UI_CloseMenu(
    uint8_t restorePreview)
{
    if (uiMode != UI_MODE_MENU)
    {
        return;
    }

    if (restorePreview &&
        uiMenuControlMode ==
            UI_MENU_CONTROL_ITEM_SELECTION)
    {
        UI_MenuSetConfirmedItem(
            uiMenuItemSelectionStartIndex
        );
    }

    uiMenuControlMode =
        UI_MENU_CONTROL_NAVIGATION;
    uiMenuPage = UI_MENU_PAGE_ROOT;

    uiMode = UI_MODE_STATE_VIEW;

    UI_Draw();
}


static void UI_MenuHandleEncoderStep(
    int8_t direction)
{
    if (uiMenuPage ==
        UI_MENU_PAGE_SAVE_PRESET)
    {
        UI_SaveHandleEncoder(direction);
        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_SAVE_NAME)
    {
        UI_NameHandleEncoder(direction);
        return;
    }

    if (direction == 0)
    {
        return;
    }

    if (uiMenuControlMode ==
        UI_MENU_CONTROL_ITEM_SELECTION)
    {
        int16_t candidateIndex =
            (direction > 0) ?
            UI_FindNextSelectableIndex(
                uiMenuPreviewItemIndex
            ) :
            UI_FindPreviousSelectableIndex(
                uiMenuPreviewItemIndex
            );

        if (candidateIndex >= 0)
        {
            uiMenuPreviewItemIndex =
                candidateIndex;

            UI_DrawMenu();
        }

        return;
    }

    if (direction > 0)
    {
        if (uiMenuSelectedRow <
            (int16_t)(UI_MenuGetRowCount() - 1U))
        {
            uiMenuSelectedRow++;
            UI_DrawMenu();
        }
    }
    else
    {
        if (uiMenuSelectedRow > 0)
        {
            uiMenuSelectedRow--;
            UI_DrawMenu();
        }
    }
}


static void UI_MenuHandleEnter(void)
{
    if (uiMenuPage ==
        UI_MENU_PAGE_SAVE_PRESET)
    {
        UI_SaveHandleEnter();
        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_SAVE_NAME)
    {
        UI_NameHandleEnter();
        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_GENERAL)
    {
        if (uiMenuSelectedRow >= 0 &&
            uiMenuSelectedRow <
                (int16_t)UI_GENERAL_MENU_ENTRY_COUNT)
        {
            uiGeneralMenuEntries[uiMenuSelectedRow].action();
        }

        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_DELETE_CONFIRM)
    {
        if (uiMenuSelectedRow ==
            UI_MENU_CONFIRM_DELETE_ROW)
        {
            UI_DeleteSelectedManualNode();
            return;
        }

        /* Cancel or title: return to root menu. */
        uiMenuPage = UI_MENU_PAGE_ROOT;
        uiMenuSelectedRow =
            UI_MENU_DELETE_NODE_ROW;
        UI_DrawMenu();
        return;
    }

    if (uiMenuControlMode ==
        UI_MENU_CONTROL_ITEM_SELECTION)
    {
        UI_MenuSetConfirmedItem(
            uiMenuPreviewItemIndex
        );

        uiMenuItemSelectionStartIndex =
            uiMenuItemIndex;

        uiMenuControlMode =
            UI_MENU_CONTROL_NAVIGATION;

        uiMenuSelectedRow =
            UI_MENU_ITEM_ROW;

        UI_DrawMenu();
        return;
    }

    if (uiMenuSelectedRow ==
        UI_MENU_ITEM_ROW)
    {
        uiMenuItemSelectionStartIndex =
            uiMenuItemIndex;

        uiMenuPreviewItemIndex =
            uiMenuItemIndex;

        uiMenuControlMode =
            UI_MENU_CONTROL_ITEM_SELECTION;

        UI_DrawMenu();
        return;
    }

    if (uiMenuSelectedRow ==
            UI_MENU_DELETE_NODE_ROW &&
        UI_MenuItemIsManualNode())
    {
        uiMenuPage =
            UI_MENU_PAGE_DELETE_CONFIRM;
        uiMenuSelectedRow =
            UI_MENU_CONFIRM_CANCEL_ROW;
        UI_DrawMenu();
        return;
    }

    if (uiMenuSelectedRow ==
        UI_MENU_GENERAL_ROW)
    {
        uiMenuPage =
            UI_MENU_PAGE_GENERAL;
        uiMenuSelectedRow = 0;
        UI_DrawMenu();
        return;
    }
}


static void UI_MenuHandleReturn(void)
{
    if (uiMenuPage ==
        UI_MENU_PAGE_SAVE_PRESET)
    {
        UI_SaveHandleReturn();
        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_SAVE_NAME)
    {
        UI_NameHandleReturn();
        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_GENERAL)
    {
        uiMenuPage = UI_MENU_PAGE_ROOT;
        uiMenuSelectedRow =
            UI_MENU_GENERAL_ROW;
        UI_DrawMenu();
        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_DELETE_CONFIRM)
    {
        uiMenuPage = UI_MENU_PAGE_ROOT;
        uiMenuSelectedRow =
            UI_MENU_DELETE_NODE_ROW;
        UI_DrawMenu();
        return;
    }

    if (uiMenuControlMode ==
        UI_MENU_CONTROL_ITEM_SELECTION)
    {
        uiMenuPreviewItemIndex =
            uiMenuItemSelectionStartIndex;

        uiMenuItemIndex =
            uiMenuItemSelectionStartIndex;

        uiMenuControlMode =
            UI_MENU_CONTROL_NAVIGATION;

        uiMenuSelectedRow =
            UI_MENU_ITEM_ROW;

        UI_DrawMenu();
        return;
    }

    /* Root-Ebene: Return schließt das Menü. */
    UI_CloseMenu(0U);
}


void UI_HandleReturnButton(void)
{
    if (uiMode == UI_MODE_MENU)
    {
        UI_MenuHandleReturn();
        return;
    }

    (void)UI_RestoreGrabStartState();
}


void UI_HandleMenuButton(void)
{
    if (uiMode == UI_MODE_MENU)
    {
        /* Unbestätigte Item-Vorschau verwerfen. */
        UI_CloseMenu(1U);
        return;
    }

    UI_OpenMenu();
}


void UI_ToggleGrab(void)
{
    int16_t focusedIndex =
        UI_GetFocusedIndex();

    if (focusedIndex < 0)
    {
        return;
    }

    /*
     * Automatische Knoten können grundsätzlich
     * nicht ausgewählt oder gegriffen werden.
     */
    if (!UI_IsSelectable(
            &uiItems[focusedIndex]))
    {
        return;
    }

    /*
     * Alten Fokusrahmen in der bisherigen Farbe
     * entfernen, bevor der Zustand geändert wird.
     */
    UI_EraseFocusFrame(
        focusedIndex
    );

    if (uiItems[focusedIndex].focus ==
        UI_FOCUS_SELECTED)
    {
        /*
         * Nur beim Beginn einer neuen Greifaktion
         * sichern. Einzelne Bewegungen überschreiben
         * diesen Snapshot nicht.
         */
        UI_SaveGrabStartState();

        uiItems[focusedIndex].focus =
            UI_FOCUS_GRABBED;
    }
    else if (uiItems[focusedIndex].focus ==
             UI_FOCUS_GRABBED)
    {
        uiItems[focusedIndex].focus =
            UI_FOCUS_SELECTED;
    }

    /*
     * Verbindungen neu zeichnen, falls beim Löschen
     * des äußeren Rahmens einzelne Pixel betroffen
     * waren.
     */
    UI_DrawConnectionsForItem(
        focusedIndex
    );

    /*
     * Dasselbe Item mit der neuen Fokusfarbe
     * wieder zeichnen.
     */
    UI_RedrawItem(
        focusedIndex
    );

    /*
     * Footer auf Blau oder Pink aktualisieren.
     */
    UI_DrawFooter();
}

void UI_HandleEncoderButton(
    uint8_t shiftPressed)
{
    if (uiMode == UI_MODE_MENU)
    {
        UI_MenuHandleEnter();
        return;
    }

    /*
     * Shift + Klick funktioniert nur bei einem
     * lediglich ausgewählten, nicht gegriffenen
     * Item.
     */
    if (shiftPressed &&
        !UI_IsGrabbed())
    {
        (void)UI_CreateManualNodeRightOfSelection();

        return;
    }


    /*
     * Ohne Shift bleibt das bisherige Verhalten:
     *
     * SELECTED ↔ GRABBED
     */
    if (!shiftPressed)
    {
        UI_ToggleGrab();
    }


    /*
     * Shift + Klick bei bereits gegriffenem Item:
     * vorerst keine Aktion.
     */
}

/* -------------------------------------------------------------------------- */
/* Movement snapshots                                                         */
/* -------------------------------------------------------------------------- */

static uint8_t UI_ItemPositionChanged(
    uint16_t itemIndex)
{
    if (itemIndex >= UI_ITEM_COUNT)
    {
        return 0;
    }


    /*
     * Alle Itemtypen berücksichtigen.
     *
     * Dadurch wird auch ein automatischer Knoten
     * als geändert erkannt, wenn dessen order beim
     * Löschen beispielsweise auf -100 gesetzt wird.
     */
    if (uiItems[itemIndex].order !=
            previousStepState[itemIndex].order ||
        uiItems[itemIndex].lane !=
            previousStepState[itemIndex].lane)
    {
        return 1;
    }


    return 0;
}


static void UI_SavePreviousStepState(void)
{
    /*
     * Itemzustände und alte Bildschirmpositionen
     * sichern.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        previousStepState[i].order =
            uiItems[i].order;

        previousStepState[i].lane =
            uiItems[i].lane;

        previousStepState[i].focus =
            uiItems[i].focus;

        previousStepGeometry[i] =
            uiGeometry[i];
    }


    /*
     * Aktuelle logische Verbindungsliste sichern.
     *
     * Dieser Snapshot wird später verwendet, um
     * die alten Pfeile schwarz zu überzeichnen.
     */
    previousStepConnectionCount =
        uiConnectionCount;

    if (previousStepConnectionCount >
        UI_MAX_CONNECTIONS)
    {
        previousStepConnectionCount =
            UI_MAX_CONNECTIONS;
    }


    for (uint16_t i = 0;
         i < previousStepConnectionCount;
         i++)
    {
        previousStepConnections[i] =
            uiConnections[i];
    }

    previousStepFirstVisibleOrder =
        uiFirstVisibleOrder;

    previousStepHighestVisibleLane =
        uiHighestVisibleLane;
}




static void UI_RestorePreviousStepState(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        uiItems[i].order =
            previousStepState[i].order;

        uiItems[i].lane =
            previousStepState[i].lane;

        uiItems[i].focus =
            previousStepState[i].focus;
    }
    uiFirstVisibleOrder =
        previousStepFirstVisibleOrder;

    uiHighestVisibleLane =
        previousStepHighestVisibleLane;
}



static uint8_t
UI_PermanentStructureChangedSincePreviousStep(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order !=
                previousStepState[i].order ||
            uiItems[i].lane !=
                previousStepState[i].lane)
        {
            return 1;
        }
    }

    return 0;
}


static int16_t UI_FindInactiveManualNodeIndex(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].type !=
            UI_ITEM_MANUAL_NODE)
        {
            continue;
        }

        /*
         * Negative Order bedeutet:
         * Reserveknoten ist derzeit inaktiv.
         */
        if (uiItems[i].order < 0)
        {
            return (int16_t)i;
        }
    }

    return -1;
}

static uint8_t UI_CreateManualNodeRightOfSelection(void)
{
    /*
     * Diese Funktion darf nur ausgeführt werden,
     * wenn kein Item gegriffen ist.
     */
    if (UI_IsGrabbed())
    {
        return 0;
    }


    int16_t selectedIndex =
        UI_GetFocusedIndex();


    if (selectedIndex < 0)
    {
        return 0;
    }


    const UI_Item *selectedItem =
        &uiItems[selectedIndex];


    if (!UI_IsSelectable(selectedItem))
    {
        return 0;
    }


    /*
     * Rechts von OUT darf kein manueller Knoten
     * entstehen, weil OUT die äußerste rechte
     * permanente Spalte bilden muss.
     */
    if (selectedItem->type ==
        UI_ITEM_OUTPUT)
    {
        return 0;
    }


    int16_t newNodeIndex =
        UI_FindInactiveManualNodeIndex();


    /*
     * Kein freier Reserveknoten vorhanden.
     */
    if (newNodeIndex < 0)
    {
        return 0;
    }


    int16_t targetOrder =
        selectedItem->order + 1;

    int16_t targetLane =
        selectedItem->lane;


    if (targetOrder < 0 ||
        targetOrder >=
            UI_MAX_ORDER_COUNT)
    {
        return 0;
    }


    /*
     * Prüfen, ob die konkrete Position rechts
     * vom ausgewählten Item durch ein permanentes
     * Item belegt ist.
     */
    int16_t targetPermanentIndex =
        UI_FindPermanentItemAt(
            targetOrder,
            targetLane,
            newNodeIndex
        );


    /*
     * Vor jeder Strukturänderung:
     *
     * - Itemzustände
     * - alte Geometrie
     * - alte Verbindungen
     * - Viewport
     *
     * sichern.
     */
    UI_SavePreviousStepState();


    if (targetPermanentIndex >= 0)
    {
        /*
         * Zielposition enthält ein permanentes Item.
         *
         * Vor der Zielorder wird eine neue Spalte
         * erzeugt. Alle permanenten Items ab dieser
         * Order wandern nach rechts.
         *
         * Der neue manuelle Knoten wird von der
         * Verschiebung ausgeschlossen.
         */
        UI_InsertOrderBefore(
            targetOrder,
            newNodeIndex
        );
    }


    /*
     * Sämtliche automatischen Knoten entfernen.
     *
     * Dadurch wird auch ein eventuell genau auf
     * der Zielposition liegender Auto-Knoten
     * verdrängt.
     */
    UI_RemoveAllAutoNodes();


    /*
     * Reserveknoten aktivieren.
     */
    uiItems[newNodeIndex].order =
        targetOrder;

    uiItems[newNodeIndex].lane =
        targetLane;

    uiItems[newNodeIndex].focus =
        UI_FOCUS_NONE;

    uiItems[newNodeIndex].loopStatus =
        UI_LOOP_STATUS_OFF;


    /*
     * Leere Spalten entfernen und Orders
     * normalisieren.
     *
     * Normalerweise entsteht hier keine Lücke,
     * die Funktion hält die Struktur aber robust.
     */
    UI_NormalizeOrders();


    /*
     * IN-/OUT-Randspalten herstellen.
     *
     * Besonders relevant, wenn der neue Knoten
     * direkt vor einem OUT eingefügt wird.
     */
    UI_EnsureEdgeColumns();


    if (!UI_ValidateEdgeRules())
    {
        UI_RestorePreviousStepState();

        UI_RebuildCalculatedConnections();

        return 0;
    }


    /*
     * Pfeile aus der neuen Struktur berechnen.
     *
     * Noch erforderliche automatische Knoten
     * werden weiterhin nicht erzeugt. An diesen
     * Stellen bleibt der Pfad offen.
     */
    if (!UI_RebuildCalculatedConnections())
    {
        UI_RestorePreviousStepState();

        UI_RebuildCalculatedConnections();

        return 0;
    }


    /*
     * Den neuen Knoten sichtbar halten.
     *
     * Falls durch das Einfügen einer neuen Order
     * horizontal gescrollt werden muss, folgt der
     * Viewport dem neuen Knoten.
     */
    uint8_t viewportChanged =
        UI_EnsureItemVisible(
            newNodeIndex
        );


    if (viewportChanged)
    {
        /*
         * Alle sichtbaren X-Positionen haben sich
         * verändert.
         */
        UI_Draw();
    }
    else
    {
        /*
         * Struktur lokal aktualisieren.
         *
         * UI_ItemPositionChanged() erkennt den
         * neuen Knoten anhand:
         *
         * vorher order < 0
         * nachher order >= 0
         */
        UI_UpdateChangedStructureDisplay();
    }


    return 1;
}

static void UI_MoveGrabbedHorizontal(
    int8_t direction)
{
    if (direction == 0)
    {
        return;
    }


    int16_t grabbedIndex =
        UI_GetFocusedIndex();


    if (grabbedIndex < 0)
    {
        return;
    }


    if (uiItems[grabbedIndex].focus !=
        UI_FOCUS_GRABBED)
    {
        return;
    }


    UI_Item *grabbedItem =
        &uiItems[grabbedIndex];


    if (!UI_IsPermanentItem(
            grabbedItem))
    {
        return;
    }


    /*
     * Inputs und Outputs selbst bleiben vorerst
     * fest in ihren Randspalten.
     */
    
    /*
     * Ein Input darf nicht weiter nach links bewegt
     * werden, weil er bereits die linke Randseite bildet.
     *
     * Ein Output darf nicht weiter nach rechts bewegt
     * werden, weil er bereits die rechte Randseite bildet.
     *
     * Bewegung nach innen bleibt erlaubt und wird später
     * durch die vollständigen Randregeln validiert.
     */
    if (grabbedItem->type == UI_ITEM_INPUT &&
        direction < 0)
    {
        return;
    }
    
    
    if (grabbedItem->type == UI_ITEM_OUTPUT &&
        direction > 0)
    {
        return;
    }

    /*
     * Vollständigen Zustand vor diesem einzelnen
     * Encoderschritt sichern.
     */
    UI_SavePreviousStepState();


    int16_t targetOrder =
        grabbedItem->order +
        ((direction > 0) ? 1 : -1);


    /*
     * Links von order 0 existiert zunächst keine
     * direkte Zielposition.
     *
     * Die neue Input-Randspalte entsteht erst,
     * wenn ein Item die bisherige order 0 betritt.
     */
   if (targetOrder < 0 ||
    targetOrder >=
        UI_MAX_ORDER_COUNT)
{
    return;
}


    int16_t targetPermanentIndex =
        UI_FindPermanentItemAt(
            targetOrder,
            grabbedItem->lane,
            grabbedIndex
        );


    /*
     * Inputs und Outputs dürfen in dieser ersten
     * Implementierung nur auf eine leere Position oder
     * auf die Position eines automatischen Knotens
     * bewegt werden.
     *
     * Ein direkter Tausch mit einem anderen permanenten
     * Item wird vorerst verhindert.
     */
    if ((grabbedItem->type == UI_ITEM_INPUT ||
     grabbedItem->type == UI_ITEM_OUTPUT) &&
    targetPermanentIndex >= 0)
{
    /*
     * Die Verschiebung ist blockiert.
     *
     * Falls sich das Item am sichtbaren Rand
     * befindet, versuchen wir stattdessen einen
     * zusätzlichen Viewport-Schritt.
     */
    UI_TryBlockedEdgeScroll(
        grabbedIndex,
        direction
    );

    return;
}
    
    /*
     * Ziel ist ein normales permanentes Item.
     */
    if (targetPermanentIndex >= 0)
    {
        UI_ItemType targetType =
            uiItems[
                targetPermanentIndex
            ].type;


        if ((targetType ==
                 UI_ITEM_INPUT &&
             direction < 0) ||
            (targetType ==
                 UI_ITEM_OUTPUT &&
             direction > 0))
        {
            /*
             * Das gegriffene Item betritt die
             * bisherige Randspalte.
             *
             * Kein Tausch mit IN beziehungsweise OUT.
             * UI_EnsureEdgeColumns() verschiebt den
             * Rand anschließend nach außen.
             */
            grabbedItem->order =
                uiItems[
                    targetPermanentIndex
                ].order;
        }
        else
        {
            /*
             * Zwei normale permanente Items tauschen
             * ihre vollständigen Rasterpositionen.
             */
            int16_t grabbedOldOrder =
                grabbedItem->order;

            int16_t grabbedOldLane =
                grabbedItem->lane;

            int16_t targetOldOrder =
                uiItems[
                    targetPermanentIndex
                ].order;

            int16_t targetOldLane =
                uiItems[
                    targetPermanentIndex
                ].lane;


            uiItems[
                targetPermanentIndex
            ].order =
                grabbedOldOrder;

            uiItems[
                targetPermanentIndex
            ].lane =
                grabbedOldLane;


            grabbedItem->order =
                targetOldOrder;

            grabbedItem->lane =
                targetOldLane;
        }
    }
    else
    {
        /*
         * Zielposition ist leer oder enthält nur
         * einen automatischen Knoten.
         */
        grabbedItem->order =
            targetOrder;
    }


    /*
     * Nach jeder Strukturänderung werden alle
     * automatischen Knoten entfernt.
     */
/*
 * Automatische Knoten nach einer vorläufigen
 * Strukturänderung entfernen.
 */
UI_RemoveAllAutoNodes();


/*
 * Gültige IN-/OUT-Randspalten herstellen und
 * vollständig leere Spalten schließen.
 */
UI_EnsureEdgeColumns();


/*
 * Der normalisierte Zustand muss gültige
 * Randspalten besitzen.
 */
if (!UI_ValidateEdgeRules())
{
    /*
     * Ungültige Struktur vollständig
     * wiederherstellen.
     */
    UI_RestorePreviousStepState();

    /*
     * Nach dem Restore besitzt das Item wieder
     * seine ursprüngliche Order.
     *
     * Jetzt darf gegebenenfalls nur der Viewport
     * über den blockierenden IN-/OUT-Rand bewegt
     * werden.
     */
    UI_TryBlockedEdgeScroll(
        grabbedIndex,
        direction
    );

    return;
}


/*
 * Entscheidend ist der normalisierte Endzustand.
 *
 * Falls alle permanenten Items wieder dieselben
 * Positionen besitzen wie vor dem Encoderschritt,
 * hatte die Bewegung keine dauerhafte Wirkung.
 *
 * Der komplette Snapshot wird wiederhergestellt,
 * damit auch die vorläufig entfernten automatischen
 * Knoten erhalten bleiben.
 */
if (!UI_PermanentStructureChangedSincePreviousStep())
{
    /*
     * Randspaltenerzeugung und Normalisierung
     * haben wieder zum ursprünglichen permanenten
     * Zustand geführt.
     *
     * Die Itembewegung hatte daher keine sichtbare
     * beziehungsweise dauerhafte Wirkung.
     */
    UI_RestorePreviousStepState();


    /*
     * Sonderfall:
     *
     * Befindet sich das gegriffene Item am sichtbaren
     * Bildschirmrand und liegt direkt außerhalb die
     * blockierende IN- oder OUT-Randspalte, wird nur
     * der Viewport um eine Position verschoben.
     *
     * order und lane des Items bleiben unverändert.
     */
    (void)UI_TryBlockedEdgeScroll(
        grabbedIndex,
        direction
    );


    return;
}


/*
 * Verbindungsliste aus der neuen seriellen
 * Rasterreihenfolge aufbauen.
 */
if (!UI_RebuildCalculatedConnections())
{
    /*
     * Neue Struktur ist für Stufe 1 nicht seriell
     * oder der Verbindungspuffer reicht nicht aus.
     *
     * Vorherigen Zustand wiederherstellen.
     */
    UI_RestorePreviousStepState();

    /*
     * Auch die alte Verbindungsliste wieder aus
     * dem wiederhergestellten Zustand erzeugen.
     */
    UI_RebuildCalculatedConnections();

    return;
}


/*
 * Alte Verbindungen aus dem Snapshot entfernen
 * und die neue Struktur lokal darstellen.
 */
/*
 * Nach der Strukturänderung sicherstellen, dass
 * das gegriffene Item weiterhin sichtbar bleibt.
 */
uint8_t viewportChanged =
    UI_EnsureItemVisible(
        grabbedIndex
    );


if (viewportChanged)
{
    /*
     * Alle sichtbaren Spalten haben sich relativ
     * zum Display verschoben.
     */
    UI_Draw();
}
else
{
    /*
     * Viewport unverändert:
     * schnelles lokales Strukturupdate.
     */
    UI_UpdateChangedStructureDisplay();
}
}

/* -------------------------------------------------------------------------- */
/* Vertical movement of grabbed item                                          */
/* -------------------------------------------------------------------------- */

static void UI_InsertOrderBefore(
    int16_t insertionOrder,
    int16_t excludedItemIndex)
{
    /*
     * Alle permanenten Items ab der Einfügeposition
     * werden eine Order nach rechts verschoben.
     *
     * Das gegriffene Item wird ausgeschlossen,
     * weil dessen endgültige Position anschließend
     * separat gesetzt wird.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if ((int16_t)i ==
            excludedItemIndex)
        {
            continue;
        }

        if (!UI_IsPermanentItem(
                &uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order >=
            insertionOrder)
        {
            uiItems[i].order++;
        }
    }
}

static void UI_MoveGrabbedVertical(
    int8_t direction)
{
    if (direction == 0)
    {
        return;
    }


    int16_t grabbedIndex =
        UI_GetFocusedIndex();


    if (grabbedIndex < 0)
    {
        return;
    }


    /*
     * Nur das aktuell gegriffene Item darf
     * vertikal bewegt werden.
     */
    if (uiItems[grabbedIndex].focus !=
        UI_FOCUS_GRABBED)
    {
        return;
    }


    UI_Item *grabbedItem =
        &uiItems[grabbedIndex];


    /*
     * Automatische Knoten sind nicht greifbar
     * und dürfen nicht direkt bewegt werden.
     */
    if (!UI_IsPermanentItem(
            grabbedItem))
    {
        return;
    }


    /*
     * Aktuelle Zuordnung:
     *
     * positive Richtung:
     * lane + 1 = nach oben
     *
     * negative Richtung:
     * lane - 1 = nach unten
     */
    int16_t targetLane =
        grabbedItem->lane +
        ((direction > 0) ? -1 : 1);


    /*
     * Prüfen, ob sich in derselben Order auf der
     * Ziel-Lane bereits ein permanentes Item
     * befindet.
     */
    int16_t targetPermanentIndex =
        UI_FindPermanentItemAt(
            grabbedItem->order,
            targetLane,
            grabbedIndex
        );


    /*
     * Zustand unmittelbar vor der tatsächlichen
     * Änderung sichern.
     *
     * Gesichert werden:
     *
     * - order und lane aller Items
     * - Fokuszustände
     * - alte Bildschirmgeometrie
     * - alte Verbindungsliste
     * - horizontaler Viewport
     */
    UI_SavePreviousStepState();


    if (targetPermanentIndex >= 0)
{
    /*
     * Regel 2.6:
     *
     * In der angrenzenden Zielposition befindet
     * sich ein permanentes Item.
     *
     * Das gegriffene Item wird nicht mit diesem
     * Item getauscht. Es wird stattdessen rechts
     * neben diesem Item platziert.
     */
    const UI_Item *adjacentItem =
        &uiItems[targetPermanentIndex];


    int16_t destinationOrder =
        adjacentItem->order + 1;

    int16_t destinationLane =
        adjacentItem->lane;


    /*
     * Prüfen, ob die gewünschte Position rechts
     * neben dem angrenzenden Item bereits durch
     * ein anderes permanentes Item belegt ist.
     */
    int16_t destinationItemIndex =
        UI_FindPermanentItemAt(
            destinationOrder,
            destinationLane,
            grabbedIndex
        );


    if (destinationItemIndex >= 0)
    {
        /*
         * Zielposition belegt:
         *
         * Vor der Zielposition wird eine neue
         * Order eingefügt. Alle permanenten Items
         * ab dieser Order wandern nach rechts.
         *
         * Das gegriffene Item sitzt anschließend
         * allein in der neu erzeugten Spalte.
         */
        UI_InsertOrderBefore(
            destinationOrder,
            grabbedIndex
        );
    }


    /*
     * In beiden Fällen nimmt das gegriffene Item
     * die Position rechts vom angrenzenden Item ein.
     *
     * Falls eine neue Order eingefügt wurde, ist
     * destinationOrder nun genau diese neue Order.
     */
    grabbedItem->order =
        destinationOrder;

    grabbedItem->lane =
        destinationLane;
}
else
{
    /*
     * Regel 2.5:
     *
     * Die angrenzende vertikale Zielposition ist
     * frei oder enthält nur einen automatischen
     * Knoten.
     *
     * Das Item bleibt in seiner Order und übernimmt
     * lediglich die neue Lane.
     */
    grabbedItem->lane =
        targetLane;
}



    /*
     * Nach einer Benutzeränderung werden alle
     * vorhandenen automatischen Knoten entfernt.
     *
     * Neue automatische Knoten werden in diesem
     * Entwicklungsschritt noch nicht erzeugt.
     */
    UI_RemoveAllAutoNodes();
/*
 * Durch das Versetzen nach rechts kann die
 * ursprüngliche Spalte des gegriffenen Items
 * vollständig leer geworden sein.
 *
 * Leere Spalten entfernen und Orders lückenlos
 * normalisieren.
 */
UI_NormalizeOrders();


/*
 * Sicherstellen, dass weiterhin ausschließlich
 * Inputs links und Outputs rechts liegen.
 *
 * Das kann insbesondere relevant werden, wenn
 * durch Regel 2.6 eine zusätzliche Spalte direkt
 * vor der Output-Spalte entsteht.
 */
UI_EnsureEdgeColumns();


    /*
     * Vertikale Bewegungen verändern keine Orders.
     *
     * Die bestehenden IN-/OUT-Randregeln müssen
     * daher weiterhin erfüllt sein.
     */
    if (!UI_ValidateEdgeRules())
    {
        UI_RestorePreviousStepState();

        /*
         * Alte Verbindungsliste aus dem
         * wiederhergestellten Zustand aufbauen.
         */
        UI_RebuildCalculatedConnections();

        return;
    }


    /*
     * Prüfen, ob sich die permanente Struktur
     * tatsächlich verändert hat.
     */
    if (!UI_PermanentStructureChangedSincePreviousStep())
    {
        UI_RestorePreviousStepState();

        return;
    }


    /*
     * Pfeile aus den neuen Itempositionen
     * vollständig neu berechnen.
     *
     * Dabei gelten weiterhin:
     *
     * - Pfeillänge genau eine Order
     * - Lane-Differenz nur 0, +1 oder -1
     *
     * Wenn kein gültiges Ziel beziehungsweise
     * keine gültige Quelle gefunden wird, bleibt
     * die Verbindung in dieser Entwicklungsstufe
     * offen.
     */
    if (!UI_RebuildCalculatedConnections())
    {
        UI_RestorePreviousStepState();

        /*
         * Alte Verbindungsliste wiederherstellen.
         */
        UI_RebuildCalculatedConnections();

        return;
    }


    /*
     * Das gegriffene Item muss nach der Bewegung
     * horizontal und vertikal sichtbar bleiben.
     * Regel 2.6 kann zusätzlich die Order verändern.
     */
    uint8_t horizontalViewportChanged =
        UI_EnsureItemVisible(
            grabbedIndex
        );

    uint8_t verticalViewportChanged =
        UI_EnsureItemVerticallyVisible(
            grabbedIndex
        );


    if (horizontalViewportChanged ||
        verticalViewportChanged)
    {
        UI_Draw();
    }
    else
    {
        UI_UpdateChangedStructureDisplay();
    }
}

/* -------------------------------------------------------------------------- */
/* Central encoder action                                                     */
/* -------------------------------------------------------------------------- */

void UI_HandleEncoderStep(
    int8_t direction,
    uint8_t shiftPressed)
{
    if (uiMode == UI_MODE_MENU)
    {
        UI_MenuHandleEncoderStep(
            direction
        );

        return;
    }

    if (direction == 0)
    {
        return;
    }


    /*
     * Im Grab-Modus entscheidet Shift über die
     * Bewegungsrichtung.
     */
    if (UI_IsGrabbed())
    {
        if (shiftPressed)
        {
            /*
             * Shift gehalten:
             * gegriffenes Item vertikal bewegen.
             */
            UI_MoveGrabbedVertical(
                direction
            );
        }
        else
        {
            /*
             * Shift nicht gehalten:
             * gegriffenes Item horizontal bewegen.
             */
            UI_MoveGrabbedHorizontal(
                direction
            );
        }

        return;
    }


    /*
     * Wenn kein Item gegriffen ist, bleibt die
     * bisherige Auswahlbewegung unverändert.
     *
     * Shift besitzt in diesem Zustand vorerst
     * keine Funktion.
     */
    if (direction > 0)
    {
        UI_SelectNext();
    }
    else
    {
        UI_SelectPrevious();
    }
}

static void UI_DrawLoop(const UI_Item *item, int16_t x, int16_t y)
{
    uint16_t fillColor = UI_GetFillColor(item);
    uint16_t textColor = UI_GetTextColor(item);
    uint16_t focusColor = UI_GetFocusColor(item);

    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        UI_DrawRoundedRect(
            x - UI_FOCUS_FRAME_GAP,
            y - UI_FOCUS_FRAME_GAP,
            UI_ELEMENT_WIDTH + (2 * UI_FOCUS_FRAME_GAP),
            UI_ELEMENT_HEIGHT + (2 * UI_FOCUS_FRAME_GAP),
            UI_FOCUS_CORNER_RADIUS,
            focusColor);
    }

    UI_FillRoundedRect(x, y, UI_ELEMENT_WIDTH, UI_ELEMENT_HEIGHT,
                       UI_LOOP_CORNER_RADIUS, fillColor);
    UI_DrawRoundedRect(x, y, UI_ELEMENT_WIDTH, UI_ELEMENT_HEIGHT,
                       UI_LOOP_CORNER_RADIUS, UI_COLOR_BORDER_NORMAL);

    UI_DrawCenteredText((uint16_t)x, (uint16_t)y,
                        UI_ELEMENT_WIDTH, UI_ELEMENT_HEIGHT,
                        item->shortName, textColor, fillColor, 1);
}

static void UI_DrawIO(const UI_Item *item, int16_t x, int16_t y)
{
    uint16_t borderColor = UI_GetFocusColor(item);
    int16_t centerX = x + (UI_ELEMENT_WIDTH / 2);
    int16_t centerY = y + (UI_ELEMENT_HEIGHT / 2);

    UI_DrawCircle(centerX, centerY, UI_IO_CIRCLE_RADIUS, borderColor);
    ST7735_DrawPixel(centerX, centerY, borderColor);

    uint16_t textWidth = Font5x7_GetStringWidth(item->shortName, 1);
    int16_t textX = centerX - ((int16_t)textWidth / 2);

    if (textX < x)
    {
        textX = x;
    }

    if ((textX + (int16_t)textWidth) > (x + UI_ELEMENT_WIDTH))
    {
        textX = x + UI_ELEMENT_WIDTH - (int16_t)textWidth;
    }

    uint16_t textY =
        (uint16_t)(centerY + UI_IO_CIRCLE_RADIUS + 3);

    Font5x7_DrawString((uint16_t)textX, textY, item->shortName,
                       UI_COLOR_TEXT_LIGHT, UI_COLOR_BACKGROUND, 1);
}

static void UI_DrawManualNode(const UI_Item *item,
                              int16_t x, int16_t y)
{
    int16_t centerX = x + (UI_ELEMENT_WIDTH / 2);
    int16_t centerY = y + (UI_ELEMENT_HEIGHT / 2);

    if (item->focus == UI_FOCUS_SELECTED ||
        item->focus == UI_FOCUS_GRABBED)
    {
        uint16_t focusColor = UI_GetFocusColor(item);
        int16_t r = UI_MANUAL_NODE_RADIUS + UI_FOCUS_FRAME_GAP;

        ST7735_DrawLine(centerX, centerY - r,
                        centerX + r, centerY, focusColor);
        ST7735_DrawLine(centerX + r, centerY,
                        centerX, centerY + r, focusColor);
        ST7735_DrawLine(centerX, centerY + r,
                        centerX - r, centerY, focusColor);
        ST7735_DrawLine(centerX - r, centerY,
                        centerX, centerY - r, focusColor);
    }

    int16_t r = UI_MANUAL_NODE_RADIUS;
    ST7735_DrawLine(centerX, centerY - r,
                    centerX + r, centerY, UI_COLOR_BORDER_NORMAL);
    ST7735_DrawLine(centerX + r, centerY,
                    centerX, centerY + r, UI_COLOR_BORDER_NORMAL);
    ST7735_DrawLine(centerX, centerY + r,
                    centerX - r, centerY, UI_COLOR_BORDER_NORMAL);
    ST7735_DrawLine(centerX - r, centerY,
                    centerX, centerY - r, UI_COLOR_BORDER_NORMAL);
}

static void UI_DrawAutoNode(
    int16_t x,
    int16_t y)
{
    int16_t centerX =
        x + (UI_ELEMENT_WIDTH / 2);

    int16_t centerY =
        y + (UI_ELEMENT_HEIGHT / 2);


    /*
     * Automatic nodes are displayed as completely
     * filled white circles.
     *
     * The radius is large enough to provide separate
     * horizontal and diagonal connection points.
     */
    UI_FillCircle(
        centerX,
        centerY,
        UI_AUTO_NODE_RADIUS,
        UI_COLOR_BORDER_NORMAL
    );
}


static void UI_DrawItem(const UI_Item *item, int16_t x, int16_t y)
{
    switch (item->type)
    {
        case UI_ITEM_LOOP:
            UI_DrawLoop(item, x, y);
            break;
        case UI_ITEM_INPUT:
        case UI_ITEM_OUTPUT:
            UI_DrawIO(item, x, y);
            break;
        case UI_ITEM_MANUAL_NODE:
            UI_DrawManualNode(item, x, y);
            break;
        case UI_ITEM_AUTO_NODE:
            UI_DrawAutoNode(x, y);
            break;
        default:
            break;
    }
}

static uint8_t UI_IsCircularItem(const UI_Item *item)
{
    if (item == NULL)
    {
        return 0;
    }

    return (item->type == UI_ITEM_INPUT ||
            item->type == UI_ITEM_OUTPUT ||
            item->type == UI_ITEM_AUTO_NODE);
}

static int16_t UI_GetCircularRadius(const UI_Item *item)
{
    if (item != NULL && item->type == UI_ITEM_AUTO_NODE)
    {
        return UI_AUTO_NODE_RADIUS;
    }

    return UI_IO_CIRCLE_RADIUS;
}

static int16_t UI_GetCircularDiagonalOffset(const UI_Item *item)
{
    if (item != NULL && item->type == UI_ITEM_AUTO_NODE)
    {
        return UI_AUTO_NODE_DIAGONAL_OFFSET;
    }

    return UI_IO_DIAGONAL_OFFSET;
}

static UI_ConnectionPoint UI_GetSourceConnectionPoint(
    const UI_ItemGeometry *geometry,
    const UI_Item *item,
    int16_t targetY)
{
    UI_ConnectionPoint point;
    int16_t centerX = geometry->x + (geometry->width / 2);
    int16_t centerY = geometry->y + (geometry->height / 2);

    if (targetY == centerY)
    {
        if (UI_IsCircularItem(item))
        {
            point.x = centerX + UI_GetCircularRadius(item);
        }
        else if (item->type == UI_ITEM_MANUAL_NODE)
        {
            point.x = centerX + UI_MANUAL_NODE_RADIUS;
        }
        else
        {
            point.x = geometry->x + geometry->width - 1;
        }
        point.y = centerY;
        return point;
    }

    if (targetY < centerY)
    {
        if (UI_IsCircularItem(item))
        {
            int16_t offset = UI_GetCircularDiagonalOffset(item);
            point.x = centerX + offset;
            point.y = centerY - offset;
        }
        else if (item->type == UI_ITEM_MANUAL_NODE)
        {
            point.x = centerX + UI_MANUAL_NODE_DIAGONAL_OFFSET;
            point.y = centerY - UI_MANUAL_NODE_DIAGONAL_OFFSET;
        }
        else
        {
            point.x = geometry->x + geometry->width - 1 -
                      UI_LOOP_DIAGONAL_INSET;
            point.y = geometry->y + UI_LOOP_DIAGONAL_INSET;
        }
        return point;
    }

    if (UI_IsCircularItem(item))
    {
        int16_t offset = UI_GetCircularDiagonalOffset(item);
        point.x = centerX + offset;
        point.y = centerY + offset;
    }
    else if (item->type == UI_ITEM_MANUAL_NODE)
    {
        point.x = centerX + UI_MANUAL_NODE_DIAGONAL_OFFSET;
        point.y = centerY + UI_MANUAL_NODE_DIAGONAL_OFFSET;
    }
    else
    {
        point.x = geometry->x + geometry->width - 1 -
                  UI_LOOP_DIAGONAL_INSET;
        point.y = geometry->y + geometry->height - 1 -
                  UI_LOOP_DIAGONAL_INSET;
    }

    return point;
}

static UI_ConnectionPoint UI_GetTargetConnectionPoint(
    const UI_ItemGeometry *geometry,
    const UI_Item *item,
    int16_t sourceY)
{
    UI_ConnectionPoint point;
    int16_t centerX = geometry->x + (geometry->width / 2);
    int16_t centerY = geometry->y + (geometry->height / 2);

    if (sourceY == centerY)
    {
        if (UI_IsCircularItem(item))
        {
            point.x = centerX - UI_GetCircularRadius(item);
        }
        else if (item->type == UI_ITEM_MANUAL_NODE)
        {
            point.x = centerX - UI_MANUAL_NODE_RADIUS;
        }
        else
        {
            point.x = geometry->x;
        }
        point.y = centerY;
        return point;
    }

    if (sourceY > centerY)
    {
        if (UI_IsCircularItem(item))
        {
            int16_t offset = UI_GetCircularDiagonalOffset(item);
            point.x = centerX - offset;
            point.y = centerY + offset;
        }
        else if (item->type == UI_ITEM_MANUAL_NODE)
        {
            point.x = centerX - UI_MANUAL_NODE_DIAGONAL_OFFSET;
            point.y = centerY + UI_MANUAL_NODE_DIAGONAL_OFFSET;
        }
        else
        {
            point.x = geometry->x + UI_LOOP_DIAGONAL_INSET;
            point.y = geometry->y + geometry->height - 1 -
                      UI_LOOP_DIAGONAL_INSET;
        }
        return point;
    }

    if (UI_IsCircularItem(item))
    {
        int16_t offset = UI_GetCircularDiagonalOffset(item);
        point.x = centerX - offset;
        point.y = centerY - offset;
    }
    else if (item->type == UI_ITEM_MANUAL_NODE)
    {
        point.x = centerX - UI_MANUAL_NODE_DIAGONAL_OFFSET;
        point.y = centerY - UI_MANUAL_NODE_DIAGONAL_OFFSET;
    }
    else
    {
        point.x = geometry->x + UI_LOOP_DIAGONAL_INSET;
        point.y = geometry->y + UI_LOOP_DIAGONAL_INSET;
    }

    return point;
}


static void UI_DrawConnectionLine(int16_t x0, int16_t y0,
                                  int16_t x1, int16_t y1,
                                  uint16_t color)
{
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t absDx = (dx >= 0) ? dx : -dx;
    int16_t absDy = (dy >= 0) ? dy : -dy;
    int16_t steps = (absDx > absDy) ? absDx : absDy;

    if (steps == 0)
    {
        ST7735_DrawPixel(x0, y0, color);
        return;
    }

    int32_t currentX = ((int32_t)x0 << 8);
    int32_t currentY = ((int32_t)y0 << 8);
    int32_t stepX = ((int32_t)dx << 8) / steps;
    int32_t stepY = ((int32_t)dy << 8) / steps;
    int16_t previousX = x0;
    int16_t previousY = y0;

    ST7735_DrawPixel(previousX, previousY, color);

    for (int16_t step = 1; step <= steps; step++)
    {
        currentX += stepX;
        currentY += stepY;

        int16_t pixelX = (int16_t)((currentX + 128) >> 8);
        int16_t pixelY = (int16_t)((currentY + 128) >> 8);

        if (pixelX != previousX && pixelY != previousY)
        {
            ST7735_DrawPixel(pixelX, previousY, color);
        }

        ST7735_DrawPixel(pixelX, pixelY, color);
        previousX = pixelX;
        previousY = pixelY;
    }
}

static void UI_DrawDirectConnection(
    int16_t startX,
    int16_t startY,
    int16_t targetX,
    int16_t targetY,
    uint16_t color)
{
    int16_t deltaX = targetX - startX;
    int16_t deltaY = targetY - startY;

    if (deltaX <= 0)
    {
        return;
    }

    int16_t absX = deltaX;
    int16_t absY = (deltaY >= 0) ? deltaY : -deltaY;
    int16_t normalization = (absX > absY) ? absX : absY;

    if (normalization == 0)
    {
        return;
    }

    int32_t directionX =
        ((int32_t)deltaX * UI_ARROW_FIXED_SCALE) / normalization;
    int32_t directionY =
        ((int32_t)deltaY * UI_ARROW_FIXED_SCALE) / normalization;

    int16_t tipX =
        targetX - (int16_t)(directionX / UI_ARROW_FIXED_SCALE);
    int16_t tipY =
        targetY - (int16_t)(directionY / UI_ARROW_FIXED_SCALE);

    int16_t baseCenterX =
        tipX - (int16_t)((directionX * UI_ARROW_LENGTH) /
                         UI_ARROW_FIXED_SCALE);
    int16_t baseCenterY =
        tipY - (int16_t)((directionY * UI_ARROW_LENGTH) /
                         UI_ARROW_FIXED_SCALE);

    int16_t perpendicularX =
        (int16_t)(((-directionY) * UI_ARROW_HALF_WIDTH) /
                  UI_ARROW_FIXED_SCALE);
    int16_t perpendicularY =
        (int16_t)((directionX * UI_ARROW_HALF_WIDTH) /
                  UI_ARROW_FIXED_SCALE);

    int16_t base1X = baseCenterX + perpendicularX;
    int16_t base1Y = baseCenterY + perpendicularY;
    int16_t base2X = baseCenterX - perpendicularX;
    int16_t base2Y = baseCenterY - perpendicularY;

    UI_DrawConnectionLine(startX, startY, tipX, tipY,
                          color);

    UI_FillTriangle(tipX, tipY,
                    base1X, base1Y,
                    base2X, base2Y,
                    color);
}

static void UI_DrawConnectionFromState(
    const UI_Connection *connection,
    const UI_ItemGeometry *geometryState,
    uint16_t color)
{
    if (connection == NULL ||
        geometryState == NULL)
    {
        return;
    }


    int16_t sourceIndex =
        UI_FindItemIndexById(
            connection->sourceId
        );

    int16_t targetIndex =
        UI_FindItemIndexById(
            connection->targetId
        );


    if (sourceIndex < 0 ||
        targetIndex < 0)
    {
        return;
    }


    const UI_ItemGeometry *sourceGeometry =
        &geometryState[sourceIndex];

    const UI_ItemGeometry *targetGeometry =
        &geometryState[targetIndex];


    /*
     * Nur Verbindungen zeichnen, deren Quelle und
     * Ziel im jeweiligen Geometriezustand sichtbar
     * waren.
     */
    if (!sourceGeometry->visible ||
        !targetGeometry->visible)
    {
        return;
    }


    const UI_Item *sourceItem =
        &uiItems[sourceIndex];

    const UI_Item *targetItem =
        &uiItems[targetIndex];


    int16_t sourceCenterY =
        sourceGeometry->y +
        (sourceGeometry->height / 2);

    int16_t targetCenterY =
        targetGeometry->y +
        (targetGeometry->height / 2);


    UI_ConnectionPoint sourcePoint =
        UI_GetSourceConnectionPoint(
            sourceGeometry,
            sourceItem,
            targetCenterY
        );

    UI_ConnectionPoint targetPoint =
        UI_GetTargetConnectionPoint(
            targetGeometry,
            targetItem,
            sourceCenterY
        );


    UI_DrawDirectConnection(
        sourcePoint.x,
        sourcePoint.y,
        targetPoint.x,
        targetPoint.y,
        color
    );
}

static void UI_ErasePreviousConnections(void)
{
    /*
     * Alte Pfeile exakt an ihren alten Positionen
     * mit der Hintergrundfarbe überzeichnen.
     */
    for (uint16_t i = 0;
         i < previousStepConnectionCount;
         i++)
    {
        UI_DrawConnectionFromState(
            &previousStepConnections[i],
            previousStepGeometry,
            UI_COLOR_BACKGROUND
        );
    }
}

static void UI_DrawCurrentConnections(void)
{
    for (uint16_t i = 0;
         i < uiConnectionCount;
         i++)
    {
        UI_DrawConnectionFromState(
            &uiConnections[i],
            uiGeometry,
            UI_COLOR_CONNECTION
        );
    }
}

static void UI_RedrawAllVisibleItems(void)
{
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (!uiGeometry[i].visible)
        {
            continue;
        }


        UI_DrawItem(
            &uiItems[i],
            uiGeometry[i].x,
            uiGeometry[i].y
        );
    }
}

static void UI_DrawConnectionByIndex(
    uint16_t connectionIndex,
    uint16_t color)
{
    if (connectionIndex >=
        uiConnectionCount)
    {
        return;
    }


    int16_t sourceIndex =
        UI_FindItemIndexById(
            uiConnections[
                connectionIndex
            ].sourceId
        );

    int16_t targetIndex =
        UI_FindItemIndexById(
            uiConnections[
                connectionIndex
            ].targetId
        );


    if (sourceIndex < 0 ||
        targetIndex < 0)
    {
        return;
    }


    const UI_ItemGeometry *sourceGeometry =
        &uiGeometry[sourceIndex];

    const UI_ItemGeometry *targetGeometry =
        &uiGeometry[targetIndex];

    const UI_Item *sourceItem =
        &uiItems[sourceIndex];

    const UI_Item *targetItem =
        &uiItems[targetIndex];


    if (!sourceGeometry->visible ||
        !targetGeometry->visible)
    {
        return;
    }


    int16_t sourceCenterY =
        sourceGeometry->y +
        (sourceGeometry->height / 2);

    int16_t targetCenterY =
        targetGeometry->y +
        (targetGeometry->height / 2);


    UI_ConnectionPoint sourcePoint =
        UI_GetSourceConnectionPoint(
            sourceGeometry,
            sourceItem,
            targetCenterY
        );

    UI_ConnectionPoint targetPoint =
        UI_GetTargetConnectionPoint(
            targetGeometry,
            targetItem,
            sourceCenterY
        );


    UI_DrawDirectConnection(
        sourcePoint.x,
        sourcePoint.y,
        targetPoint.x,
        targetPoint.y,
        color
    );
}

  
static void UI_DrawConnectionsForItem(
    int16_t itemIndex)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return;
    }


    uint16_t itemId =
        uiItems[itemIndex].id;


    for (uint16_t i = 0;
         i < uiConnectionCount;
         i++)
    {
        if (uiConnections[i].sourceId ==
                itemId ||
            uiConnections[i].targetId ==
                itemId)
        {
            UI_DrawConnectionByIndex(
                i,
                UI_COLOR_CONNECTION
            );
        }
    }
}


static void UI_DrawConnections(void)
{
    for (uint16_t i = 0;
         i < uiConnectionCount;
         i++)
    {
        UI_DrawConnectionByIndex(
            i,
            UI_COLOR_CONNECTION
        );
    }
}

static void UI_UpdateChangedStructureDisplay(void)
{
    /*
     * 1. Alte Verbindungen anhand der alten
     *    Verbindungsliste und Geometrie entfernen.
     */
    UI_ErasePreviousConnections();


    /*
     * 2. Alte Positionen bewegter oder entfernter
     *    Items löschen.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (UI_ItemPositionChanged(i))
        {
            UI_ClearGeometryArea(
                &previousStepGeometry[i]
            );
        }
    }


    /*
     * 3. Neue Bildschirmgeometrie berechnen.
     */
    UI_CalculateGeometry();


    /*
     * 4. Neue Zielbereiche säubern.
     *
     *    Bei einem Tausch steht dort eventuell noch
     *    die alte Darstellung des anderen Items.
     */
    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (UI_ItemPositionChanged(i))
        {
            UI_ClearGeometryArea(
                &uiGeometry[i]
            );
        }
    }


    /*
     * 5. Neue logische Verbindungen anhand der
     *    neuen Geometrie zeichnen.
     */
    UI_DrawCurrentConnections();


    /*
     * 6. Alle sichtbaren Items über den
     *    Verbindungslinien wiederherstellen.
     *
     *    Dadurch bleiben Rahmen, Kreise und
     *    Anschlusskanten vollständig erhalten.
     */
    UI_RedrawAllVisibleItems();


    /*
     * 7. Footer aktualisieren.
     */
    UI_DrawFooter();
}

static const UI_Item *UI_GetFocusedItem(void)
{
    for (uint16_t i = 0; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].focus == UI_FOCUS_SELECTED ||
            uiItems[i].focus == UI_FOCUS_GRABBED)
        {
            return &uiItems[i];
        }
    }

    return NULL;
}

static void UI_DrawShiftDebugIndicator(void)
{
    uint16_t fillColor =
        uiShiftDebugState ?
        UI_COLOR_BORDER_GRABBED :
        UI_COLOR_BACKGROUND;

    uint16_t textColor =
        uiShiftDebugState ?
        UI_COLOR_TEXT_DARK :
        UI_COLOR_TEXT_LIGHT;


    /*
     * Indikatorfläche löschen beziehungsweise
     * in der aktuellen Zustandsfarbe füllen.
     */
    ST7735_FillRect(
        UI_SHIFT_INDICATOR_X,
        UI_SHIFT_INDICATOR_Y,
        UI_SHIFT_INDICATOR_WIDTH,
        UI_SHIFT_INDICATOR_HEIGHT,
        fillColor
    );


    /*
     * Weißen Rahmen zeichnen.
     */
    ST7735_DrawLine(
        UI_SHIFT_INDICATOR_X,
        UI_SHIFT_INDICATOR_Y,
        UI_SHIFT_INDICATOR_X +
            UI_SHIFT_INDICATOR_WIDTH - 1,
        UI_SHIFT_INDICATOR_Y,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        UI_SHIFT_INDICATOR_X,
        UI_SHIFT_INDICATOR_Y +
            UI_SHIFT_INDICATOR_HEIGHT - 1,
        UI_SHIFT_INDICATOR_X +
            UI_SHIFT_INDICATOR_WIDTH - 1,
        UI_SHIFT_INDICATOR_Y +
            UI_SHIFT_INDICATOR_HEIGHT - 1,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        UI_SHIFT_INDICATOR_X,
        UI_SHIFT_INDICATOR_Y,
        UI_SHIFT_INDICATOR_X,
        UI_SHIFT_INDICATOR_Y +
            UI_SHIFT_INDICATOR_HEIGHT - 1,
        UI_COLOR_BORDER_NORMAL
    );

    ST7735_DrawLine(
        UI_SHIFT_INDICATOR_X +
            UI_SHIFT_INDICATOR_WIDTH - 1,
        UI_SHIFT_INDICATOR_Y,
        UI_SHIFT_INDICATOR_X +
            UI_SHIFT_INDICATOR_WIDTH - 1,
        UI_SHIFT_INDICATOR_Y +
            UI_SHIFT_INDICATOR_HEIGHT - 1,
        UI_COLOR_BORDER_NORMAL
    );


    /*
     * "S" innerhalb des Indikators darstellen.
     */
    Font5x7_DrawString(
        UI_SHIFT_INDICATOR_X + 4U,
        UI_SHIFT_INDICATOR_Y + 2U,
        "S",
        textColor,
        fillColor,
        1U
    );
}

void UI_SetShiftDebugState(
    uint8_t pressed)
{
    uint8_t newState =
        pressed ?
        1U :
        0U;


    /*
     * Kein Displayzugriff, wenn sich der Zustand
     * nicht verändert hat.
     */
    if (newState ==
        uiShiftDebugState)
    {
        return;
    }


    uiShiftDebugState =
        newState;


    /*
     * Nur den kleinen Indikator neu zeichnen.
     */
    UI_DrawShiftDebugIndicator();
}

static void UI_DrawFooter(void)
{
    const UI_Item *focusedItem = UI_GetFocusedItem();

    ST7735_FillRect(0, UI_FOOTER_TOP,
                    UI_DISPLAY_WIDTH, UI_FOOTER_HEIGHT,
                    UI_COLOR_BACKGROUND);

    if (focusedItem == NULL)
{
    UI_DrawShiftDebugIndicator();

    return;
}

    int16_t focusedIndex = UI_FindItemIndexById(focusedItem->id);
if (focusedIndex < 0 ||
    !uiGeometry[focusedIndex].visible)
{
    UI_DrawShiftDebugIndicator();

    return;
}

    const UI_ItemGeometry *geometry = &uiGeometry[focusedIndex];
    uint16_t textWidth =
        Font5x7_GetStringWidth(focusedItem->longName, 1);
    int16_t itemCenterX = geometry->x + (geometry->width / 2);
    int16_t textX = itemCenterX - ((int16_t)textWidth / 2);

    if (textX < 0)
    {
        textX = 0;
    }

   int16_t footerTextRightLimit =
    UI_SHIFT_INDICATOR_X - 2;


if ((textX + (int16_t)textWidth) >
    footerTextRightLimit)
{
    textX =
        footerTextRightLimit -
        (int16_t)textWidth;
}


if (textX < 0)
{
    textX = 0;
}

    uint16_t textHeight = Font5x7_GetHeight(1);
    uint16_t textY = UI_FOOTER_TOP;

    if (textHeight < UI_FOOTER_HEIGHT)
    {
        textY += (UI_FOOTER_HEIGHT - textHeight) / 2;
    }

    Font5x7_DrawString((uint16_t)textX, textY,
                       focusedItem->longName,
                       UI_GetFocusColor(focusedItem),
                       UI_COLOR_BACKGROUND, 1);
/*
 * Shift-Debugindikator nach dem Footertext
 * wiederherstellen.
 */
UI_DrawShiftDebugIndicator();
    

}

void UI_Init(void)
{
    grabStartStateValid = 0U;
    uiMode = UI_MODE_STATE_VIEW;
    uiMenuPage = UI_MENU_PAGE_ROOT;
    uiMenuControlMode =
        UI_MENU_CONTROL_NAVIGATION;
    uiMenuSelectedRow =
        UI_MENU_ITEM_ROW;
    uiMenuItemIndex = -1;
    uiMenuPreviewItemIndex = -1;
    uiMenuItemSelectionStartIndex = -1;

    int16_t firstFocusedIndex = -1;

    for (uint16_t i = 0;
         i < UI_ITEM_COUNT;
         i++)
    {
        if (uiItems[i].focus == UI_FOCUS_SELECTED ||
            uiItems[i].focus == UI_FOCUS_GRABBED)
        {
            if (firstFocusedIndex < 0 &&
                UI_IsSelectable(&uiItems[i]))
            {
                firstFocusedIndex =
                    (int16_t)i;
            }
            else
            {
                uiItems[i].focus =
                    UI_FOCUS_NONE;
            }
        }
    }

    /*
     * Falls kein gültiges Item fokussiert ist,
     * erstes auswählbares Item verwenden.
     */
    if (firstFocusedIndex < 0)
    {
        for (uint16_t i = 0;
             i < UI_ITEM_COUNT;
             i++)
        {
            if (UI_IsSelectable(&uiItems[i]))
            {
                uiItems[i].focus =
                    UI_FOCUS_SELECTED;

                break;
            }
        }
    }

    if (!UI_RebuildCalculatedConnections())
    {
        /*
         * Für diesen Test bedeutet ein Fehler:
         * Keine Verbindung darstellen.
         */
        UI_ClearConnections();
    }


    uiFirstVisibleOrder = 0;
    uiHighestVisibleLane = 1;

    UI_ClampHorizontalViewport();

    int16_t focusedIndex =
        UI_GetFocusedIndex();

    if (focusedIndex >= 0)
    {
        (void)UI_EnsureItemVisible(
            focusedIndex
        );

        (void)UI_EnsureItemVerticallyVisible(
            focusedIndex
        );
    }
}

void UI_Draw(void)
{
    ST7735_FillScreen(UI_COLOR_BACKGROUND);
    UI_CalculateGeometry();
    UI_DrawConnections();

    for (uint16_t i = 0; i < UI_ITEM_COUNT; i++)
    {
        if (!uiGeometry[i].visible)
        {
            continue;
        }

        UI_DrawItem(&uiItems[i],
                    uiGeometry[i].x,
                    uiGeometry[i].y);
    }

    UI_DrawFooter();
}
