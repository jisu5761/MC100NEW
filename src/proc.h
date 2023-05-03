#ifndef   __PROC_H__
#define   __PROC_H__

#define    MODEL_VERSION   4.0                 //SCREEN UPDATE
#define		MODEL_NAME			"MC100"

#define		MAGIC_NUMBER		    0x4567
#define		LAN_USE				      0x01
#define		DHCP_USE			      0x02
#define		EZHOME_USE			    0x04

#define   BKP_DR_NUMBER		    256
#define		EEPROM_SIZE			    BKP_DR_NUMBER



#define		WIFI_SET			      0x20
#define		ETHERNET_SET		    0x10
#define		DHCP_SET			      0x02
#define		DHCP_RESET			    0x01

#define   CMD_CRC_SIZE        27

#define		AIR_SATUS_MODE        10
#define		FULL_DSIP_MODE				11
#define		MISE_DSIP_MODE				12
#define		GDNG_DSIP_MODE				13
#define		MISENUM_DSIP_MODE			14
#define		CHOMISENUM_DSIP_MODE	15
#define		CLOCK_DSIP_MODE				16


#define   BASE_DISP				    0x01
#define   MISE_DISP				    0x02
#define   GDBG_DISP				    0x04
#define   MISENUM_DISP			  0x08
#define   CHOMISENUM_DISP			0x10
#define   CLOCK_DISP     			0x20

#define   IDLE_MODE           0
#define   ERASE_MODE          10
#define   DATA_MODE           20
#define   DFAT_MODE           30
#define   CRC_MODE            40

#define	  ONCOUNT		          2
#define	  LONCOUNT		        50
#define	  UPDATECOUNT		      500

#define	IMG_SIZE	            (35*9)
#define NUM_LEDS              (315*1)
#define BRIGHTNESS            32//64
#define LED_TYPE              WS2812B
#define COLOR_ORDER           GRB

#define   SL_GAP   300          //425
#define   SH_GAP   1000          //575

#define   LL_GAP   1100
#define   LH_GAP   2100         //1725  


#define   SL_GAP315   200          //425
#define   SH_GAP315   700          //575

#define   LL_GAP315   800
#define   LH_GAP315   1760         //1725

#pragma pack(push)
#pragma pack(1)

struct bk_struct {
  uint32_t    deviceID;		//27
  uint8_t     volume;
  uint8_t     lvolume;
  uint8_t	    mac[6];
  uint8_t     comm_config;
  uint8_t     data;
  uint8_t	    ip[4];
  uint8_t	    subnetmask[4];
  uint8_t	    gateway[4];
  uint8_t	    dns[4];
  char  	    ssid[32];			//70
  char  	    password[32];
  char  	    serverName[32];// = "openapi.airkorea.or.kr";
  uint16_t    port;
  uint16_t    magicnum;
  uint8_t	    bright;
  char		    measurestation[27];
  char		    measurestation2[27];
  uint8_t	    sounddelay;
  uint16_t    NightStart;
  uint16_t    NightEnd;
  uint16_t    dispSelect;
  uint16_t    soundInterval;
  char		  reserve[60];
  uint16_t    chksum;
};
union ubk_struct {
  struct bk_struct  s;
  uint32_t   _dw[BKP_DR_NUMBER/4];
  uint16_t  _w[BKP_DR_NUMBER/2];
  uint8_t   _b[BKP_DR_NUMBER];
};

struct  ST_STATUS
{
	uint8_t 	stx;
	uint8_t 	num;
	uint8_t 	cmd;
	uint32_t 	soutceip;
	uint32_t 	sm;
	uint32_t 	gw;
	uint32_t 	dns;
	uint32_t 	devicenum;
	uint16_t 	port;
	uint8_t 	dhcp;
	uint8_t 	etx;
	uint16_t 	crc16;
};

union   UST
{
	struct  ST_STATUS  s;
	uint8_t           _b[29];
};

