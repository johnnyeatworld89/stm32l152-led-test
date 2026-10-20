#include "ui.h"
#include "st7735.h"
#include "font5x7.h"
#include "preset_store.h"
#include "midi_presets.h"
#include "loop_midi.h"

#include <string.h>

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
#define UI_MENU_HEIGHT             104U
#define UI_MENU_ROW_HEIGHT          24U
#define UI_MENU_TEXT_X_OFFSET        8U
#define UI_MENU_TEXT_Y_OFFSET        8U
#define UI_MENU_GENERAL_ROW          0U
#define UI_MENU_ITEM_ROW             1U
#define UI_MENU_DELETE_NODE_ROW      2U
#define UI_MENU_LOOP_MIDI_ROW        2U
#define UI_MENU_LOOP_CONVERT_ROW     3U

/* Anzahl der Zeilen, die das Menue-Overlay gleichzeitig anzeigen kann. */
#define UI_MENU_VISIBLE_ROWS         4U

#define UI_MENU_CONFIRM_TITLE_ROW    0U
#define UI_MENU_CONFIRM_CANCEL_ROW   1U
#define UI_MENU_CONFIRM_DELETE_ROW   2U

/* Abfrage "Convert presets too?" (Loop-Menue, Convert to In + Out). */
#define UI_MENU_CONVERT_YES_ROW      1U
#define UI_MENU_CONVERT_NO_ROW       2U
#define UI_MENU_CONVERT_SELECT_ROW   3U

/*
 * Welche Presets "Convert to In + Out" bzw. "Merge to Loop" mit
 * umbaut: keine (No), alle (Yes) oder die unter "Select" markierten.
 */
#define UI_PRESETS_NONE              0U
#define UI_PRESETS_ALL               1U
#define UI_PRESETS_SELECTED          2U

/* In-/Out-Menue: "Merge to Loop" (nur Return/Send umgewandelter Loops). */
#define UI_MENU_IO_MERGE_ROW         2U
#define UI_MENU_MERGE_ERROR_OK_ROW   3U

/*
 * Ersatz-Items fuer "Convert to In + Out" (Loop-Menue): je Loop ein
 * manueller Knoten "Lnn" an der Stelle des Loops, ein Input "Return Lnn"
 * (Return-Buchse) und ein Output "Send Lnn" (Send-Buchse).
 * id = Basis + Loopnummer (1 = L01, Zaehlung wie in UI_LoopStatusBits).
 * Sie sind inaktiv, solange der Loop nicht umgewandelt ist. Ob ein Loop
 * umgewandelt ist, steht damit allein im Layout (auch in den Presets).
 */
#define UI_CONVERT_MAX_LOOPS          31U
#define UI_CONVERT_NODE_ID_BASE      104U
#define UI_CONVERT_RETURN_ID_BASE    136U
#define UI_CONVERT_SEND_ID_BASE      168U

#define UI_CONVERT_ITEMS(number, text)                  \
    {                                                   \
        .id = UI_CONVERT_NODE_ID_BASE + (number),       \
        .type = UI_ITEM_MANUAL_NODE,                    \
        .order = -100,                                  \
        .lane = 0,                                      \
        .loopStatus = UI_LOOP_STATUS_OFF,               \
        .focus = UI_FOCUS_NONE,                         \
        .shortName = "L" text,                          \
        .longName = "Node L" text                       \
    },                                                  \
    {                                                   \
        .id = UI_CONVERT_RETURN_ID_BASE + (number),     \
        .type = UI_ITEM_INPUT,                          \
        .order = -100,                                  \
        .lane = 0,                                      \
        .loopStatus = UI_LOOP_STATUS_OFF,               \
        .focus = UI_FOCUS_NONE,                         \
        .shortName = "RL" text,                         \
        .longName = "Return L" text                     \
    },                                                  \
    {                                                   \
        .id = UI_CONVERT_SEND_ID_BASE + (number),       \
        .type = UI_ITEM_OUTPUT,                         \
        .order = -100,                                  \
        .lane = 0,                                      \
        .loopStatus = UI_LOOP_STATUS_OFF,               \
        .focus = UI_FOCUS_NONE,                         \
        .shortName = "SL" text,                         \
        .longName = "Send L" text                       \
    }

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
     * Ersatz-Items fuer "Convert to In + Out", je Loop
     * (siehe UI_CONVERT_ITEMS). Bei weiteren Loops hier
     * ergaenzen.
     */
    UI_CONVERT_ITEMS(1, "01"),
    UI_CONVERT_ITEMS(2, "02"),
    UI_CONVERT_ITEMS(3, "03"),
    UI_CONVERT_ITEMS(4, "04"),
    UI_CONVERT_ITEMS(5, "05"),
    UI_CONVERT_ITEMS(6, "06"),


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
static uint8_t UI_MenuItemIsConvertible(void);
static uint8_t UI_MenuItemIsMergeable(void);
static void UI_MenuSavePreset(void);
static void UI_MenuManagePresets(void);
static void UI_ConvertSelectedLoop(uint8_t presetMode);
static void UI_MergeSelectedItem(uint8_t presetMode);
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
    UI_MENU_PAGE_SAVE_NAME,
    UI_MENU_PAGE_MANAGE_PRESETS,
    UI_MENU_PAGE_MANAGE_ACTIONS,
    UI_MENU_PAGE_LOOP_MIDI,
    UI_MENU_PAGE_LOOP_MIDI_GROUP,
    UI_MENU_PAGE_PRESET_MIDI,
    UI_MENU_PAGE_CONVERT_CONFIRM,
    UI_MENU_PAGE_MERGE_CONFIRM,
    UI_MENU_PAGE_MERGE_ERROR,
    UI_MENU_PAGE_MERGE_REPORT,
    UI_MENU_PAGE_PRESET_SELECT

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
    UI_SAVE_STATUS_NAME_HINT,
    UI_SAVE_STATUS_RENAMED,
    UI_SAVE_STATUS_DELETED,
    UI_SAVE_STATUS_EMPTY_SLOT,
    UI_SAVE_STATUS_LOAD_FAILED,
    UI_SAVE_STATUS_DELETE_CONFIRM

} UI_SaveStatus;

/* Aktuell gewaehlte Bank (0 bis 63), bleibt beim Schliessen erhalten. */
static uint8_t uiPresetBank = 0U;
static uint8_t uiPresetBankBackup = 0U;
static uint8_t uiPresetBankEditing = 0U;

/* Index des ersten sichtbaren Presets (0 bis 16). */
static uint8_t uiPresetScrollTop = 0U;

static UI_SaveStatus uiSaveStatus = UI_SAVE_STATUS_NONE;

/*
 * Seite PRESET_SELECT ("Select" bei "Convert presets too?" bzw. "Merge
 * presets too?"): markierte Presets, Bit n von uiPresetSelection[bank] =
 * Preset n. Die Bank vor dem Oeffnen wird beim Verlassen wieder gewaehlt.
 */
static uint32_t uiPresetSelection[PRESET_BANK_COUNT];
static uint8_t uiPresetSelectMerge = 0U;
static uint8_t uiPresetSelectBankBackup = 0U;


/* -------------------------------------------------------------------------- */
/* Speichern per MIDI: Zustand und Anzeige auf dem Statusbildschirm            */
/* -------------------------------------------------------------------------- */

/*
 * CC0 (siehe midi_presets.h) schaltet das Speichern "scharf". Die naechste
 * Preset-Nummer (CC1 bis CC24) speichert dann das aktuelle Layout in der
 * aktuellen Bank. Erneutes CC0 bricht ab.
 */
static uint8_t uiMidiSaveArmed = 0U;

typedef enum
{
    UI_MIDI_SAVE_RESULT_NONE = 0,
    UI_MIDI_SAVE_RESULT_SAVED,
    UI_MIDI_SAVE_RESULT_FULL,
    UI_MIDI_SAVE_RESULT_ERROR

} UI_MidiSaveResult;

/*
 * Ergebnis des letzten Speicherns per MIDI. Es wird in der Fusszeile
 * gezeigt, bis sich dort etwas aendert (z.B. beim Wechsel der Auswahl).
 */
static UI_MidiSaveResult uiMidiSaveResult = UI_MIDI_SAVE_RESULT_NONE;
static uint8_t uiMidiSaveResultSlot = 0U;
static uint8_t uiMidiSaveResultFresh = 0U;

/* Breite des Bereichs links in der Fusszeile (Bank / Speichern). */
#define UI_BANK_INDICATOR_X             2U
#define UI_BANK_INDICATOR_WIDTH        50U
#define UI_COLOR_ERROR                 0xF800U

/*
 * Zeichen-Cache der Textzeilen.
 *
 * Jede Textzeile merkt sich, was zuletzt gezeichnet wurde (Text und
 * Farben). Beim Neuzeichnen werden nur Zeichen ausgegeben, die sich
 * geaendert haben. Beim Scrollen der Preset-Liste aendern sich dadurch
 * meist nur die Ziffern der Nummer.
 *
 * Slots: 0 = Bank-Zeile, 1 bis 8 = sichtbare Preset-Zeilen,
 *        9 = Statuszeile.
 */
#define UI_SAVE_SHADOW_SLOTS          11U
#define UI_SAVE_SHADOW_STATUS_SLOT     9U
#define UI_SAVE_SHADOW_RENAME_SLOT    10U

static char uiSaveShadowText[UI_SAVE_SHADOW_SLOTS][UI_SAVE_TEXT_CAPACITY];
static uint16_t uiSaveShadowFg[UI_SAVE_SHADOW_SLOTS];
static uint16_t uiSaveShadowBg[UI_SAVE_SHADOW_SLOTS];
static uint8_t uiSaveShadowValid[UI_SAVE_SHADOW_SLOTS];


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

/*
 * Das Untermenue wird fuer drei Aufgaben benutzt:
 *   - Preset speichern (Layout und Name)
 *   - Preset umbenennen (nur der Name)
 *   - Bank umbenennen (die Banknummer steht fest davor)
 */
typedef enum
{
    UI_NAME_MODE_SAVE_PRESET = 0,
    UI_NAME_MODE_RENAME_PRESET,
    UI_NAME_MODE_RENAME_BANK

} UI_NameMode;

static UI_NameMode uiNameMode = UI_NAME_MODE_SAVE_PRESET;

/* Seite, auf die Cancel und Return zurueckfuehren. */
static UI_MenuPage uiNameCancelPage = UI_MENU_PAGE_SAVE_PRESET;

/* Feststehender Text vor dem Namen, z.B. "3 " bei der Bank. */
static char uiNamePrefix[6] = "";


/* -------------------------------------------------------------------------- */
/* Manage Presets: Konstanten und Zustand                                     */
/* -------------------------------------------------------------------------- */

/*
 * Die Liste sieht aus wie bei Save Preset, hat aber oben eine zusaetzliche
 * Zeile "Rename bank". Dadurch bleiben 7 Zeilen fuer die Presets:
 *
 *   y   2 ..  13   "Rename bank"
 *   y  14 ..  25   Bank-Zeile
 *   y  28 .. 111   7 sichtbare Preset-Zeilen (scrollen)
 */
#define UI_MANAGE_RENAME_Y             2U
#define UI_MANAGE_BANK_Y              14U
#define UI_MANAGE_LIST_Y              28U
#define UI_MANAGE_VISIBLE_PRESETS      7U

/* Aktionen nach einem Klick auf ein Preset. */
#define UI_ACTION_LOAD                 0U
#define UI_ACTION_RENAME               1U
#define UI_ACTION_MIDI                 2U
#define UI_ACTION_DELETE               3U
#define UI_ACTION_COUNT                4U

static const char *const uiActionLabels[UI_ACTION_COUNT] =
{
    "Load",
    "Rename",
    "Midi",
    "Delete"
};

static uint8_t uiActionsSlot = 0U;
static uint8_t uiActionsRow = UI_ACTION_LOAD;
static uint8_t uiActionsDeletePending = 0U;

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
    /*
     * Je Pixelzeile ein Rechteck statt einzelner Pixel.
     */
    for (int16_t y = -radius; y <= radius; y++)
    {
        int16_t half = radius;

        while ((half * half) + (y * y) > (radius * radius))
        {
            half--;
        }

        int16_t left = cx - half;
        int16_t right = cx + half;
        int16_t row = cy + y;

        if (row < 0 || right < 0)
        {
            continue;
        }

        if (left < 0)
        {
            left = 0;
        }

        ST7735_FillRect((uint16_t)left, (uint16_t)row,
                        (uint16_t)(right - left + 1), 1U, color);
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

/*
 * Ersatz-Items fuer "Convert to In + Out" (siehe UI_CONVERT_ITEMS).
 */
static uint8_t UI_IsConvertItem(
    const UI_Item *item)
{
    return
        (item->id > UI_CONVERT_NODE_ID_BASE &&
         item->id <= (UI_CONVERT_SEND_ID_BASE +
                      UI_CONVERT_MAX_LOOPS)) ?
        1U :
        0U;
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

    /*
     * Richtung 0 (naechstes Ziel in derselben Lane): Ist die eigene
     * Lane blockiert (z.B. durch einen inneren Input bzw. Output),
     * werden zusaetzlich beide Nachbar-Lanes versucht, zuerst die
     * Richtung zur Lane 0. Sonst wuerde immer wieder in derselben
     * Lane eine Spalte eingefuegt und das Routing kaeme nie zum Ende.
     */
    int8_t sideDirection =
        (direction != 0) ?
        direction :
        ((sourceItem->lane > 0) ? -1 : 1);

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

    if (direction == 0 &&
        candidateCount == 1)
    {
        candidateLanes[candidateCount++] =
            candidateLanes[0] + sideDirection;

        candidateLanes[candidateCount++] =
            candidateLanes[0] - sideDirection;
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
            sourceItem->lane + sideDirection
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

    /*
     * Richtung 0 (naechstes Ziel in derselben Lane): Ist die eigene
     * Lane blockiert (z.B. durch einen inneren Input bzw. Output),
     * werden zusaetzlich beide Nachbar-Lanes versucht, zuerst die
     * Richtung zur Lane 0. Sonst wuerde immer wieder in derselben
     * Lane eine Spalte eingefuegt und das Routing kaeme nie zum Ende.
     */
    int8_t sideDirection =
        (direction != 0) ?
        direction :
        ((targetItem->lane > 0) ? -1 : 1);

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

    if (direction == 0 &&
        candidateCount == 1)
    {
        candidateLanes[candidateCount++] =
            candidateLanes[0] + sideDirection;

        candidateLanes[candidateCount++] =
            candidateLanes[0] - sideDirection;
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
            targetItem->lane + sideDirection
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
     * Alle Items werden um eine Spalte nach rechts
     * verschoben, ausser den Inputs in der linken
     * Randspalte. Inputs weiter innen wandern mit.
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
                    UI_ITEM_INPUT &&
                uiItems[i].order ==
                    minimumOrder)
            {
                continue;
            }

            uiItems[i].order++;
        }
    }


    /*
     * Ungültige rechte Randspalte:
     *
     * Die Outputs der rechten Randspalte wandern
     * rechts neben die äußerste Nicht-Output-Spalte
     * (nach einer möglichen linken Erweiterung neu
     * bestimmt). Outputs weiter innen bleiben.
     */
    if (invalidRightEdge)
    {
        int16_t edgeOutputOrder =
            UI_GetMaximumPermanentOrder();

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
                /*
                 * Inaktive Outputs (z.B. "Send Lnn" eines
                 * nicht umgewandelten Loops) bleiben inaktiv.
                 */
                if (UI_IsPermanentItem(&uiItems[i]) &&
                    uiItems[i].type ==
                        UI_ITEM_OUTPUT &&
                    uiItems[i].order ==
                        edgeOutputOrder)
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


/* -------------------------------------------------------------------------- */
/* Preset-Liste: Geometrie (Save Preset und Manage Presets)                   */
/* -------------------------------------------------------------------------- */

/* Die Manage-Seite hat zusaetzlich die Zeile "Rename bank" ganz oben. */
static uint8_t UI_ListIsManage(void)
{
    return (uiMenuPage == UI_MENU_PAGE_MANAGE_PRESETS ||
            uiMenuPage == UI_MENU_PAGE_PRESET_SELECT) ? 1U : 0U;
}


/*
 * Die Auswahl-Seite (PRESET_SELECT) sieht aus wie Manage Presets, oben
 * steht "Confirm" statt "Rename bank".
 */
static uint8_t UI_ListIsSelect(void)
{
    return (uiMenuPage == UI_MENU_PAGE_PRESET_SELECT) ? 1U : 0U;
}


/* Zeilennummer der Bank-Zeile: 0 bei Save, 1 bei Manage. */
static uint8_t UI_ListBankRow(void)
{
    return UI_ListIsManage() ? 1U : 0U;
}


/*
 * Auswahl-Seite: Sie zeigt nur belegte Presets. index = Position in der
 * Liste (0 = erstes belegtes Preset der Bank). Rueckgabe: Preset-Nummer
 * (0 bis 23) bzw. -1, wenn es so viele belegte Presets nicht gibt.
 */
static int16_t UI_SelectSlotOfIndex(
    uint8_t bank,
    uint8_t index)
{
    char name[PRESET_NAME_LENGTH];

    for (uint8_t slot = 0U; slot < PRESET_SLOTS_PER_BANK; slot++)
    {
        if (!PresetStore_GetSlotName(bank, slot, name))
        {
            continue;
        }

        if (index == 0U)
        {
            return (int16_t)slot;
        }

        index--;
    }

    return -1;
}


static uint8_t UI_SelectUsedCount(uint8_t bank)
{
    char name[PRESET_NAME_LENGTH];
    uint8_t count = 0U;

    for (uint8_t slot = 0U; slot < PRESET_SLOTS_PER_BANK; slot++)
    {
        if (PresetStore_GetSlotName(bank, slot, name))
        {
            count++;
        }
    }

    return count;
}


/* Anzahl der Preset-Zeilen (Auswahl-Seite: nur belegte Presets). */
static uint8_t UI_ListPresetCount(void)
{
    return UI_ListIsSelect() ?
           UI_SelectUsedCount(uiPresetBank) :
           (uint8_t)PRESET_SLOTS_PER_BANK;
}


/* Preset-Nummer (0 bis 23) einer Preset-Zeile, -1 = keine. */
static int16_t UI_ListSlotOfRow(uint8_t row)
{
    int16_t index = (int16_t)row - (int16_t)UI_ListBankRow() - 1;

    if (index < 0)
    {
        return -1;
    }

    if (UI_ListIsSelect())
    {
        return UI_SelectSlotOfIndex(uiPresetBank, (uint8_t)index);
    }

    return (index < (int16_t)PRESET_SLOTS_PER_BANK) ? index : -1;
}


/* Zeilennummer der letzten Preset-Zeile. */
static int16_t UI_ListLastRow(void)
{
    return (int16_t)(UI_ListBankRow() + UI_ListPresetCount());
}


static uint8_t UI_ListVisiblePresets(void)
{
    return UI_ListIsManage() ?
           UI_MANAGE_VISIBLE_PRESETS :
           UI_SAVE_VISIBLE_PRESETS;
}


static uint16_t UI_ListFirstY(void)
{
    return UI_ListIsManage() ?
           UI_MANAGE_LIST_Y :
           UI_SAVE_LIST_Y;
}


static uint16_t UI_ListBankY(void)
{
    return UI_ListIsManage() ?
           UI_MANAGE_BANK_Y :
           UI_SAVE_HEADER_Y;
}


/*
 * Manage-Seite, Zeile 0:  "Rename bank"
 * Bank-Zeile:             "<bank> <bankname>"      z.B. "3 bank 3"
 * Preset-Zeile:           "<nn>: <name>"           z.B. "05: empty"
 */
static void UI_SaveBuildRowText(
    uint8_t row,
    char *text)
{
    uint8_t position = 0U;
    uint8_t bankRow = UI_ListBankRow();

    text[0] = '\0';

    if (UI_ListIsManage() && row == 0U)
    {
        UI_SaveAppendText(
            text,
            0U,
            UI_SAVE_TEXT_CAPACITY,
            UI_ListIsSelect() ? "Confirm" : "Rename bank");

        return;
    }

    if (row == bankRow)
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

    int16_t slotOfRow = UI_ListSlotOfRow(row);

    if (slotOfRow < 0)
    {
        /* Auswahl-Seite: leere Zeile unter dem letzten Preset. */
        return;
    }

    uint8_t number = (uint8_t)(slotOfRow + 1);
    char presetName[PRESET_NAME_LENGTH];

    uint8_t used = PresetStore_GetSlotName(
        uiPresetBank, (uint8_t)(number - 1U), presetName);

    /*
     * Auswahl-Seite: Haken vor markierten Presets.
     */
    if (UI_ListIsSelect())
    {
        static const char checked[] = { FONT5X7_CHAR_CHECK, ' ', '\0' };

        position = UI_SaveAppendText(
            text,
            position,
            UI_SAVE_TEXT_CAPACITY,
            ((uiPresetSelection[uiPresetBank] &
              (1UL << (number - 1U))) != 0UL) ?
            checked :
            "  ");
    }

    position = UI_SaveAppendUInt2(
        text, position, UI_SAVE_TEXT_CAPACITY, number);
    position = UI_SaveAppendText(
        text, position, UI_SAVE_TEXT_CAPACITY, ": ");
    UI_SaveAppendText(
        text,
        position,
        UI_SAVE_TEXT_CAPACITY,
        used ? presetName : "empty");
}


/* -------------------------------------------------------------------------- */
/* Save Preset: Textzeilen mit Zeichen-Cache                                  */
/* -------------------------------------------------------------------------- */

/*
 * Muss aufgerufen werden, nachdem der Bildschirm geloescht oder
 * anders ueberschrieben wurde.
 */
static void UI_SaveShadowInvalidate(void)
{
    for (uint8_t i = 0U; i < UI_SAVE_SHADOW_SLOTS; i++)
    {
        uiSaveShadowValid[i] = 0U;
    }
}


/*
 * Zeichnet eine Textzeile.
 *
 * Sind Farben unveraendert und der Slot gueltig, werden nur die
 * Zeichen neu gezeichnet, die sich gegenueber dem letzten Mal
 * geaendert haben. Sonst wird die Zeile komplett neu gefuellt und
 * gezeichnet.
 */
static void UI_SaveDrawTextLine(
    uint8_t slot,
    uint16_t rowY,
    uint16_t rowWidth,
    uint16_t rowHeight,
    uint16_t textY,
    const char *text,
    uint16_t fg,
    uint16_t bg)
{
    char *shadow = uiSaveShadowText[slot];

    if (!uiSaveShadowValid[slot] ||
        uiSaveShadowFg[slot] != fg ||
        uiSaveShadowBg[slot] != bg)
    {
        ST7735_FillRect(0U, rowY, rowWidth, rowHeight, bg);

        Font5x7_DrawString(
            UI_SAVE_TEXT_X_OFFSET,
            textY,
            text,
            fg,
            bg,
            1U
        );
    }
    else
    {
        uint8_t ended = 0U;

        for (uint8_t i = 0U;
             i < (UI_SAVE_TEXT_CAPACITY - 1U);
             i++)
        {
            /*
             * Hinter dem Ende des neuen Textes gilt '\0', ohne
             * weiter im Text zu lesen.
             */
            char newChar = '\0';

            if (!ended)
            {
                newChar = text[i];

                if (newChar == '\0')
                {
                    ended = 1U;
                }
            }

            char oldChar = shadow[i];

            if (newChar == '\0' && oldChar == '\0')
            {
                break;
            }

            if (newChar == oldChar)
            {
                continue;
            }

            /*
             * Reste des alten Textes werden mit Leerzeichen
             * ueberschrieben.
             */
            char cell[2];

            cell[0] = (newChar == '\0') ? ' ' : newChar;
            cell[1] = '\0';

            Font5x7_DrawString(
                (uint16_t)(UI_SAVE_TEXT_X_OFFSET + (6U * i)),
                textY,
                cell,
                fg,
                bg,
                1U
            );
        }
    }

    /*
     * Cache aktualisieren.
     */
    uint8_t i = 0U;

    while (text[i] != '\0' &&
           i < (UI_SAVE_TEXT_CAPACITY - 1U))
    {
        shadow[i] = text[i];
        i++;
    }

    while (i < UI_SAVE_TEXT_CAPACITY)
    {
        shadow[i] = '\0';
        i++;
    }

    uiSaveShadowFg[slot] = fg;
    uiSaveShadowBg[slot] = bg;
    uiSaveShadowValid[slot] = 1U;
}


/* -------------------------------------------------------------------------- */
/* Save Preset: Zeichnen der Liste                                            */
/* -------------------------------------------------------------------------- */

static void UI_SaveDrawRow(uint8_t row)
{
    uint16_t y;
    uint8_t slot;
    uint8_t bankRow = UI_ListBankRow();

    if (UI_ListIsManage() && row == 0U)
    {
        y = UI_MANAGE_RENAME_Y;
        slot = UI_SAVE_SHADOW_RENAME_SLOT;
    }
    else if (row == bankRow)
    {
        y = UI_ListBankY();
        slot = 0U;
    }
    else
    {
        int16_t position =
            (int16_t)row - (int16_t)bankRow - 1 -
            (int16_t)uiPresetScrollTop;

        if (position < 0 ||
            position >= (int16_t)UI_ListVisiblePresets())
        {
            return;
        }

        y = (uint16_t)(UI_ListFirstY() +
            ((uint16_t)position * UI_SAVE_ROW_HEIGHT));
        slot = (uint8_t)(position + 1);
    }

    uint16_t background = UI_COLOR_BACKGROUND;

    if ((int16_t)row == uiMenuSelectedRow)
    {
        background =
            (row == bankRow && uiPresetBankEditing) ?
            UI_COLOR_BORDER_GRABBED :
            UI_COLOR_BORDER_SELECTED;
    }

    char text[UI_SAVE_TEXT_CAPACITY];

    UI_SaveBuildRowText(row, text);

    UI_SaveDrawTextLine(
        slot,
        y,
        UI_SAVE_ROW_WIDTH,
        UI_SAVE_ROW_HEIGHT,
        y + UI_SAVE_TEXT_Y_OFFSET,
        text,
        UI_COLOR_TEXT_LIGHT,
        background
    );
}


static void UI_SaveDrawScrollbar(void)
{
    uint16_t visible = UI_ListVisiblePresets();

    uint16_t trackHeight =
        (uint16_t)(visible * UI_SAVE_ROW_HEIGHT);

    /*
     * Auswahl-Seite: nur belegte Presets, bei wenigen fuellt der
     * Schieber die ganze Leiste.
     */
    uint16_t total = UI_ListPresetCount();

    if (total < visible)
    {
        total = visible;
    }

    uint16_t thumbHeight =
        (uint16_t)((trackHeight * visible) / total);

    uint16_t thumbY =
        (uint16_t)(UI_ListFirstY() +
            (((uint32_t)trackHeight * uiPresetScrollTop) / total));

    ST7735_FillRect(
        UI_SAVE_SCROLLBAR_X,
        UI_ListFirstY(),
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
    uint8_t firstRow = (uint8_t)(UI_ListBankRow() + 1U);

    for (uint8_t i = 0U; i < UI_ListVisiblePresets(); i++)
    {
        UI_SaveDrawRow(
            (uint8_t)(firstRow + uiPresetScrollTop + i));
    }

    UI_SaveDrawScrollbar();
}


/*
 * Haengt "<n>/<max>" (belegte von maximal moeglichen Presets) an.
 */
static uint8_t UI_SaveAppendUsedCount(
    char *text,
    uint8_t position)
{
    position = UI_SaveAppendUInt(
        text, position, UI_SAVE_TEXT_CAPACITY,
        PresetStore_GetUsedCount());
    position = UI_SaveAppendText(
        text, position, UI_SAVE_TEXT_CAPACITY, "/");

    return UI_SaveAppendUInt(
        text, position, UI_SAVE_TEXT_CAPACITY,
        PRESET_MAX_RECORDS);
}


/*
 * Statuszeile unter der Trennlinie. Wird von allen Seiten dieses
 * Bereichs gemeinsam benutzt.
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
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Saving");
            break;

        case UI_SAVE_STATUS_MEMORY_FULL:
            color = UI_COLOR_LOOP_UNCONFIRMED;
            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Memory full ");
            UI_SaveAppendUsedCount(text, position);
            break;

        case UI_SAVE_STATUS_ERROR:
            color = UI_COLOR_LOOP_UNCONFIRMED;
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Write failed");
            break;

        case UI_SAVE_STATUS_NAME_HINT:
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY,
                "Enter set  Return del");
            break;

        case UI_SAVE_STATUS_RENAMED:
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Renamed");
            break;

        case UI_SAVE_STATUS_EMPTY_SLOT:
            color = UI_COLOR_LOOP_UNCONFIRMED;
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Preset is empty");
            break;

        case UI_SAVE_STATUS_LOAD_FAILED:
            color = UI_COLOR_LOOP_UNCONFIRMED;
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Load failed");
            break;

        case UI_SAVE_STATUS_DELETE_CONFIRM:
            color = UI_COLOR_LOOP_UNCONFIRMED;
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY,
                "Sure? Enter:yes Return:no");
            break;

        case UI_SAVE_STATUS_SAVED:
            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Saved - Used ");
            UI_SaveAppendUsedCount(text, position);
            break;

        case UI_SAVE_STATUS_DELETED:
            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Deleted - Used ");
            UI_SaveAppendUsedCount(text, position);
            break;

        case UI_SAVE_STATUS_NONE:
        default:
            if (UI_ListIsSelect())
            {
                uint8_t selected = 0U;

                for (uint8_t bank = 0U; bank < PRESET_BANK_COUNT; bank++)
                {
                    for (uint8_t slot = 0U;
                         slot < PRESET_SLOTS_PER_BANK;
                         slot++)
                    {
                        if ((uiPresetSelection[bank] &
                             (1UL << slot)) != 0UL)
                        {
                            selected++;
                        }
                    }
                }

                position = UI_SaveAppendText(
                    text, 0U, UI_SAVE_TEXT_CAPACITY, "Selected ");
                UI_SaveAppendUInt(
                    text, position, UI_SAVE_TEXT_CAPACITY, selected);
                break;
            }

            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Used ");
            UI_SaveAppendUsedCount(text, position);
            break;
    }

    UI_SaveDrawTextLine(
        UI_SAVE_SHADOW_STATUS_SLOT,
        UI_SAVE_SEPARATOR_Y + 1U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT - (UI_SAVE_SEPARATOR_Y + 1U),
        UI_SAVE_STATUS_Y,
        text,
        color,
        UI_COLOR_BACKGROUND
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

    UI_SaveShadowInvalidate();

    if (UI_ListIsManage())
    {
        UI_SaveDrawRow(0U);
    }

    UI_SaveDrawRow(UI_ListBankRow());
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
/* Loop-Status                                                                */
/* -------------------------------------------------------------------------- */

/*
 * Schaltet einen Loop ein oder aus.
 *
 * fromMidi = 1: Das Effektgeraet hat per MIDI gemeldet, dass es an bzw.
 * aus ist (loop_midi.h). Der Loop gilt dann als bestaetigt aktiv und wird
 * in der Statusansicht sofort neu gezeichnet.
 *
 * fromMidi = 0: Der Router schaltet den Loop (z.B. beim Laden eines
 * Presets). Aendert sich der Zustand, bekommt das Effektgeraet die
 * passenden MIDI-Befehle. Das Neuzeichnen macht der Aufrufer.
 */
static void UI_LoopSetActive(
    int16_t itemIndex,
    uint8_t active,
    uint8_t fromMidi)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT ||
        uiItems[itemIndex].type != UI_ITEM_LOOP)
    {
        return;
    }

    UI_Item *item = &uiItems[itemIndex];

    uint8_t wasActive =
        (item->loopStatus != UI_LOOP_STATUS_OFF) ? 1U : 0U;

    UI_LoopStatus status = UI_LOOP_STATUS_OFF;

    if (active)
    {
        if (fromMidi)
        {
            status = UI_LOOP_STATUS_ACTIVE_CONFIRMED;
        }
        else
        {
            status = wasActive ?
                item->loopStatus :
                UI_LOOP_STATUS_ACTIVE_UNCONFIRMED;
        }
    }

    if (!fromMidi &&
        wasActive != (active ? 1U : 0U) &&
        item->id <= 255U)
    {
        LoopMidi_SetLoopState((uint8_t)item->id, active ? 1U : 0U);
    }

    if (status == item->loopStatus)
    {
        return;
    }

    item->loopStatus = status;

    if (fromMidi && uiMode == UI_MODE_STATE_VIEW)
    {
        UI_RedrawItem(itemIndex);
    }
}


/*
 * Setzt nur den angezeigten Zustand eines Loops, ohne MIDI zu senden
 * (die Meldungen hat UI_LoopApplyStates schon geschickt). Das Neuzeichnen
 * macht der Aufrufer.
 */
static void UI_LoopSetActiveQuiet(
    int16_t itemIndex,
    uint8_t active)
{
    UI_Item *item = &uiItems[itemIndex];

    if (!active)
    {
        item->loopStatus = UI_LOOP_STATUS_OFF;
    }
    else if (item->loopStatus == UI_LOOP_STATUS_OFF)
    {
        item->loopStatus = UI_LOOP_STATUS_ACTIVE_UNCONFIRMED;
    }
}


/*
 * Loop-Status fuer ein Preset: Bit n = n-ter Loop in uiItems (nur Loops
 * im Layout). PRESET_LOOP_STATUS_VALID ist immer gesetzt.
 */
static uint32_t UI_LoopStatusBits(void)
{
    uint32_t bits = PRESET_LOOP_STATUS_VALID;
    uint8_t loopNumber = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
            uiItems[i].order >= 0 &&
            uiItems[i].loopStatus != UI_LOOP_STATUS_OFF)
        {
            bits |= (1UL << loopNumber);
        }

        loopNumber++;
    }

    return bits;
}


