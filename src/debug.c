#if DEBUG

#include "global.h"
#include "list_menu.h"
#include "main.h"
#include "map_name_popup.h"
#include "menu.h"
#include "menu_helpers.h"
#include "script.h"
#include "sound.h"
#include "strings.h"
#include "string_util.h"
#include "international_string_util.h"
#include "item.h"
#include "constants/items.h"
#include "task.h"
#include "constants/songs.h"
#include "mgba.h"
#include "printf.h"
#include "text_window.h"

#define DEBUG_MAIN_MENU_HEIGHT 6
#define DEBUG_MAIN_MENU_WIDTH 11

#define MENU_TASK_ID data[0]
#define WINDOW_ID data[1]
#define MENU_IDX data[2]
#define OLD_MENU_TASK_ID data[3]
#define OLD_WINDOW_ID data[4]
#define ITEM_ID data[5]
#define ITEM_QTY data[6]

#define MAX_ITEM_QTY 100

void Debug_ShowMainMenu(void);
static void Debug_DestroyMainMenu(u8);
static void Debug_DestroyItemMenu(u8);
static void DebugAction_NoOp(u8);
static void DebugAction_GetItem(u8);
static void DebugAction_AddItem(u8);
static void DebugAction_Cancel(u8);
static void DebugAction_ItemMenuCancel(u8);
static void DebugAdjust_NoOp(u8, s8);
static void DebugAdjust_Item(u8, s8);
static void DebugAdjust_Quantity(u8, s8);
static void DebugTask_HandleMainMenuInput(u8);
static void DebugTask_HandleItemMenuInput(u8);
static void ChangeItemMenuItem(u8 taskId);
static void ItemMenuPrintItemFunc(u8 windowId, s32 item, u8 y);
static void CloseMessage(u8 taskId);

enum {
    DEBUG_MENU_ITEM_GET_ITEM,
    DEBUG_MENU_ITEM_CANCEL,
};

enum {
    DEBUG_ITEM_MENU_ITEM,
    DEBUG_ITEM_MENU_QUANTITY,
    DEBUG_ITEM_MENU_ADD,
    DEBUG_ITEM_MENU_CANCEL
};

enum {
    DEBUG_MENU,
    DEBUG_ITEM_MENU
};

static const u8 gDebugText_GetItem[] = _("Get Item");
static const u8 gDebugText_Quantity[] = _("Quantity");
static const u8 gDebugText_AddItem[] = _("Add Item");
static const u8 gDebugText_Cancel[] = _("Cancel");
static const u8 gDebugText_GetItemResponse[] = _("{STR_VAR_1} added to bag!{PAUSE_UNTIL_PRESS}");

static const struct ListMenuItem sDebugMenuItems[] =
{
    [DEBUG_MENU_ITEM_GET_ITEM] = {gDebugText_GetItem, DEBUG_MENU_ITEM_GET_ITEM},
    [DEBUG_MENU_ITEM_CANCEL] = {gDebugText_Cancel, DEBUG_MENU_ITEM_CANCEL}
};

static void (*const sDebugMenuActions[])(u8) =
{
    [DEBUG_MENU_ITEM_GET_ITEM] = DebugAction_GetItem,
    [DEBUG_MENU_ITEM_CANCEL] = DebugAction_Cancel
};

static const struct ListMenuItem sDebugItemMenuItems[] =
{
    [DEBUG_ITEM_MENU_ITEM] = {gStringVar2, DEBUG_ITEM_MENU_ITEM},
    [DEBUG_ITEM_MENU_QUANTITY] = {gDebugText_Quantity, DEBUG_ITEM_MENU_QUANTITY},
    [DEBUG_ITEM_MENU_ADD] = {gDebugText_AddItem, DEBUG_ITEM_MENU_ADD},
    [DEBUG_ITEM_MENU_CANCEL] = {gDebugText_Cancel, DEBUG_ITEM_MENU_CANCEL}
};
static void (*const sDebugItemMenuActions[])(u8) =
{
    [DEBUG_ITEM_MENU_ITEM] = DebugAction_NoOp,
    [DEBUG_ITEM_MENU_QUANTITY] = DebugAction_NoOp,
    [DEBUG_ITEM_MENU_ADD] = DebugAction_AddItem,
    [DEBUG_ITEM_MENU_CANCEL] = DebugAction_ItemMenuCancel
};
static void (*const sDebugItemMenuAdjust[])(u8, s8) =
{
    [DEBUG_ITEM_MENU_ITEM] = DebugAdjust_Item,
    [DEBUG_ITEM_MENU_QUANTITY] = DebugAdjust_Quantity,
    [DEBUG_ITEM_MENU_ADD] = DebugAdjust_NoOp,
    [DEBUG_ITEM_MENU_CANCEL] = DebugAdjust_NoOp
};

