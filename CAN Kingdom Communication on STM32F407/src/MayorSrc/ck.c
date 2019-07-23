/* ck.c
 *
 * Mats �kerblom  Kvaser AB  1995-09-25
 * Last changed              2000-09-25
 *
 */

#include "./rtos/FreeRTOS.h"
#include <stdio.h>
#include <string.h>
#include "ck.h"
#include "../include/rtos/canmsgs.h"

//#include "ckhal.h"

/// From CkHal.c

// Set communication mode. Depending on hardware, some modes may not be supported, in 
// which case a higher mode is selected.
void canSetCommMode(ckCommMode commMode) {
  switch(commMode) {
    case ckSilent:      // The output is disabled, not even acks's are transmitted.
     // can_bus_off(can_fd);
      break;
    case ckListenOnly:  // We listen but respond only to Kings Letters.
    case ckCommunicate: // Normal communication.
    //  can_bus_on(can_fd);
      break;
    default:
      break;
  }
}

// The variables ckSlots[], ckGroups[] and ckConf are not static so they can
// be read by the application for debug purposes. They should however
// never be modified or used in any aother way, and is not declared in
// any header file.

// We store the folders in 'slots'; the index is not necessary the same
// as the folder number unless there are room for 256 folders.
ckFolderT ckSlots[folderMaxCount];  // ckSlots[n] is associated with CAN msg-buf n.
uint8_t ckGroups[groupMaxCount]; // The groups we belong to or 0. The list is ended by 0.
ckConfig ckConf;
ckKingPageFive ckPageFive[folderMaxCount];

ckActMode actMode;    // The action state of the module.
ckCommMode commMode;  // The communiaction state of the module.
static uint32_t startTime;     // When we was started.
struct s_canmsg message;
uint8_t actReactMode;


static uint8_t ckRcvF;        // Any kind of message is received.

typedef enum {
   BOOT_MSG_NONE = 0xFF,
   BOOT_MSG_DEFAULT_MSG_RECEIVED = 0xAA,
   BOOT_MSG_STAY_IN_BOOT = 0xBB,
} boot_msg_t;

static boot_msg_t __attribute__((section (".boot_com"))) boot_msg = { BOOT_MSG_NONE};

// --- Prototypes ----------------------------------------------
static void goSilent(void);
static void goListenOnly(void);
static void goCommunicate(void);
//static void ckSetFreeze(void);


static int findSlotDoc(uint16_t docNo, uint8_t txF);
static int findSlotEnv(uint32_t env, uint8_t extF, uint8_t txF);
//int findSlotFolder(uint8_t folder);
int findSlotEnvRTR(uint32_t env, uint8_t extF);

static void KingsDoc(uint8_t *msgBuf, uint8_t msgLen);
/*int findFolderDoc(uint16_t docNo, uint8_t txF);
int findFolderEnv(uint32_t env, uint8_t extF);
int findFolderFolder(uint8_t folder);*/
static int createSlot(uint8_t folder, uint16_t doc, uint8_t docF, uint32_t env, uint8_t extF);
static int updateSlot(int i);
static void deleteSlot(int i);
static void KP0(uint8_t *msg);
static void KP1(uint8_t *msg);
static void updateMayor(void);
static void KP2(uint8_t *msg);
static void KP3(uint8_t *msgBuf);
static void KP4(uint8_t *msgBuf);
static void KP5(uint8_t *msgBuf);
static void hifi(uint8_t *msgBuf);
static void KP8(uint8_t *msgBuf);
static void KP9(uint8_t *msgBuf);
static void KP16(uint8_t *msg);
static void KP21(uint8_t *msg);
static void KP22(uint8_t *msg);
static void KP129(uint8_t *msg);
//static void KP130(uint8_t *msg);
//static int ckGetFolder(uint8_t i, uint8_t page);

//extern void ckappSetActionMode(ckActMode am);
//extern void ckappDispatchFreeze(uint16_t doc, uint8_t *msgBuf, uint8_t msgLen);
//extern void ckappDispatchRTR(uint16_t doc, bool txF);
//extern int ckappMayorsPage(uint8_t page, uint8_t *buf);
//extern void ckappDispatch (uint16_t doc, uint8_t *msgBuf, uint8_t msgLen);

can_cfg_t defdocCfg = {125000, 13, 2, 1, 0};
#define defdocEnv 2031L
#define defdocData "\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa"
uint8_t buf[8];


void goSilent(void) {
  commMode = ckSilent;
  canSetCommMode(ckSilent);
}
void goListenOnly(void) {
  commMode = ckListenOnly;
  canSetCommMode(ckListenOnly);
}
void goCommunicate(void) {
  commMode = ckCommunicate;
  canSetCommMode(ckCommunicate);
}

// Sets the module to freeze mode. Can be called by some external function,
// but only after startModule() has been called.
void ckSetFreeze(void) {
  actMode = ckFreeze;
  //ckappSetActionMode(actMode);
}

void ckSetRunning(void) {
  actMode = ckRunning;
 // ckappSetActionMode(actMode);
}

uint8_t ckRunningQ(void) {
  return actMode == ckRunning;
}

/* Return an EAN code that will be included in the
* Mayors' page 0. A pointer to a static 5 character string
* should be returned (the value is 40-bit).
* This is usually a constant for a certain implementation.
*/
uint8_t *ckhalGetEAN(void) {
  return (uint8_t*)"\1\2\3\4\5";
}

/* Return the serial number. It is 40-bit, LSB first.
* This is usually read from NVRAM.
*/
uint8_t *ckhalGetSerial(void) {
  return (uint8_t*)"\1\0\0\0\0";
}