/*
 * On/Bypass-Zustand fuer ein Preset: fuer die Loops im Layout mit
 * On/Bypass-Gruppen (hoechstens PRESET_LOOP_STATE_MAX_LOOPS) der Zustand
 * jeder Gruppe. Ein Loop, dessen Gruppen nicht zu seinem Status passen
 * (z.B. an, aber keine Gruppe bekannt), bekommt keinen Zustand und wird
 * beim Laden wie bisher ueber den Loop-Status geschaltet.
 */
static void UI_LoopStatesBuild(PresetLoopStates *states)
{
    uint8_t loopNumber = 0U;
    uint8_t count = 0U;

    memset(states, 0, sizeof(*states));

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
            count < PRESET_LOOP_STATE_MAX_LOOPS &&
            uiItems[i].order >= 0 &&
            uiItems[i].id <= 255U &&
            LoopMidi_HasGroups((uint8_t)uiItems[i].id))
        {
            uint32_t state =
                LoopMidi_GetGroupState((uint8_t)uiItems[i].id);
            uint8_t active =
                (uiItems[i].loopStatus != UI_LOOP_STATUS_OFF) ? 1U : 0U;

            if ((state != 0UL) == (active != 0U))
            {
                states->mask |= (1UL << loopNumber);
                states->state[loopNumber] = state;
                count++;
            }
        }

        loopNumber++;
    }
}


/*
 * Gegenstueck zu UI_LoopStatusBits und UI_LoopStatesBuild: Loops mit
 * gespeichertem On/Bypass-Zustand bekommen genau diesen, die uebrigen
 * werden ueber den Loop-Status geschaltet. Presets ohne gespeicherten
 * Loop-Status (aeltere Presets) lassen die Loops unveraendert.
 */
static void UI_LoopApplyStatusBits(
    uint32_t bits,
    const PresetLoopStates *states)
{
    uint8_t loopNumber = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber >= PRESET_LOOP_STATUS_MAX_LOOPS ||
            uiItems[i].order < 0)
        {
            loopNumber++;
            continue;
        }

        int8_t applied = -1;

        if ((states->mask & (1UL << loopNumber)) != 0UL &&
            uiItems[i].id <= 255U)
        {
            applied = LoopMidi_ApplyGroupState(
                (uint8_t)uiItems[i].id, states->state[loopNumber]);
        }

        if (applied >= 0)
        {
            UI_LoopSetActiveQuiet((int16_t)i, (uint8_t)applied);
        }
        else if ((bits & PRESET_LOOP_STATUS_VALID) != 0UL)
        {
            UI_LoopSetActive(
                (int16_t)i,
                ((bits >> loopNumber) & 1UL) ? 1U : 0U,
                0U);
        }

        loopNumber++;
    }
}


/*
 * Prueft, ob die gespeicherten On/Bypass-Zustaende den aktuellen
 * entsprechen (nur Loops im Layout mit Gruppen).
 */
static uint8_t UI_LoopStatesMatch(const PresetLoopStates *states)
{
    uint8_t loopNumber = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
            (states->mask & (1UL << loopNumber)) != 0UL &&
            uiItems[i].order >= 0 &&
            uiItems[i].id <= 255U &&
            LoopMidi_HasGroups((uint8_t)uiItems[i].id) &&
            LoopMidi_GetGroupState((uint8_t)uiItems[i].id) !=
                states->state[loopNumber])
        {
            return 0U;
        }

        loopNumber++;
    }

    return 1U;
}


/*
 * Programme der Effektgeraete fuer ein Preset: Byte n = n-ter Loop in
 * uiItems (wie beim Loop-Status), PRESET_LOOP_PROGRAM_VALID | Programm
 * fuer Loops mit Program Change und bekanntem Programm, sonst 0.
 */
static void UI_LoopProgramsBuild(uint8_t *programs)
{
    uint8_t loopNumber = 0U;

    for (uint8_t i = 0U; i < PRESET_LOOP_PROGRAM_BYTES; i++)
    {
        programs[i] = 0U;
    }

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
            uiItems[i].id <= 255U)
        {
            uint8_t program =
                LoopMidi_GetProgram((uint8_t)uiItems[i].id);

            if (program != LOOP_MIDI_PROGRAM_NONE)
            {
                programs[loopNumber] =
                    (uint8_t)(PRESET_LOOP_PROGRAM_VALID | program);
            }
        }

        loopNumber++;
    }
}


/*
 * Sendet die gespeicherten Programme an die Effektgeraete (nur Loops,
 * bei denen Program Change an ist). Loops ohne gespeichertes Programm
 * bleiben unveraendert.
 */
static void UI_LoopApplyPrograms(const uint8_t *programs)
{
    uint8_t loopNumber = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
            uiItems[i].id <= 255U &&
            (programs[loopNumber] & PRESET_LOOP_PROGRAM_VALID) != 0U)
        {
            LoopMidi_SendProgram(
                (uint8_t)uiItems[i].id,
                (uint8_t)(programs[loopNumber] &
                          (uint8_t)~PRESET_LOOP_PROGRAM_VALID));
        }

        loopNumber++;
    }
}


/*
 * Prueft, ob die gespeicherten Programme den aktuellen entsprechen.
 * Loops ohne gespeichertes Programm zaehlen als gleich.
 */
static uint8_t UI_LoopProgramsMatch(const uint8_t *programs)
{
    uint8_t current[PRESET_LOOP_PROGRAM_BYTES];

    UI_LoopProgramsBuild(current);

    for (uint8_t i = 0U; i < PRESET_LOOP_PROGRAM_BYTES; i++)
    {
        if ((programs[i] & PRESET_LOOP_PROGRAM_VALID) != 0U &&
            programs[i] != current[i])
        {
            return 0U;
        }
    }

    return 1U;
}


/*
 * Sendet die zusaetzlichen MIDI-Meldungen eines Presets (Manage Presets,
 * "Midi"). Sie laufen ueber den Echo-Filter der Loops.
 */
static void UI_PresetSendMidiMessages(
    const PresetMidiMessage *messages,
    uint8_t count)
{
    for (uint8_t i = 0U; i < count; i++)
    {
        uint8_t channel = (uint8_t)(messages[i].status & 0x0FU);

        if ((messages[i].status & 0xF0U) == 0xC0U)
        {
            LoopMidi_SendChannelProgram(channel, messages[i].data1);
        }
        else
        {
            LoopMidi_SendControlChange(
                channel, messages[i].data1, messages[i].data2);
        }
    }
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
 * Gegenstueck zu UI_PresetSerializeLayout: stellt die Positionen der
 * Items aus dem Puffer wieder her und baut Auto-Nodes und Verbindungen
 * neu auf.
 *
 * Der Puffer wird vollstaendig geprueft, bevor etwas geaendert wird.
 * Bei einem Fehler bleibt das aktuelle Layout unveraendert.
 *
 * Nicht im Puffer enthaltene Items werden inaktiv, Loops darunter werden
 * ausgeschaltet. Den Status der uebrigen Loops setzt UI_PresetLoad.
 *
 * Rueckgabe: 1 bei Erfolg, 0 bei ungueltigem Puffer.
 */
static uint8_t UI_PresetApplyLayout(
    const uint8_t *layout,
    uint8_t length)
{
    uint8_t seen[UI_ITEM_COUNT];
    int8_t newOrder[UI_ITEM_COUNT];
    int8_t newLane[UI_ITEM_COUNT];

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        seen[i] = 0U;
        newOrder[i] = 0;
        newLane[i] = 0;
    }

    if (length < 1U)
    {
        return 0U;
    }

    uint8_t count = layout[0];

    if ((uint16_t)(1U + (3U * count)) > length)
    {
        return 0U;
    }

    /*
     * Pruefen.
     */
    for (uint8_t entry = 0U; entry < count; entry++)
    {
        uint8_t base = (uint8_t)(1U + (3U * entry));
        int16_t itemIndex = UI_FindItemIndexById(layout[base]);
        int8_t order = (int8_t)layout[base + 1U];
        int8_t lane = (int8_t)layout[base + 2U];

        if (itemIndex < 0 ||
            uiItems[itemIndex].type == UI_ITEM_AUTO_NODE ||
            seen[itemIndex] ||
            order < 0 ||
            order >= UI_MAX_ORDER_COUNT)
        {
            return 0U;
        }

        seen[itemIndex] = 1U;
        newOrder[itemIndex] = order;
        newLane[itemIndex] = lane;
    }

    /*
     * Ein- und Ausgaenge muessen immer vorhanden sein (ausser denen
     * umgewandelter Loops, siehe UI_CONVERT_ITEMS).
     */
    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if ((uiItems[i].type == UI_ITEM_INPUT ||
             uiItems[i].type == UI_ITEM_OUTPUT) &&
            !UI_IsConvertItem(&uiItems[i]) &&
            !seen[i])
        {
            return 0U;
        }
    }

    /*
     * Uebernehmen.
     */
    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type == UI_ITEM_AUTO_NODE)
        {
            continue;
        }

        uiItems[i].focus = UI_FOCUS_NONE;

        if (seen[i])
        {
            uiItems[i].order = newOrder[i];
            uiItems[i].lane = newLane[i];
        }
        else
        {
            uiItems[i].order = UI_INACTIVE_ORDER;
            uiItems[i].lane = 0;

            if (uiItems[i].type == UI_ITEM_LOOP)
            {
                UI_LoopSetActive((int16_t)i, 0U, 0U);
            }
            else
            {
                uiItems[i].loopStatus = UI_LOOP_STATUS_OFF;
            }
        }
    }

    grabStartStateValid = 0U;

    UI_RemoveAllAutoNodes();
    UI_NormalizeOrders();
    UI_EnsureEdgeColumns();

    if (!UI_RebuildCalculatedConnections())
    {
        UI_ClearConnections();
    }

    int16_t selectedIndex = UI_FindFirstSelectableIndex();

    if (selectedIndex >= 0)
    {
        UI_MenuSetConfirmedItem(selectedIndex);
    }

    return 1U;
}


/*
 * Liest ein Preset aus dem Speicher und stellt sein Layout und den
 * Loop-Status her. Bei Erfolg wird es per MIDI gemeldet (Program Change
 * und Control Change), danach schalten die Loops ihre Effektgeraete.
 * Das Neuzeichnen macht der Aufrufer.
 *
 * Rueckgabe: 1 bei Erfolg, 0 bei leerem Slot oder ungueltigem Layout.
 */
static uint8_t UI_PresetLoad(
    uint8_t bank,
    uint8_t slot)
{
    uint8_t layout[PRESET_LAYOUT_MAX_BYTES];
    uint8_t length = 0U;
    uint32_t loopStatus = 0UL;
    uint8_t programs[PRESET_LOOP_PROGRAM_BYTES];
    PresetMidiMessage messages[PRESET_MIDI_MAX_MESSAGES];
    uint8_t messageCount = 0U;
    PresetLoopStates states;

    if (PresetStore_ReadLayout(bank, slot, layout, &length) !=
            PRESET_STORE_OK ||
        PresetStore_ReadLoopStatus(bank, slot, &loopStatus) !=
            PRESET_STORE_OK ||
        PresetStore_ReadLoopPrograms(bank, slot, programs) !=
            PRESET_STORE_OK ||
        PresetStore_ReadMidiMessages(
            bank, slot, messages, &messageCount) != PRESET_STORE_OK ||
        PresetStore_ReadLoopStates(bank, slot, &states) !=
            PRESET_STORE_OK)
    {
        return 0U;
    }

    if (!UI_PresetApplyLayout(layout, length))
    {
        return 0U;
    }

    MidiPresets_NotifyPresetChanged(bank, slot);

    /*
     * Erst die Programme und die zusaetzlichen Meldungen, dann
     * On/Bypass: ein Programmwechsel kann den Bypass-Zustand des
     * Geraets aendern.
     */
    UI_LoopApplyPrograms(programs);
    UI_PresetSendMidiMessages(messages, messageCount);
    UI_LoopApplyStatusBits(loopStatus, &states);

    return 1U;
}


/*
 * Waehlt die Bank vor (Program Change). Im Menue wird das ignoriert,
 * weil dort eine Bank-Aenderung ein laufendes Speichern oder Umbenennen
 * auf eine andere Bank umlenken wuerde.
 *
 * Rueckgabe: 1 wenn die Bank gewaehlt wurde.
 */
uint8_t UI_SelectBank(uint8_t bank)
{
    if (bank >= PRESET_BANK_COUNT ||
        uiMode == UI_MODE_MENU)
    {
        return 0U;
    }

    if (bank != uiPresetBank)
    {
        uiPresetBank = bank;
        uiMidiSaveResult = UI_MIDI_SAVE_RESULT_NONE;

        /*
         * Die Bank steht in der Fusszeile der Statusansicht.
         */
        UI_DrawFooter();
    }

    return 1U;
}


/*
 * Prueft, ob das aktuelle Layout dem gespeicherten Preset entspricht.
 *
 * Wird fuer die MIDI-Steuerung gebraucht: Kommt die eigene Meldung
 * ueber eine Schleife (MIDI OUT zurueck in MIDI IN) wieder an, verlangt
 * sie den Zustand, in dem man schon ist, und es passiert nichts mehr.
 *
 * Der Loop-Status wird mit verglichen, wenn das Preset einen hat. Wenn
 * spaeter der Schaltplan zum Preset gehoert, muss er hier mit verglichen
 * werden.
 */
static uint8_t UI_PresetMatchesCurrent(
    uint8_t bank,
    uint8_t slot)
{
    uint8_t current[PRESET_LAYOUT_MAX_BYTES];
    uint8_t stored[PRESET_LAYOUT_MAX_BYTES];
    uint8_t storedLength = 0U;

    uint8_t currentLength =
        UI_PresetSerializeLayout(current, sizeof(current));

    if (currentLength == 0U ||
        PresetStore_ReadLayout(bank, slot, stored, &storedLength) !=
            PRESET_STORE_OK ||
        storedLength != currentLength)
    {
        return 0U;
    }

    for (uint8_t i = 0U; i < currentLength; i++)
    {
        if (current[i] != stored[i])
        {
            return 0U;
        }
    }

    uint32_t storedLoopStatus = 0UL;

    if (PresetStore_ReadLoopStatus(bank, slot, &storedLoopStatus) !=
            PRESET_STORE_OK)
    {
        return 0U;
    }

    /*
     * Loops, die nicht im Layout sind (z.B. umgewandelt per "Convert to
     * In + Out"), zaehlen nicht: ihr gespeicherter Status bleibt fuer ein
     * spaeteres Zusammenfuehren erhalten.
     */
    uint32_t inactiveLoops = 0UL;
    uint8_t loopNumber = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
            uiItems[i].order < 0)
        {
            inactiveLoops |= (1UL << loopNumber);
        }

        loopNumber++;
    }

    if ((storedLoopStatus & PRESET_LOOP_STATUS_VALID) != 0UL &&
        (storedLoopStatus & ~inactiveLoops) != UI_LoopStatusBits())
    {
        return 0U;
    }

    uint8_t storedPrograms[PRESET_LOOP_PROGRAM_BYTES];

    if (PresetStore_ReadLoopPrograms(bank, slot, storedPrograms) !=
            PRESET_STORE_OK ||
        !UI_LoopProgramsMatch(storedPrograms))
    {
        return 0U;
    }

    PresetLoopStates storedStates;

    if (PresetStore_ReadLoopStates(bank, slot, &storedStates) !=
            PRESET_STORE_OK ||
        !UI_LoopStatesMatch(&storedStates))
    {
        return 0U;
    }

    return 1U;
}


