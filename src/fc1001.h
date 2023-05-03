#ifndef   __FC1001_H__
#define   __FC1001_H__
//===================================================================================
// Serial Flash Register Address
//===================================================================================
//-------- code flash
#define F_WRSR      0x01  //Write Status Register
#define F_PP      0x02  //Page Program
#define F_READ      0x03  //Read Data Bytes
//#define F_WRDI      0x04  //Write Disable
#define F_RDSR      0x05  //Read Status Register
#define F_WREN      0x06  //Write Enable
//#define F_FAST_READ   0x0B  //Read Data Bytes at Higher Speed
//#define F_SE        0x20  //Sector Erase
//#define F_OTP       0x3A  //Enter OTP Mode
//#define F_BE1       0x52  //Block Erase
//#define F_CE1       0x60  //Chip Erase
//#define F_RDM       0x90  //Read Manufacturer/Device ID
#define F_RDID      0x9F  //Read Device ID
//#define F_RDI       0xAB  //Release from Deep Power-down and Read Device ID
//#define F_DP        0xB9  //Deep Power-down
#define F_CE      0xC7  //Chip Erase
//#define F_BE        0xD8  //Block Erase


//=========================================================
// Serial Flash STATUS BIT
//=========================================================
//-------- code flash
#define F_SRP     0x80  //Status Register Protect
#define F_BP2     0x10  //Block Protect Bits
#define F_BP1     0x08  //Block Protect Bits
#define F_BP0     0x04  //Block Protect Bits
#define F_WEL     0x02  //Write Enable Latch
#define F_WIP     0x01  //Write In Progress (=busy)


//-------- Atmel AT45DB flash
#define F_WIPdb     0x80  //(=busyb)
#define F_WELdb     0x40  //Write Enable Latch

#define   SYS_CON       0x01
#define   SBC_CON       0x02
#define   DEC_CHAP      0x03
#define   VOL_CON       0x04
#define   AUDIO_CON     0x05
#define   LED_CON       0x07
#define   SBC_DATA      0x0A
#define   INDEX_CHAP    0x0B
#define   MAN_ID        0x11
#define   MEM_TYP       0x12
#define   MEM_CAPA      0x13
#define   TOTAL_CHAP    0x14
#define   SF_CRC        0x15
#define   CRC_RESULT    0x16
#define   TESTMODE0     0x1C
#define   TESTMODE1     0x1E
#define   TESTMODE2     0x1F


#define   SBC_WRITE     0x00
#define   SBC_READ      0x40    
//-----------------------------------------------------------------------------
#define   MAX_SOUND     45  
  
#define   SOUND_BEEP            0
#define   SOUND_GOOD            1
#define   SOUND_NORMAL          2
#define   SOUND_BAD             3
#define   SOUND_WORST           4

void  SFbusy_check(void);
void  fc1004_control(void);
void  play_sound(uint8_t  no);
uint8_t  read_fc1004(uint8_t addr, uint8_t lng, uint8_t *buf);
void  write_fc1004(uint8_t addr, uint8_t lng, uint8_t *buf);
#endif