/* Sends a CAN message with the city identification number */
uint8_t mayorInitTx(uint8_t mayorId){
  uint8_t buf[8];
  memcpy(buf+2, ckhalGetEAN(), 5);
  buf[0] = 0;
  buf[1] = mayorId;
  buf[7] = 0;
  can_transmit(CAN1, mayorId, false, false, 8, buf);
}

// -------------------------------------------------------------
// Initialize the module.
// If startupMode is ckStartDefault or ckStartListenForDefaultLetter and a default
// letter is received, 1 is returned, otherwise 0.
int ckInit (ckStartupMode startupMode)
{
   int i;
   int canSetupDone = 0;
   can_cfg_t can_cfg;
   

   for (i = 0; i < folderMaxCount; i++)
   {
      ckSlots[i].flags = 0; // Clears the ffInUse-flag (among others).
      ckPageFive[i].city = -1;      
   }
   for (i = 0; i < groupMaxCount; i++)
   {
      ckGroups[i] = 0; // Unused.
   }

   ckRcvF = 0;
   startTime = xTaskGetTickCount();//ckhalReadTimer ();
   if ((startupMode == ckStartListenForDefaultLetter) || (startupMode == ckStartDefault))
   {
      canSetupDone = 1;
   }
   if (startupMode == ckStartListenForDefaultLetter)
   {
      goListenOnly ();

#ifdef NO_BOOT_MSG
      startupMode = ckStartNormal;
      for (;;)
      {
         uint32_t id;
         uint8_t dlc;
         uint8_t buf[8];
         if (canReadMessage (&id, buf, &dlc) == canOK && id == defdocEnv
               && dlc == 8 && memcmp (buf, defdocData, 8) == 0)
         {
            // A default letter is received!
            startupMode = ckStartDefault;
            ckRcvF = 1;
            break;
         }
         if (ckhalReadTimer () - startTime > tick_from_ms (200))
         {
            break;
         }
         ckhalDoze ();
      }
#else
      if (boot_msg == BOOT_MSG_DEFAULT_MSG_RECEIVED)
      {
         startupMode = ckStartDefault;
      }
      else
      {
         startupMode = ckStartNormal;
      }
#endif
   }
   if (startupMode == ckStartNormal)
   {
      // if (ckhalGetConfig (&ckConf))
      // {
      //    ckhalGetDefaultConfig (&ckConf);
      // }
      // if (!canSetupDone)
      // {
      //    ckhal_btr_to_can_cfg (ckConf.btr, &can_cfg);
      //    canSetup (&can_cfg);
      // }
      // else
      // {
      //    ckhal_btr_to_can_cfg (ckConf.btr, &can_cfg);
      //    canSetBaudrate (&can_cfg);
      // }
   }
   else
   {
     // ckhalGetDefaultConfig (&ckConf); // Do not use the btr in ckConf, but remain at 125 kbps
   }

   ckClearFolders (1); // Create folder 0 and 1 after clearing all.
   if (startupMode == ckStartNormal)
   {
      //ckhalLoadFolders (); // create even them if the NVRAM is invalid.
      goCommunicate (); // Open the CAN-output so we can send Ack's
      updateMayor ();
      ckSetRunning ();
      mayorInitTx(mayorID);
   }
   else
   {
      goListenOnly (); // Open the CAN-output so we can send Ack's
      updateMayor ();
      ckSetRunning ();
      mayorInitTx(mayorID);
   }

   return startupMode == ckStartDefault;
}

// Defines a folder slot. Returns 0 if ok, non-zero if e.g. there is no more
// room.
// Dlc is left undefined if dlc<0.
int ckDefineLocalFolder(uint8_t folderNo, uint16_t doc, uint32_t env, uint8_t dlc,
                   uint8_t enableF, uint8_t txF, uint8_t rtrF, uint8_t extF) {
  int i;

  i = createSlot(folderNo, doc, 1, env, extF);
  if (i < 0)
    return 1; // No more room.

  if (enableF)
    setBM(ckSlots[i].flags, ffEnabled);
  if (txF)
    setBM(ckSlots[i].flags, ffTx);
  if (rtrF)
    setBM(ckSlots[i].flags, ffRTR);
  if (dlc <= 8) {
    ckSlots[i].dlc = dlc;
    setBM(ckSlots[i].flags, ffDLC);
  }
  i = updateSlot(i);

  return 0;
}

uint8_t checkReactionDocument()
{
   if (CAN_RF0R(CAN1) & CAN_RF0R_FMP0_MASK) 
		{		
        	message = receive(0); 
         // if(message.data[0] == mayorID){
            ckProcessMessage (message.msgid, (void *)&message.data, message.length);                    
         // }         
   	}

     if (CAN_RF1R(CAN1) & CAN_RF1R_FMP1_MASK) 
		{		
        	message = receive(1);
          //if(message.data[0] == mayorID){
            ckProcessMessage (message.msgid, (void *)&message.data, message.length);                    
          //}  
   	}  

    if(message.data[7] == 123)
    {
      return 1;
    } 
    else
    {
      return 0;
    }
}

/* Check if any CAN messages have been received; if so, call ckProcessMessage().
 * Either this function should be called periodically by the main program,
 * or ckProcessMessage should be called whenever a CAN message is reveiced.
 */