/*
 * Laedt ein Preset der gewaehlten Bank (Control Change) und zeichnet
 * die Statusansicht neu. Im Menue wird das ignoriert. Ein leerer Slot
 * aendert nichts.
 *
 * Ist das Layout des Presets schon aktiv, passiert nichts: es wird weder
 * neu gezeichnet noch per MIDI gemeldet. Das beendet Rueckkopplungen,
 * wenn MIDI OUT wieder in MIDI IN fuehrt.
 *
 * Rueckgabe: 1 wenn das Preset geladen wurde.
 */
uint8_t UI_RecallPreset(uint8_t slot)
{
    if (slot >= PRESET_SLOTS_PER_BANK ||
        uiMode == UI_MODE_MENU)
    {
        return 0U;
    }

    if (UI_PresetMatchesCurrent(uiPresetBank, slot) ||
        !UI_PresetLoad(uiPresetBank, slot))
    {
        return 0U;
    }

    uiMenuControlMode = UI_MENU_CONTROL_NAVIGATION;
    uiMenuPage = UI_MENU_PAGE_ROOT;
    uiMode = UI_MODE_STATE_VIEW;

    UI_Draw();

    return 1U;
}


/*
 * Zeigt das Ergebnis des Speicherns in der Fusszeile.
 */
static void UI_MidiSaveShowResult(
    UI_MidiSaveResult result,
    uint8_t slot)
{
    uiMidiSaveResult = result;
    uiMidiSaveResultSlot = slot;
    uiMidiSaveResultFresh = 1U;

    UI_DrawFooter();

    uiMidiSaveResultFresh = 0U;
}


/*
 * CC0: schaltet das Speichern per MIDI scharf bzw. bricht es ab.
 * Im Menue wird es ignoriert.
 *
 * Rueckgabe: 1 wenn das Speichern danach scharf ist.
 */
uint8_t UI_SaveArmToggle(void)
{
    if (uiMode == UI_MODE_MENU)
    {
        uiMidiSaveArmed = 0U;
        return 0U;
    }

    uiMidiSaveArmed = (uint8_t)!uiMidiSaveArmed;
    uiMidiSaveResult = UI_MIDI_SAVE_RESULT_NONE;

    UI_DrawFooter();

    return uiMidiSaveArmed;
}


uint8_t UI_SaveArmed(void)
{
    return uiMidiSaveArmed;
}


/*
 * Speichert das aktuelle Layout in der aktuellen Bank unter der
 * Preset-Nummer slot + 1 (slot = 0 bis 23) und meldet es per MIDI.
 *
 * Ein vorhandener Name bleibt erhalten, ein leerer Slot bekommt den
 * Namen "preset nn" wie im Menue. Danach ist das Speichern nicht mehr
 * scharf, auch wenn es fehlschlaegt.
 *
 * Rueckgabe: 1 bei Erfolg.
 */
uint8_t UI_SavePresetFromMidi(uint8_t slot)
{
    uiMidiSaveArmed = 0U;

    if (slot >= PRESET_SLOTS_PER_BANK ||
        uiMode == UI_MODE_MENU)
    {
        return 0U;
    }

    uint8_t layout[PRESET_LAYOUT_MAX_BYTES];
    char name[PRESET_NAME_LENGTH];

    uint8_t length =
        UI_PresetSerializeLayout(layout, sizeof(layout));

    if (length == 0U)
    {
        UI_MidiSaveShowResult(UI_MIDI_SAVE_RESULT_ERROR, slot);
        return 0U;
    }

    if (!PresetStore_GetSlotName(uiPresetBank, slot, name))
    {
        uint8_t position = UI_SaveAppendText(
            name, 0U, sizeof(name), "preset ");

        UI_SaveAppendUInt2(
            name, position, sizeof(name), (uint8_t)(slot + 1U));
    }

    uint8_t programs[PRESET_LOOP_PROGRAM_BYTES];
    PresetLoopStates states;

    UI_LoopProgramsBuild(programs);
    UI_LoopStatesBuild(&states);

    PresetStoreStatus result = PresetStore_SavePreset(
        uiPresetBank, slot, name, layout, length,
            UI_LoopStatusBits(), programs, &states);

    if (result != PRESET_STORE_OK)
    {
        UI_MidiSaveShowResult(
            (result == PRESET_STORE_ERR_FULL) ?
                UI_MIDI_SAVE_RESULT_FULL :
                UI_MIDI_SAVE_RESULT_ERROR,
            slot);

        return 0U;
    }

    MidiPresets_NotifyPresetChanged(uiPresetBank, slot);
    UI_MidiSaveShowResult(UI_MIDI_SAVE_RESULT_SAVED, slot);

    return 1U;
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

        uint8_t programs[PRESET_LOOP_PROGRAM_BYTES];
        PresetLoopStates states;

        UI_LoopProgramsBuild(programs);
        UI_LoopStatesBuild(&states);

        PresetStoreStatus result = PresetStore_SavePreset(
            uiPresetBank, slot, name, layout, length,
            UI_LoopStatusBits(), programs, &states);

        if (result == PRESET_STORE_OK)
        {
            uiSaveStatus = UI_SAVE_STATUS_SAVED;

            MidiPresets_NotifyPresetChanged(uiPresetBank, slot);
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


static uint8_t UI_NameEquals(
    const char *a,
    const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }

    return (*a == *b) ? 1U : 0U;
}


static int8_t UI_NameAlphabetIndex(char c)
{
    /*
     * Kleinbuchstaben (z.B. aus aelteren Presets) werden wie
     * Grossbuchstaben behandelt, weil der Zeichensatz nur diese kennt.
     */
    if (c >= 'a' && c <= 'z')
    {
        c = (char)(c - ('a' - 'A'));
    }

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
 * Kopfzeile der Untermenues:
 *   Preset:  "Bank n Preset nn"
 *   Bank:    "Rename bank n"
 */
static void UI_SaveBuildHeader(
    char *header,
    uint8_t bankMode,
    uint8_t slot)
{
    uint8_t position;

    if (bankMode)
    {
        position = UI_SaveAppendText(
            header, 0U, UI_SAVE_TEXT_CAPACITY, "Rename bank ");
        UI_SaveAppendUInt(
            header, position, UI_SAVE_TEXT_CAPACITY, uiPresetBank);

        return;
    }

    position = UI_SaveAppendText(
        header, 0U, UI_SAVE_TEXT_CAPACITY, "Bank ");
    position = UI_SaveAppendUInt(
        header, position, UI_SAVE_TEXT_CAPACITY, uiPresetBank);
    position = UI_SaveAppendText(
        header, position, UI_SAVE_TEXT_CAPACITY, " Preset ");
    UI_SaveAppendUInt2(
        header, position, UI_SAVE_TEXT_CAPACITY,
        (uint8_t)(slot + 1U));
}


static uint8_t UI_NamePrefixLength(void)
{
    uint8_t length = 0U;

    while (uiNamePrefix[length] != '\0' &&
           length < (sizeof(uiNamePrefix) - 1U))
    {
        length++;
    }

    return length;
}


/*
 * X-Position des Zeichens, das hinter den bereits gesetzten Zeichen
 * folgt. Die Breite wird mit der Schrift gemessen: Breite von
 * "<gesetzt>A" minus Breite von "A". Ein feststehender Praefix (die
 * Banknummer) wird davor mitgerechnet.
 */
static uint16_t UI_NameNextCharX(void)
{
    uint16_t base = (uint16_t)(UI_SAVE_TEXT_X_OFFSET +
                               (6U * UI_NamePrefixLength()));

    if (uiNameLength == 0U)
    {
        return base;
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

    return (uint16_t)(base + fullWidth - singleWidth);
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

    /*
     * Namenszeile: feststehender Praefix und Name.
     */
    char line[PRESET_NAME_LENGTH + 6U];

    uint8_t position = UI_SaveAppendText(
        line, 0U, sizeof(line), uiNamePrefix);
    UI_SaveAppendText(
        line, position, sizeof(line), uiNameBuffer);

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET, textY, line,
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

    ST7735_FillRect(
        0U,
        0U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT,
        UI_COLOR_BACKGROUND
    );

    UI_SaveShadowInvalidate();

    UI_SaveBuildHeader(
        header,
        (uiNameMode == UI_NAME_MODE_RENAME_BANK) ? 1U : 0U,
        uiNameSlot);

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
 * Namensvorschlag:
 *   Preset:  "preset nn"   (nn = Slotnummer)
 *   Bank:    "bank n"      (Standardname der Bank)
 */
static void UI_NameBuildSuggestion(
    uint8_t slot,
    char *name)
{
    uint8_t position;

    if (uiNameMode == UI_NAME_MODE_RENAME_BANK)
    {
        position = UI_SaveAppendText(
            name, 0U, PRESET_NAME_LENGTH, "bank ");

        UI_SaveAppendUInt(
            name, position, PRESET_NAME_LENGTH, uiPresetBank);

        return;
    }

    position = UI_SaveAppendText(
        name, 0U, PRESET_NAME_LENGTH, "preset ");

    UI_SaveAppendUInt2(
        name, position, PRESET_NAME_LENGTH, (uint8_t)(slot + 1U));
}


/*
 * Oeffnet das Untermenue.
 *
 * Der Name ist bei einem belegten Preset der vorhandene, bei einem
 * leeren der Vorschlag "preset nn". Bei der Bank ist es der aktuelle
 * Banknamen, davor steht fest die Banknummer. Der Cursor steht auf
 * "Save".
 *
 * cancelPage ist die Seite, zu der Cancel und Return zurueckfuehren.
 */
static void UI_NameOpen(
    UI_NameMode mode,
    uint8_t slot,
    UI_MenuPage cancelPage)
{
    char name[PRESET_NAME_LENGTH];

    uiNameMode = mode;
    uiNameCancelPage = cancelPage;
    uiNameSlot = slot;
    uiNamePrefix[0] = '\0';

    if (mode == UI_NAME_MODE_RENAME_BANK)
    {
        uint8_t position = UI_SaveAppendUInt(
            uiNamePrefix, 0U, sizeof(uiNamePrefix), uiPresetBank);

        UI_SaveAppendText(
            uiNamePrefix, position, sizeof(uiNamePrefix), " ");

        PresetStore_GetBankName(uiPresetBank, name);
    }
    else if (!PresetStore_GetSlotName(uiPresetBank, slot, name))
    {
        UI_NameBuildSuggestion(slot, name);
    }

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


/*
 * Cancel und Return: zurueck zur Seite, von der das Untermenue
 * geoeffnet wurde.
 */
static void UI_NameCancel(void)
{
    uiNameEditing = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;
    uiMenuPage = uiNameCancelPage;
    UI_DrawMenu();
}


/*
 * Click auf die Namenszeile.
 *
 * Steht dort der Namensvorschlag ("preset nn"), wird er geloescht und
 * die Eingabe beginnt mit dem ersten Zeichen.
 *
 * Jeder andere Name bleibt erhalten. Der Editor startet dann hinter
 * dem letzten Zeichen: ein neues Zeichen wird angehaengt, Return holt
 * das letzte Zeichen zum Aendern zurueck, Enter beendet den Editor
 * ohne Aenderung.
 */
static void UI_NameStartEditing(void)
{
    char suggestion[PRESET_NAME_LENGTH];

    UI_NameBuildSuggestion(uiNameSlot, suggestion);

    if (UI_NameEquals(uiNameBuffer, suggestion))
    {
        uiNameBuffer[0] = '\0';
        uiNameLength = 0U;
        uiNameCandidate = -1;
    }
    else
    {
        uiNameLength = UI_NameLengthOf(uiNameBuffer);
        uiNameCandidate = -1;

        if (uiNameLength >= UI_NAME_MAX_LENGTH)
        {
            /*
             * Name ist voll: das letzte Zeichen direkt editierbar
             * machen, weil dahinter kein Platz mehr ist.
             */
            uiNameLength--;
            uiNameCandidate =
                UI_NameAlphabetIndex(uiNameBuffer[uiNameLength]);
            uiNameBuffer[uiNameLength] = '\0';
        }
    }

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


/*
 * "Save" im Untermenue.
 *
 *   Preset speichern:  Layout und Name
 *   Preset umbenennen: nur der Name
 *   Bank umbenennen:   Bankname (der Standardname stellt den
 *                      Standard wieder her)
 *
 * Danach geht es zurueck zur Liste, die das Ergebnis in der
 * Statuszeile zeigt.
 */
static void UI_NameCommit(void)
{
    if (uiNameMode == UI_NAME_MODE_SAVE_PRESET)
    {
        UI_SaveExecute(uiNameSlot, uiNameBuffer);
        return;
    }

    /*
     * Das Schreiben blockiert, deshalb vorher anzeigen.
     */
    uiSaveStatus = UI_SAVE_STATUS_SAVING;
    UI_SaveDrawStatus();

    PresetStoreStatus result;

    if (uiNameMode == UI_NAME_MODE_RENAME_PRESET)
    {
        result = PresetStore_SetPresetName(
            uiPresetBank, uiNameSlot, uiNameBuffer);
    }
    else
    {
        char defaultName[PRESET_NAME_LENGTH];

        UI_NameBuildSuggestion(uiNameSlot, defaultName);

        result = PresetStore_SetBankName(
            uiPresetBank,
            UI_NameEquals(uiNameBuffer, defaultName) ?
                "" : uiNameBuffer);
    }

    uiSaveStatus =
        (result == PRESET_STORE_OK) ?
        UI_SAVE_STATUS_RENAMED :
        UI_SAVE_STATUS_ERROR;

    uiNameEditing = 0U;
    uiMenuPage = UI_MENU_PAGE_MANAGE_PRESETS;
    UI_DrawMenu();
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
        UI_NameCommit();
    }
    else
    {
        UI_NameCancel();
    }
}


static void UI_NameHandleReturn(void)
{
    if (!uiNameEditing)
    {
        UI_NameCancel();
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

/* -------------------------------------------------------------------------- */
/* Manage Presets: ausgehende MIDI-Meldungen eines Presets                    */
/* -------------------------------------------------------------------------- */

/*
 * Ganzseitige Liste im Stil der Preset-Liste mit allen Meldungen, die
 * das Preset beim Laden sendet (nach Program Change Bank und CC Preset,
 * vor On/Bypass der Loops):
 *
 *   Ch2 PC5 L01        Programm des Effektgeraets (Loop mit Program
 *                      Change auf diesem Kanal, Kurzname des Loops)
 *   Ch1 CC20, 127      zusaetzliche Control Change
 *   Ch3 PC7            zusaetzliche Program Change
 *   add message
 *   L01 On/Bypass 1: On 2   Zustand der On/Bypass-Gruppen der Loops im
 *   L01 On/Bypass 2: Off    Layout des Presets ("On" bei nur einem
 *                           Schalter, sonst "On <Schalter>")
 *
 * Bearbeiten: Enter waehlt nacheinander Kanal, Typ (CC / PC / empty),
 * Nummer und bei CC den Wert. "empty" loescht die Meldung. Statt zu
 * drehen kann man die Meldung auch per MIDI senden (MIDI Learn), nach
 * einer gelernten neuen Meldung wartet "add message" auf die naechste.
 *
 * Eine Program Change auf dem Kanal eines Loops mit Program Change wird
 * als Programm dieses Loops gespeichert (wie beim Speichern des
 * Presets), alle anderen Meldungen als zusaetzliche Meldungen (hoechstens
 * PRESET_MIDI_MAX_MESSAGES). Gespeichert wird beim Verlassen der Seite
 * (auch per Menue-Taste).
 *
 * On/Bypass-Zeilen: Enter, drehen (Off, On 1 ... On n), Enter. Ein Loop,
 * dessen Zeilen geaendert wurden (oder der schon einen gespeicherten
 * Zustand hat), wird beim Laden genau in diesen Zustand gebracht, die
 * uebrigen wie bisher ueber den Loop-Status.
 */
#define UI_PM_MAX_ENTRIES \
    (PRESET_MIDI_MAX_MESSAGES + 16U)
#define UI_PM_MAX_GROUP_ROWS \
    (PRESET_LOOP_STATE_MAX_LOOPS * LOOP_MIDI_MAX_GROUPS)

typedef enum
{
    UI_PM_TYPE_EMPTY = 0,
    UI_PM_TYPE_CC,
    UI_PM_TYPE_PC

} UI_PmType;

typedef enum
{
    UI_PM_EDIT_NONE = 0,
    UI_PM_EDIT_CHANNEL,
    UI_PM_EDIT_TYPE,
    UI_PM_EDIT_NUMBER,
    UI_PM_EDIT_VALUE,
    UI_PM_EDIT_STATE

} UI_PmEdit;

typedef enum
{
    UI_PM_STATUS_NONE = 0,
    UI_PM_STATUS_FULL,
    UI_PM_STATUS_WRITE_ERROR

} UI_PmStatus;

typedef struct
{
    uint8_t type;       /* UI_PmType */
    uint8_t channel;    /* 0 bis 15 */
    uint8_t number;     /* Controller bzw. Programm */
    uint8_t value;      /* nur bei CC */

} UI_PmEntry;

static UI_PmEntry uiPmEntries[UI_PM_MAX_ENTRIES];
static uint8_t uiPmCount = 0U;

/* Eine On/Bypass-Gruppe eines Loops. */
typedef struct
{
    uint8_t itemIndex;
    uint8_t loopNumber;     /* wie beim Loop-Status */
    uint8_t group;
    uint8_t switchCount;
    uint8_t value;          /* 0 = aus, k = Schalter k an */

} UI_PmGroupRow;

static UI_PmGroupRow uiPmGroups[UI_PM_MAX_GROUP_ROWS];
static uint8_t uiPmGroupCount = 0U;

/* Gespeicherter Loop-Status und On/Bypass-Zustand des Presets. */
static uint32_t uiPmLoopStatus = 0UL;
static PresetLoopStates uiPmStates;

/* Loops (Bit = loopNumber), deren Zustand gespeichert wird. */
static uint32_t uiPmExplicit = 0UL;
static uint8_t uiPmEditState = 0U;

/* Gespeicherte Programme, Grundlage beim Zurueckschreiben. */
static uint8_t uiPmPrograms[PRESET_LOOP_PROGRAM_BYTES];

static uint8_t uiPmSlot = 0U;
static int16_t uiPmRow = 0;
static uint8_t uiPmScrollTop = 0U;

static UI_PmEdit uiPmEdit = UI_PM_EDIT_NONE;
static UI_PmEntry uiPmEditEntry;

static uint8_t uiPmDirty = 0U;
static UI_PmStatus uiPmStatus = UI_PM_STATUS_NONE;


static uint8_t UI_PmIsLearning(void)
{
    return (uiMode == UI_MODE_MENU &&
            uiMenuPage == UI_MENU_PAGE_PRESET_MIDI &&
            uiPmEdit != UI_PM_EDIT_NONE &&
            uiPmEdit != UI_PM_EDIT_STATE) ? 1U : 0U;
}


/* Zeile der ersten On/Bypass-Gruppe (nach "add message"). */
static uint8_t UI_PmFirstGroupRow(void)
{
    return (uint8_t)(uiPmCount +
        ((uiPmCount < UI_PM_MAX_ENTRIES) ? 1U : 0U));
}


static uint8_t UI_PmIsAddRow(int16_t row)
{
    return (row == (int16_t)uiPmCount &&
            uiPmCount < UI_PM_MAX_ENTRIES) ? 1U : 0U;
}


/* Index in uiPmGroups oder -1. */
static int16_t UI_PmGroupIndex(int16_t row)
{
    int16_t index = row - (int16_t)UI_PmFirstGroupRow();

    return (index >= 0 && index < (int16_t)uiPmGroupCount) ? index : -1;
}


/* Meldungen, "add message" und On/Bypass-Gruppen. */
static uint8_t UI_PmRowCount(void)
{
    return (uint8_t)(UI_PmFirstGroupRow() + uiPmGroupCount);
}


/*
 * Erster Loop mit Program Change auf dem Kanal (Index in uiItems),
 * -1 = keiner. loopNumber bekommt die Nummer fuer die Preset-Programme.
 */
static int16_t UI_PmProgramLoop(
    uint8_t channel,
    uint8_t *loopNumber)
{
    uint8_t number = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (number < PRESET_LOOP_STATUS_MAX_LOOPS &&
            uiItems[i].id <= 255U &&
            LoopMidi_GetProgramChannel((uint8_t)uiItems[i].id) ==
                channel)
        {
            if (loopNumber != NULL)
            {
                *loopNumber = number;
            }

            return (int16_t)i;
        }

        number++;
    }

    return -1;
}


/* Eintrag, der als Programm eines Loops gespeichert wird. */
static uint8_t UI_PmIsLoopProgram(const UI_PmEntry *entry)
{
    return (entry->type == UI_PM_TYPE_PC &&
            UI_PmProgramLoop(entry->channel, NULL) >= 0) ? 1U : 0U;
}


/* Anzahl der Eintraege, die als zusaetzliche Meldungen zaehlen. */
static uint8_t UI_PmCustomCount(void)
{
    uint8_t count = 0U;

    for (uint8_t i = 0U; i < uiPmCount; i++)
    {
        if (!UI_PmIsLoopProgram(&uiPmEntries[i]))
        {
            count++;
        }
    }

    return count;
}


/*
 * On/Bypass-Zeilen: je Gruppe der Loops im Layout des Presets, die
 * On/Bypass-Gruppen haben (hoechstens PRESET_LOOP_STATE_MAX_LOOPS
 * Loops). Ohne gespeicherten Zustand wird er aus dem Loop-Status
 * abgeleitet (an = Gruppe 1, Schalter 1).
 */
static void UI_PmLoadGroups(void)
{
    uint8_t layout[PRESET_LAYOUT_MAX_BYTES];
    uint8_t length = 0U;

    uiPmGroupCount = 0U;
    uiPmExplicit = 0UL;

    if (PresetStore_ReadLoopStatus(
            uiPresetBank, uiPmSlot, &uiPmLoopStatus) != PRESET_STORE_OK)
    {
        uiPmLoopStatus = 0UL;
    }

    if (PresetStore_ReadLoopStates(
            uiPresetBank, uiPmSlot, &uiPmStates) != PRESET_STORE_OK)
    {
        memset(&uiPmStates, 0, sizeof(uiPmStates));
    }

    if (PresetStore_ReadLayout(
            uiPresetBank, uiPmSlot, layout, &length) != PRESET_STORE_OK ||
        length < 1U ||
        (uint16_t)(1U + (3U * layout[0])) > length)
    {
        return;
    }

    uint8_t loopNumber = 0U;
    uint8_t loops = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        uint8_t number = loopNumber++;

        if (number >= PRESET_LOOP_STATUS_MAX_LOOPS ||
            loops >= PRESET_LOOP_STATE_MAX_LOOPS ||
            uiItems[i].id > 255U ||
            i > 255U)
        {
            continue;
        }

        uint8_t inLayout = 0U;

        for (uint8_t e = 0U; e < layout[0]; e++)
        {
            if (layout[1U + (3U * e)] == (uint8_t)uiItems[i].id)
            {
                inLayout = 1U;
                break;
            }
        }

        LoopMidiConfig config;

        LoopMidi_GetConfig((uint8_t)uiItems[i].id, &config);

        if (!inLayout || config.groupCount == 0U)
        {
            continue;
        }

        uint8_t stored =
            ((uiPmStates.mask & (1UL << number)) != 0UL) ? 1U : 0U;
        uint8_t active =
            ((uiPmLoopStatus & PRESET_LOOP_STATUS_VALID) != 0UL &&
             ((uiPmLoopStatus >> number) & 1UL) != 0UL) ? 1U : 0U;

        if (stored)
        {
            uiPmExplicit |= (1UL << number);
        }

        loops++;

        for (uint8_t g = 0U; g < config.groupCount; g++)
        {
            UI_PmGroupRow *row = &uiPmGroups[uiPmGroupCount++];

            row->itemIndex = (uint8_t)i;
            row->loopNumber = number;
            row->group = g;
            row->switchCount =
                LoopMidi_SwitchCount(&config.groups[g]);

            if (stored)
            {
                row->value = (uint8_t)
                    ((uiPmStates.state[number] >> (4U * g)) & 0x0FUL);
            }
            else
            {
                row->value = (active && g == 0U) ? 1U : 0U;
            }

            if (row->value > row->switchCount)
            {
                row->value = 0U;
            }
        }
    }
}


/*
 * Liest Programme und zusaetzliche Meldungen des Presets in die Liste:
 * zuerst je Kanal das Programm der Loops mit Program Change, dann die
 * zusaetzlichen Meldungen (Reihenfolge wie beim Senden).
 */
static void UI_PmLoad(void)
{
    PresetMidiMessage messages[PRESET_MIDI_MAX_MESSAGES];
    uint8_t messageCount = 0U;

    uiPmCount = 0U;

    if (PresetStore_ReadLoopPrograms(
            uiPresetBank, uiPmSlot, uiPmPrograms) != PRESET_STORE_OK)
    {
        memset(uiPmPrograms, 0, sizeof(uiPmPrograms));
    }

    if (PresetStore_ReadMidiMessages(
            uiPresetBank, uiPmSlot, messages, &messageCount) !=
        PRESET_STORE_OK)
    {
        messageCount = 0U;
    }

    uint8_t channelsUsed[16];

    memset(channelsUsed, 0, sizeof(channelsUsed));

    uint8_t loopNumber = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
            uiItems[i].id <= 255U &&
            (uiPmPrograms[loopNumber] & PRESET_LOOP_PROGRAM_VALID) != 0U)
        {
            uint8_t channel =
                LoopMidi_GetProgramChannel((uint8_t)uiItems[i].id);

            if (channel < 16U && !channelsUsed[channel])
            {
                UI_PmEntry *entry = &uiPmEntries[uiPmCount++];

                channelsUsed[channel] = 1U;
                entry->type = UI_PM_TYPE_PC;
                entry->channel = channel;
                entry->number = (uint8_t)(uiPmPrograms[loopNumber] &
                    (uint8_t)~PRESET_LOOP_PROGRAM_VALID);
                entry->value = 0U;
            }
        }

        loopNumber++;
    }

    for (uint8_t m = 0U;
         m < messageCount && uiPmCount < UI_PM_MAX_ENTRIES;
         m++)
    {
        UI_PmEntry *entry = &uiPmEntries[uiPmCount++];

        entry->type =
            ((messages[m].status & 0xF0U) == 0xC0U) ?
            UI_PM_TYPE_PC :
            UI_PM_TYPE_CC;
        entry->channel = (uint8_t)(messages[m].status & 0x0FU);
        entry->number = messages[m].data1;
        entry->value = messages[m].data2;
    }

    UI_PmLoadGroups();
}


/*
 * Schreibt die Liste zurueck: Programme der Loops (andere Loops behalten
 * ihr gespeichertes Programm) und zusaetzliche Meldungen.
 */
static PresetStoreStatus UI_PmStore(void)
{
    uint8_t programs[PRESET_LOOP_PROGRAM_BYTES];
    PresetMidiMessage messages[PRESET_MIDI_MAX_MESSAGES];
    uint8_t messageCount = 0U;

    memcpy(programs, uiPmPrograms, sizeof(programs));

    uint8_t loopNumber = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
            uiItems[i].id <= 255U &&
            LoopMidi_GetProgramChannel((uint8_t)uiItems[i].id) !=
                LOOP_MIDI_PROGRAM_NONE)
        {
            programs[loopNumber] = 0U;
        }

        loopNumber++;
    }

    for (uint8_t e = 0U; e < uiPmCount; e++)
    {
        const UI_PmEntry *entry = &uiPmEntries[e];

        if (!UI_PmIsLoopProgram(entry))
        {
            if (messageCount < PRESET_MIDI_MAX_MESSAGES)
            {
                PresetMidiMessage *message = &messages[messageCount++];

                message->status = (uint8_t)(
                    ((entry->type == UI_PM_TYPE_PC) ? 0xC0U : 0xB0U) |
                    entry->channel);
                message->data1 = entry->number;
                message->data2 =
                    (entry->type == UI_PM_TYPE_PC) ? 0U : entry->value;
            }

            continue;
        }

        /*
         * Alle Loops auf dem Kanal bekommen das Programm (ein Geraet,
         * mehrere Loops), wie bei UI_LoopProgramsBuild.
         */
        loopNumber = 0U;

        for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
        {
            if (uiItems[i].type != UI_ITEM_LOOP)
            {
                continue;
            }

            if (loopNumber < PRESET_LOOP_STATUS_MAX_LOOPS &&
                uiItems[i].id <= 255U &&
                LoopMidi_GetProgramChannel((uint8_t)uiItems[i].id) ==
                    entry->channel)
            {
                programs[loopNumber] = (uint8_t)
                    (PRESET_LOOP_PROGRAM_VALID | entry->number);
            }

            loopNumber++;
        }
    }

    /*
     * On/Bypass: geaenderte Loops bekommen ihren Zustand, der Loop-Status
     * passt dazu (fuer den Rueckkopplungsschutz).
     */
    PresetLoopStates states = uiPmStates;
    uint32_t loopStatus = uiPmLoopStatus;

    for (uint8_t r = 0U; r < uiPmGroupCount; r++)
    {
        const UI_PmGroupRow *row = &uiPmGroups[r];
        uint32_t bit = 1UL << row->loopNumber;

        if ((uiPmExplicit & bit) == 0UL)
        {
            continue;
        }

        if (row->group == 0U)
        {
            states.state[row->loopNumber] = 0UL;
        }

        states.mask |= bit;
        states.state[row->loopNumber] |=
            (uint32_t)row->value << (4U * row->group);
    }

    for (uint8_t n = 0U; n < PRESET_LOOP_STATUS_MAX_LOOPS; n++)
    {
        uint32_t bit = 1UL << n;

        if ((uiPmExplicit & bit) == 0UL ||
            (loopStatus & PRESET_LOOP_STATUS_VALID) == 0UL)
        {
            continue;
        }

        if (states.state[n] != 0UL)
        {
            loopStatus |= bit;
        }
        else
        {
            loopStatus &= ~bit;
        }
    }

    return PresetStore_WriteMidi(
        uiPresetBank, uiPmSlot, loopStatus, programs, &states,
        messages, messageCount);
}


/* Haengt "<label><zahl>" an, mit Klammern als Markierung beim Bearbeiten. */
static uint8_t UI_PmAppendField(
    char *text,
    uint8_t position,
    const char *label,
    int16_t number,
    uint8_t marked)
{
    if (marked)
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, "(");
    }

    position = UI_SaveAppendText(
        text, position, UI_SAVE_TEXT_CAPACITY, label);

    if (number >= 0)
    {
        position = UI_SaveAppendUInt(
            text, position, UI_SAVE_TEXT_CAPACITY, (uint8_t)number);
    }

    if (marked)
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, ")");
    }

    return position;
}


