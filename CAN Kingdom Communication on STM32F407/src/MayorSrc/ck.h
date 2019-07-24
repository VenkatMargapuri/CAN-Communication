/* ck.h
 *
 * Mats �kerblom  Kvaser AB  1995-09-25
 * Last changed              2000-08-19
 *
 */

#ifndef __CK_H
#define __CK_H
#include "./rtos/FreeRTOS.h"
#include "../include/rtos/canmsgs.h"

#include "ckCommon.h"

#define ckSendSuccess 0
#define ckSendFailed 1 
#define ckSendBusy 2

typedef enum {
  ckStartListenForDefaultLetter = 0,
  ckStartDefault,
  ckStartNormal
} ckStartupMode;

typedef enum {
  ckKeepActMode = 0,
  ckRunning = 1,
  ckFreeze = 2,
  ckReset = 3,
} ckActMode;

/** Configuration of CAN node */
typedef struct can_cfg
{
   uint32_t baudrate;           /** Baudrate [bps] */
   uint8_t  bs1;                /** Bit segment 1 (PROP_SEG + PHASE_SEG1) */
   uint8_t  bs2;                /** Bit segment 2 (PHASE_SEG2) */
   uint8_t  sjw;                /** Resynchronisation jump width */
   uint8_t  silent;             /** Silent mode, activated when set to 1 */
} can_cfg_t;

typedef enum {
  ckKeepCommMode = 0,
  ckSilent = 1,     // The output is disabled, not even acks's are transmitted
  ckListenOnly = 2, // We listen but respond only to Kings Letters
  ckCommunicate = 3 // Normal communication
} ckCommMode;


typedef struct __attribute__((__packed__)) {
  uint32_t baseNumber;
  uint8_t nodeNumber;
  uint16_t btr; // If two bytes, LSB is btr0, MSB btr1
} ckConfig;

#ifdef __cplusplus
extern "C" {
#endif

can_cfg_t defdocCfg;

int ckInit(ckStartupMode startupMode);
void ckProcessMessage(uint32_t cmId, uint8_t *cmBuf, uint8_t cmDlc);
void ckMain(void);
uint8_t checkReactionDocument(void);
uint8_t ckSend(uint16_t doc, uint8_t *buf, uint8_t len, uint8_t sendIfFreeze);
uint8_t mayorTx(uint8_t page);
void ckSetFreeze(void);
void ckSetRunning(void);
int ckDefineLocalFolder(uint8_t folderNo, uint16_t doc, uint32_t env, uint8_t dlc,
                   uint8_t enableF, uint8_t txF, uint8_t rtrF, uint8_t extF);
uint8_t *ckGetFolder(uint8_t i, uint8_t page);   
int findSlotFolder(uint8_t folder);                
void ckListFolders(void);

int ckSaveBaseNodeNumbers(uint8_t nodeF, uint8_t baseF);
int ckLoadBaseNodeNumbers(uint8_t nodeF, uint8_t baseF, uint8_t defF);
void ckClearFolders(int mk01);
void ckDumpFolders(void);

uint8_t ckRunningQ(void);

#ifdef __cplusplus
}
#endif

// The documents handled in ck.c. More are defined in ckslave.h
#define tdocMayor 1
#define rdocKing  0
#define mayorID   1


typedef struct __attribute__((__packed__)) ckFolderS {
  uint32_t envelope;   // envInvalid if invalid.
  uint32_t doc;
  uint8_t dlc;
  uint8_t folderNo;
  uint8_t flags;
} ckFolderT;

typedef struct __attribute__((packed)) ckKingPageFiveS {
  int city;
  uint8_t actionFolderNo;
  uint8_t actionPageNo;
  uint8_t reactionFolderNo;
  uint8_t reactionPageNo;
} ckKingPageFive;

/* Bit meaning in ckFolderT.flags
 */
#define ffInUse 1     // If the Slots[]-element is used
#define ffEnabled 2   // If a message can be received or transmitted
#define ffTx 4        // If 0, it is a receive folder
#define ffRTR 8       // RTR-flag
#define ffExt 16      // Extended CAN?
#define ffDLC 32      // The dlc was specified
#define ffDocValid 64 // Set if doc is assigned a valid value


// How many folders can be defined; Valid values are 2..256.
// If it is 256, the code will be faster as no search is needed.
#define folderMaxCount 256
#define groupMaxCount 8 // How many groups we can belong to.

#define CK_CAN_CONTROLLER_82527 3 // Used in KP8

extern ckFolderT ckSlots[folderMaxCount];  // Slots[n] is associated with CAN msg-buf n.
extern uint8_t ckGroups[groupMaxCount]; // The groups we belong to or 0. The list is ended by 0.
extern ckConfig ckConf;
extern ckActMode actMode;    // The action state of the module.
extern ckCommMode commMode;  // The communiaction state of the module.

#endif