void ckMain (void)
{
   uint32_t cmId;
   uint8_t cmDlc;
   uint8_t cmBuf[8];
     
     if (CAN_RF0R(CAN1) & CAN_RF0R_FMP0_MASK) 
		{		

        	message = receive(0);  
         // if(message.data[0] == mayorID){
            ckProcessMessage (message.msgid, (void *)&message.data, message.length);                    
         // }                
   	}
     
     if (CAN_RF1R(CAN1) & CAN_RF1R_FMP1_MASK) 
		{		
        	message = receive(1);          
         // if(message.data[0] == mayorID){
            ckProcessMessage (message.msgid, (void *)&message.data, message.length);                    
         // }
   	}            
}

uint8_t ShouldMessageReact(uint8_t *msg)
{
  for(int i = 0; i < folderMaxCount; i++)
  {    
    if(ckPageFive[i].actionFolderNo == msg[2])
    {
      for(int j = 0; j < folderMaxCount; i++)
      {
        if(ckPageFive[j].actionPageNo == msg[3])
        {
          return 1;
        }
      }
    }
  }
  return -1;
}

void ckProcessActReactMessage(uint8_t *msg)
{
  uint8_t buf[8];

    int actionFolder = findSlotFolder(msg[2]);
    if(actionFolder == -1){
        buf[7] = 255;
      (void)ckSend(tdocMayor, buf, 8, 1);      
    }else{
      uint8_t* actionDocument = ckGetFolder(actionFolder, 161);            
      if(actionDocument[6] != msg[3]){
        (void)ckSend(tdocMayor, buf, 8, 1);
      }else{
        int reactionFolder = findSlotFolder(msg[2]);
        if(reactionFolder == -1){
          (void)ckSend(tdocMayor, buf, 8, 1);
        }else{
          uint8_t* reactionDocument = ckGetFolder(reactionFolder, 161);
          if(reactionDocument[6] != msg[3]){
            (void)ckSend(tdocMayor, buf, 8, 1);
          }else{
            (void)ckSend(tdocMayor, (void *)reactionDocument, 8, 1);
          }
        }
      }
    }
    memset(buf, 0, 8 * sizeof(buf[0]));
}

/* Handle a received CAN message. If it is a message in a folder
 * for other than the Kings document, the apropriate handler is called.
 * This routine may be called from the interrupt handler or the main program.
 * Or call ckMain() to read the CAN message.
 */
void ckProcessMessage(uint32_t cmId, uint8_t *cmBuf, uint8_t cmDlc) {
  int cmSlot;

  ckRcvF = 1;
  if (cmId & envRTR) {
    // An RTR message.
    cmSlot = findSlotEnvRTR(cmId, (cmId & envExtended) != 0);
    if (cmSlot >= 0 && (ckSlots[cmSlot].flags & ffEnabled)) {
      if (ckSlots[cmSlot].doc == tdocMayor)
        (void)mayorTx(0);  // Will call ckSend()
      else if (actMode == ckRunning){
        //  ckappDispatchRTR(ckSlots[cmSlot].doc, ckSlots[cmSlot].flags & ffTx); // Might call ckSend() if ffTx
      }      
    }
  } else {
    // Does the message belong to a receive folder?
    cmSlot = findSlotEnv((cmId & envMask), (cmId & envExtended) != 0, 0);
    if (cmSlot >= 0 && (ckSlots[cmSlot].flags & ffEnabled)) {
      uint8_t doc = ckSlots[cmSlot].doc;
      if (doc == rdocKing)
        KingsDoc(cmBuf, cmDlc);
      else {
        if (actMode == ckRunning){
           if(actReactMode == 1){
             if(ShouldMessageReact(cmBuf) == 1){
               ckProcessActReactMessage(cmBuf);
             }             
           }           
        }        
        else if (actMode == ckFreeze){
          //  ckappDispatchFreeze(doc, cmBuf, cmDlc);
        }        
      }
    }
  }
}

/* Handles the Kings Document.
 */
void KingsDoc(uint8_t *msgBuf, uint8_t msgLen) {
  uint8_t ad = msgBuf[0];
  uint8_t page = msgBuf[1];
  int i;

  if (msgLen != 8)
    return;

  // Is it meant for us?
  if (ad && // All cities belongs to group 0.
      ad != ckConf.nodeNumber) {
    for (i = 0; i < groupMaxCount; i++) {
      if (ckGroups[i] == 0)
        return; // End of group list
      if (ckGroups[i] == ad)
        break;
    }
    if (i == groupMaxCount)
      return;
  }

  switch(page) {
    case 0:
      KP0(msgBuf);
      break;
    case 1:
      KP1(msgBuf);
      break;
    case 2:
      KP2(msgBuf);
      break;
    // case 3:
    //   KP3(msgBuf);
    //   break;
    // case 4:
    //   KP4(msgBuf);
    //   break;
        case 5: 
         // hifi(msgBuf);
         KP5(msgBuf);         
          break;
    // case 8:
    //   KP8(msgBuf);
    //   break;
    // case 9:
    //   KP9(msgBuf);
    //   break;
     case 16:
       KP16(msgBuf);
       break;
    // case 21:
    //   KP21(msgBuf);
    //   break;
    // case 22:
    //   KP22(msgBuf);
    //   break;
    // case 129:
    //   KP129(msgBuf);
    //   break;
/*  case 130:
      KP130(msgBuf);
      break; */
    default:
      break;
  }
}

/* Locates the slot index (0..slotsMaxCount-1) containing document docNo
 * or -1 if not found. There might be two slots with the same document
 * number, one being a receive and one being a transmit folder.
 */
int findSlotDoc(uint16_t docNo, uint8_t txF) {
  int i;
  if (txF) {
    for (i = 0; i < folderMaxCount; i++)
      if (ckSlots[i].doc == docNo &&
          (ckSlots[i].flags & (ffInUse | ffTx)) == (ffInUse | ffTx))
        return i;
  } else {
    for (i = 0; i < folderMaxCount; i++)
      if (ckSlots[i].doc == docNo &&
          (ckSlots[i].flags & (ffInUse | ffTx)) == (ffInUse))
        return i;
  }
  return -1;
}