static void UI_PmBuildRowText(
    uint8_t row,
    char *text)
{
    uint8_t editing =
        ((int16_t)row == uiPmRow &&
         uiPmEdit != UI_PM_EDIT_NONE) ? 1U : 0U;

    text[0] = '\0';

    int16_t groupIndex = UI_PmGroupIndex((int16_t)row);

    if (groupIndex >= 0)
    {
        const UI_PmGroupRow *group = &uiPmGroups[groupIndex];
        uint8_t value = editing ? uiPmEditState : group->value;

        uint8_t position = UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY,
            uiItems[group->itemIndex].shortName);

        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, " On/Bypass ");
        position = UI_SaveAppendUInt(
            text, position, UI_SAVE_TEXT_CAPACITY,
            (uint8_t)(group->group + 1U));
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, ": ");

        if (value == 0U)
        {
            UI_PmAppendField(text, position, "Off", -1, editing);
        }
        else
        {
            UI_PmAppendField(
                text, position, (group->switchCount > 1U) ? "On " : "On",
                (group->switchCount > 1U) ? (int16_t)value : -1,
                editing);
        }

        return;
    }

    if (row >= uiPmCount && !editing)
    {
        UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "add message");
        return;
    }

    const UI_PmEntry *entry =
        editing ? &uiPmEditEntry : &uiPmEntries[row];
    UI_PmEdit edit = editing ? uiPmEdit : UI_PM_EDIT_NONE;

    uint8_t position = UI_PmAppendField(
        text, 0U, "Ch", (int16_t)(entry->channel + 1U),
        (edit == UI_PM_EDIT_CHANNEL) ? 1U : 0U);

    position = UI_SaveAppendText(
        text, position, UI_SAVE_TEXT_CAPACITY, " ");

    if (entry->type == UI_PM_TYPE_EMPTY)
    {
        UI_PmAppendField(
            text, position, "empty", -1,
            (edit == UI_PM_EDIT_TYPE) ? 1U : 0U);
        return;
    }

    position = UI_PmAppendField(
        text, position,
        (entry->type == UI_PM_TYPE_PC) ? "PC" : "CC", -1,
        (edit == UI_PM_EDIT_TYPE) ? 1U : 0U);

    position = UI_PmAppendField(
        text, position, "", (int16_t)entry->number,
        (edit == UI_PM_EDIT_NUMBER) ? 1U : 0U);

    if (entry->type == UI_PM_TYPE_CC)
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, ", ");
        UI_PmAppendField(
            text, position, "", (int16_t)entry->value,
            (edit == UI_PM_EDIT_VALUE) ? 1U : 0U);
        return;
    }

    int16_t loopIndex = UI_PmProgramLoop(entry->channel, NULL);

    if (loopIndex >= 0)
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, " ");
        UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY,
            uiItems[loopIndex].shortName);
    }
}


/* -------------------------------------------------------------------------- */
/* Preset-Midi: Zeichnen                                                      */
/* -------------------------------------------------------------------------- */

static void UI_ListDrawScrollbar(
    uint16_t count,
    uint8_t scrollTop);


static void UI_PmDrawRow(uint8_t row)
{
    int16_t position = (int16_t)row - (int16_t)uiPmScrollTop;

    if (position < 0 ||
        position >= (int16_t)UI_SAVE_VISIBLE_PRESETS)
    {
        return;
    }

    uint16_t y = (uint16_t)(UI_SAVE_LIST_Y +
        ((uint16_t)position * UI_SAVE_ROW_HEIGHT));

    uint16_t background = UI_COLOR_BACKGROUND;
    char text[UI_SAVE_TEXT_CAPACITY];

    if (row < UI_PmRowCount())
    {
        UI_PmBuildRowText(row, text);

        if ((int16_t)row == uiPmRow)
        {
            background =
                (uiPmEdit != UI_PM_EDIT_NONE) ?
                UI_COLOR_BORDER_GRABBED :
                UI_COLOR_BORDER_SELECTED;
        }
    }
    else
    {
        text[0] = '\0';
    }

    UI_SaveDrawTextLine(
        (uint8_t)(position + 1),
        y,
        UI_SAVE_ROW_WIDTH,
        UI_SAVE_ROW_HEIGHT,
        y + UI_SAVE_TEXT_Y_OFFSET,
        text,
        UI_COLOR_TEXT_LIGHT,
        background
    );
}


static void UI_PmDrawList(void)
{
    for (uint8_t i = 0U; i < UI_SAVE_VISIBLE_PRESETS; i++)
    {
        UI_PmDrawRow((uint8_t)(uiPmScrollTop + i));
    }

    UI_ListDrawScrollbar(UI_PmRowCount(), uiPmScrollTop);
}


static void UI_PmDrawStatus(void)
{
    char text[UI_SAVE_TEXT_CAPACITY];
    uint16_t color = UI_COLOR_TEXT_LIGHT;
    uint8_t position = 0U;

    text[0] = '\0';

    if (uiPmStatus == UI_PM_STATUS_FULL)
    {
        color = UI_COLOR_LOOP_UNCONFIRMED;
        position = UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Max ");
        position = UI_SaveAppendUInt(
            text, position, UI_SAVE_TEXT_CAPACITY,
            PRESET_MIDI_MAX_MESSAGES);
        UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, " messages");
    }
    else if (uiPmStatus == UI_PM_STATUS_WRITE_ERROR)
    {
        color = UI_COLOR_LOOP_UNCONFIRMED;
        UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Write failed");
    }
    else if (uiPmEdit == UI_PM_EDIT_STATE)
    {
        UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Turn: Off / On");
    }
    else if (uiPmEdit != UI_PM_EDIT_NONE)
    {
        UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Turn or send CC/PC");
    }
    else
    {
        position = UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Messages ");
        position = UI_SaveAppendUInt(
            text, position, UI_SAVE_TEXT_CAPACITY, UI_PmCustomCount());
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, "/");
        UI_SaveAppendUInt(
            text, position, UI_SAVE_TEXT_CAPACITY,
            PRESET_MIDI_MAX_MESSAGES);
    }

    UI_SaveDrawTextLine(
        UI_SAVE_SHADOW_STATUS_SLOT,
        UI_SAVE_SEPARATOR_Y + 1U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT - (UI_SAVE_SEPARATOR_Y + 1U),
        UI_SAVE_STATUS_Y,
        text,
        color,
        UI_COLOR_BACKGROUND
    );
}


static void UI_PmDrawPage(void)
{
    char header[UI_SAVE_TEXT_CAPACITY];

    ST7735_FillRect(
        0U,
        0U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT,
        UI_COLOR_BACKGROUND
    );

    UI_SaveShadowInvalidate();

    UI_SaveBuildHeader(header, 0U, uiPmSlot);

    uint8_t position = 0U;

    while (header[position] != '\0')
    {
        position++;
    }

    UI_SaveAppendText(
        header, position, UI_SAVE_TEXT_CAPACITY, " Midi");

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        UI_SAVE_HEADER_Y,
        header,
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND,
        1U
    );

    UI_PmDrawList();

    ST7735_DrawLine(
        0U,
        UI_SAVE_SEPARATOR_Y,
        UI_DISPLAY_WIDTH - 1U,
        UI_SAVE_SEPARATOR_Y,
        UI_COLOR_BORDER_NORMAL
    );

    UI_PmDrawStatus();
}


/* Setzt die Auswahl und scrollt bei Bedarf (wie UI_LmSelectRow). */
static void UI_PmSelectRow(int16_t row)
{
    int16_t count = (int16_t)UI_PmRowCount();
    int16_t visible = (int16_t)UI_SAVE_VISIBLE_PRESETS;

    if (row >= count)
    {
        row = (int16_t)(count - 1);
    }

    if (row < 0)
    {
        row = 0;
    }

    int16_t oldRow = uiPmRow;
    uint8_t oldScrollTop = uiPmScrollTop;

    uiPmRow = row;

    if (row < (int16_t)uiPmScrollTop)
    {
        uiPmScrollTop = (uint8_t)row;
    }
    else if (row >= (int16_t)uiPmScrollTop + visible)
    {
        uiPmScrollTop = (uint8_t)(row - visible + 1);
    }

    if (count <= visible)
    {
        uiPmScrollTop = 0U;
    }
    else if ((int16_t)uiPmScrollTop > (int16_t)(count - visible))
    {
        uiPmScrollTop = (uint8_t)(count - visible);
    }

    if (uiPmScrollTop != oldScrollTop)
    {
        UI_PmDrawList();
        return;
    }

    if (oldRow >= 0 && oldRow != row)
    {
        UI_PmDrawRow((uint8_t)oldRow);
    }

    UI_PmDrawRow((uint8_t)row);
}


/* Nach einer Aenderung der Zeilen: Liste und Status neu zeichnen. */
static void UI_PmRefresh(int16_t row)
{
    uiPmRow = -1;
    UI_PmSelectRow(row);
    UI_PmDrawList();
    UI_PmDrawStatus();
}


/* -------------------------------------------------------------------------- */
/* Preset-Midi: Bearbeiten                                                    */
/* -------------------------------------------------------------------------- */

/*
 * Beginnt die Eingabe in der ausgewaehlten Zeile mit dem Kanal. Eine
 * neue Meldung startet mit den Werten der letzten (oder CC0, 127 auf
 * dem Sendekanal der Presets).
 */
static void UI_PmStartEdit(void)
{
    if (uiPmRow < (int16_t)uiPmCount)
    {
        uiPmEditEntry = uiPmEntries[uiPmRow];
    }
    else if (uiPmCount > 0U)
    {
        uiPmEditEntry = uiPmEntries[uiPmCount - 1U];

        if (uiPmEditEntry.type == UI_PM_TYPE_PC)
        {
            uiPmEditEntry.value = 127U;
        }
    }
    else
    {
        uiPmEditEntry.type = UI_PM_TYPE_CC;
        uiPmEditEntry.channel = (uint8_t)MIDI_PRESETS_TX_CHANNEL;
        uiPmEditEntry.number = 0U;
        uiPmEditEntry.value = 127U;
    }

    uiPmEdit = UI_PM_EDIT_CHANNEL;
    uiPmStatus = UI_PM_STATUS_NONE;

    UI_PmDrawRow((uint8_t)uiPmRow);
    UI_PmDrawStatus();
}


static void UI_PmEndEdit(void)
{
    uiPmEdit = UI_PM_EDIT_NONE;

    if (uiPmRow >= 0)
    {
        UI_PmDrawRow((uint8_t)uiPmRow);
    }

    UI_PmDrawStatus();
}


/*
 * Uebernimmt die eingegebene oder gelernte Meldung in die Zeile. "empty"
 * loescht eine bestehende Meldung. Nach einer per MIDI gelernten neuen
 * Meldung wartet "add message" gleich auf die naechste.
 */
static void UI_PmCommit(uint8_t learned)
{
    int16_t row = uiPmRow;
    uint8_t appended = (row >= (int16_t)uiPmCount) ? 1U : 0U;
    UI_PmEntry entry = uiPmEditEntry;

    uiPmEdit = UI_PM_EDIT_NONE;

    if (entry.type == UI_PM_TYPE_EMPTY)
    {
        if (!appended)
        {
            for (uint8_t i = (uint8_t)row;
                 (uint8_t)(i + 1U) < uiPmCount;
                 i++)
            {
                uiPmEntries[i] = uiPmEntries[i + 1U];
            }

            uiPmCount--;
            uiPmDirty = 1U;
        }

        UI_PmRefresh(row);
        return;
    }

    if (entry.type == UI_PM_TYPE_PC)
    {
        entry.value = 0U;
    }

    UI_PmEntry backup = entry;

    if (appended)
    {
        if (uiPmCount >= UI_PM_MAX_ENTRIES)
        {
            uiPmStatus = UI_PM_STATUS_FULL;
            UI_PmRefresh(row);
            return;
        }

        uiPmEntries[uiPmCount++] = entry;
    }
    else
    {
        backup = uiPmEntries[row];
        uiPmEntries[row] = entry;
    }

    if (UI_PmCustomCount() > PRESET_MIDI_MAX_MESSAGES)
    {
        if (appended)
        {
            uiPmCount--;
        }
        else
        {
            uiPmEntries[row] = backup;
        }

        uiPmStatus = UI_PM_STATUS_FULL;
        UI_PmRefresh(row);
        return;
    }

    uiPmDirty = 1U;
    uiPmStatus = UI_PM_STATUS_NONE;

    UI_PmRefresh(appended ? (int16_t)uiPmCount : row);

    if (learned && appended &&
        UI_PmIsAddRow(uiPmRow))
    {
        UI_PmStartEdit();
    }
}


/* Offene Eingabe verwerfen, eine geaenderte Liste speichern. */
static PresetStoreStatus UI_PmFinish(void)
{
    uiPmEdit = UI_PM_EDIT_NONE;

    if (!uiPmDirty)
    {
        return PRESET_STORE_OK;
    }

    uiPmDirty = 0U;

    return UI_PmStore();
}


static void UI_PmOpen(uint8_t slot)
{
    uiPmSlot = slot;

    UI_PmLoad();

    uiPmEdit = UI_PM_EDIT_NONE;
    uiPmDirty = 0U;
    uiPmStatus = UI_PM_STATUS_NONE;
    uiPmRow = 0;
    uiPmScrollTop = 0U;

    uiMenuPage = UI_MENU_PAGE_PRESET_MIDI;
    UI_DrawMenu();
}


static int16_t UI_PmClamp(
    int16_t value,
    int16_t minimum,
    int16_t maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}


static void UI_PmHandleEncoder(int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    UI_PmEntry *entry = &uiPmEditEntry;

    switch (uiPmEdit)
    {
        case UI_PM_EDIT_CHANNEL:
            entry->channel = (uint8_t)UI_PmClamp(
                (int16_t)entry->channel + direction, 0, 15);
            break;

        case UI_PM_EDIT_TYPE:
            entry->type = (uint8_t)UI_PmClamp(
                (int16_t)entry->type + direction,
                UI_PM_TYPE_EMPTY, UI_PM_TYPE_PC);
            break;

        case UI_PM_EDIT_NUMBER:
            entry->number = (uint8_t)UI_PmClamp(
                (int16_t)entry->number + direction, 0, 127);
            break;

        case UI_PM_EDIT_VALUE:
            entry->value = (uint8_t)UI_PmClamp(
                (int16_t)entry->value + direction, 0, 127);
            break;

        case UI_PM_EDIT_STATE:
            uiPmEditState = (uint8_t)UI_PmClamp(
                (int16_t)uiPmEditState + direction, 0,
                (int16_t)uiPmGroups[UI_PmGroupIndex(uiPmRow)].switchCount);
            break;

        case UI_PM_EDIT_NONE:
        default:
            if (uiPmStatus != UI_PM_STATUS_NONE)
            {
                uiPmStatus = UI_PM_STATUS_NONE;
                UI_PmDrawStatus();
            }

            UI_PmSelectRow((int16_t)(uiPmRow + direction));
            return;
    }

    UI_PmDrawRow((uint8_t)uiPmRow);
}


static void UI_PmHandleEnter(void)
{
    int16_t groupIndex = UI_PmGroupIndex(uiPmRow);

    if (groupIndex >= 0)
    {
        UI_PmGroupRow *group = &uiPmGroups[groupIndex];

        if (uiPmEdit == UI_PM_EDIT_NONE)
        {
            uiPmEditState = group->value;
            uiPmEdit = UI_PM_EDIT_STATE;
            uiPmStatus = UI_PM_STATUS_NONE;
            UI_PmDrawRow((uint8_t)uiPmRow);
            UI_PmDrawStatus();
            return;
        }

        /*
         * Ab jetzt wird der Zustand des ganzen Loops gespeichert.
         */
        if (uiPmEditState != group->value ||
            (uiPmExplicit & (1UL << group->loopNumber)) == 0UL)
        {
            group->value = uiPmEditState;
            uiPmExplicit |= (1UL << group->loopNumber);
            uiPmDirty = 1U;
        }

        UI_PmEndEdit();
        return;
    }

    switch (uiPmEdit)
    {
        case UI_PM_EDIT_NONE:
            UI_PmStartEdit();
            return;

        case UI_PM_EDIT_CHANNEL:
            uiPmEdit = UI_PM_EDIT_TYPE;
            break;

        case UI_PM_EDIT_TYPE:
            if (uiPmEditEntry.type == UI_PM_TYPE_EMPTY)
            {
                UI_PmCommit(0U);
                return;
            }

            uiPmEdit = UI_PM_EDIT_NUMBER;
            break;

        case UI_PM_EDIT_NUMBER:
            if (uiPmEditEntry.type == UI_PM_TYPE_CC)
            {
                uiPmEdit = UI_PM_EDIT_VALUE;
                break;
            }

            UI_PmCommit(0U);
            return;

        case UI_PM_EDIT_VALUE:
        default:
            UI_PmCommit(0U);
            return;
    }

    UI_PmDrawRow((uint8_t)uiPmRow);
}


static void UI_PmHandleReturn(void)
{
    if (uiPmEdit != UI_PM_EDIT_NONE)
    {
        UI_PmEndEdit();
        return;
    }

    if (UI_PmFinish() != PRESET_STORE_OK)
    {
        /*
         * Der Fehler bleibt stehen, das naechste Return verlaesst die
         * Seite.
         */
        uiPmStatus = UI_PM_STATUS_WRITE_ERROR;
        UI_PmDrawStatus();
        return;
    }

    /*
     * Zurueck zum Aktionsmenue, Cursor auf "Midi".
     */
    uiActionsRow = UI_ACTION_MIDI;
    uiActionsDeletePending = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;

    uiMenuPage = UI_MENU_PAGE_MANAGE_ACTIONS;
    UI_DrawMenu();
}


/*
 * MIDI Learn: eine empfangene Control Change oder Program Change wird
 * mit Kanal und Werten uebernommen.
 */
