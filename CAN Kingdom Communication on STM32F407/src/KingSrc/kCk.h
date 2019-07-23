/* kCk.h
 *
 * CAN Kingdom support.
 *
 * Mats �kerblom  1996-03-28
 * Last changed   2000-09-22
 */

#ifndef __KCK_H
#define __KCK_H

//#include "./rtos/FreeRTOS.h"
#include "ckCommon.h"
#include "stdbool.h"

typedef enum {amKeep = 0, amRun = 1, amFreeze = 2, amReset = 3} actModeT;
typedef enum {cmKeep = 0, cmSilent = 1, cmListenOnly = 2, cmCommunicate = 3} commModeT;
#define ckRTR 1
#define ckTx 2
#define ckRcv 0
#define ckEnable 4
#define ckExt 8
#define cityMaxCount 256


#if __cplusplus
extern "C" {
#endif
// Mayor Page
void ckMP1(uint8_t city, uint8_t pageNo, uint8_t actionFolderNo, uint8_t actionFormNo, uint8_t reactionFolderNo, uint8_t reactionFormNo);

typedef struct __attribute__((packed)) MayorAddressT {
  int8_t city;
  uint8_t identificationNo;
} MayorAddress;

MayorAddress mayorInfo[cityMaxCount];


void ckKP0(uint8_t city, actModeT am, commModeT cm);
void ckKP1(uint8_t city, long baseNo, uint8_t respPage, bool extF);
void ckKP2(uint8_t city, uint32_t env, uint8_t folder, bool extF);
void ckKP3(uint8_t city, uint8_t gr1, uint8_t gr2, uint8_t gr3, uint8_t gr4, uint8_t gr5, uint8_t gr6);
void ckKP4(uint8_t city, uint8_t gr1, uint8_t gr2, uint8_t gr3, uint8_t gr4, uint8_t gr5, uint8_t gr6);
void ckKP5(uint8_t city, uint8_t pageNo, uint8_t actionFolderNo, uint8_t actionFormNo, uint8_t reactionFolderNo, uint8_t reactionFormNo);
void ckKP8(uint8_t city, int brChip, uint8_t btr0, uint8_t btr1);
void ckKP9(uint8_t city, int newCity, uint8_t respPage);
void ckKP16(uint8_t city, uint8_t folder, uint16_t doc, uint8_t dlc, bool txF, bool rtrF,
            int enableF);
void ckKP20(uint8_t city, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, bool extF, uint8_t fPrimitiveNo, uint8_t fNo);

void ckDefineFolder(uint8_t city, uint8_t folder, uint32_t env, uint16_t doc, uint8_t dlc,
                    uint8_t flags);      
void KingInit();                                  

#if __cplusplus
}
#endif

#endif // __KCK_H