/* Locates the slot containing envelope env.
* With txF one should select if it is a transmit or a receive
* folder.
*/
int findSlotEnv(uint32_t env, uint8_t extF, uint8_t txF) {
  int i;
  uint8_t flags = ffInUse;  // Which flag-bits should be set.
  if (extF)
    flags |= ffExt;
  if (txF)
    flags |= ffTx;
  for (i = 0; i < folderMaxCount; i++)
    if (ckSlots[i].envelope == env &&
        (ckSlots[i].flags & (ffExt | ffInUse | ffTx)) == flags)
      return i;
  return -1;
}

/* Locates the slot containing envelope env where RTR is set.
* Should there be one receive and one transmit folder with the same
* CAN id and both with the RTR bit set, only one will be returned.
*/
int findSlotEnvRTR(uint32_t env, uint8_t extF) {
  int i;
  uint8_t flags = ffInUse | ffRTR;  // Which flag-bits should be set.
  if (extF)
    flags |= ffExt;
  for (i = 0; i < folderMaxCount; i++)
    if (ckSlots[i].envelope == env &&
        (ckSlots[i].flags & (ffExt | ffInUse | ffRTR)) == flags)
      return i;
  return -1;
}

/* Locates the slot containing folder no folder.
*/
int findSlotFolder(uint8_t folder) {
#if folderMaxCount == 256
  if (ckSlots[folder].flags & ffInUse)
    return folder;
  else
    return -1;
#else
  int i;
  for (i = 0; i < folderMaxCount; i++)
    if (ckSlots[i].folderNo == folder && (ckSlots[i].flags & ffInUse))
      return i;
  return -1;
#endif
}

/* Searches for a unused slot and assigns folder, doc, env to it.
 * Returns the slot-no, or -1 if no one was available.
 *
 * If folder folder already is defined, its index is returned (and its
 * doc/env are updated).
 *
 * Assigns InUse and maybe extF to flags.
 */
int createSlot(uint8_t folder, uint16_t doc, uint8_t docF, uint32_t env, uint8_t extF) {
  int i;

#if folderMaxCount == 256
  i = folder;
#else
  int oldI = -1, newI = -1;
  for (i = folderMaxCount-1; i >= 0; i--)
    if (ckSlots[i].flags & ffInUse) {
      if (ckSlots[i].folderNo == folder) {
        oldI = i;
        break;
      }
    } else
      newI = i;

  if (oldI >= 0)
    i = oldI; // Use the old slot; folder is already assigned.
  else {
    if (newI < 0)
      return -1;  // There was no unused slot left.
    i = newI;
  }
#endif
  ckSlots[i].folderNo = folder;

  ckSlots[i].doc = doc;
  ckSlots[i].envelope = env;
  ckSlots[i].flags = ffInUse;
  if (docF)
    setBM(ckSlots[i].flags, ffDocValid);
  if (extF)
    setBM(ckSlots[i].flags, ffExt);
  return i;
}

// Called when a slot/folder is updated. Could be used to maintain
// filters etc or maybe sort the ckSlots[] vector.
// Should return the slot index which might change if the slot is moved.
int updateSlot(int i) {
  return i;
}


/* Delete the slot number i; i.e. clear the ffInUse-bit and disable the
 * can message object.
 */
void deleteSlot(int i) {
  if (i >= 0) {
    ckSlots[i].flags = 0; // Clears ffInUse
    ckSlots[i].envelope = envInvalid;
  }
}


/* Kings page 0. Terminating the setup phase.
 *
 */
void KP0(uint8_t *msg) {
  ckActMode newActMode = (ckActMode)( msg[2] & 0x03);
  ckCommMode newCommMode = (ckCommMode)(msg[3] & 0x03);

  if (newActMode != ckKeepActMode) {
    actMode = newActMode;
    if (actMode == ckReset){
      // ckhalReset();
    }     
    else{
    //  ckappSetActionMode(actMode);
    }
    
  }

  if (newCommMode != ckKeepCommMode) {
    commMode = newCommMode;
    if (newCommMode == ckSilent)
      goSilent();
    else if (newCommMode == ckListenOnly)
      goListenOnly();
    else if (newCommMode == ckCommunicate)
      goCommunicate();
  }
}

/* Kings page 1. Provides the base number and asks for Mayor's response.
 */
void KP1(uint8_t *msg) {
  uint32_t baseNumberIn;
  baseNumberIn = (uint32_t)msg[4]+((uint32_t)msg[5]<<8)+((uint32_t)msg[6]<<16)+
                 ((uint32_t)(msg[7]&0x1f)<<24);

  if (baseNumberIn) {
    if (baseNumberIn == 0x1fffffffL)
      ckConf.baseNumber = envInvalid;
    else {
      ckConf.baseNumber = baseNumberIn;
      if (msg[7] & 0x80)
        ckConf.baseNumber |= envExtended;
    }
    updateMayor();
  }

  if (msg[2] != 0xff)
    (void)mayorTx(msg[2]);
}

/* Transmits the mayor's document, page p. Returns non-zero if it was
 * impossible (tx-object busy, unknown page).
 */