static void UI_PmLearn(
    uint8_t type,
    uint8_t channel,
    uint8_t number,
    uint8_t value)
{
    if (number > 127U || value > 127U)
    {
        return;
    }

    uiPmEditEntry.type = type;
    uiPmEditEntry.channel = (uint8_t)(channel & 0x0FU);
    uiPmEditEntry.number = number;
    uiPmEditEntry.value = value;

    UI_PmCommit(1U);
}


/* -------------------------------------------------------------------------- */
/* Manage Presets: Aktionsmenue (Load / Rename / Midi / Delete)               */
/* -------------------------------------------------------------------------- */

static void UI_ActionsDrawRow(uint8_t row)
{
    uint16_t y = (uint16_t)(UI_NAME_FIRST_ROW_Y +
                 ((uint16_t)row * UI_NAME_ROW_PITCH));

    uint16_t background =
        (row == uiActionsRow) ?
        UI_COLOR_BORDER_SELECTED :
        UI_COLOR_BACKGROUND;

    uint16_t color =
        (row == UI_ACTION_DELETE && uiActionsDeletePending) ?
        UI_COLOR_LOOP_UNCONFIRMED :
        UI_COLOR_TEXT_LIGHT;

    ST7735_FillRect(
        0U,
        y,
        UI_SAVE_ROW_WIDTH,
        UI_NAME_ROW_HEIGHT,
        background
    );

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        y + UI_NAME_TEXT_Y_OFFSET,
        uiActionLabels[row],
        color,
        background,
        1U
    );
}


static void UI_ActionsDrawPage(void)
{
    char header[UI_SAVE_TEXT_CAPACITY];

    ST7735_FillRect(
        0U,
        0U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT,
        UI_COLOR_BACKGROUND
    );

    UI_SaveShadowInvalidate();

    UI_SaveBuildHeader(header, 0U, uiActionsSlot);

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        UI_SAVE_HEADER_Y,
        header,
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND,
        1U
    );

    for (uint8_t row = 0U; row < UI_ACTION_COUNT; row++)
    {
        UI_ActionsDrawRow(row);
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


/* Oeffnet das Aktionsmenue fuer ein belegtes Preset. Cursor auf "Load". */
static void UI_ActionsOpen(uint8_t slot)
{
    uiActionsSlot = slot;
    uiActionsRow = UI_ACTION_LOAD;
    uiActionsDeletePending = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;

    uiMenuPage = UI_MENU_PAGE_MANAGE_ACTIONS;
    UI_DrawMenu();
}


/* Zurueck zur Preset-Liste der Manage-Seite. */
static void UI_ActionsBack(void)
{
    uiActionsDeletePending = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;

    uiMenuPage = UI_MENU_PAGE_MANAGE_PRESETS;
    UI_DrawMenu();
}


/*
 * Load: stellt das gespeicherte Layout her, meldet es per MIDI und
 * verlaesst das Menue. Bei einem Fehler bleibt das aktuelle Layout
 * unveraendert und die Meldung erscheint in der Statuszeile.
 */
static void UI_ActionLoad(void)
{
    if (!UI_PresetLoad(uiPresetBank, uiActionsSlot))
    {
        uiSaveStatus = UI_SAVE_STATUS_LOAD_FAILED;
        UI_SaveDrawStatus();
        return;
    }

    uiActionsDeletePending = 0U;
    uiMenuControlMode = UI_MENU_CONTROL_NAVIGATION;
    uiMenuPage = UI_MENU_PAGE_ROOT;
    uiMode = UI_MODE_STATE_VIEW;

    UI_Draw();
}


static void UI_ActionDelete(void)
{
    PresetStoreStatus result =
        PresetStore_DeletePreset(uiPresetBank, uiActionsSlot);

    uiActionsDeletePending = 0U;

    uiSaveStatus =
        (result == PRESET_STORE_OK) ?
        UI_SAVE_STATUS_DELETED :
        UI_SAVE_STATUS_ERROR;

    uiMenuPage = UI_MENU_PAGE_MANAGE_PRESETS;
    UI_DrawMenu();
}


/* Eine offene Loeschen-Rueckfrage zuruecknehmen. */
static void UI_ActionsCancelDelete(void)
{
    uiActionsDeletePending = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;

    UI_ActionsDrawRow(UI_ACTION_DELETE);
    UI_SaveDrawStatus();
}


static void UI_ActionsHandleEncoder(int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    /*
     * Eine offene Rueckfrage verfaellt beim Weiterdrehen.
     */
    if (uiActionsDeletePending)
    {
        UI_ActionsCancelDelete();
    }

    int16_t oldRow = uiActionsRow;
    int16_t newRow = oldRow + direction;

    if (newRow < 0)
    {
        newRow = 0;
    }

    if (newRow > (int16_t)(UI_ACTION_COUNT - 1U))
    {
        newRow = (int16_t)(UI_ACTION_COUNT - 1U);
    }

    if (newRow == oldRow)
    {
        return;
    }

    uiActionsRow = (uint8_t)newRow;

    UI_ActionsDrawRow((uint8_t)oldRow);
    UI_ActionsDrawRow((uint8_t)newRow);
}


static void UI_ActionsHandleEnter(void)
{
    switch (uiActionsRow)
    {
        case UI_ACTION_LOAD:
            UI_ActionLoad();
            break;

        case UI_ACTION_RENAME:
            UI_NameOpen(
                UI_NAME_MODE_RENAME_PRESET,
                uiActionsSlot,
                UI_MENU_PAGE_MANAGE_ACTIONS);
            break;

        case UI_ACTION_MIDI:
            UI_PmOpen(uiActionsSlot);
            break;

        case UI_ACTION_DELETE:
        default:
            if (!uiActionsDeletePending)
            {
                /*
                 * Erster Klick: Rueckfrage. Zweiter Klick: loeschen.
                 */
                uiActionsDeletePending = 1U;
                uiSaveStatus = UI_SAVE_STATUS_DELETE_CONFIRM;

                UI_ActionsDrawRow(UI_ACTION_DELETE);
                UI_SaveDrawStatus();
            }
            else
            {
                UI_ActionDelete();
            }
            break;
    }
}


static void UI_ActionsHandleReturn(void)
{
    if (uiActionsDeletePending)
    {
        UI_ActionsCancelDelete();
        return;
    }

    UI_ActionsBack();
}


static void UI_SaveEnsureSelectionVisible(void)
{
    int16_t firstRow = (int16_t)(UI_ListBankRow() + 1U);

    if (uiMenuSelectedRow < firstRow)
    {
        return;
    }

    int16_t preset = uiMenuSelectedRow - firstRow;
    int16_t visible = (int16_t)UI_ListVisiblePresets();

    if (preset < (int16_t)uiPresetScrollTop)
    {
        uiPresetScrollTop = (uint8_t)preset;
    }
    else if (preset >= (int16_t)uiPresetScrollTop + visible)
    {
        uiPresetScrollTop = (uint8_t)(preset - visible + 1);
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
        /*
         * Auswahl-Seite: leere Baenke werden uebersprungen (gibt es
         * keine belegte, bleibt die Bank).
         */
        uint8_t bank = uiPresetBank;

        for (uint8_t tries = 0U; tries < PRESET_BANK_COUNT; tries++)
        {
            if (direction > 0)
            {
                bank = (uint8_t)((bank + 1U) % PRESET_BANK_COUNT);
            }
            else
            {
                bank = (uint8_t)
                    ((bank + PRESET_BANK_COUNT - 1U) %
                     PRESET_BANK_COUNT);
            }

            if (!UI_ListIsSelect() ||
                UI_SelectUsedCount(bank) > 0U)
            {
                uiPresetBank = bank;
                break;
            }
        }

        if (UI_ListIsSelect())
        {
            uiPresetScrollTop = 0U;
        }

        uiSaveStatus = UI_SAVE_STATUS_NONE;

        UI_SaveDrawRow(UI_ListBankRow());
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

    if (newRow > UI_ListLastRow())
    {
        newRow = UI_ListLastRow();
    }

    if (newRow == oldRow)
    {
        return;
    }

    uiMenuSelectedRow = newRow;
    UI_SaveEnsureSelectionVisible();

    if (uiPresetScrollTop != oldTop)
    {
        UI_SaveDrawList();
    }
    else
    {
        UI_SaveDrawRow((uint8_t)oldRow);
        UI_SaveDrawRow((uint8_t)newRow);
    }

    /*
     * Eine Meldung vom letzten Vorgang verschwindet beim Weiterdrehen.
     */
    if (uiSaveStatus != UI_SAVE_STATUS_NONE)
    {
        uiSaveStatus = UI_SAVE_STATUS_NONE;
        UI_SaveDrawStatus();
    }
}


static void UI_SaveHandleEnter(void)
{
    uint8_t manage = UI_ListIsManage();
    uint8_t bankRow = UI_ListBankRow();

    if (UI_ListIsSelect() && uiMenuSelectedRow == 0)
    {
        /*
         * "Confirm": nur die markierten Presets mit umbauen.
         */
        uiPresetBank = uiPresetSelectBankBackup;
        uiPresetBankEditing = 0U;

        if (uiPresetSelectMerge)
        {
            UI_MergeSelectedItem(UI_PRESETS_SELECTED);
        }
        else
        {
            UI_ConvertSelectedLoop(UI_PRESETS_SELECTED);
        }

        return;
    }

    if (manage && uiMenuSelectedRow == 0)
    {
        /*
         * "Rename bank": Name der gewaehlten Bank editieren.
         */
        UI_NameOpen(
            UI_NAME_MODE_RENAME_BANK,
            0U,
            UI_MENU_PAGE_MANAGE_PRESETS);
        return;
    }

    if (uiMenuSelectedRow == (int16_t)bankRow)
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

        UI_SaveDrawRow(bankRow);
        UI_SaveDrawStatus();
        return;
    }

    int16_t slotOfRow = UI_ListSlotOfRow((uint8_t)uiMenuSelectedRow);

    if (slotOfRow < 0)
    {
        return;
    }

    uint8_t slot = (uint8_t)slotOfRow;

    if (UI_ListIsSelect())
    {
        /*
         * Auswahl-Seite: Haken setzen bzw. entfernen.
         */
        uiPresetSelection[uiPresetBank] ^= (1UL << slot);
        uiSaveStatus = UI_SAVE_STATUS_NONE;
        UI_SaveDrawRow((uint8_t)uiMenuSelectedRow);
        UI_SaveDrawStatus();
        return;
    }

    if (!manage)
    {
        /*
         * Save Preset: Untermenue mit Namenseditor oeffnen.
         */
        UI_NameOpen(
            UI_NAME_MODE_SAVE_PRESET,
            slot,
            UI_MENU_PAGE_SAVE_PRESET);
        return;
    }

    /*
     * Manage Presets: Aktionsmenue, aber nur fuer belegte Presets.
     */
    char name[PRESET_NAME_LENGTH];

    if (PresetStore_GetSlotName(uiPresetBank, slot, name))
    {
        UI_ActionsOpen(slot);
    }
    else
    {
        uiSaveStatus = UI_SAVE_STATUS_EMPTY_SLOT;
        UI_SaveDrawStatus();
    }
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

        UI_SaveDrawRow(UI_ListBankRow());
        UI_SaveDrawList();
        return;
    }

    /*
     * Auswahl-Seite: zurueck zur Abfrage "... presets too?" (Zeile
     * "Select"), ohne etwas umzubauen.
     */
    if (UI_ListIsSelect())
    {
        uiPresetBank = uiPresetSelectBankBackup;
        uiMenuPage = uiPresetSelectMerge ?
            UI_MENU_PAGE_MERGE_CONFIRM :
            UI_MENU_PAGE_CONVERT_CONFIRM;
        uiMenuSelectedRow = UI_MENU_CONVERT_SELECT_ROW;

        UI_Draw();
        UI_DrawMenu();
        return;
    }

    /*
     * Zurueck zum General Menu: erst den Statusbildschirm darunter
     * neu zeichnen, dann das Overlay. Der Cursor steht auf dem
     * Eintrag, aus dem man kam.
     */
    uiMenuSelectedRow = UI_ListIsManage() ? 1 : 0;
    uiMenuPage = UI_MENU_PAGE_GENERAL;

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


/*
 * Oeffnet Manage Presets. Der Cursor steht auf der Bank-Zeile, also
 * auf der zweiten Zeile unter "Rename bank".
 */
static void UI_MenuManagePresets(void)
{
    if (uiPresetBankEditing)
    {
        uiPresetBank = uiPresetBankBackup;
    }

    uiPresetBankEditing = 0U;
    uiNameEditing = 0U;
    uiActionsDeletePending = 0U;
    uiSaveStatus = UI_SAVE_STATUS_NONE;
    uiPresetScrollTop = 0U;

    uiMenuPage = UI_MENU_PAGE_MANAGE_PRESETS;
    uiMenuSelectedRow = 1;

    UI_DrawMenu();
}


/*
 * "Select" bei "Convert presets too?" (merge = 0) bzw. "Merge presets
 * too?" (merge = 1): Auswahl-Seite mit leerer Auswahl oeffnen.
 */
static void UI_PresetSelectOpen(uint8_t merge)
{
    memset(uiPresetSelection, 0, sizeof(uiPresetSelection));

    uiPresetSelectMerge = merge;
    uiPresetSelectBankBackup = uiPresetBank;
    uiPresetBankEditing = 0U;

    /*
     * Leere Baenke werden nicht gezeigt: ab der aktuellen Bank die
     * erste mit belegten Presets waehlen.
     */
    for (uint8_t tries = 0U; tries < PRESET_BANK_COUNT; tries++)
    {
        uint8_t bank = (uint8_t)
            ((uiPresetSelectBankBackup + tries) % PRESET_BANK_COUNT);

        if (UI_SelectUsedCount(bank) > 0U)
        {
            uiPresetBank = bank;
            break;
        }
    }
    uiSaveStatus = UI_SAVE_STATUS_NONE;
    uiPresetScrollTop = 0U;

    uiMenuPage = UI_MENU_PAGE_PRESET_SELECT;
    uiMenuSelectedRow = 1;

    UI_DrawMenu();
}


/* -------------------------------------------------------------------------- */
/* Loop-Menue "Midi": Konstanten und Zustand                                  */
/* -------------------------------------------------------------------------- */

/*
 * Zwei ganzseitige Listen im Stil der Preset-Liste:
 *
 *   Midi-Seite:        Midi Channel: 1
 *                      Program Change: Off
 *                      On/Bypass 1
 *                      ...
 *                      add On/Bypass
 *
 *   On/Bypass-Seite:   On: CC01, 127      (Schalter mit On und Off)
 *                      Off: CC01, 0
 *                      Toggle: CC02, 64   (On und Off gleich)
 *                      empty              (naechste Meldung)
 *
 *   y   2          Kopfzeile "Loop 01 Midi" bzw. "Loop 01 On/Bypass 2"
 *   y  16 .. 111   8 sichtbare Zeilen (scrollen)
 *   y 114          Trennlinie
 *   y 118          Statuszeile
 *
 * Bearbeitet wird eine Kopie der Konfiguration. Sie wird beim Verlassen
 * der Midi-Seite (Return oder Menue-Taste) gespeichert.
 */
#define UI_LM_VISIBLE_ROWS            UI_SAVE_VISIBLE_PRESETS
#define UI_LM_MAX_GROUP_ROWS          (LOOP_MIDI_MAX_MESSAGES + 1U)

/* Zeilen der Midi-Seite, danach die Gruppen und "add On/Bypass". */
#define UI_LM_CHANNEL_ROW             0
#define UI_LM_PROGRAM_ROW             1
#define UI_LM_FIRST_GROUP_ROW         2

typedef enum
{
    UI_LM_EDIT_NONE = 0,
    UI_LM_EDIT_CHANNEL,
    UI_LM_EDIT_PROGRAM,
    UI_LM_EDIT_CONTROLLER,
    UI_LM_EDIT_VALUE

} UI_LmEdit;

typedef enum
{
    UI_LM_ROW_ON = 0,
    UI_LM_ROW_OFF,
    UI_LM_ROW_TOGGLE,
    UI_LM_ROW_EMPTY

} UI_LmRowKind;

typedef struct
{
    uint8_t kind;       /* UI_LmRowKind */
    uint8_t message;    /* Index der (ersten) Meldung in der Gruppe */

} UI_LmRow;

typedef enum
{
    UI_LM_STATUS_NONE = 0,
    UI_LM_STATUS_FULL,
    UI_LM_STATUS_WRITE_ERROR

} UI_LmStatus;

static LoopMidiConfig uiLmConfig;
static int16_t uiLmItemIndex = -1;

/* Bearbeitete Gruppe; == groupCount bei einer neuen Gruppe. */
static uint8_t uiLmGroup = 0U;

static int16_t uiLmRow = 0;
static uint8_t uiLmScrollTop = 0U;

static UI_LmEdit uiLmEdit = UI_LM_EDIT_NONE;
static uint8_t uiLmChannelBackup = 0U;
static uint8_t uiLmProgramBackup = 0U;

/* Werte waehrend der Eingabe einer Meldung, Controller -1 = empty. */
static int16_t uiLmEditController = -1;
static uint8_t uiLmEditValue = 0U;

static uint8_t uiLmDirty = 0U;
static UI_LmStatus uiLmStatus = UI_LM_STATUS_NONE;

/* Zeilen der On/Bypass-Seite, aus den Meldungen der Gruppe gebaut. */
static UI_LmRow uiLmRows[UI_LM_MAX_GROUP_ROWS];
static uint8_t uiLmRowCount = 0U;


static uint8_t UI_MenuItemIsLoop(void)
{
    return
        (uiMenuItemIndex >= 0 &&
         uiMenuItemIndex < (int16_t)UI_ITEM_COUNT &&
         uiItems[uiMenuItemIndex].type == UI_ITEM_LOOP &&
         uiItems[uiMenuItemIndex].id <= 255U) ?
        1U :
        0U;
}


static uint8_t UI_LmIsGroupPage(void)
{
    return (uiMenuPage == UI_MENU_PAGE_LOOP_MIDI_GROUP) ? 1U : 0U;
}


static uint8_t UI_LmIsLearning(void)
{
    return (uiMode == UI_MODE_MENU &&
            (uiMenuPage == UI_MENU_PAGE_LOOP_MIDI ||
             uiMenuPage == UI_MENU_PAGE_LOOP_MIDI_GROUP) &&
            uiLmEdit != UI_LM_EDIT_NONE &&
            uiLmEdit != UI_LM_EDIT_PROGRAM) ? 1U : 0U;
}


static LoopMidiGroup *UI_LmCurrentGroup(void)
{
    return &uiLmConfig.groups[uiLmGroup];
}


/*
 * Baut die Zeilen der On/Bypass-Seite: je Schalter On und Off bzw. eine
 * Toggle-Zeile, eine noch fehlende Off-Meldung und die naechste freie
 * Meldung erscheinen als "empty".
 */
static void UI_LmBuildGroupRows(void)
{
    const LoopMidiGroup *group = UI_LmCurrentGroup();
    uint8_t count = 0U;

    for (uint8_t m = 0U; m < group->messageCount; m = (uint8_t)(m + 2U))
    {
        uint8_t k = (uint8_t)(m / 2U);

        if (LoopMidi_SwitchIsToggle(group, k))
        {
            uiLmRows[count].kind = UI_LM_ROW_TOGGLE;
            uiLmRows[count++].message = m;
            continue;
        }

        uiLmRows[count].kind = UI_LM_ROW_ON;
        uiLmRows[count++].message = m;

        if ((uint8_t)(m + 1U) < group->messageCount)
        {
            uiLmRows[count].kind = UI_LM_ROW_OFF;
            uiLmRows[count++].message = (uint8_t)(m + 1U);
        }
    }

    if (group->messageCount < LOOP_MIDI_MAX_MESSAGES)
    {
        uiLmRows[count].kind = UI_LM_ROW_EMPTY;
        uiLmRows[count++].message = group->messageCount;
    }

    uiLmRowCount = count;
}


static uint8_t UI_LmRowCount(void)
{
    if (UI_LmIsGroupPage())
    {
        return uiLmRowCount;
    }

    /* Kanal, Program Change, Gruppen, "add On/Bypass" */
    return (uint8_t)((uint8_t)UI_LM_FIRST_GROUP_ROW +
        uiLmConfig.groupCount +
        ((uiLmConfig.groupCount < LOOP_MIDI_MAX_GROUPS) ? 1U : 0U));
}


static int16_t UI_LmFindEmptyRow(void)
{
    for (uint8_t i = 0U; i < uiLmRowCount; i++)
    {
        if (uiLmRows[i].kind == UI_LM_ROW_EMPTY)
        {
            return (int16_t)i;
        }
    }

    return -1;
}


/* "CC01, 127"; markiert das gerade bearbeitete Feld mit Klammern. */
static uint8_t UI_LmAppendMessage(
    char *text,
    uint8_t position,
    int16_t controller,
    uint8_t value,
    UI_LmEdit edit)
{
    if (edit == UI_LM_EDIT_CONTROLLER)
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, "(");
    }

    if (controller < 0)
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, "empty");
    }
    else
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, "CC");
        position = UI_SaveAppendUInt2(
            text, position, UI_SAVE_TEXT_CAPACITY, (uint8_t)controller);
    }

    if (edit == UI_LM_EDIT_CONTROLLER)
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, ")");
    }

    if (controller < 0)
    {
        return position;
    }

    position = UI_SaveAppendText(
        text, position, UI_SAVE_TEXT_CAPACITY,
        (edit == UI_LM_EDIT_VALUE) ? ", (" : ", ");
    position = UI_SaveAppendUInt(
        text, position, UI_SAVE_TEXT_CAPACITY, value);

    if (edit == UI_LM_EDIT_VALUE)
    {
        position = UI_SaveAppendText(
            text, position, UI_SAVE_TEXT_CAPACITY, ")");
    }

    return position;
}


static void UI_LmBuildRowText(
    uint8_t row,
    char *text)
{
    uint8_t position = 0U;

    text[0] = '\0';

    if (!UI_LmIsGroupPage())
    {
        if (row == UI_LM_CHANNEL_ROW)
        {
            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "Midi Channel: ");
            UI_SaveAppendUInt(
                text, position, UI_SAVE_TEXT_CAPACITY,
                (uint8_t)(uiLmConfig.channel + 1U));
        }
        else if (row == UI_LM_PROGRAM_ROW)
        {
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY,
                uiLmConfig.programChange ?
                    "Program Change: On" :
                    "Program Change: Off");
        }
        else if (row < (uint8_t)UI_LM_FIRST_GROUP_ROW +
                       uiLmConfig.groupCount)
        {
            position = UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "On/Bypass ");
            UI_SaveAppendUInt(
                text, position, UI_SAVE_TEXT_CAPACITY,
                (uint8_t)(row - (uint8_t)UI_LM_FIRST_GROUP_ROW + 1U));
        }
        else
        {
            UI_SaveAppendText(
                text, 0U, UI_SAVE_TEXT_CAPACITY, "add On/Bypass");
        }

        return;
    }

    if (row >= uiLmRowCount)
    {
        return;
    }

    const UI_LmRow *entry = &uiLmRows[row];
    const LoopMidiGroup *group = UI_LmCurrentGroup();
    uint8_t editing =
        ((int16_t)row == uiLmRow &&
         uiLmEdit != UI_LM_EDIT_NONE) ? 1U : 0U;

    if (entry->kind == UI_LM_ROW_EMPTY && !editing)
    {
        UI_SaveAppendText(text, 0U, UI_SAVE_TEXT_CAPACITY, "empty");
        return;
    }

    const char *label = "On: ";

    if (entry->kind == UI_LM_ROW_OFF ||
        (entry->kind == UI_LM_ROW_EMPTY &&
         (entry->message & 1U) != 0U))
    {
        label = "Off: ";
    }
    else if (entry->kind == UI_LM_ROW_TOGGLE)
    {
        label = "Toggle: ";
    }

    position = UI_SaveAppendText(
        text, 0U, UI_SAVE_TEXT_CAPACITY, label);

    if (editing)
    {
        UI_LmAppendMessage(
            text, position,
            uiLmEditController, uiLmEditValue, uiLmEdit);
    }
    else
    {
        const LoopMidiMessage *message =
            &group->messages[entry->message];

        UI_LmAppendMessage(
            text, position,
            message->controller, message->value, UI_LM_EDIT_NONE);
    }
}


/* -------------------------------------------------------------------------- */
/* Loop-Menue "Midi": Zeichnen                                                */
/* -------------------------------------------------------------------------- */