struct  STINFO
{
  uint8_t     cmd;
  uint8_t     deviceID;
  uint8_t     volume;
  uint8_t     lvolume;
  uint16_t    nightstart;
  uint16_t    nightend;
  uint8_t     guiderssicut;
  uint8_t     guiderssi;
  uint8_t     signalrssicut;  
  uint8_t     signalrssi;
  uint8_t     dipsw;
  uint8_t     rf253rssi;
  uint16_t    bitsel;
  uint8_t     birdvolume;
  uint8_t     cricketvolume;
  uint8_t     dingvolume;
  uint8_t     womanvolume;
  uint8_t     manvolume;
  uint8_t     melodyvolume;
  uint8_t     etcvolume;
  uint8_t     womanmute1;
  uint8_t     womanmute2;
  uint8_t     manmute1;
  uint8_t     manmute2;
  uint8_t     ver;
  uint32_t    pctime;
  
};
union  USTINFO
{
  struct  STINFO    s;
  uint8_t           _b[32];
};
#pragma pack(push)
#pragma pack(1)

typedef struct{
  uint8_t   fc_mode;
  char      xbuf[600];
  uint8_t   tpos;
  uint8_t   xpos;
  uint8_t   m_mode;
  uint8_t   oldcmd;
  uint8_t   station_mode;
  uint8_t   rcnt3582;
  uint16_t  soundelaytime;
  uint8_t   server_mode;
  uint8_t   display_mode;
  uint16_t  soundInterval;

  uint16_t  khaiValue;
  uint8_t   khaiGrade;
  uint16_t  pm10Value;
  uint8_t   pm10Grade;  
  uint8_t   pm10Flag;  
  uint16_t  pm25Value;
  uint8_t   pm25Grade;  
  uint8_t   pm25Flag;
}MC100LIVE;


extern	union UBIT  ctrl_flag1;
#define   s1flag				          ctrl_flag1._b.bit0
#define   olds1flag				        ctrl_flag1._b.bit1
#define   s2flag				          ctrl_flag1._b.bit2
#define   olds2flag				        ctrl_flag1._b.bit3
#define   s3flag				          ctrl_flag1._b.bit4
#define   olds3flag				        ctrl_flag1._b.bit5
#define   playsound_flag				  ctrl_flag1._b.bit6
#define   req_satstu_flag				  ctrl_flag1._b.bit7
#define   flash_backup					  ctrl_flag1._b.bit8
#define   test_flag						    ctrl_flag1._b.bit9
#define   setup_flag						  ctrl_flag1._b.bit10
#define   datavalidate_flag				ctrl_flag1._b.bit11
#define   guidesound_flag				  ctrl_flag1._b.bit12
#define   offtime_flag					  ctrl_flag1._b.bit13
#define   longs1flag				      ctrl_flag1._b.bit14

extern  union   ubk_struct  ubk;
extern  MC100LIVE   mc100live;
extern  CFastLED FastLED;
extern  CRGB leds[NUM_LEDS];

void mc100_init(void);
void eeprom_init(void);
void read_key(void);		//2msec
void backdata_init(void);
uint8_t	check_eeprom(void);
void fc1004_control(void);      //10msec
void check_serial(void);
void analysis_data(uint8_t *rxdata);
void push_status(void);
void convert_img_ws2812(CRGB c, uint8_t mode);
void color_fill(CRGB c);
void eeprom_datacontrol(void);
void microdust_control(void);
void display_control(void);
uint8_t	check_nextdisp(uint8_t dmode, uint16_t sel);
void key_control(void);
uint8_t	hputs(uint8_t x, uint8_t y, char *s, uint32_t mode);
uint16_t 	 CRC16(uint16_t wLength, const uint8_t *nData);
void check_soundbusy(void);
void checkplay_sound(void);
String getChipId();
uint64_t getChipId_int();
void wifiupdate(void);
#endif