static void (**const sDebugActionsByMenu[])(u8) = {
    [DEBUG_MENU] = sDebugMenuActions,
    [DEBUG_ITEM_MENU] = sDebugItemMenuActions
};

static void (**const sDebugAdjustByMenu[])(u8, s8) = {
    [DEBUG_MENU] = NULL,
    [DEBUG_ITEM_MENU] = sDebugItemMenuAdjust
};

static void (*const sDebugCloseMenu[])(u8) =
{
    [DEBUG_MENU] = Debug_DestroyMainMenu,
    [DEBUG_ITEM_MENU] = Debug_DestroyItemMenu
};

static const struct WindowTemplate sDebugMenuWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = DEBUG_MAIN_MENU_WIDTH,
    .height = 2 * DEBUG_MAIN_MENU_HEIGHT,
    .paletteNum = 15,
    .baseBlock = 1,
};

static const struct ListMenuTemplate sDebugMenuListTemplate =
{
    .items = sDebugMenuItems,
    .moveCursorFunc = ListMenuDefaultCursorMoveFunc,
    .totalItems = ARRAY_COUNT(sDebugMenuItems),
    .maxShowed = DEBUG_MAIN_MENU_HEIGHT,
    .windowId = 0,
    .header_X = 0,
    .item_X = 8,
    .cursor_X = 0,
    .upText_Y = 1,
    .cursorPal = 2,
    .fillValue = 1,
    .cursorShadowPal = 3,
    .lettersSpacing = 1,
    .itemVerticalPadding = 0,
    .scrollMultiple = LIST_NO_MULTIPLE_SCROLL,
    .fontId = 1,
    .cursorKind = 0
};

// ITEM MENU

static const struct WindowTemplate sDebugItemMenuWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = DEBUG_MAIN_MENU_WIDTH + 3,
    .tilemapTop = 1,
    .width = 15,
    .height = 2 * DEBUG_MAIN_MENU_HEIGHT,
    .paletteNum = 15,
    .baseBlock = (DEBUG_MAIN_MENU_WIDTH *  2 * DEBUG_MAIN_MENU_HEIGHT) + 1,
};

static const struct WindowTemplate sDebugMenuMessageBox =
{
    .bg = 0,
    .tilemapLeft = 2,
    .tilemapTop = 15,
    .width = 27,
    .height = 4,
    .paletteNum = 15,
    .baseBlock = (DEBUG_MAIN_MENU_WIDTH *  2 * DEBUG_MAIN_MENU_HEIGHT) + (15 *  2 * DEBUG_MAIN_MENU_HEIGHT) + 1,
};

static const struct ListMenuTemplate sDebugItemMenuListTemplate =
{
    .items = sDebugItemMenuItems,
    .moveCursorFunc = ListMenuDefaultCursorMoveFunc,
    .itemPrintFunc = ItemMenuPrintItemFunc,
    .totalItems = ARRAY_COUNT(sDebugItemMenuItems),
    .maxShowed = DEBUG_MAIN_MENU_HEIGHT,
    .windowId = 0,
    .header_X = 0,
    .item_X = 8,
    .cursor_X = 0,
    .upText_Y = 1,
    .cursorPal = 2,
    .fillValue = 1,
    .cursorShadowPal = 3,
    .lettersSpacing = 1,
    .itemVerticalPadding = 0,
    .scrollMultiple = LIST_NO_MULTIPLE_SCROLL,
    .fontId = 7,
    .cursorKind = 0
};

EWRAM_DATA u8 currTaskId = 0;
EWRAM_DATA u16 messageWindowId = 0;