static void UI_LmDrawRow(uint8_t row)
{
    int16_t position = (int16_t)row - (int16_t)uiLmScrollTop;

    if (position < 0 ||
        position >= (int16_t)UI_LM_VISIBLE_ROWS)
    {
        return;
    }

    uint16_t y = (uint16_t)(UI_SAVE_LIST_Y +
        ((uint16_t)position * UI_SAVE_ROW_HEIGHT));

    uint16_t background = UI_COLOR_BACKGROUND;
    char text[UI_SAVE_TEXT_CAPACITY];

    if (row < UI_LmRowCount())
    {
        UI_LmBuildRowText(row, text);

        if ((int16_t)row == uiLmRow)
        {
            background =
                (uiLmEdit != UI_LM_EDIT_NONE) ?
                UI_COLOR_BORDER_GRABBED :
                UI_COLOR_BORDER_SELECTED;
        }
    }
    else
    {
        text[0] = '\0';
    }

    UI_SaveDrawTextLine(
        (uint8_t)(position + 1),
        y,
        UI_SAVE_ROW_WIDTH,
        UI_SAVE_ROW_HEIGHT,
        y + UI_SAVE_TEXT_Y_OFFSET,
        text,
        UI_COLOR_TEXT_LIGHT,
        background
    );
}


/*
 * Bildlaufleiste einer ganzseitigen Liste mit UI_SAVE_VISIBLE_PRESETS
 * sichtbaren Zeilen (Loop-Midi, Preset-Midi). Ohne Scrollen bleibt sie
 * leer.
 */
static void UI_ListDrawScrollbar(
    uint16_t count,
    uint8_t scrollTop)
{
    uint16_t trackHeight =
        (uint16_t)(UI_LM_VISIBLE_ROWS * UI_SAVE_ROW_HEIGHT);

    if (count <= UI_LM_VISIBLE_ROWS)
    {
        ST7735_FillRect(
            UI_SAVE_SCROLLBAR_X,
            UI_SAVE_LIST_Y,
            UI_SAVE_SCROLLBAR_WIDTH,
            trackHeight,
            UI_COLOR_BACKGROUND
        );
        return;
    }

    uint16_t thumbHeight =
        (uint16_t)((trackHeight * UI_LM_VISIBLE_ROWS) / count);

    uint16_t thumbY =
        (uint16_t)(UI_SAVE_LIST_Y +
            (((uint32_t)trackHeight * scrollTop) / count));

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


static void UI_LmDrawList(void)
{
    for (uint8_t i = 0U; i < UI_LM_VISIBLE_ROWS; i++)
    {
        UI_LmDrawRow((uint8_t)(uiLmScrollTop + i));
    }

    UI_ListDrawScrollbar(UI_LmRowCount(), uiLmScrollTop);
}


static void UI_LmDrawStatus(void)
{
    char text[UI_SAVE_TEXT_CAPACITY];
    uint16_t color = UI_COLOR_TEXT_LIGHT;
    uint8_t position = 0U;

    text[0] = '\0';

    if (uiLmStatus == UI_LM_STATUS_FULL)
    {
        color = UI_COLOR_LOOP_UNCONFIRMED;
        UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Memory full");
    }
    else if (uiLmStatus == UI_LM_STATUS_WRITE_ERROR)
    {
        color = UI_COLOR_LOOP_UNCONFIRMED;
        UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Write failed");
    }
    else if (uiLmEdit == UI_LM_EDIT_CHANNEL)
    {
        UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Turn or send CC");
    }
    else if (uiLmEdit == UI_LM_EDIT_PROGRAM)
    {
        UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Turn: On / Off");
    }
    else if (uiLmEdit != UI_LM_EDIT_NONE)
    {
        position = UI_SaveAppendText(
            text, 0U, UI_SAVE_TEXT_CAPACITY, "Turn or send CC Ch ");
        UI_SaveAppendUInt(
            text, position, UI_SAVE_TEXT_CAPACITY,
            (uint8_t)(uiLmConfig.channel + 1U));
    }

    UI_SaveDrawTextLine(
        UI_SAVE_SHADOW_STATUS_SLOT,
        UI_SAVE_SEPARATOR_Y + 1U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT - (UI_SAVE_SEPARATOR_Y + 1U),
        UI_SAVE_STATUS_Y,
        text,
        color,
        UI_COLOR_BACKGROUND
    );
}


static void UI_LmDrawPage(void)
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

    UI_SaveShadowInvalidate();

    if (uiLmItemIndex >= 0)
    {
        position = UI_SaveAppendText(
            header, 0U, UI_SAVE_TEXT_CAPACITY,
            uiItems[uiLmItemIndex].longName);
    }

    if (UI_LmIsGroupPage())
    {
        position = UI_SaveAppendText(
            header, position, UI_SAVE_TEXT_CAPACITY, " On/Bypass ");
        UI_SaveAppendUInt(
            header, position, UI_SAVE_TEXT_CAPACITY,
            (uint8_t)(uiLmGroup + 1U));
    }
    else
    {
        UI_SaveAppendText(
            header, position, UI_SAVE_TEXT_CAPACITY, " Midi");
    }

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        UI_SAVE_HEADER_Y,
        header,
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND,
        1U
    );

    UI_LmDrawList();

    ST7735_DrawLine(
        0U,
        UI_SAVE_SEPARATOR_Y,
        UI_DISPLAY_WIDTH - 1U,
        UI_SAVE_SEPARATOR_Y,
        UI_COLOR_BORDER_NORMAL
    );

    UI_LmDrawStatus();
}


/*
 * Setzt die Auswahl und scrollt bei Bedarf. Gezeichnet werden nur die
 * alte und die neue Zeile bzw. beim Scrollen die Liste (der Zeichen-
 * Cache gibt dann nur geaenderte Zeichen aus).
 */
static void UI_LmSelectRow(int16_t row)
{
    int16_t count = (int16_t)UI_LmRowCount();

    if (row >= count)
    {
        row = (int16_t)(count - 1);
    }

    if (row < 0)
    {
        row = 0;
    }

    int16_t oldRow = uiLmRow;
    uint8_t oldScrollTop = uiLmScrollTop;

    uiLmRow = row;

    if (row < (int16_t)uiLmScrollTop)
    {
        uiLmScrollTop = (uint8_t)row;
    }
    else if (row >= (int16_t)(uiLmScrollTop + UI_LM_VISIBLE_ROWS))
    {
        uiLmScrollTop = (uint8_t)(row - (int16_t)UI_LM_VISIBLE_ROWS + 1);
    }

    if (count <= (int16_t)UI_LM_VISIBLE_ROWS)
    {
        uiLmScrollTop = 0U;
    }
    else if ((int16_t)uiLmScrollTop >
             (int16_t)(count - (int16_t)UI_LM_VISIBLE_ROWS))
    {
        uiLmScrollTop = (uint8_t)(count - (int16_t)UI_LM_VISIBLE_ROWS);
    }

    if (uiLmScrollTop != oldScrollTop)
    {
        UI_LmDrawList();
        return;
    }

    if (oldRow >= 0 && oldRow != row)
    {
        UI_LmDrawRow((uint8_t)oldRow);
    }

    UI_LmDrawRow((uint8_t)row);
}


/* Nach einer Aenderung der Zeilen: Liste und Status neu zeichnen. */
static void UI_LmRefresh(int16_t row)
{
    uiLmRow = -1;
    UI_LmSelectRow(row);
    UI_LmDrawList();
    UI_LmDrawStatus();
}


/* -------------------------------------------------------------------------- */
/* Loop-Menue "Midi": Bearbeiten                                              */
/* -------------------------------------------------------------------------- */

/* Beginnt die Eingabe der Meldung in der ausgewaehlten Zeile. */
static void UI_LmStartMessageEdit(void)
{
    if (uiLmRow < 0 || uiLmRow >= (int16_t)uiLmRowCount)
    {
        return;
    }

    const UI_LmRow *entry = &uiLmRows[uiLmRow];
    const LoopMidiGroup *group = UI_LmCurrentGroup();

    if (entry->kind == UI_LM_ROW_EMPTY)
    {
        if ((entry->message & 1U) != 0U)
        {
            /*
             * Off: gleicher Controller wie On, anderer Wert. Fuer einen
             * Toggle stellt man denselben Wert ein (oder sendet dieselbe
             * Meldung noch einmal).
             */
            const LoopMidiMessage *on =
                &group->messages[entry->message - 1U];

            uiLmEditController = on->controller;
            uiLmEditValue = (on->value == 0U) ? 127U : 0U;
        }
        else
        {
            uiLmEditController = -1;
            uiLmEditValue = 127U;
        }
    }
    else
    {
        uiLmEditController =
            group->messages[entry->message].controller;
        uiLmEditValue = group->messages[entry->message].value;
    }

    uiLmEdit = UI_LM_EDIT_CONTROLLER;
    uiLmStatus = UI_LM_STATUS_NONE;

    UI_LmDrawRow((uint8_t)uiLmRow);
    UI_LmDrawStatus();
}


static void UI_LmEndEdit(void)
{
    uiLmEdit = UI_LM_EDIT_NONE;

    if (uiLmRow >= 0)
    {
        UI_LmDrawRow((uint8_t)uiLmRow);
    }

    UI_LmDrawStatus();
}


/*
 * Prueft, ob die Arbeitskopie (mit einer gerade neu angelegten Gruppe)
 * noch in den EEPROM-Block passt.
 */
static uint8_t UI_LmConfigFits(void)
{
    if (uiLmGroup < uiLmConfig.groupCount)
    {
        return LoopMidi_ConfigFits(&uiLmConfig);
    }

    LoopMidiConfig candidate = uiLmConfig;

    candidate.groupCount = (uint8_t)(uiLmGroup + 1U);

    return LoopMidi_ConfigFits(&candidate);
}


/* Loescht den Schalter der ausgewaehlten Zeile (On und Off). */
static void UI_LmDeleteSwitch(void)
{
    LoopMidiGroup *group = UI_LmCurrentGroup();
    const UI_LmRow *entry = &uiLmRows[uiLmRow];

    uint8_t first = (uint8_t)(entry->message & ~1U);
    uint8_t removed =
        ((uint8_t)(first + 1U) < group->messageCount) ? 2U : 1U;

    for (uint8_t m = first;
         (uint8_t)(m + removed) < group->messageCount;
         m++)
    {
        group->messages[m] = group->messages[m + removed];
    }

    group->messageCount = (uint8_t)(group->messageCount - removed);
    uiLmDirty = 1U;
    uiLmEdit = UI_LM_EDIT_NONE;

    int16_t row = uiLmRow;

    UI_LmBuildGroupRows();
    UI_LmRefresh(row);
}


/*
 * Uebernimmt die eingegebene oder empfangene Meldung in die Zeile.
 *
 * Eine neue Meldung wird angehaengt: die erste eines Schalters ist On,
 * die zweite Off (gleich der ersten: Toggle). Nach einer per MIDI
 * gelernten neuen Meldung wartet die naechste "empty"-Zeile gleich auf
 * die naechste Meldung, so koennen alle Schalter des Effektgeraets der
 * Reihe nach gedrueckt werden.
 */
static void UI_LmCommitMessage(
    uint8_t controller,
    uint8_t value,
    uint8_t learned)
{
    LoopMidiGroup *group = UI_LmCurrentGroup();
    LoopMidiGroup backup = *group;
    const UI_LmRow *entry = &uiLmRows[uiLmRow];
    uint8_t appended = 0U;

    LoopMidiMessage message;

    message.controller = controller;
    message.value = value;

    if (entry->kind == UI_LM_ROW_EMPTY)
    {
        if (group->messageCount >= LOOP_MIDI_MAX_MESSAGES)
        {
            UI_LmEndEdit();
            return;
        }

        group->messages[group->messageCount++] = message;
        appended = 1U;
    }
    else if (entry->kind == UI_LM_ROW_TOGGLE)
    {
        group->messages[entry->message] = message;
        group->messages[entry->message + 1U] = message;
    }
    else
    {
        group->messages[entry->message] = message;
    }

    if (!UI_LmConfigFits())
    {
        *group = backup;
        uiLmStatus = UI_LM_STATUS_FULL;
        UI_LmEndEdit();
        return;
    }

    uiLmDirty = 1U;
    uiLmStatus = UI_LM_STATUS_NONE;
    uiLmEdit = UI_LM_EDIT_NONE;

    uint8_t messageIndex = entry->message;

    UI_LmBuildGroupRows();

    int16_t row = 0;

    if (appended)
    {
        row = UI_LmFindEmptyRow();

        if (row < 0)
        {
            row = (int16_t)(uiLmRowCount - 1U);
        }
    }
    else
    {
        for (uint8_t i = 0U; i < uiLmRowCount; i++)
        {
            if (uiLmRows[i].message == messageIndex ||
                (uiLmRows[i].kind == UI_LM_ROW_TOGGLE &&
                 (uint8_t)(uiLmRows[i].message + 1U) == messageIndex))
            {
                row = (int16_t)i;
                break;
            }
        }
    }

    UI_LmRefresh(row);

    if (learned && appended &&
        uiLmRows[uiLmRow].kind == UI_LM_ROW_EMPTY)
    {
        UI_LmStartMessageEdit();
    }
}


/*
 * Verlassen der On/Bypass-Seite: eine neue Gruppe mit Meldungen wird
 * angelegt, eine Gruppe ohne Meldungen geloescht. Eine einzelne Meldung
 * ohne Off wird zum Toggle.
 */
static void UI_LmCloseGroupData(void)
{
    LoopMidiGroup *group = UI_LmCurrentGroup();

    uiLmEdit = UI_LM_EDIT_NONE;

    if ((group->messageCount & 1U) != 0U)
    {
        group->messages[group->messageCount] =
            group->messages[group->messageCount - 1U];
        group->messageCount++;
        uiLmDirty = 1U;
    }

    if (uiLmGroup >= uiLmConfig.groupCount)
    {
        if (group->messageCount > 0U)
        {
            uiLmConfig.groupCount = (uint8_t)(uiLmGroup + 1U);
        }

        return;
    }

    if (group->messageCount > 0U)
    {
        return;
    }

    for (uint8_t g = uiLmGroup;
         (uint8_t)(g + 1U) < uiLmConfig.groupCount;
         g++)
    {
        uiLmConfig.groups[g] = uiLmConfig.groups[g + 1U];
    }

    uiLmConfig.groupCount--;
    uiLmConfig.groups[uiLmConfig.groupCount].messageCount = 0U;
    uiLmDirty = 1U;
}


/*
 * Beendet das Midi-Menue des Loops: offene Eingaben verwerfen, die
 * aktuelle Gruppe abschliessen und eine geaenderte Konfiguration
 * speichern.
 */
static LoopMidiStatus UI_LmFinish(void)
{
    if (uiLmEdit == UI_LM_EDIT_CHANNEL)
    {
        uiLmConfig.channel = uiLmChannelBackup;
    }
    else if (uiLmEdit == UI_LM_EDIT_PROGRAM)
    {
        uiLmConfig.programChange = uiLmProgramBackup;
    }

    if (UI_LmIsGroupPage())
    {
        UI_LmCloseGroupData();
    }

    uiLmEdit = UI_LM_EDIT_NONE;

    if (!uiLmDirty)
    {
        return LOOP_MIDI_OK;
    }

    uiLmDirty = 0U;

    return LoopMidi_StoreConfig(&uiLmConfig);
}


static void UI_LmOpen(void)
{
    if (!UI_MenuItemIsLoop())
    {
        return;
    }

    uiLmItemIndex = uiMenuItemIndex;

    LoopMidi_GetConfig(
        (uint8_t)uiItems[uiLmItemIndex].id,
        &uiLmConfig);

    uiLmEdit = UI_LM_EDIT_NONE;
    uiLmDirty = 0U;
    uiLmStatus = UI_LM_STATUS_NONE;
    uiLmRow = UI_LM_CHANNEL_ROW;
    uiLmScrollTop = 0U;

    uiMenuPage = UI_MENU_PAGE_LOOP_MIDI;
    UI_DrawMenu();
}


static void UI_LmOpenGroup(uint8_t group)
{
    uiLmGroup = group;

    if (uiLmGroup >= uiLmConfig.groupCount)
    {
        uiLmConfig.groups[uiLmGroup].messageCount = 0U;
    }

    UI_LmBuildGroupRows();

    uiLmEdit = UI_LM_EDIT_NONE;
    uiLmStatus = UI_LM_STATUS_NONE;
    uiLmRow = 0;
    uiLmScrollTop = 0U;

    uiMenuPage = UI_MENU_PAGE_LOOP_MIDI_GROUP;
    UI_DrawMenu();
}


static void UI_LmBackToMain(void)
{
    UI_LmCloseGroupData();

    uint8_t group = uiLmGroup;

    uiMenuPage = UI_MENU_PAGE_LOOP_MIDI;
    uiLmStatus = UI_LM_STATUS_NONE;
    uiLmScrollTop = 0U;

    /* Cursor auf die Gruppe bzw. auf "add On/Bypass". */
    uiLmRow =
        (group < uiLmConfig.groupCount) ?
        (int16_t)(group + (uint8_t)UI_LM_FIRST_GROUP_ROW) :
        (int16_t)(UI_LmRowCount() - 1U);

    if (uiLmRow >= (int16_t)UI_LM_VISIBLE_ROWS)
    {
        uiLmScrollTop =
            (uint8_t)(uiLmRow - (int16_t)UI_LM_VISIBLE_ROWS + 1);
    }

    UI_DrawMenu();
}


/* -------------------------------------------------------------------------- */
/* Loop-Menue "Midi": Encoder, Enter, Return, MIDI Learn                      */
/* -------------------------------------------------------------------------- */

static int16_t UI_LmClamp(
    int16_t value,
    int16_t minimum,
    int16_t maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}


static void UI_LmHandleEncoder(int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    switch (uiLmEdit)
    {
        case UI_LM_EDIT_CHANNEL:
            uiLmConfig.channel = (uint8_t)UI_LmClamp(
                (int16_t)uiLmConfig.channel + direction, 0, 15);
            UI_LmDrawRow((uint8_t)uiLmRow);
            UI_LmDrawStatus();
            return;

        case UI_LM_EDIT_PROGRAM:
            uiLmConfig.programChange = (direction > 0) ? 1U : 0U;
            UI_LmDrawRow((uint8_t)uiLmRow);
            return;

        case UI_LM_EDIT_CONTROLLER:
            uiLmEditController = UI_LmClamp(
                (int16_t)(uiLmEditController + direction), -1, 127);
            UI_LmDrawRow((uint8_t)uiLmRow);
            return;

        case UI_LM_EDIT_VALUE:
            uiLmEditValue = (uint8_t)UI_LmClamp(
                (int16_t)uiLmEditValue + direction, 0, 127);
            UI_LmDrawRow((uint8_t)uiLmRow);
            return;

        case UI_LM_EDIT_NONE:
        default:
            break;
    }

    if (uiLmStatus != UI_LM_STATUS_NONE)
    {
        uiLmStatus = UI_LM_STATUS_NONE;
        UI_LmDrawStatus();
    }

    UI_LmSelectRow((int16_t)(uiLmRow + direction));
}


static void UI_LmHandleEnter(void)
{
    if (!UI_LmIsGroupPage())
    {
        if (uiLmEdit == UI_LM_EDIT_CHANNEL)
        {
            if (uiLmConfig.channel != uiLmChannelBackup)
            {
                uiLmDirty = 1U;
            }

            UI_LmEndEdit();
            return;
        }

        if (uiLmEdit == UI_LM_EDIT_PROGRAM)
        {
            if (uiLmConfig.programChange != uiLmProgramBackup)
            {
                uiLmDirty = 1U;
            }

            UI_LmEndEdit();
            return;
        }

        if (uiLmRow == UI_LM_CHANNEL_ROW)
        {
            uiLmChannelBackup = uiLmConfig.channel;
            uiLmEdit = UI_LM_EDIT_CHANNEL;
            uiLmStatus = UI_LM_STATUS_NONE;
            UI_LmDrawRow((uint8_t)uiLmRow);
            UI_LmDrawStatus();
            return;
        }

        if (uiLmRow == UI_LM_PROGRAM_ROW)
        {
            uiLmProgramBackup = uiLmConfig.programChange;
            uiLmEdit = UI_LM_EDIT_PROGRAM;
            uiLmStatus = UI_LM_STATUS_NONE;
            UI_LmDrawRow((uint8_t)uiLmRow);
            UI_LmDrawStatus();
            return;
        }

        if (uiLmRow >= UI_LM_FIRST_GROUP_ROW)
        {
            UI_LmOpenGroup((uint8_t)(uiLmRow - UI_LM_FIRST_GROUP_ROW));
        }

        return;
    }

    switch (uiLmEdit)
    {
        case UI_LM_EDIT_NONE:
            UI_LmStartMessageEdit();
            return;

        case UI_LM_EDIT_CONTROLLER:
            if (uiLmEditController >= 0)
            {
                uiLmEdit = UI_LM_EDIT_VALUE;
                UI_LmDrawRow((uint8_t)uiLmRow);
                return;
            }

            /*
             * "empty" gewaehlt: eine bestehende Zeile loescht ihren
             * Schalter, die freie Zeile bleibt frei.
             */
            if (uiLmRows[uiLmRow].kind != UI_LM_ROW_EMPTY)
            {
                UI_LmDeleteSwitch();
                UI_LmDrawStatus();
                return;
            }

            UI_LmEndEdit();
            return;

        case UI_LM_EDIT_VALUE:
            UI_LmCommitMessage(
                (uint8_t)uiLmEditController, uiLmEditValue, 0U);
            return;

        case UI_LM_EDIT_CHANNEL:
        case UI_LM_EDIT_PROGRAM:
        default:
            return;
    }
}


static void UI_LmHandleReturn(void)
{
    if (uiLmEdit == UI_LM_EDIT_CHANNEL)
    {
        uiLmConfig.channel = uiLmChannelBackup;
        UI_LmEndEdit();
        return;
    }

    if (uiLmEdit == UI_LM_EDIT_PROGRAM)
    {
        uiLmConfig.programChange = uiLmProgramBackup;
        UI_LmEndEdit();
        return;
    }

    if (uiLmEdit != UI_LM_EDIT_NONE)
    {
        UI_LmEndEdit();
        return;
    }

    if (UI_LmIsGroupPage())
    {
        UI_LmBackToMain();
        return;
    }

    LoopMidiStatus result = UI_LmFinish();

    if (result == LOOP_MIDI_ERR_WRITE)
    {
        /*
         * Die Einstellungen gelten bis zum Ausschalten. Der Fehler
         * bleibt stehen, das naechste Return verlaesst die Seite.
         */
        uiLmStatus = UI_LM_STATUS_WRITE_ERROR;
        UI_LmDrawStatus();
        return;
    }

    /*
     * Zurueck zum Menue des Loops, Cursor auf "Midi".
     */
    uiMenuPage = UI_MENU_PAGE_ROOT;
    uiMenuSelectedRow = UI_MENU_LOOP_MIDI_ROW;

    UI_Draw();
    UI_DrawMenu();
}


/*
 * MIDI Learn: Kanal-Zeile im Bearbeiten-Modus uebernimmt den Kanal der
 * Meldung, eine Meldungs-Zeile die Meldung (nur auf dem Kanal des Loops).
 */
static void UI_LmLearn(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    if (uiLmEdit == UI_LM_EDIT_CHANNEL)
    {
        uiLmConfig.channel = (uint8_t)(channel & 0x0FU);

        if (uiLmConfig.channel != uiLmChannelBackup)
        {
            uiLmDirty = 1U;
        }

        UI_LmEndEdit();
        return;
    }

    if (!UI_LmIsGroupPage() ||
        channel != uiLmConfig.channel ||
        controller > 127U ||
        value > 127U)
    {
        return;
    }

    UI_LmCommitMessage(controller, value, 1U);
}


/* -------------------------------------------------------------------------- */
/* MIDI-Steuerung der Loops (Aufruf aus midi_presets.c)                       */
/* -------------------------------------------------------------------------- */

uint8_t UI_LoopMidiClaims(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    if (UI_LmIsLearning() || UI_PmIsLearning())
    {
        return 1U;
    }

    return LoopMidi_IsAssigned(channel, controller, value);
}


/*
 * Eine Meldung eines Effektgeraets schaltet die Loops, zu denen sie
 * gehoert. Das geschieht auch bei offenem Menue (ohne Neuzeichnen, die
 * Statusansicht wird beim Schliessen ohnehin neu gezeichnet), damit der
 * Zustand der Effektgeraete nicht verloren geht. Nur beim Lernen im
 * Midi-Menue wird die Meldung dort verwendet.
 */