uint8_t mayorTx(uint8_t page) {
  if((ckConf.baseNumber != envInvalid) &&
     commMode != ckSilent) {
    // A response is requested (and can be made).
    // Locate the folder to use when sending the response.
    uint8_t buf[8];
    memset(buf, 0, 8);
    if (page >= 128) {
     // if (ckappMayorsPage(page, buf))
     if(false)
        return 1; // Unknown page
    } else {
      switch(page) {
        case 0:  // City identification, EAN-13 Code.
          memcpy(buf+2, ckhalGetEAN(), 5);
          break;
        case 1:  // City identification, Serial Number.
          memcpy(buf+2, ckhalGetSerial(), 5);
          break;
        default:
          return 1;
      }
    }
    buf[0] = 0;
    buf[1] = page;



    return ckSend(tdocMayor, buf, 8, 1);
  } else
    return 0;
}


/* Kings page 2. Assigning or expelling an envelope to/from a folder.
 *
 * If an envelope is deassigned and the slots document is undefined, the
 * slot/folder is released.
 */
void KP2(uint8_t *msg) {
  uint32_t env = (uint32_t)msg[2]+((uint32_t)msg[3]<<8)+((uint32_t)msg[4]<<16)+
              ((uint32_t)(msg[5]&0x1f)<<24);
  uint8_t extF = msg[5] & 0x80,
//      comprF = msg[5] & 0x40,
        folderNo = msg[6],
        enableF = msg[7] & 1,
        cmd = (msg[7] >> 1); // We demand the reserved bits to be zero.
  int fs,i;
  uint8_t flags, done;

  switch(cmd) {
    case 0: // Keep current assignment, ignore folderNo, only regard enableF.
      // We scan all folders/slots if there should be more than one folder
      // with envelope env
      flags = ffInUse;  // Which flag-bits should be set.
      if (extF)
        flags |= ffExt;
      for (i = 0; i < folderMaxCount; i++)
        if (ckSlots[i].envelope == env &&
            (ckSlots[i].flags & (ffExt | ffInUse)) == flags) {
          if (enableF)
            setBM(ckSlots[i].flags, ffEnabled);
          else
            clrBM(ckSlots[i].flags, ffEnabled);
        }
      break;

    case 1: // Assign the envelope env to the folder folderNo.
            // Opposed to what is written in the CAN Kingdom Spec,
            // we do only allow one envelope per folder.
      fs = findSlotFolder(folderNo);
      if (fs < 0)
        fs = createSlot(folderNo, 0, 0, envInvalid, 0);
      if (fs < 0)
        return; // No more room.
      if (fs >= 0) {
        ckSlots[fs].envelope = env;
        if (extF)
          setBM(ckSlots[fs].flags, ffExt);
        else
          clrBM(ckSlots[fs].flags, ffExt);
        if (enableF)
          setBM(ckSlots[fs].flags, ffEnabled);
        else
          clrBM(ckSlots[fs].flags, ffEnabled);
        fs = updateSlot(fs);
      }
      break;

    case 3: // Transfer the envelope env to to the folder folderNo.
            // The old assignment is canceled.
            // We search for a folder with envelope env and move the contens
            // to folder folderNo. If there are severak folders with envelope
            // env, all but one of them are deleted.
      fs = findSlotFolder(folderNo);
      if (fs < 0)
        fs = createSlot(folderNo, 0, 0, envInvalid, 0);
      if (fs < 0)
        return; // No more room.
      done = 0;
      flags = ffInUse;  // Which flag-bits should be set.
      if (extF)
        flags |= ffExt;
      for (i = 0; i < folderMaxCount; i++)
        if ((ckSlots[i].flags & (ffInUse | ffExt)) == flags &&
            ckSlots[i].envelope == env) {
          if (!done) {
            if (i != fs) {
#if folderMaxCount == 256
              memcpy(&ckSlots[fs], &ckSlots[i], sizeof(ckSlots[fs]));
#else
              if (fs >= 0)
                deleteSlot(fs);
              ckSlots[i].folderNo = folderNo;
#endif
              done = 1;
            }
          } else
            deleteSlot(i);
        }
      break;

    case 2: // Expel this envelope from any assignment. Folder No is ignored.
            // The slot remains, i.e. the connection document-folder is kept.
      if (!enableF) { // AAD has to be 100.
        flags = ffInUse;  // Which flag-bits should be set.
        if (extF)
          flags |= ffExt;
        for (i = 0; i < folderMaxCount; i++)
          if ((ckSlots[i].flags & (ffInUse | ffExt)) == flags &&
              ckSlots[i].envelope == env) {
            if (!(ckSlots[i].flags & ffDocValid))
              deleteSlot(i);
            else {
              clrBM(ckSlots[i].flags, ffEnabled);
              ckSlots[i].envelope = envInvalid;
              i = updateSlot(i);
            }
          }
      }
      break;

    default:
      break;
  }
}

// /* Kings page 3. Assigning the city to groups.
//  */
// void KP3(uint8_t *msgBuf) {
//   int i, j;

//   for (i = 2; i < 8; i++) {
//     if (msgBuf[i] == 0)
//       break; // The end.
//     // Are we already a member of group msg[i]?
//     for (j = 0; j < groupMaxCount; j++)
//       if (ckGroups[j] == msgBuf[i])
//         break;
//     if (j == groupMaxCount) {
//       // No. Add the group. Find an empty position in Groups[].
//       for (j = 0; j < groupMaxCount; j++)
//         if (ckGroups[j] == 0)
//           break;
//       if (j < groupMaxCount) // Are there any room?
//         ckGroups[j] = msgBuf[i];
//     }
//   }
// }


/* Kings page 4. Removing the city from groups.
 * As a 0 end the list, we might need to move a group number.
 */
