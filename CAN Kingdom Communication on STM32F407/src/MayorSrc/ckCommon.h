/* ckCommon.h
 *
 * Mats �kerblom   1995-06-19
 * Last changed    2000-09-19
 *
 * Common definitions: typedef's, addresses etc.
 */

#ifndef __CKCOMMON_H
#define __CKCOMMON_H

#define DEBUG // If defined, debug printouts etc are included.

// An assert that is evaluated during compilation (and uses no code space).
// If the expression is zero, the compiler will warn that the vector
// _Dummy[] has zero elements, otherwise it is silent.
//
// Lint warning 506 is "Constant value Boolean",
// 762 is "Redundantly declared ... previously declared at line ..."
#define CompilerAssert(e)     \
  /*lint -save -e506 -e762 */ \
  extern char _Dummy[(e)?1:0] \
  /*lint -restore */

/* Bithandling, bits by number (0, 1, 2, 3, ...) */
#define   checkbit(var,bit)  (var & (0x01 << (bit)))
#define   setbit(var,bit)    (var |= (0x01 << (bit)))
#define   togglebit(var,bit) (var ^= (0x01 << (bit)))
#define   clrbit(var,bit)    (var &= (~(0x01 << (bit))))

/* Bithandling, bitmasks (1, 2, 4, 8, ...) */
#define checkBM(var, bm) ((var) & (bm))
#define setBM(var, bm) ((var) |= bm)
#define clrBM(var, bm) ((var) &= ~(bm))
//#define assignBM(var, bm, f) ((var) = ((var) & ~(bm)) | (f ? bm : 0))
#define assignBM(var, bm, f) {if (f) setBM(var, bm) else clrBM(var,bm)}

#define BM0 0x01
#define BM1 0x02
#define BM2 0x04
#define BM3 0x08
#define BM4 0x10
#define BM5 0x20
#define BM6 0x40
#define BM7 0x80

#ifndef min
 #define min(a,b) (a <= b ? a : b)
 #define max(a,b) (a >= b ? a : b)
#endif

#define envInvalid 0xffffffff // Marks that the envelope isn't defined
#define envMask 0x1fffffff // Masks the id bits only
#define envExtended 0x80000000L // Bit set in extended CAN id's
#define envRTR 0x40000000L // Bit set in the ID of an RTR message id.

typedef void(*funPtr)(void);
/* typedef void interrupt (*ifunPtr)(void); */

#define MSB(b) (((uint16_t)(b) >> 8) & 255)
#define LSB(b) ((uint16_t)(b) & 255)
#define mkSHORT(b0,b1) (short)((uint16_t)(uint8_t)(b0)+((uint16_t)(uint8_t)(b1)<<8))
#define mkuint16_t(b0,b1) (uint16_t)((uint16_t)(uint8_t)(b0)+((uint16_t)(uint8_t)(b1)<<8))
#define MKuint32_t(b0,b1,b2,b3) ((uint32_t)(b0)+((uint32_t)(b1)<<8)+((uint32_t)(b2)<<16)+\
                             ((uint32_t)(b3)<<24))
// Translate an uint8_t-pointer to a short or uint16_t value (Intel ordering).
//#define uint16_t(p) mkuint16_t((p)[0], (p)[1])
//#define SHORT(p) mkSHORT((p)[0], (p)[1])
#define SHORT(p) (*(short*)(p))
#define uint16_t(p) (*(uint16_t*)(p))

#endif