void UI_LoopMidiControlChange(
    uint8_t channel,
    uint8_t controller,
    uint8_t value)
{
    if (UI_LmIsLearning())
    {
        UI_LmLearn(channel, controller, value);
        return;
    }

    if (UI_PmIsLearning())
    {
        UI_PmLearn(UI_PM_TYPE_CC, channel, controller, value);
        return;
    }

    if (LoopMidi_ConsumeEcho(channel, controller, value))
    {
        return;
    }

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP ||
            uiItems[i].id > 255U)
        {
            continue;
        }

        int8_t state = LoopMidi_Receive(
            (uint8_t)uiItems[i].id, channel, controller, value);

        if (state >= 0)
        {
            UI_LoopSetActive((int16_t)i, (uint8_t)state, 1U);
        }
    }
}


/*
 * Program Change eines Effektgeraets: das Programm wird fuer die Presets
 * gemerkt (auch bei offenem Menue, es aendert die Anzeige nicht). Beim
 * Bearbeiten einer Meldung im Preset-Midi-Menue wird sie dort gelernt.
 */
uint8_t UI_LoopMidiClaimsProgram(
    uint8_t channel,
    uint8_t program)
{
    if (UI_PmIsLearning())
    {
        return 1U;
    }

    return LoopMidi_ProgramIsAssigned(channel, program);
}


void UI_LoopMidiProgramChange(
    uint8_t channel,
    uint8_t program)
{
    if (UI_PmIsLearning())
    {
        UI_PmLearn(UI_PM_TYPE_PC, channel, program, 0U);
        return;
    }

    LoopMidi_ReceiveProgram(channel, program);
}


/* -------------------------------------------------------------------------- */
/* Loop-Schalter am Router (Aufruf aus main.c)                                */
/* -------------------------------------------------------------------------- */

/*
 * Index des loopNumber-ten Loops (0 = L01) in uiItems, -1 wenn es ihn
 * nicht gibt. Zaehlung wie in UI_LoopStatusBits.
 */
static int16_t UI_LoopIndexByNumber(uint8_t loopNumber)
{
    uint8_t number = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_LOOP)
        {
            continue;
        }

        if (number == loopNumber)
        {
            return (int16_t)i;
        }

        number++;
    }

    return -1;
}


/*
 * Schaltet einen Loop am Router um (z.B. per Fussschalter). Wie beim
 * Laden eines Presets bekommt das Effektgeraet die passenden MIDI-Befehle,
 * eingeschaltet gilt der Loop als unbestaetigt aktiv. Das geschieht auch
 * bei offenem Menue (ohne Neuzeichnen, wie bei Loop-MIDI).
 */
uint8_t UI_LoopToggle(uint8_t loopNumber)
{
    int16_t index = UI_LoopIndexByNumber(loopNumber);

    if (index < 0)
    {
        return 0U;
    }

    uint8_t active =
        (uiItems[index].loopStatus == UI_LOOP_STATUS_OFF) ? 1U : 0U;

    UI_LoopSetActive(index, active, 0U);

    if (uiMode == UI_MODE_STATE_VIEW)
    {
        UI_RedrawItem(index);
    }

    return 1U;
}


UI_LoopStatus UI_LoopGetStatus(uint8_t loopNumber)
{
    int16_t index = UI_LoopIndexByNumber(loopNumber);

    if (index < 0)
    {
        return UI_LOOP_STATUS_OFF;
    }

    return uiItems[index].loopStatus;
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
            UI_MENU_PAGE_CONVERT_CONFIRM ||
        uiMenuPage ==
            UI_MENU_PAGE_MERGE_CONFIRM ||
        uiMenuPage ==
            UI_MENU_PAGE_MERGE_ERROR)
    {
        return 4U;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_GENERAL)
    {
        return UI_GENERAL_MENU_ENTRY_COUNT;
    }

    if (UI_MenuItemIsLoop())
    {
        return UI_MenuItemIsConvertible() ?
            4U :
            3U;
    }

    return (UI_MenuItemIsManualNode() ||
            UI_MenuItemIsMergeable()) ?
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


/* -------------------------------------------------------------------------- */
/* Loop-Menue: Convert to In + Out                                            */
/* -------------------------------------------------------------------------- */

/*
 * Ids eines Loops und seiner Ersatz-Items (siehe UI_CONVERT_ITEMS).
 */
typedef struct
{
    uint8_t loopNumber;     /* 0 = L01, wie in UI_LoopStatusBits */
    uint8_t loopId;
    uint8_t nodeId;
    uint8_t returnId;
    uint8_t sendId;

} UI_ConvertIds;


/*
 * Ermittelt die Ids zum Loop an loopIndex. Rueckgabe 0, wenn es kein
 * Loop ist oder seine Ersatz-Items in uiItems fehlen.
 */
static uint8_t UI_ConvertGetIds(
    int16_t loopIndex,
    UI_ConvertIds *ids)
{
    if (loopIndex < 0 ||
        loopIndex >= (int16_t)UI_ITEM_COUNT ||
        uiItems[loopIndex].type != UI_ITEM_LOOP ||
        uiItems[loopIndex].id > 255U)
    {
        return 0U;
    }

    uint8_t loopNumber = 0U;

    for (int16_t i = 0; i < loopIndex; i++)
    {
        if (uiItems[i].type == UI_ITEM_LOOP)
        {
            loopNumber++;
        }
    }

    if (loopNumber >= UI_CONVERT_MAX_LOOPS)
    {
        return 0U;
    }

    ids->loopNumber = loopNumber;
    ids->loopId = (uint8_t)uiItems[loopIndex].id;
    ids->nodeId =
        (uint8_t)(UI_CONVERT_NODE_ID_BASE + loopNumber + 1U);
    ids->returnId =
        (uint8_t)(UI_CONVERT_RETURN_ID_BASE + loopNumber + 1U);
    ids->sendId =
        (uint8_t)(UI_CONVERT_SEND_ID_BASE + loopNumber + 1U);

    return
        (UI_FindItemIndexById(ids->nodeId) >= 0 &&
         UI_FindItemIndexById(ids->returnId) >= 0 &&
         UI_FindItemIndexById(ids->sendId) >= 0) ?
        1U :
        0U;
}


static uint8_t UI_MenuItemIsConvertible(void)
{
    UI_ConvertIds ids;

    return
        (UI_MenuItemIsLoop() &&
         uiItems[uiMenuItemIndex].order >= 0 &&
         UI_ConvertGetIds(uiMenuItemIndex, &ids)) ?
        1U :
        0U;
}


/*
 * Niedrigste Lane der aktiven Items vom Typ type in der Spalte order.
 * Rueckgabe 0, wenn es dort keines gibt.
 */
static uint8_t UI_ConvertLowestLane(
    UI_ItemType type,
    int16_t order,
    int16_t *lane)
{
    uint8_t found = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (!UI_IsPermanentItem(&uiItems[i]) ||
            uiItems[i].type != type ||
            uiItems[i].order != order)
        {
            continue;
        }

        if (!found || uiItems[i].lane < *lane)
        {
            *lane = uiItems[i].lane;
            found = 1U;
        }
    }

    return found;
}


/*
 * Wandelt im aktuellen Layout (uiItems) den Loop in In + Out um:
 *
 * - der manuelle Knoten "Lnn" nimmt den Platz des Loops ein, damit die
 *   Struktur dahinter erhalten bleibt (nur wenn der Loop im Layout ist),
 * - "Return Lnn" kommt in die linke Spalte unter den Input mit der
 *   niedrigsten Lane,
 * - "Send Lnn" kommt in die rechte Spalte unter den Output mit der
 *   niedrigsten Lane.
 *
 * Danach werden Auto-Nodes und Verbindungen neu berechnet. MIDI und
 * Fokus bleiben unberuehrt (macht der Aufrufer).
 *
 * Rueckgabe 0 (nichts geaendert), wenn der Loop schon umgewandelt ist.
 */
static uint8_t UI_ConvertLoopItems(const UI_ConvertIds *ids)
{
    int16_t loopIndex = UI_FindItemIndexById(ids->loopId);
    int16_t nodeIndex = UI_FindItemIndexById(ids->nodeId);
    int16_t returnIndex = UI_FindItemIndexById(ids->returnId);
    int16_t sendIndex = UI_FindItemIndexById(ids->sendId);

    if (loopIndex < 0 ||
        nodeIndex < 0 ||
        returnIndex < 0 ||
        sendIndex < 0 ||
        uiItems[nodeIndex].order >= 0 ||
        uiItems[returnIndex].order >= 0 ||
        uiItems[sendIndex].order >= 0)
    {
        return 0U;
    }

    int16_t inputOrder = UI_GetMinimumPermanentOrder();
    int16_t outputOrder = UI_GetMaximumPermanentOrder();
    int16_t inputLane = 0;
    int16_t outputLane = 0;

    if (!UI_ConvertLowestLane(UI_ITEM_INPUT, inputOrder, &inputLane) ||
        !UI_ConvertLowestLane(UI_ITEM_OUTPUT, outputOrder, &outputLane))
    {
        return 0U;
    }

    if (uiItems[loopIndex].order >= 0)
    {
        uiItems[nodeIndex].order = uiItems[loopIndex].order;
        uiItems[nodeIndex].lane = uiItems[loopIndex].lane;
        uiItems[nodeIndex].focus = UI_FOCUS_NONE;

        uiItems[loopIndex].order = UI_INACTIVE_ORDER;
        uiItems[loopIndex].lane = 0;
        uiItems[loopIndex].focus = UI_FOCUS_NONE;
    }

    uiItems[returnIndex].order = inputOrder;
    uiItems[returnIndex].lane = (int16_t)(inputLane - 1);
    uiItems[returnIndex].focus = UI_FOCUS_NONE;

    uiItems[sendIndex].order = outputOrder;
    uiItems[sendIndex].lane = (int16_t)(outputLane - 1);
    uiItems[sendIndex].focus = UI_FOCUS_NONE;

    UI_RemoveAllAutoNodes();
    UI_NormalizeOrders();
    UI_EnsureEdgeColumns();

    if (!UI_RebuildCalculatedConnections())
    {
        UI_ClearConnections();
    }

    return 1U;
}


/*
 * Uebernimmt die Positionen eines gespeicherten Layouts in uiItems
 * (ohne MIDI, Loop-Status und Fokus). Nicht enthaltene Items werden
 * inaktiv. Der Puffer wird vorher wie in UI_PresetApplyLayout geprueft.
 *
 * Rueckgabe 0 bei ungueltigem Puffer (uiItems unveraendert).
 */
static uint8_t UI_ConvertSetPositions(
    const uint8_t *layout,
    uint8_t length)
{
    uint8_t seen[UI_ITEM_COUNT];

    memset(seen, 0, sizeof(seen));

    if (length < 1U ||
        (uint16_t)(1U + (3U * layout[0])) > length)
    {
        return 0U;
    }

    for (uint8_t entry = 0U; entry < layout[0]; entry++)
    {
        uint8_t base = (uint8_t)(1U + (3U * entry));
        int16_t itemIndex = UI_FindItemIndexById(layout[base]);
        int8_t order = (int8_t)layout[base + 1U];

        if (itemIndex < 0 ||
            uiItems[itemIndex].type == UI_ITEM_AUTO_NODE ||
            seen[itemIndex] ||
            order < 0 ||
            order >= UI_MAX_ORDER_COUNT)
        {
            return 0U;
        }

        seen[itemIndex] = 1U;
    }

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if (uiItems[i].type != UI_ITEM_AUTO_NODE)
        {
            uiItems[i].order = UI_INACTIVE_ORDER;
            uiItems[i].lane = 0;
        }
    }

    for (uint8_t entry = 0U; entry < layout[0]; entry++)
    {
        uint8_t base = (uint8_t)(1U + (3U * entry));
        int16_t itemIndex = UI_FindItemIndexById(layout[base]);

        uiItems[itemIndex].order = (int8_t)layout[base + 1U];
        uiItems[itemIndex].lane = (int8_t)layout[base + 2U];
    }

    UI_RemoveAllAutoNodes();

    return 1U;
}


/*
 * Baut alle gespeicherten Presets entsprechend um (Antwort "Yes" auf
 * "Convert presets too?", mit merge = 1 auf "Merge presets too?", dann
 * mit UI_MergeLoopItems statt UI_ConvertLoopItems). Mit selection
 * (Bitmaske je Bank, siehe uiPresetSelection) nur die markierten. Jedes Layout wird dafuer kurz in uiItems
 * hergestellt, umgewandelt, neu berechnet und wieder serialisiert, damit
 * es genau dem entspricht, was beim Laden entsteht (Rueckkopplungsschutz
 * in UI_PresetMatchesCurrent). Danach wird das aktuelle Layout
 * wiederhergestellt.
 *
 * Name, Loop-Status, Programme, On/Bypass-Zustaende und zusaetzliche
 * MIDI-Meldungen bleiben. Presets, in denen
 * der Loop schon umgewandelt ist, bleiben unveraendert.
 */
typedef enum
{
    UI_MERGE_OK = 0,
    UI_MERGE_NOTHING,       /* Loop nicht umgewandelt */
    UI_MERGE_NO_LEFT_IN,    /* ganz links bliebe kein Input */
    UI_MERGE_NO_RIGHT_OUT   /* ganz rechts bliebe kein Output */

} UI_MergeResult;

/* Ergebnis der letzten Pruefung fuer die Fehlermeldung. */
static UI_MergeResult uiMergeError = UI_MERGE_OK;


/*
 * Presets, die "Merge presets too?" wegen der Randregel nicht
 * zurueckbauen konnte (Anzeige auf der Seite MERGE_REPORT).
 */
static uint8_t uiMergeFailBank[PRESET_MAX_RECORDS];
static uint8_t uiMergeFailSlot[PRESET_MAX_RECORDS];
static uint8_t uiMergeFailCount = 0U;
static uint8_t uiMergeReportScrollTop = 0U;

static UI_MergeResult UI_MergeLoopItems(const UI_ConvertIds *ids);

static void UI_ConvertPresets(
    const UI_ConvertIds *ids,
    uint8_t merge,
    const uint32_t *selection)
{
    int16_t savedOrder[UI_ITEM_COUNT];
    int16_t savedLane[UI_ITEM_COUNT];
    UI_Connection savedConnections[UI_MAX_CONNECTIONS];
    uint16_t savedConnectionCount = uiConnectionCount;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        savedOrder[i] = uiItems[i].order;
        savedLane[i] = uiItems[i].lane;
    }

    memcpy(savedConnections, uiConnections, sizeof(savedConnections));

    uiMergeFailCount = 0U;

    for (uint8_t bank = 0U; bank < PRESET_BANK_COUNT; bank++)
    {
        for (uint8_t slot = 0U; slot < PRESET_SLOTS_PER_BANK; slot++)
        {
            char name[PRESET_NAME_LENGTH];
            uint8_t layout[PRESET_LAYOUT_MAX_BYTES];
            uint8_t length = 0U;
            uint32_t loopStatus = 0UL;
            uint8_t programs[PRESET_LOOP_PROGRAM_BYTES];
            PresetLoopStates states;

            /*
             * selection != NULL: nur die markierten Presets.
             */
            if (selection != NULL &&
                (selection[bank] & (1UL << slot)) == 0UL)
            {
                continue;
            }

            if (!PresetStore_GetSlotName(bank, slot, name) ||
                PresetStore_ReadLayout(bank, slot, layout, &length) !=
                    PRESET_STORE_OK ||
                PresetStore_ReadLoopStatus(bank, slot, &loopStatus) !=
                    PRESET_STORE_OK ||
                PresetStore_ReadLoopPrograms(bank, slot, programs) !=
                    PRESET_STORE_OK ||
                PresetStore_ReadLoopStates(bank, slot, &states) !=
                    PRESET_STORE_OK)
            {
                continue;
            }

            if (!UI_ConvertSetPositions(layout, length))
            {
                continue;
            }

            if (merge)
            {
                UI_MergeResult result = UI_MergeLoopItems(ids);

                if (result != UI_MERGE_OK)
                {
                    if (result != UI_MERGE_NOTHING &&
                        uiMergeFailCount < PRESET_MAX_RECORDS)
                    {
                        uiMergeFailBank[uiMergeFailCount] = bank;
                        uiMergeFailSlot[uiMergeFailCount] = slot;
                        uiMergeFailCount++;
                    }

                    continue;
                }
            }
            else if (!UI_ConvertLoopItems(ids))
            {
                continue;
            }

            length = UI_PresetSerializeLayout(layout, sizeof(layout));

            if (length == 0U)
            {
                continue;
            }

            /*
             * Loop-Status und On/Bypass-Zustand des Loops bleiben
             * gespeichert: Waehrend der Loop umgewandelt ist, werden sie
             * beim Laden nicht verwendet (Loop nicht im Layout), nach
             * "Merge presets too?" schaltet das Preset ihn wieder wie
             * vorher. Ein Preset, das mit umgewandeltem Loop gespeichert
             * wurde, hat ihn als aus gespeichert.
             */
            (void)PresetStore_SavePreset(
                bank,
                slot,
                name,
                layout,
                length,
                loopStatus,
                programs,
                &states);
        }
    }

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        uiItems[i].order = savedOrder[i];
        uiItems[i].lane = savedLane[i];
    }

    memcpy(uiConnections, savedConnections, sizeof(savedConnections));
    uiConnectionCount = savedConnectionCount;
}


/*
 * "Convert to In + Out" fuer den Loop im Menue. Der Loop wird wie beim
 * Laden eines Presets ohne ihn ausgeschaltet (Off an das Effektgeraet),
 * danach umgewandelt. presetMode (UI_PRESETS_*): keine, alle oder die
 * markierten gespeicherten Presets werden ebenfalls umgebaut, sonst nur
 * das aktuelle Layout (und damit kuenftig gespeicherte Presets). Alte Presets bringen beim Laden den
 * Loop zurueck.
 */
static void UI_ConvertSelectedLoop(uint8_t presetMode)
{
    UI_ConvertIds ids;

    if (!UI_MenuItemIsConvertible() ||
        !UI_ConvertGetIds(uiMenuItemIndex, &ids))
    {
        return;
    }

    int16_t loopIndex = uiMenuItemIndex;

    if (UI_ConvertLoopItems(&ids))
    {
        UI_LoopSetActive(loopIndex, 0U, 0U);

        grabStartStateValid = 0U;

        if (presetMode != UI_PRESETS_NONE)
        {
            UI_ConvertPresets(
                &ids,
                0U,
                (presetMode == UI_PRESETS_SELECTED) ?
                uiPresetSelection :
                NULL);
        }

        int16_t nodeIndex = UI_FindItemIndexById(ids.nodeId);

        UI_MenuSetConfirmedItem(
            (nodeIndex >= 0 && uiItems[nodeIndex].order >= 0) ?
            nodeIndex :
            UI_FindFirstSelectableIndex()
        );
    }

    uiMenuPage = UI_MENU_PAGE_ROOT;
    uiMenuControlMode =
        UI_MENU_CONTROL_NAVIGATION;
    uiMode = UI_MODE_STATE_VIEW;

    UI_Draw();
}

/* -------------------------------------------------------------------------- */
/* In-/Out-Menue: Merge to Loop                                               */
/* -------------------------------------------------------------------------- */

/*
 * Ids zum Return- bzw. Send-Item an itemIndex (siehe UI_CONVERT_ITEMS).
 * Rueckgabe 0, wenn es keines ist.
 */
static uint8_t UI_MergeGetIds(
    int16_t itemIndex,
    UI_ConvertIds *ids)
{
    if (itemIndex < 0 ||
        itemIndex >= (int16_t)UI_ITEM_COUNT)
    {
        return 0U;
    }

    uint16_t id = uiItems[itemIndex].id;
    uint16_t loopNumber;

    if (uiItems[itemIndex].type == UI_ITEM_INPUT &&
        id > UI_CONVERT_RETURN_ID_BASE &&
        id <= UI_CONVERT_RETURN_ID_BASE + UI_CONVERT_MAX_LOOPS)
    {
        loopNumber = (uint16_t)(id - UI_CONVERT_RETURN_ID_BASE - 1U);
    }
    else if (uiItems[itemIndex].type == UI_ITEM_OUTPUT &&
             id > UI_CONVERT_SEND_ID_BASE &&
             id <= UI_CONVERT_SEND_ID_BASE + UI_CONVERT_MAX_LOOPS)
    {
        loopNumber = (uint16_t)(id - UI_CONVERT_SEND_ID_BASE - 1U);
    }
    else
    {
        return 0U;
    }

    return UI_ConvertGetIds(
        UI_LoopIndexByNumber((uint8_t)loopNumber),
        ids);
}


static uint8_t UI_MenuItemIsMergeable(void)
{
    UI_ConvertIds ids;

    if (uiMenuItemIndex < 0 ||
        uiMenuItemIndex >= (int16_t)UI_ITEM_COUNT ||
        uiItems[uiMenuItemIndex].order < 0 ||
        !UI_MergeGetIds(uiMenuItemIndex, &ids))
    {
        return 0U;
    }

    return
        (uiItems[UI_FindItemIndexById(ids.loopId)].order < 0) ?
        1U :
        0U;
}


/*
 * Prueft im aktuellen Layout (uiItems), ob der Loop zusammengefuehrt
 * werden kann: Ohne Return und Send muessen ganz links weiterhin nur und
 * mindestens ein Input, ganz rechts nur und mindestens ein Output liegen.
 * Aendert nichts.
 */
static UI_MergeResult UI_MergeCheck(const UI_ConvertIds *ids)
{
    int16_t loopIndex = UI_FindItemIndexById(ids->loopId);
    int16_t returnIndex = UI_FindItemIndexById(ids->returnId);
    int16_t sendIndex = UI_FindItemIndexById(ids->sendId);

    if (loopIndex < 0 ||
        returnIndex < 0 ||
        sendIndex < 0 ||
        uiItems[loopIndex].order >= 0 ||
        (uiItems[returnIndex].order < 0 &&
         uiItems[sendIndex].order < 0))
    {
        return UI_MERGE_NOTHING;
    }

    int16_t minimumOrder = 32767;
    int16_t maximumOrder = -1;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if ((int16_t)i == returnIndex ||
            (int16_t)i == sendIndex ||
            !UI_IsPermanentItem(&uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order < minimumOrder)
        {
            minimumOrder = uiItems[i].order;
        }

        if (uiItems[i].order > maximumOrder)
        {
            maximumOrder = uiItems[i].order;
        }
    }

    uint8_t leftOk = (maximumOrder > minimumOrder) ? 1U : 0U;
    uint8_t rightOk = leftOk;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if ((int16_t)i == returnIndex ||
            (int16_t)i == sendIndex ||
            !UI_IsPermanentItem(&uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order == minimumOrder &&
            uiItems[i].type != UI_ITEM_INPUT)
        {
            leftOk = 0U;
        }

        if (uiItems[i].order == maximumOrder &&
            uiItems[i].type != UI_ITEM_OUTPUT)
        {
            rightOk = 0U;
        }
    }

    if (!leftOk)
    {
        return UI_MERGE_NO_LEFT_IN;
    }

    if (!rightOk)
    {
        return UI_MERGE_NO_RIGHT_OUT;
    }

    return UI_MERGE_OK;
}


/*
 * Fuehrt im aktuellen Layout (uiItems) Return und Send wieder zum Loop
 * zusammen. Der Loop nimmt den Platz des manuellen Knotens "Lnn" ein,
 * falls es ihn noch gibt. Sonst kommt er in die zweite Spalte von rechts
 * (Lane 0); ist dort Lane 0 belegt oder gibt es nur die Randspalten,
 * wird vor der Output-Spalte eine neue Spalte eingefuegt.
 *
 * Danach werden Auto-Nodes und Verbindungen neu berechnet. MIDI und
 * Fokus bleiben unberuehrt. Bei einem anderen Ergebnis als UI_MERGE_OK
 * bleibt das Layout unveraendert.
 */