void KP4(uint8_t *msgBuf) {
  int i, j;

  for (i = 2; i < 8; i++) {
    if (msgBuf[i] == 0)
      break; // The end.
    // Are we a member of group msg[i]?
    for (j = 0; j < groupMaxCount; j++) {
      if (ckGroups[j] == 0 || ckGroups[j] == msgBuf[i])
        break;
    }
    if (j < groupMaxCount && ckGroups[j]) {
      // Yes. Delete it.
      ckGroups[j] = 0;
      // Is there any more groups in the list after position j?
      // If so, move it to the newly freed position.
      for (i = j+1; i < groupMaxCount; i++)
        if (ckGroups[i] == 0)
          break;
      if (i < groupMaxCount && i != j+1) {
        ckGroups[j] = ckGroups[i-1];
        ckGroups[i] = 0;
      }
    }
  }
}  

void KP5(uint8_t *msgBuf) {
   uint8_t ckPageFiveIndex;
    for(ckPageFiveIndex = 0; ckPageFiveIndex < folderMaxCount; ckPageFiveIndex++){
      if(ckPageFive[ckPageFiveIndex].city == -1){
        break;
      }
    }
    ckPageFive[ckPageFiveIndex].city = msgBuf[0];
    ckPageFive[ckPageFiveIndex].actionFolderNo = msgBuf[2];
    ckPageFive[ckPageFiveIndex].actionPageNo = msgBuf[3];
    ckPageFive[ckPageFiveIndex].reactionFolderNo = msgBuf[5];
    ckPageFive[ckPageFiveIndex].reactionPageNo = msgBuf[6]; 
    
    actReactMode = 1;
}  

// // Save changed baud rate settings to NVRAM. Doesn't change the actual
// // Baudrate (use KP129 for this).
// void KP8 (uint8_t *msg)
// {
//    if (msg[2] == CK_CAN_CONTROLLER_82527)
//    {
//       ckConf.btr = (msg[5] << 8) + msg[4];

//      // ckhalSaveConfig(&ckConf);
//    }
// }

// /* Kings page 9. Change the city's node number (in RAM only).
//  */
// void KP9(uint8_t *msgBuf) {
//   ckConf.nodeNumber = msgBuf[3];
//   updateMayor();
//   if (msgBuf[2] != 0xff)
//     (void)mayorTx(msgBuf[2]);
// }

/* Kings page 16. Setting the folder label and/or placing a document
 * into a folder.
 *
 * If the document is removed and the envelope of the slot is envInvalid, the
 * slot/folder is released.
 *
 * We do not allow the same document to be placed in two tx-folders.
 */
void KP16(uint8_t *msg) {
  uint8_t folder = msg[2];
  uint8_t txF = msg[4] & 0x1,
        insertF = msg[4] & 0x10,
        removeF = msg[4] & 0x20,
        enableF = msg[4] & 0x40;
  uint16_t doc = (msg[5] << 8) + msg[6];
  int i;

  if (folder < 2)
    return;

  i = findSlotFolder(folder);

  if (removeF && !insertF && i < 0) {
    // We should remove the current document without inserting anything in
    // its place. As the envelope is undefined, the slot should be deleted.
    // If so, we need not regard line 3 (DLC/RTR) or enableF and do not want
    // to create a slot just to be able to assign the flags.
    return;
  }

  if (i < 0)
    i = createSlot(folder, 0, 0, envInvalid, 0);
  if (i < 0)
    return; // No more room.

  if (msg[3]) {
    uint8_t dlc = msg[3] & 0x0f;
    if (dlc <= 8) {
      ckSlots[i].dlc = dlc;
      setBM(ckSlots[i].flags, ffDLC);
    } else
      clrBM(ckSlots[i].flags, ffDLC);
    if (msg[3] & 0x40)
      setBM(ckSlots[i].flags, ffRTR);
    else
      clrBM(ckSlots[i].flags, ffRTR);
  }
  if (msg[4]) {
    if (insertF) {
      // Insert the document on line 5 and 6.
      ckSlots[i].doc = doc;
      setBM(ckSlots[i].flags, ffDocValid);
    } else if (removeF)
      clrBM(ckSlots[i].flags, ffDocValid); // removeF && !insertF

    if (enableF) // Enable/disable
      setBM(ckSlots[i].flags, ffEnabled);
    else
      clrBM(ckSlots[i].flags, ffEnabled);

    if (txF) // Transmit/receive
      setBM(ckSlots[i].flags, ffTx);
    else
      clrBM(ckSlots[i].flags, ffTx);
  }
  if (ckSlots[i].envelope == envInvalid && !(ckSlots[i].flags & ffDocValid))
    deleteSlot(i);
  else
    i = updateSlot(i); // If tx/rcv status changed
}

// uint32_t respEnv = envInvalid;
// /* Kings page 21.
//  */
// void KP21(uint8_t *msg) {
//   uint8_t *p1,*p2;
//   int i;

//   switch(msg[1]) {
//     case 0:
//       respEnv = (uint32_t)msg[3]+((uint32_t)msg[4]<<8)+((uint32_t)msg[5]<<16)+
//                 ((uint32_t)(msg[6]&0x1f)<<24);
//       break;
//     case 1:
//       // We should respond if our serial number is less than or equal to the one given in the message
//       p1 = ckhalGetSerial()+4; // points to the MSB
//       p2 = msg+3+4;
//       for (i = 0; i < 5; i++) {
//         if (*p1 > *p2)
//           return;
//         else if (*p1-- != *p2--)
//           break;
//       }
//       // The serial number matched. Transmit a response
//       (void)canWriteMessage(respEnv, NULL, 0);
//       break;
//     default:
//       break;
//   }
// }

