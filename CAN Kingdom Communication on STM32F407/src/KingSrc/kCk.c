#include "./rtos/FreeRTOS.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include "../include/rtos/canmsgs.h"


#ifdef _Windows
  #include <windows.h>
#endif

#define kingsEnv 0

#include "kCk.h"

static void fillBuf(uint8_t *buf, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7) {
  buf[0] = b0;
  buf[1] = b1;
  buf[2] = b2;
  buf[3] = b3;
  buf[4] = b4;
  buf[5] = b5;
  buf[6] = b6;
  buf[7] = b7;  
}

// Kings page 0
void ckKP0(uint8_t city, actModeT am, commModeT cm) {
  uint8_t buf[8];
  fillBuf(buf,
     (uint8_t)city, 0, (uint8_t)am, (uint8_t)cm,
     0, // Keep current city mode
     0,0,0);
	can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);

}

// Kings page 1. Supports only standard id's.
void ckKP1(uint8_t city, long baseNo, uint8_t respPage, bool extF) {
  uint8_t buf[8];
  fillBuf(buf,
    (uint8_t)city, 1,
    respPage, // Ask the mayor to respond with this page
    0,
    LSB(baseNo), MSB(baseNo), (uint8_t)((baseNo >> 16) & 0xff),
    (uint8_t)((baseNo >> 24) & 0xff));
  if (extF)
    buf[7] |= 0x80;

  can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);
}

// Kings page 2. Always enabled.
void ckKP2(uint8_t city, uint32_t env, uint8_t folder, bool extF /* = 0 */) {
  uint8_t buf[8];

  buf[0] = (uint8_t)city;
  buf[1] = 2; // Page 2
  buf[2] = (uint8_t)(env & 255);
  buf[3] = (uint8_t)((env >> 8) & 255);
  buf[4] = (uint8_t)((env >> 16) & 255);
  buf[5] = (uint8_t)((env >> 24) & 31);
  if (extF)
    buf[5] |= 0x80; // No compressed envelopes.
  buf[6] = (uint8_t)folder;
  buf[7] = 3; // Enable the folder, assign the new envelope.

	can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);
}

// Kings page 3. Assigning a City to Groups
void ckKP3(uint8_t city, uint8_t gr1, uint8_t gr2, uint8_t gr3, uint8_t gr4, uint8_t gr5, uint8_t gr6) {
  uint8_t buf[8];
  fillBuf(buf,
    (uint8_t)city, 3, (uint8_t)gr1, (uint8_t)gr2, (uint8_t)gr3, (uint8_t)gr4, (uint8_t)gr5, (uint8_t)gr6);

  can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);
  
}

// Kings page 4. Removing a City from Groups
void ckKP4(uint8_t city, uint8_t gr1, uint8_t gr2, uint8_t gr3, uint8_t gr4, uint8_t gr5, uint8_t gr6) {
  uint8_t buf[8];
  fillBuf(buf,
    (uint8_t)city, 4, (uint8_t)gr1, (uint8_t)gr2, (uint8_t)gr3, (uint8_t)gr4, (uint8_t)gr5, (uint8_t)gr6);
    	
  can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);
}

void ckKP5(uint8_t city, uint8_t pageNo, uint8_t actionFolderNo, uint8_t actionFormNo, uint8_t reactionFolderNo, uint8_t reactionFormNo)
{
  uint8_t buf[8];
  fillBuf(buf, city, pageNo, actionFolderNo, actionFormNo, 0, reactionFolderNo, reactionFormNo, 0);
  can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);      
}

// Kings page 8. Change the baudrate.
void ckKP8(uint8_t city, int brChip, uint8_t btr0, uint8_t btr1) {
  uint8_t buf[8];
  fillBuf(buf,
    (uint8_t)city, 8, (uint8_t)brChip, 0, (uint8_t)btr0, (uint8_t)btr1, 0,0);

  can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);    
}

// Kings page 9. Change of City physical address
void ckKP9(uint8_t city, int newCity, uint8_t respPage /* = 255 */) {
  uint8_t buf[8];
  fillBuf(buf,
    (uint8_t)city, 9,
    respPage,  // Mayor's response page
    (uint8_t)newCity,
    0, 0, 0, 0);
	can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);
}

// Kings page 20. Set filters.
void ckKP20(uint8_t city, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, bool extF, uint8_t fPrimitiveNo, uint8_t fNo) {
  uint8_t buf[8];
  b3 &= 0x1f;
  if (extF)
    b3 |= 0x80;
  fillBuf(buf,
     (uint8_t)city, 20,
     b0,b1,b2,b3,
     (uint8_t)fPrimitiveNo, (uint8_t)fNo);
	can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);
}


// Kings page 16
void ckKP16(uint8_t city, uint8_t folder, uint16_t doc, uint8_t dlc, bool txF, bool rtrF,
            int enableF) {
  uint8_t buf[8];

  buf[0] = (uint8_t)city;
  buf[1] = 16; // Page 16
  buf[2] = (uint8_t)folder;
  buf[3] = 0x80 + dlc;
  if (rtrF)
    buf[3] |= 0x40;
  buf[4] = (uint8_t)(0xb0 | (enableF ? 64 : 0) | (txF ? 1 : 0));  // Enable, remove old, insert new document
  buf[5] = (uint8_t)(doc >> 8);
  buf[6] = (uint8_t)(doc & 255);
  buf[7] = 0;
	int i = can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);
  while(i == -1){
    can_transmit(CAN1, kingsEnv, false, false, 8, (void*) &buf);
  }
}

void ckDefineFolder(uint8_t city, uint8_t folder, uint32_t env, uint16_t doc, uint8_t dlc,
                    uint8_t flags) {
  ckKP2(city, env, folder, flags & ckExt);
  ckKP16(city, folder, doc, dlc, flags & ckTx, flags & ckRTR, flags & ckEnable);
}


/* ToDo: The method should be moved to a Mayor class */
//Message from the mayor with id != 0
void ckMP1(uint8_t city, uint8_t pageNo, uint8_t actionFolderNo, uint8_t actionFormNo, uint8_t reactionFolderNo, uint8_t reactionFormNo)
{
  uint8_t buf[8];
  fillBuf(buf, city, pageNo, actionFolderNo, actionFormNo, 0, reactionFolderNo, reactionFormNo, 0);
  can_transmit(CAN1, 2, false, false, 8, (void*) &buf);      
}