void Debug_ShowMainMenu(void) {
    struct ListMenuTemplate menuTemplate;
    u8 windowId;
    u8 menuTaskId;
    u8 inputTaskId;
    struct Task *task;

    // create window
    HideMapNamePopUpWindow();
    LoadMessageBoxAndBorderGfx();
    windowId = AddWindow(&sDebugMenuWindowTemplate);
    DrawStdWindowFrame(windowId, FALSE);

    // create list menu
    menuTemplate = sDebugMenuListTemplate;
    menuTemplate.windowId = windowId;
    menuTaskId = ListMenuInit(&menuTemplate, 0, 0);

    // draw everything
    CopyWindowToVram(windowId, 3);

    // create input handler task
    inputTaskId = CreateTask(DebugTask_HandleMainMenuInput, 3);
    task = &gTasks[inputTaskId];
    task->MENU_TASK_ID = menuTaskId;
    task->WINDOW_ID = windowId;
    task->MENU_IDX  = DEBUG_MENU;
}

static void Debug_DestroyMainMenu(u8 taskId)
{
    DestroyListMenuTask(gTasks[taskId].data[0], NULL, NULL);
    ClearStdWindowAndFrame(gTasks[taskId].data[1], TRUE);
    RemoveWindow(gTasks[taskId].data[1]);
    DestroyTask(taskId);
    EnableBothScriptContexts();
}

static void Debug_DestroyItemMenu(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    u8 menuTaskId = task->OLD_MENU_TASK_ID;
    u8 windowId = task->OLD_WINDOW_ID;

    DestroyListMenuTask(task->MENU_TASK_ID, NULL, NULL);
    ClearStdWindowAndFrame(task->WINDOW_ID, TRUE);
    RemoveWindow(task->WINDOW_ID);

    gMain.state = 0;
    task->MENU_IDX = DEBUG_MENU;
    task->MENU_TASK_ID = menuTaskId;
    task->WINDOW_ID = windowId;
}

static void DebugTask_HandleMainMenuInput(u8 taskId)
{
    void (*func)(u8) = NULL;
    void (*adjust)(u8, s8) = NULL;
    u32 input;
    struct Task *task = &gTasks[taskId];
    struct ListMenu *list = (void*)gTasks[task->MENU_TASK_ID].data;
    switch (gMain.state)
    {
    default:
    case 0:
        // Wait a frame to avoid picking up inputs from previous menu
        gMain.state++;
        break;
    case 1:
        input = ListMenu_ProcessInput(task->MENU_TASK_ID);

        if (gMain.newKeys & A_BUTTON)
        {
            PlaySE(SE_SELECT);
            func = sDebugActionsByMenu[task->MENU_IDX][input];
        }
        else if (gMain.newKeys & B_BUTTON)
        {
            PlaySE(SE_SELECT);
            func = sDebugCloseMenu[task->MENU_IDX];
        } 
        if ((gMain.newAndRepeatedKeys & DPAD_RIGHT || gMain.newAndRepeatedKeys & DPAD_LEFT) && sDebugAdjustByMenu[task->MENU_IDX] != NULL) {
            input = list->template.items[list->scrollOffset + list->selectedRow].id;
            adjust = sDebugAdjustByMenu[task->MENU_IDX][input];
            if (adjust != NULL)
                adjust(taskId, ((gMain.newAndRepeatedKeys & DPAD_RIGHT) ? 1 : -1) * (gMain.heldKeys & A_BUTTON ? 10 : 1));
        }
        if (func != NULL)
            func(taskId);
    }
}

static void ChangeItemMenuItem(u8 taskId) {
    u32 i;
    struct Task *task = &gTasks[taskId];
    u16 itemId = task->ITEM_ID;
    StringCopy(gStringVar2, ItemId_GetName(itemId));

    for (i = 0; i < 17 && gStringVar2[i] != EOS; i++){}
    mgba_printf(MGBA_LOG_INFO, "Change Item: %d", i);
    for (; i < 17; i++) {
        gStringVar2[i] = CHAR_SPACE;
    }
    gStringVar2[i] = EOS;
    RedrawListMenu(task->MENU_TASK_ID);
}

static void ItemMenuPrintItemFunc(u8 windowId, s32 item, u8 y) {
    u8 x;
    struct Task *task = &gTasks[currTaskId];

    if (item == DEBUG_ITEM_MENU_ITEM) {
        ConvertIntToDecimalStringN(
            gStringVar1,
            task->ITEM_ID,
            STR_CONV_MODE_LEFT_ALIGN,
            4);
        x = GetStringRightAlignXOffset(7, gStringVar1, 120);
        AddTextPrinterParameterized(windowId, 7, gStringVar1, x, y, -1, 0);
    }
    if (item == DEBUG_ITEM_MENU_QUANTITY) {
        ConvertIntToDecimalStringN(
            gStringVar1,
            task->ITEM_QTY,
            STR_CONV_MODE_LEADING_ZEROS,
            2);
        StringExpandPlaceholders(gStringVar4, gText_xVar1);
         x = GetStringRightAlignXOffset(7, gStringVar4, 120);
        AddTextPrinterParameterized(windowId, 7, gStringVar4, x, y, -1, 0);
    }
}