// /* Kings page 22.
//  */
// void KP22(uint8_t *msg) {
//   uint8_t *p1,*p2;
//   int i;

//   p1 = ckhalGetSerial();
//   p2 = msg+3;
//   for (i = 0; i < 5; i++) {
//     if (*p1++ != *p2++)
//       return;
//   }
//   ckConf.nodeNumber = msg[2];
//   updateMayor();
// }

// // Functions used by KP129
// #define kp129SaveBaseNo 0
// #define kp129SaveNodeNo 1
// #define kp129SaveFolders 2
// #define kp129LoadFolders 3
// #define kp129ClearFolders 4
// #define kp129GetFolder 5
// #define KP129UseSavedBaudrate 6
// //#define kp129SetSystemSignature 7
// #define kp129SetStartupMode 8
// //#define kp129FormatNVRAM 42


// void KP129 (uint8_t *msg)
// {
//    // {nodeNo, 129, cmd, -,-,-,-,-}
//    switch (msg[2])
//    { // Function
//       case kp129SaveBaseNo: // Save the base number to NVRAM
//          (void)ckSaveBaseNodeNumbers (0, 1);
//          break;
//       case kp129SaveNodeNo: // Save the node number to NVRAM
//          (void)ckSaveBaseNodeNumbers (1, 0);
//          break;
//       case kp129SaveFolders: // Save the folders to NVRAM
//          (void)ckhalSaveFolders ();
//          break;
//       case kp129LoadFolders: // Load the folders from NVRAM
//          // We don't allow this although it is always possible to reset the module.
//          // But the king may have turned off this function for security reasons.
//          (void)ckhalLoadFolders ();
//          break;
//       case kp129ClearFolders: // Clear all folders except 0 and 1
//          ckClearFolders (1); // which are reinitialized.
//          break;
//       case kp129GetFolder: // Get folder information for the i'th slot
//          // {nodeNo,129,kp129GetFolder,slotNo,respPage,0,0,0}
//          (void)ckGetFolder (msg[3], msg[4]); // 160/161, slot no
//          break;
//       case KP129UseSavedBaudrate:
//          {
//             ckConfig cfg;
//             can_cfg_t can_cfg;
//             //if (!ckhalGetConfig (&cfg))
//             //{
//              //  ckhal_btr_to_can_cfg (cfg.btr, &can_cfg);
//              //  canSetBaudrate (&can_cfg);
//             //}
//          }
//          break;
//       default:
//          break;
//    }
// }


/* Handle time synchronization.
*/
/*
void KP130(uint8_t *msg) {
  static uint32_t t0 = 0;
  if (msg[2] == 0)
    t0 = timerGetValue();
  else {
    unsigned long msgTime = ((uint32_t)msg[3]) +
                            (((uint32_t)msg[4]) <<  8) +
                            (((uint32_t)msg[5]) << 16) +
                            (((uint32_t)msg[6]) << 24);
    uint32_t dt = msgTime-t0;
    timerAdjust(dt);
  }
}
*/

// === Handle save and restore to / from the NVRAM ===========================

// Save base and/or nodenumber to NVRAM.
int ckSaveBaseNodeNumbers(uint8_t nodeF, uint8_t baseF) {
  ckConfig cfg;

 // if (ckhalGetConfig(&cfg))
 if(false)
    return 1;

  if (nodeF)
    cfg.nodeNumber = ckConf.nodeNumber;
  if (baseF)
    cfg.baseNumber = ckConf.baseNumber;

  //ckhalSaveConfig(&cfg);
  return 0;
}


// Should be called whenever the base number or node number is changed.
void updateMayor(void) {
  int i = findSlotDoc(tdocMayor, 1);
  if (i >= 0) {
    if (ckConf.baseNumber != envInvalid) {
      ckSlots[i].envelope = (ckConf.baseNumber & envMask)+ckConf.nodeNumber;
      if (ckConf.baseNumber & envExtended)
        setBM(ckSlots[i].flags, ffExt);
      else
        clrBM(ckSlots[i].flags, ffExt);
    } else
      ckSlots[i].envelope = envInvalid;
    i = updateSlot(i);
  }
}

// Clears the folders, if mk01, folder 0 and 1 are reinitialized.
void ckClearFolders(int mk01) {
  int i;
  for (i = 0; i < folderMaxCount; i++)
    if (ckSlots[i].flags & ffInUse)
      deleteSlot(i);

  if (mk01) {
    (void)ckDefineLocalFolder(0, rdocKing,  (uint32_t)0, 8, 1, 0, 0, 0);
    (void)ckDefineLocalFolder(1, tdocMayor, (ckConf.baseNumber & envMask)+ckConf.nodeNumber, 8, 1, 1, 1,
                         (ckConf.baseNumber & envExtended) != 0);
  }
}

/* Returns the config data about the i'th used slot. The answer is delivered as
 * Mayors page 160-161.
 *
 * folder, doc, env, dlc, flags
 *
 * {nodeNo, 160, env:0-7,env:8-15,env:16-23,env:24-28|EC, folder, D}
 * {nodeNo, 161, folder, dlc|RTR, flags(T|E), docList, docNo, 0}
 *    Byte 7 is used as a flag: 255 if error.
 *
 */