static UI_MergeResult UI_MergeLoopItems(const UI_ConvertIds *ids)
{
    UI_MergeResult result = UI_MergeCheck(ids);

    if (result != UI_MERGE_OK)
    {
        return result;
    }

    int16_t loopIndex = UI_FindItemIndexById(ids->loopId);
    int16_t nodeIndex = UI_FindItemIndexById(ids->nodeId);
    int16_t returnIndex = UI_FindItemIndexById(ids->returnId);
    int16_t sendIndex = UI_FindItemIndexById(ids->sendId);

    if (UI_GetMaximumPermanentOrder() + 1 >= UI_MAX_ORDER_COUNT)
    {
        return UI_MERGE_NOTHING;
    }

    uiItems[returnIndex].order = UI_INACTIVE_ORDER;
    uiItems[returnIndex].lane = 0;
    uiItems[returnIndex].focus = UI_FOCUS_NONE;

    uiItems[sendIndex].order = UI_INACTIVE_ORDER;
    uiItems[sendIndex].lane = 0;
    uiItems[sendIndex].focus = UI_FOCUS_NONE;

    UI_RemoveAllAutoNodes();
    UI_NormalizeOrders();

    if (nodeIndex >= 0 && uiItems[nodeIndex].order >= 0)
    {
        uiItems[loopIndex].order = uiItems[nodeIndex].order;
        uiItems[loopIndex].lane = uiItems[nodeIndex].lane;

        uiItems[nodeIndex].order = UI_INACTIVE_ORDER;
        uiItems[nodeIndex].lane = 0;
        uiItems[nodeIndex].focus = UI_FOCUS_NONE;
    }
    else
    {
        int16_t outputOrder = UI_GetMaximumPermanentOrder();
        int16_t loopOrder = outputOrder - 1;

        if (loopOrder <= UI_GetMinimumPermanentOrder() ||
            UI_FindPermanentItemAt(loopOrder, 0, -1) >= 0)
        {
            UI_InsertOrderBefore(outputOrder, -1);
            loopOrder = outputOrder;
        }

        uiItems[loopIndex].order = loopOrder;
        uiItems[loopIndex].lane = 0;
    }

    uiItems[loopIndex].focus = UI_FOCUS_NONE;

    UI_NormalizeOrders();
    UI_EnsureEdgeColumns();

    if (!UI_RebuildCalculatedConnections())
    {
        UI_ClearConnections();
    }

    return UI_MERGE_OK;
}


/* -------------------------------------------------------------------------- */
/* Seite MERGE_REPORT: nicht zurueckgebaute Presets                           */
/* -------------------------------------------------------------------------- */

static void UI_MergeReportDrawRow(uint8_t position)
{
    uint8_t entry = (uint8_t)(uiMergeReportScrollTop + position);
    uint16_t y = (uint16_t)(UI_SAVE_LIST_Y +
        ((uint16_t)position * UI_SAVE_ROW_HEIGHT));
    char text[UI_SAVE_TEXT_CAPACITY];

    text[0] = '\0';

    /*
     * "B<Bank> <Preset>: <Name>", Bank wie in der Preset-Liste ab 0,
     * Preset ab 1.
     */
    if (entry < uiMergeFailCount)
    {
        char name[PRESET_NAME_LENGTH];
        uint8_t textPosition = 0U;

        (void)PresetStore_GetSlotName(
            uiMergeFailBank[entry], uiMergeFailSlot[entry], name);

        textPosition = UI_SaveAppendText(
            text, textPosition, UI_SAVE_TEXT_CAPACITY, "B");
        textPosition = UI_SaveAppendUInt(
            text, textPosition, UI_SAVE_TEXT_CAPACITY,
            uiMergeFailBank[entry]);
        textPosition = UI_SaveAppendText(
            text, textPosition, UI_SAVE_TEXT_CAPACITY, " ");
        textPosition = UI_SaveAppendUInt2(
            text, textPosition, UI_SAVE_TEXT_CAPACITY,
            (uint8_t)(uiMergeFailSlot[entry] + 1U));
        textPosition = UI_SaveAppendText(
            text, textPosition, UI_SAVE_TEXT_CAPACITY, ": ");
        UI_SaveAppendText(
            text, textPosition, UI_SAVE_TEXT_CAPACITY, name);
    }

    UI_SaveDrawTextLine(
        (uint8_t)(position + 1U),
        y,
        UI_SAVE_ROW_WIDTH,
        UI_SAVE_ROW_HEIGHT,
        y + UI_SAVE_TEXT_Y_OFFSET,
        text,
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND
    );
}


static void UI_MergeReportDrawList(void)
{
    for (uint8_t position = 0U;
         position < UI_SAVE_VISIBLE_PRESETS;
         position++)
    {
        UI_MergeReportDrawRow(position);
    }

    UI_ListDrawScrollbar(uiMergeFailCount, uiMergeReportScrollTop);
}


static void UI_MergeReportDrawPage(void)
{
    ST7735_FillRect(
        0U,
        0U,
        UI_DISPLAY_WIDTH,
        UI_DISPLAY_HEIGHT,
        UI_COLOR_BACKGROUND
    );

    UI_SaveShadowInvalidate();

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        UI_SAVE_HEADER_Y,
        "Not merged (In/Out):",
        UI_COLOR_LOOP_UNCONFIRMED,
        UI_COLOR_BACKGROUND,
        1U
    );

    UI_MergeReportDrawList();

    ST7735_DrawLine(
        0U,
        UI_SAVE_SEPARATOR_Y,
        UI_DISPLAY_WIDTH - 1U,
        UI_SAVE_SEPARATOR_Y,
        UI_COLOR_BORDER_NORMAL
    );

    Font5x7_DrawString(
        UI_SAVE_TEXT_X_OFFSET,
        UI_SAVE_STATUS_Y,
        "Enter: OK",
        UI_COLOR_TEXT_LIGHT,
        UI_COLOR_BACKGROUND,
        1U
    );
}


static void UI_MergeReportHandleEncoder(int8_t direction)
{
    int16_t top = (int16_t)uiMergeReportScrollTop + direction;
    int16_t maximumTop =
        (int16_t)uiMergeFailCount - (int16_t)UI_SAVE_VISIBLE_PRESETS;

    if (maximumTop < 0)
    {
        maximumTop = 0;
    }

    if (top < 0)
    {
        top = 0;
    }

    if (top > maximumTop)
    {
        top = maximumTop;
    }

    if (top == (int16_t)uiMergeReportScrollTop)
    {
        return;
    }

    uiMergeReportScrollTop = (uint8_t)top;

    UI_MergeReportDrawList();
}


/*
 * "Merge to Loop" fuer das Return-/Send-Item im Menue. presetMode
 * (UI_PRESETS_*): keine, alle oder die markierten gespeicherten Presets
 * werden ebenfalls zurueckgebaut. Presets, in
 * denen das nach der Randregel nicht geht, bleiben unveraendert und
 * werden danach auf der Seite MERGE_REPORT aufgelistet. Sonst nur das
 * aktuelle Layout. Der Loop bleibt ausgeschaltet.
 */
static void UI_MergeSelectedItem(uint8_t presetMode)
{
    UI_ConvertIds ids;

    if (!UI_MenuItemIsMergeable() ||
        !UI_MergeGetIds(uiMenuItemIndex, &ids))
    {
        return;
    }

    if (UI_MergeLoopItems(&ids) == UI_MERGE_OK)
    {
        grabStartStateValid = 0U;

        UI_MenuSetConfirmedItem(UI_FindItemIndexById(ids.loopId));

        if (presetMode != UI_PRESETS_NONE)
        {
            UI_ConvertPresets(
                &ids,
                1U,
                (presetMode == UI_PRESETS_SELECTED) ?
                uiPresetSelection :
                NULL);

            /*
             * Presets, die wegen der Randregel nicht zurueckgebaut
             * werden konnten, auflisten. Enter, Return oder die
             * Menue-Taste schliessen das Menue danach.
             */
            if (uiMergeFailCount > 0U)
            {
                uiMergeReportScrollTop = 0U;
                uiMenuPage = UI_MENU_PAGE_MERGE_REPORT;
                UI_DrawMenu();
                return;
            }
        }
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
            UI_MENU_PAGE_SAVE_PRESET ||
        uiMenuPage ==
            UI_MENU_PAGE_MANAGE_PRESETS ||
        uiMenuPage ==
            UI_MENU_PAGE_PRESET_SELECT)
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

    if (uiMenuPage ==
        UI_MENU_PAGE_MANAGE_ACTIONS)
    {
        UI_ActionsDrawPage();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_LOOP_MIDI ||
        uiMenuPage == UI_MENU_PAGE_LOOP_MIDI_GROUP)
    {
        UI_LmDrawPage();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_PRESET_MIDI)
    {
        UI_PmDrawPage();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_MERGE_REPORT)
    {
        UI_MergeReportDrawPage();
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
            UI_MENU_PAGE_CONVERT_CONFIRM ||
        uiMenuPage ==
            UI_MENU_PAGE_MERGE_CONFIRM)
    {
        UI_DrawMenuRow(
            UI_MENU_CONFIRM_TITLE_ROW,
            (uiMenuPage ==
             UI_MENU_PAGE_CONVERT_CONFIRM) ?
            "Convert presets too?" :
            "Merge presets too?"
        );

        UI_DrawMenuRow(
            UI_MENU_CONVERT_YES_ROW,
            "Yes"
        );

        UI_DrawMenuRow(
            UI_MENU_CONVERT_NO_ROW,
            "No"
        );

        UI_DrawMenuRow(
            UI_MENU_CONVERT_SELECT_ROW,
            "Select"
        );

        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_MERGE_ERROR)
    {
        uint8_t left =
            (uiMergeError == UI_MERGE_NO_LEFT_IN) ?
            1U :
            0U;

        UI_DrawMenuRow(0U, "Merge not possible:");
        UI_DrawMenuRow(
            1U,
            left ? "Leftmost column" : "Rightmost column");
        UI_DrawMenuRow(
            2U,
            left ? "needs an In" : "needs an Out");
        UI_DrawMenuRow(UI_MENU_MERGE_ERROR_OK_ROW, "OK");

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
    else if (UI_MenuItemIsLoop())
    {
        UI_DrawMenuRow(
            UI_MENU_LOOP_MIDI_ROW,
            "Midi"
        );

        if (UI_MenuItemIsConvertible())
        {
            UI_DrawMenuRow(
                UI_MENU_LOOP_CONVERT_ROW,
                "Convert to In + Out"
            );
        }
    }
    else if (UI_MenuItemIsMergeable())
    {
        UI_DrawMenuRow(
            UI_MENU_IO_MERGE_ROW,
            "Merge to Loop"
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

    /*
     * Ein "scharfes" Speichern per MIDI verfaellt, wenn das Menue
     * geoeffnet wird, sonst wuerde es nach dem Schliessen ueberraschend
     * ausgefuehrt.
     */
    uiMidiSaveArmed = 0U;
    uiMidiSaveResult = UI_MIDI_SAVE_RESULT_NONE;
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

    /*
     * Midi-Menue eines Loops: Aenderungen werden auch beim Schliessen
     * mit der Menue-Taste gespeichert.
     */
    if (uiMenuPage == UI_MENU_PAGE_LOOP_MIDI ||
        uiMenuPage == UI_MENU_PAGE_LOOP_MIDI_GROUP)
    {
        (void)UI_LmFinish();
    }

    if (uiMenuPage == UI_MENU_PAGE_PRESET_MIDI)
    {
        (void)UI_PmFinish();
    }

    /*
     * Auswahl-Seite: Menue-Taste bricht ab, nichts wird umgebaut.
     */
    if (uiMenuPage == UI_MENU_PAGE_PRESET_SELECT)
    {
        uiPresetBank = uiPresetSelectBankBackup;
        uiPresetBankEditing = 0U;
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
            UI_MENU_PAGE_SAVE_PRESET ||
        uiMenuPage ==
            UI_MENU_PAGE_MANAGE_PRESETS ||
        uiMenuPage ==
            UI_MENU_PAGE_PRESET_SELECT)
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

    if (uiMenuPage ==
        UI_MENU_PAGE_MANAGE_ACTIONS)
    {
        UI_ActionsHandleEncoder(direction);
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_LOOP_MIDI ||
        uiMenuPage == UI_MENU_PAGE_LOOP_MIDI_GROUP)
    {
        UI_LmHandleEncoder(direction);
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_PRESET_MIDI)
    {
        UI_PmHandleEncoder(direction);
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_MERGE_REPORT)
    {
        UI_MergeReportHandleEncoder(direction);
        return;
    }

    if (direction == 0 ||
        uiMenuPage ==
            UI_MENU_PAGE_MERGE_ERROR)
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
            UI_MENU_PAGE_SAVE_PRESET ||
        uiMenuPage ==
            UI_MENU_PAGE_MANAGE_PRESETS ||
        uiMenuPage ==
            UI_MENU_PAGE_PRESET_SELECT)
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
        UI_MENU_PAGE_MANAGE_ACTIONS)
    {
        UI_ActionsHandleEnter();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_LOOP_MIDI ||
        uiMenuPage == UI_MENU_PAGE_LOOP_MIDI_GROUP)
    {
        UI_LmHandleEnter();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_PRESET_MIDI)
    {
        UI_PmHandleEnter();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_MERGE_REPORT)
    {
        UI_CloseMenu(0U);
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

    if (uiMenuPage ==
        UI_MENU_PAGE_CONVERT_CONFIRM)
    {
        if (uiMenuSelectedRow ==
                UI_MENU_CONVERT_YES_ROW ||
            uiMenuSelectedRow ==
                UI_MENU_CONVERT_NO_ROW)
        {
            UI_ConvertSelectedLoop(
                (uiMenuSelectedRow ==
                 UI_MENU_CONVERT_YES_ROW) ?
                UI_PRESETS_ALL :
                UI_PRESETS_NONE
            );
        }
        else if (uiMenuSelectedRow ==
                 UI_MENU_CONVERT_SELECT_ROW)
        {
            UI_PresetSelectOpen(0U);
        }

        /* Titelzeile: nichts tun. */
        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_MERGE_CONFIRM)
    {
        if (uiMenuSelectedRow ==
                UI_MENU_CONVERT_YES_ROW ||
            uiMenuSelectedRow ==
                UI_MENU_CONVERT_NO_ROW)
        {
            UI_MergeSelectedItem(
                (uiMenuSelectedRow ==
                 UI_MENU_CONVERT_YES_ROW) ?
                UI_PRESETS_ALL :
                UI_PRESETS_NONE
            );
        }
        else if (uiMenuSelectedRow ==
                 UI_MENU_CONVERT_SELECT_ROW)
        {
            UI_PresetSelectOpen(1U);
        }

        return;
    }

    if (uiMenuPage ==
        UI_MENU_PAGE_MERGE_ERROR)
    {
        uiMenuPage = UI_MENU_PAGE_ROOT;
        uiMenuSelectedRow =
            UI_MENU_IO_MERGE_ROW;
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
            UI_MENU_LOOP_MIDI_ROW &&
        UI_MenuItemIsLoop())
    {
        UI_LmOpen();
        return;
    }

    if (uiMenuSelectedRow ==
            UI_MENU_IO_MERGE_ROW &&
        UI_MenuItemIsMergeable())
    {
        UI_ConvertIds ids;

        uiMergeError =
            UI_MergeGetIds(uiMenuItemIndex, &ids) ?
            UI_MergeCheck(&ids) :
            UI_MERGE_NOTHING;

        if (uiMergeError == UI_MERGE_OK)
        {
            uiMenuPage =
                UI_MENU_PAGE_MERGE_CONFIRM;
            uiMenuSelectedRow =
                UI_MENU_CONVERT_NO_ROW;
        }
        else
        {
            uiMenuPage =
                UI_MENU_PAGE_MERGE_ERROR;
            uiMenuSelectedRow =
                UI_MENU_MERGE_ERROR_OK_ROW;
        }

        UI_DrawMenu();
        return;
    }

    if (uiMenuSelectedRow ==
            UI_MENU_LOOP_CONVERT_ROW &&
        UI_MenuItemIsConvertible())
    {
        uiMenuPage =
            UI_MENU_PAGE_CONVERT_CONFIRM;
        uiMenuSelectedRow =
            UI_MENU_CONVERT_NO_ROW;
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
            UI_MENU_PAGE_SAVE_PRESET ||
        uiMenuPage ==
            UI_MENU_PAGE_MANAGE_PRESETS ||
        uiMenuPage ==
            UI_MENU_PAGE_PRESET_SELECT)
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
        UI_MENU_PAGE_MANAGE_ACTIONS)
    {
        UI_ActionsHandleReturn();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_LOOP_MIDI ||
        uiMenuPage == UI_MENU_PAGE_LOOP_MIDI_GROUP)
    {
        UI_LmHandleReturn();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_PRESET_MIDI)
    {
        UI_PmHandleReturn();
        return;
    }

    if (uiMenuPage == UI_MENU_PAGE_MERGE_REPORT)
    {
        UI_CloseMenu(0U);
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

    if (uiMenuPage ==
        UI_MENU_PAGE_CONVERT_CONFIRM)
    {
        uiMenuPage = UI_MENU_PAGE_ROOT;
        uiMenuSelectedRow =
            UI_MENU_LOOP_CONVERT_ROW;
        UI_DrawMenu();
        return;
    }

    if (uiMenuPage ==
            UI_MENU_PAGE_MERGE_CONFIRM ||
        uiMenuPage ==
            UI_MENU_PAGE_MERGE_ERROR)
    {
        uiMenuPage = UI_MENU_PAGE_ROOT;
        uiMenuSelectedRow =
            UI_MENU_IO_MERGE_ROW;
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
                UI_ITEM_MANUAL_NODE ||
            UI_IsConvertItem(&uiItems[i]))
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
     * Ein Input in der linken Randspalte darf nicht
     * weiter nach links bewegt werden, ein Output in
     * der rechten Randspalte nicht weiter nach rechts.
     *
     * Sonst duerfen Inputs und Outputs frei bewegt
     * werden. Die Randregeln (ganz links nur und
     * mindestens ein Input, ganz rechts nur und
     * mindestens ein Output) werden anschliessend
     * geprueft.
     */
    uint8_t grabbedIsIo =
        (grabbedItem->type == UI_ITEM_INPUT ||
         grabbedItem->type == UI_ITEM_OUTPUT) ?
        1U :
        0U;

    if (grabbedItem->type == UI_ITEM_INPUT &&
        direction < 0 &&
        grabbedItem->order ==
            UI_GetMinimumPermanentOrder())
    {
        return;
    }
    
    
    if (grabbedItem->type == UI_ITEM_OUTPUT &&
        direction > 0 &&
        grabbedItem->order ==
            UI_GetMaximumPermanentOrder())
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
     * Ziel ist ein permanentes Item.
     */
    if (targetPermanentIndex >= 0)
    {
        UI_ItemType targetType =
            uiItems[
                targetPermanentIndex
            ].type;

        int16_t targetOrderOfItem =
            uiItems[
                targetPermanentIndex
            ].order;


        if (!grabbedIsIo &&
            ((targetType ==
                  UI_ITEM_INPUT &&
              direction < 0 &&
              targetOrderOfItem ==
                  UI_GetMinimumPermanentOrder()) ||
             (targetType ==
                  UI_ITEM_OUTPUT &&
              direction > 0 &&
              targetOrderOfItem ==
                  UI_GetMaximumPermanentOrder())))
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

/*
 * Vertikale Grenze: Ein Item darf hoechstens eine Lane ueber die hoechste
 * bzw. unter die niedrigste Lane der permanenten Items in den
 * Nachbarspalten hinaus. Nachbarspalten sind die naechsten Spalten links
 * und rechts mit permanenten Items (Spalten nur mit Auto-Nodes zaehlen
 * nicht). Ohne solche Items gibt es keine Grenze.
 */
static uint8_t UI_LaneWithinNeighbourRange(
    int16_t itemIndex,
    int16_t lane)
{
    int16_t order = uiItems[itemIndex].order;
    int16_t previousOrder = -1;
    int16_t nextOrder = -1;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if ((int16_t)i == itemIndex ||
            !UI_IsPermanentItem(&uiItems[i]))
        {
            continue;
        }

        if (uiItems[i].order < order &&
            uiItems[i].order > previousOrder)
        {
            previousOrder = uiItems[i].order;
        }

        if (uiItems[i].order > order &&
            (nextOrder < 0 || uiItems[i].order < nextOrder))
        {
            nextOrder = uiItems[i].order;
        }
    }

    int16_t lowestLane = 0;
    int16_t highestLane = 0;
    uint8_t found = 0U;

    for (uint16_t i = 0U; i < UI_ITEM_COUNT; i++)
    {
        if ((int16_t)i == itemIndex ||
            !UI_IsPermanentItem(&uiItems[i]) ||
            (uiItems[i].order != previousOrder &&
             uiItems[i].order != nextOrder))
        {
            continue;
        }

        if (!found || uiItems[i].lane < lowestLane)
        {
            lowestLane = uiItems[i].lane;
        }

        if (!found || uiItems[i].lane > highestLane)
        {
            highestLane = uiItems[i].lane;
        }

        found = 1U;
    }

    return
        (!found ||
         (lane <= highestLane + 1 &&
          lane >= lowestLane - 1)) ?
        1U :
        0U;
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
     * Auf eine freie Position nur innerhalb der
     * Grenze durch die Nachbarspalten.
     */
    if (targetPermanentIndex < 0 &&
        !UI_LaneWithinNeighbourRange(
            grabbedIndex,
            targetLane))
    {
        return;
    }


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

/*
 * Bereich links in der Fusszeile:
 *
 *   Bank 12     aktuell gewaehlte Bank
 *   Save B12    Speichern per MIDI ist scharf (CC0 wurde empfangen)
 *   Saved 07    Preset 07 wurde per MIDI gespeichert
 *   Mem full    Speicher voll
 *   Save err    Schreibfehler
 *
 * Die Meldungen nach dem Speichern bleiben, bis die Fusszeile aus einem
 * anderen Grund neu gezeichnet wird (z.B. beim Wechsel der Auswahl).
 */
static void UI_DrawBankIndicator(void)
{
    char text[12];
    uint16_t color = UI_COLOR_BORDER_SELECTED;
    uint8_t position = 0U;

    if (!uiMidiSaveResultFresh)
    {
        uiMidiSaveResult = UI_MIDI_SAVE_RESULT_NONE;
    }

    switch (uiMidiSaveResult)
    {
        case UI_MIDI_SAVE_RESULT_SAVED:
            color = UI_COLOR_LOOP_CONFIRMED;
            position = UI_SaveAppendText(
                text, 0U, sizeof(text), "Saved ");
            UI_SaveAppendUInt2(
                text, position, sizeof(text),
                (uint8_t)(uiMidiSaveResultSlot + 1U));
            break;

        case UI_MIDI_SAVE_RESULT_FULL:
            color = UI_COLOR_ERROR;
            UI_SaveAppendText(text, 0U, sizeof(text), "Mem full");
            break;

        case UI_MIDI_SAVE_RESULT_ERROR:
            color = UI_COLOR_ERROR;
            UI_SaveAppendText(text, 0U, sizeof(text), "Save err");
            break;

        case UI_MIDI_SAVE_RESULT_NONE:
        default:
            if (uiMidiSaveArmed)
            {
                color = UI_COLOR_LOOP_UNCONFIRMED;
                position = UI_SaveAppendText(
                    text, 0U, sizeof(text), "Save B");
            }
            else
            {
                position = UI_SaveAppendText(
                    text, 0U, sizeof(text), "Bank ");
            }

            UI_SaveAppendUInt(
                text, position, sizeof(text), uiPresetBank);
            break;
    }

    uint16_t textHeight = Font5x7_GetHeight(1);
    uint16_t textY = UI_FOOTER_TOP;

    if (textHeight < UI_FOOTER_HEIGHT)
    {
        textY += (UI_FOOTER_HEIGHT - textHeight) / 2;
    }

    Font5x7_DrawString(
        UI_BANK_INDICATOR_X,
        textY,
        text,
        color,
        UI_COLOR_BACKGROUND,
        1
    );
}


static void UI_DrawFooter(void)
{
    const UI_Item *focusedItem = UI_GetFocusedItem();

    ST7735_FillRect(0, UI_FOOTER_TOP,
                    UI_DISPLAY_WIDTH, UI_FOOTER_HEIGHT,
                    UI_COLOR_BACKGROUND);

    UI_DrawBankIndicator();

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

    if (textX < (int16_t)(UI_BANK_INDICATOR_X + UI_BANK_INDICATOR_WIDTH))
    {
        textX = (int16_t)(UI_BANK_INDICATOR_X + UI_BANK_INDICATOR_WIDTH);
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


if (textX < (int16_t)(UI_BANK_INDICATOR_X + UI_BANK_INDICATOR_WIDTH))
{
    textX = (int16_t)(UI_BANK_INDICATOR_X + UI_BANK_INDICATOR_WIDTH);
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