static void DebugAction_GetItem(u8 taskId) {
    struct ListMenuTemplate menuTemplate;
    u8 windowId;
    u8 menuTaskId;
    u8 inputTaskId;
    struct Task *task;

    // create window
    windowId = AddWindow(&sDebugItemMenuWindowTemplate);
    DrawStdWindowFrame(windowId, FALSE);

    // create list menu
    menuTemplate = sDebugItemMenuListTemplate;
    menuTemplate.windowId = windowId;
    menuTaskId = ListMenuInit(&menuTemplate, 0, 0);

    // draw everything
    CopyWindowToVram(windowId, 3);

    // create input handler task
    gMain.state = 0;
    currTaskId = inputTaskId = CreateTask(DebugTask_HandleMainMenuInput, 3);
    task = &gTasks[inputTaskId];
    task->MENU_TASK_ID = menuTaskId;
    task->WINDOW_ID = windowId;
    task->MENU_IDX  = DEBUG_ITEM_MENU;
    task->OLD_MENU_TASK_ID = (&gTasks[taskId])->MENU_TASK_ID;
    task->OLD_WINDOW_ID = (&gTasks[taskId])->WINDOW_ID;
    task->ITEM_QTY = 1;

    // default item
    task->ITEM_ID = 1;
    ChangeItemMenuItem(inputTaskId);

    // remove main menu handler
    DestroyTask(taskId);
}
static void DebugAction_AddItem(u8 taskId) {
    u16 windowId;
    u16 tileNum = sDebugMenuMessageBox.baseBlock + (sDebugMenuMessageBox.width * sDebugMenuMessageBox.height) + 1;
    struct Task *task = &gTasks[taskId];
    bool8 success = AddBagItem(task->ITEM_ID, task->ITEM_QTY);
    if (success) {
        messageWindowId = AddWindow(&sDebugMenuMessageBox);
        FillWindowPixelBuffer(messageWindowId, PIXEL_FILL(1));
        LoadMessageBoxGfx(messageWindowId, tileNum, 0xD0);
        PutWindowTilemap(messageWindowId);
        CopyItemName(task->ITEM_ID, gStringVar1);
        DisplayMessageAndContinueTask(taskId, messageWindowId,  tileNum, 13, 1, GetPlayerTextSpeedDelay(), gDebugText_GetItemResponse, CloseMessage);
        
        ScheduleBgCopyTilemapToVram(0);
    }
}

static void CloseMessage(u8 taskId) {
    if (messageWindowId > 0) {
        ClearDialogWindowAndFrameToTransparent(messageWindowId, 1);
        ClearWindowTilemap(messageWindowId);
        messageWindowId = 0;
    }
    gTasks[taskId].func = DebugTask_HandleMainMenuInput;
}
static void DebugAction_ItemMenuCancel(u8 taskId)
{
    
    Debug_DestroyItemMenu(taskId);
}

static void DebugAction_Cancel(u8 taskId)
{
    Debug_DestroyMainMenu(taskId);
}

static void DebugAdjust_NoOp(u8 taskId, s8 amount){}

static void DebugAdjust_Item(u8 taskId, s8 amount){
    struct Task *task = &gTasks[taskId];
    if (task->ITEM_ID + amount <= 0) {
        task->ITEM_ID = ITEMS_COUNT - 2;
    } else {
        task->ITEM_ID = (task->ITEM_ID + amount) % (ITEMS_COUNT - 1);
    }

    ChangeItemMenuItem(taskId);
}

static void DebugAdjust_Quantity(u8 taskId, s8 amount){
    struct Task *task = &gTasks[taskId];
    if (task->ITEM_QTY + amount <= 0) {
        task->ITEM_QTY = MAX_ITEM_QTY - 1;
    } else {
    task->ITEM_QTY = (task->ITEM_QTY + amount) % (MAX_ITEM_QTY);
    }

    RedrawListMenu(task->MENU_TASK_ID);
}

static void DebugAction_NoOp(u8 taskId){}
#endif