uint8_t *ckGetFolder(uint8_t i, uint8_t page) {
  // Find the i'th used slot.
  int j;

  if (page != 160 && page != 161)
    return 1;

  for (j = 0; j < folderMaxCount; j++)
    if (ckSlots[j].flags & ffInUse) {
      if (i == 0)
        break;
      i--;
    }

  buf[0] = 2; // Fulfil the requirements for a Mayor's page:
  buf[1] = page;

  if (j == folderMaxCount) {
    buf[7] = 255; // Failure
    (void)ckSend(tdocMayor, buf, 8, 1);  // Ignore any error code
    return 1; // There was no slot number i
  }

  if (page == 160) { // Almost as Kings Page 2
    buf[2] = (uint8_t)(ckSlots[j].envelope & 0xff); /* Bit 0..7 */
    buf[3] = (uint8_t)((ckSlots[j].envelope >> 8) & 0xff); /* Bit 8..15 */
    buf[4] = (uint8_t)((ckSlots[j].envelope >> 16) & 0xff); /* Bit 16..23 */
    buf[5] = (uint8_t)((ckSlots[j].envelope >> 24) & 0xff); /* Bit 24..28 */
    if (ckSlots[j].flags & ffExt)
      buf[5] |= 0x80;
    buf[6] = ckSlots[j].folderNo;
    if (ckSlots[j].flags & ffEnabled)
      buf[7] = 1;
    else
      buf[7] = 0;

  } else { // page == 161, almost as Kings Page 16
    buf[2] = ckSlots[j].folderNo;
    buf[3] = 0x80 + (ckSlots[j].flags & ffDLC ? ckSlots[j].dlc : 15);
    if (ckSlots[j].flags & ffRTR)
      buf[3] |= 0x40;
    buf[4] = 0x80;
    if (ckSlots[j].flags & ffEnabled)
      buf[4] |= 0x40;
    if (ckSlots[j].flags & ffTx)
      buf[4] |= 0x01;
    buf[5] = 0; // docList
    buf[6] = ckSlots[j].doc;
    buf[7] = 123;
  }

 // return ckSend(tdocMayor, buf, 8, 1);
  return &buf;
}

/*
// Print the state variables and all folders to stdout.
void ckDumpFolders(void) {
  int i, j;
  const char *actModes[4] = {"-", "Run", "Freeze", "Reset"};
  const char *commModes[4] = {"-", "Silent", "ListenOnly", "Communicate"};
  printf("Node number %d, base number ", ckConf.nodeNumber);
  if (ckConf.baseNo != envInvalid)
    printf("%ld%c\n", ckConf.baseNumber & envMask, (ckConf.baseNumber & envExtended) ? 'x' : ' ');
  else
    printf("-\n");
  printf("Action Mode: %s, Communication Mode: %s\n", actModes[actMode], commModes[commMode]);
  printf("Groups: (");
  j = 0;
  for (i = 0; i < groupMaxCount; i++)
    if (Groups[i]) {
      if (j++)
        printf(" ");
      printf("%d", Groups[i]);
    } else
      break;
  printf(")\n");
  printf("Slot Folder Doc   CAN-id  DLC En Rx Tx RTR\n");
  for (i = 0; i < slotsMaxCount; i++) {
    if (ckSlots[i].flags & ffInUse) {
      printf("%4d %6d", i, ckSlots[i].folderNo);
      if (!(ckSlots[i].flags & ffDocValid))
        printf("   -");
      else
        printf(" %3d", ckSlots[i].doc);
      if (ckSlots[i].envelope == envInvalid)
        printf("        - ");
      else {
        printf(" %8x", ckSlots[i].envelope);
        if (ckSlots[i].flags & ffExt)
          putchar('x');
        else
          putchar(' ');
      }
      if (ckSlots[i].flags & ffDLC)
        printf(" %3d", ckSlots[i].dlc);
      else
        printf("   -");
      printf(ckSlots[i].flags & ffEnabled ? " x " : "   ");
      printf(ckSlots[i].flags & ffTx ? "    x " : " x    ");
      printf(ckSlots[i].flags & ffRTR ? " x" : "");
      printf("\n");
    }
   }
}

*/

// ============================================================================
/* Tries to send the document doc. Message in buf, length len.
 * If the document isn't requested (doesn't exist in ckSlots[]), nothing
 * is transmitted.
 *
 * If commMode == ckSilent, nothing is transmitted.
 * If commMode == ckListenOnly, only Mayor's documents are transmitted.
 *
 * The message is put in the buffer and will be transmitted as soon as
 * possible.
 * Returns ckSendSuccess = 0 if it is done, ckSendFailed if it was
 * impossible due to e.g. no folder for the document or we aren't in the
 * Running mode or ckSendBusy if it may be an idea to try later.
 */
uint8_t ckSend(uint16_t doc, uint8_t *buf, uint8_t len, uint8_t sendIfFreeze) {
  int i;
  int res;

  if (actMode != ckRunning && doc != tdocMayor && !sendIfFreeze)
    return ckSendFailed;
  if (commMode == ckSilent)
    return ckSendFailed;
  if (commMode == ckListenOnly && doc != tdocMayor)
    return ckSendFailed;
  
  // Make sure no KP0/1/2/16 etc gets called messing things up
  //ckhalDisableCANInterrupts();
  i = findSlotDoc(doc, 0);
  if (i < 0 ||
      (ckSlots[i].flags & (ffInUse | ffEnabled)) !=
      (ffInUse | ffEnabled) || ckSlots[i].envelope == envInvalid) {
    //ckhalEnableCANInterrupts();
    return ckSendFailed;
  }
  if (ckSlots[i].flags & ffDLC)
    len = ckSlots[i].dlc; // Override the dlc.

  res = can_transmit(CAN1, ckSlots[i].envelope, false, false, len, buf);
  //canWriteMessage(ckSlots[i].envelope, buf, len);
  // ckhalEnableCANInterrupts();
  if (res == -1)
    return ckSendSuccess;
  else
    return ckSendFailed;
}


