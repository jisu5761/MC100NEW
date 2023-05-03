#include <Arduino.h>
#include <EEPROM.h>
#include <FastLED.h>
#include <Ethernet.h>
#include <HTTPClient.h>
#include "RestClient.h"
#include <UrlEncode.h>
#include <ArduinoJson.h>
#include <HTTPUpdate.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Si446x.h>
#include "portdef.h"
#include "mytype.h"
#include "fc1001.h"
#include "proc.h"
#include "font5x7.h"
#include "esp_system.h"
const char* ssid = "eztek";
const char* password =  "eztekpoweron";
//-----------------------------------------------------------------------------
const char *wifiapikey = "http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?serviceKey=UTtJn%2FonwbUxkVwiHfSlxwn1q98%2BgWbmijJDK4Byn8wamaMMVXV4hAMnvIxLY3OJKZ%2FCmqSzX7aBIgfqvUjIUg%3D%3D&returnType=json&numOfRows=1&pageNo=1&stationName=";
const char *apikey =     "http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?serviceKey=UTtJn%2FonwbUxkVwiHfSlxwn1q98%2BgWbmijJDK4Byn8wamaMMVXV4hAMnvIxLY3OJKZ%2FCmqSzX7aBIgfqvUjIUg%3D%3D&returnType=json&numOfRows=1&pageNo=1&stationName=";
//-----------------------------------------------------------------------------
union   UST         ust;
union   ubk_struct  ubk;
union		UBIT		ctrl_flag1;
MC100LIVE   mc100live;

const   uint8_t volref[5] = { 48,40,32,24,18 };
//const   uint8_t brightref[5] = { 16,32,48,64,128 };
const   uint8_t brightref[5] = { 8,16,32,48,64 };
int status = WL_IDLE_STATUS;

byte mac[] = {
	0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
//-----------------------------------------------------------------------------
const uint32_t wdtTimeout = 60000;  //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;
WiFiUDP udp;
NTPClient timeClient(udp, "kr.pool.ntp.org", 32400, 86400000);		//1 시간간격(3600000)		24시간 86400000
RestClient Rclient = RestClient("apis.data.go.kr");
//-----------------------------------------------------------------------------
const CRGB dust_level[4] = { CRGB::Blue,CRGB::Green,CRGB::Yellow,CRGB::Red };

CRGB leds[NUM_LEDS];
#define UPDATES_PER_SECOND 3
CRGBPalette16 currentPalette;
TBlendType    currentBlending;
//-----------------------------------------------------------------------------
void IRAM_ATTR resetModule() {
	//	Serial.println("reboot\n");
	esp_restart();
}

//---------------------------------------------------------------------------------------------------------
void IRAM_ATTR interruptFunction()
{
	static  uint8_t   int_mode358, decode_mode358, d_mode358, scount358, rcnt358;
	static  uint16_t  timegap, oldtime;
	static  uint32_t  rfkey358;

	//digitalWrite(STLEDPIN, digitalRead(RX358_DATA));

	switch (int_mode358)
	{
	case    0:
		if (!digitalRead(RX358_DATA))
		{
			timegap = micros() - oldtime;
			int_mode358++;
		}
		break;

	case  1:
		if (digitalRead(RX358_DATA))
		{
			timegap = micros() - oldtime;
			int_mode358 = 0;
		}
		break;
	}
	oldtime = micros();
	if ((timegap > 12250) && (timegap < 17250))
	{
		if (decode_mode358 == 0)
		{

			decode_mode358 = 1;
			d_mode358 = 0;
			rfkey358 = 0;
			scount358 = 0;
			return;
		}
	}
	if (decode_mode358)
	{
		switch (d_mode358)
		{
		case    0:
			if ((timegap > SL_GAP) && (timegap < SH_GAP))                       d_mode358 = 10;
			else if ((timegap > LL_GAP) && (timegap < LH_GAP))                  d_mode358 = 20;
			break;

		case    20:
			d_mode358 = 0;
			rfkey358 = rfkey358 << 1;
			//                      if((rfin_buf[rr_ptr]>SL_GAP )&& (rfin_buf[rr_ptr]<SH_GAP ))           
			if (timegap < SH_GAP)//&& (rfin_buf[rr_ptr]<SH_GAP ))                                 
			{
				rfkey358 |= 0x01;
				scount358++;
			}
			else
			{
				decode_mode358 = 0;
			}
			break;

		case    10:
			d_mode358 = 0;
			rfkey358 = rfkey358 << 1;
			if ((timegap > LL_GAP) && (timegap < LH_GAP))
				//if(timegap > LL_GAP )
			{
				rfkey358 |= 0x00;
				scount358++;
			}
			else
			{
				decode_mode358 = 0;
			}
			break;


		}
	}
	if (scount358 > 23)
	{
		scount358 = 0;
		if (rfkey358 == 3072)
		{
			guidesound_flag = 1;
			mc100live.soundelaytime = ubk.s.sounddelay * 5;
			rfkey358 = 0;
			//Serial.println((int)Si446x_getRSSI());
			//rfrssibuf2[rcnt3582++] = Si446x_getRSSI(); 
			if (mc100live.rcnt3582 > 1)
			{
				mc100live.rcnt3582 = 0;
				//ubk.s.guiderssi = calculator_average(rfrssibuf2);

				//	if (ubk.s.guiderssicut <= ubk.s.guiderssi)
				//	{
				//	}
			}
		}
	}
}
//-----------------------------------------------------------------------------
void str2mac(const char* str, uint8_t* mac) 
{
  char buffer[20];
  strcpy(buffer, str);
  const char* delims = ":";
  char* tok = strtok(buffer, delims);
  for (int i = 5; i >= 0; i--) {
    mac[i] = strtol(tok, NULL, 16);
    tok = strtok(NULL, delims);
  }
}
//-----------------------------------------------------------------------------
void mc100_init(void)
{

	Serial.begin(115200);  
	pinMode(SI446X_SDN, OUTPUT);
	pinMode(SW1, INPUT);
	pinMode(SW2, INPUT);
	pinMode(PIR, INPUT);
	pinMode(SSDATO, INPUT);
	pinMode(SSCK, OUTPUT);
	pinMode(SSDATI, OUTPUT);
	pinMode(SCSB, OUTPUT);
	pinMode(SCSB_SEL, OUTPUT);
	pinMode(STLEDPIN, OUTPUT);
	pinMode(LED_PIN, OUTPUT);
	pinMode(W5500RST, OUTPUT);
	pinMode(N358_PIN, OUTPUT);

	SI446X_SDN_LOW;
	N358_HIGH;
	SCSB_SEL_HIGH;
	SCSB_HIGH;
	digitalWrite(W5500RST, LOW);
	delay(1000);
	digitalWrite(W5500RST, HIGH);
	delay(100);

  String macstr = WiFi.macAddress();
  Serial.println(macstr);
  str2mac(macstr.c_str(),mac);

	ust.s.stx = 0x02;
	ust.s.etx = 0x03;  

	eeprom_init();
	if (!check_eeprom())		backdata_init();

	FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(2);
	fill_solid(leds, 315, CRGB::Black);	
	FastLED.show();
  delay(500);
	fill_solid(leds, 315, CRGB::Red);
	FastLED.show();
	delay(500);
	fill_solid(leds, 315, CRGB::Blue);
	FastLED.show();
	delay(500);
	fill_solid(leds, 315, CRGB::Green);
	FastLED.show();
	delay(500);



	if (ubk.s.bright > 4)	ubk.s.bright = 2;

	int eeprombright = brightref[ubk.s.bright];

	for(int i = 8; i < eeprombright; i ++)
	{
		FastLED.setBrightness(i);
		FastLED.show();
		delay(30);
	}


	FastLED.setBrightness(brightref[ubk.s.bright]);
	currentPalette = RainbowColors_p;
	currentBlending = NOBLEND;

  color_fill(CRGB::Black);
	sprintf(mc100live.xbuf, "%s", MODEL_NAME);
	hputs(0, 0, mc100live.xbuf, 0);
	convert_img_ws2812(CRGB::Blue, 0);
	FastLED.show();
	delay(1000);
	sprintf(mc100live.xbuf, "ver%3.1f", MODEL_VERSION);
	hputs(0, 0, mc100live.xbuf, 0);
	convert_img_ws2812(CRGB::Blue, 0);

	Serial.println(mc100live.xbuf);


	if (!digitalRead(SW1) && !digitalRead(SW2))
	{
		if (!setup_flag)
		{
			setup_flag = 1;
			hputs(0, 0, "set", 0);
			convert_img_ws2812(CRGB::Blue, 0);
			FastLED.show();
			play_sound(SOUND_BEEP);
		}
	}
	else
	{
		setup_flag = 0;
	}

	if (!setup_flag)
	{
		if ((ubk.s.data & ETHERNET_SET) == ETHERNET_SET)
		{
			//color_fill(CRGB::Black);
			sprintf(mc100live.xbuf, "eth0");
			hputs(0, 0, mc100live.xbuf, 0);
			convert_img_ws2812(CRGB::Blue, 0);
			FastLED.show();

			Serial.println("Enthernet init start");
			IPAddress ip(ubk.s.ip[0], ubk.s.ip[1], ubk.s.ip[2], ubk.s.ip[3]);
			IPAddress gateway(ubk.s.gateway[0], ubk.s.gateway[1], ubk.s.gateway[2], ubk.s.gateway[3]);
			IPAddress dns(ubk.s.dns[0], ubk.s.dns[1], ubk.s.dns[2], ubk.s.dns[3]);
			IPAddress subnet(ubk.s.subnetmask[0], ubk.s.subnetmask[1], ubk.s.subnetmask[2], ubk.s.subnetmask[3]);
//   phymode.by = PHY_CONFBY_SW;		0x40
//   phymode.mode = PHY_MODE_MANUAL;	
//   phymode.speed = PHY_SPEED_10;
//   phymode.duplex = PHY_DUPLEX_FULL; // Choose PHY_DUPLEX_HALF or PHY_DUPLEX_FULL			

			Ethernet.init(5);			
			if ((ubk.s.data & DHCP_SET) == DHCP_SET)
			{
				delay(1000);
				//color_fill(CRGB::Black);
				sprintf(mc100live.xbuf, "dhcp");
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(CRGB::Blue, 0);
				FastLED.show();
				WiFi.macAddress(mac);

				delay(50);
				Ethernet.setPHYCFG_W5500(0xC8);
				if (Ethernet.begin(mac) == 0) {
					Ethernet.begin(mac, ip, dns, gateway, subnet);
					Serial.println("Enthernet DHCP Fail");
					delay(1000);
					//color_fill(CRGB::Black);
					sprintf(mc100live.xbuf, "error");
					hputs(0, 0, mc100live.xbuf, 0);
					convert_img_ws2812(CRGB::Yellow, 0);
					FastLED.show();

				}
			}
			else
			{
				delay(1000);
								//color_fill(CRGB::Black);
				sprintf(mc100live.xbuf, "static");
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(CRGB::Blue, 0);
				FastLED.show();
				Ethernet.begin(mac, ip, dns, gateway, subnet);
				delay(50);
				Ethernet.setPHYCFG_W5500(0xC8);				
			}
			delay(1000);
			sprintf(mc100live.xbuf, "Conn");
			hputs(0, 0, mc100live.xbuf, 0);
			convert_img_ws2812(CRGB::Blue, 0);
			FastLED.show();
			Serial.println("Enthernet init");
			Serial.println("");
			Serial.println("TCP connected");
			Serial.println("DNS IP: ");
			Serial.println(Ethernet.dnsServerIP());
			Serial.println("IP: ");
			Serial.println(Ethernet.localIP());
			Serial.println("MAC address: ");
			Serial.println(WiFi.macAddress());
		}
		else
		{
			Serial.println("WiFi init start");
			sprintf(mc100live.xbuf, "WiFi");
			hputs(0, 0, mc100live.xbuf, 0);
			convert_img_ws2812(CRGB::Blue, 0);
			FastLED.show();
			delay(500);
			if (test_flag)
			{
			}
			else
			{
//				WiFi.disconnect(true);
				delay(1000);
				while (WiFi.status() != WL_CONNECTED) {
					status = WiFi.begin(ubk.s.ssid, ubk.s.password);
//					status = WiFi.begin("eztek", "eztekpoweron");
					delay(7000);
					Serial.println(".");
				}
			}
			sprintf(mc100live.xbuf, "Conn");
			hputs(0, 0, mc100live.xbuf, 0);
			convert_img_ws2812(CRGB::Blue, 0);
			FastLED.show();

			Serial.println("");
			Serial.println("WiFi connected");
			Serial.println("IP address: ");
			Serial.println(WiFi.localIP());
			Serial.println("MAC address: ");
			Serial.println(WiFi.macAddress());

			Serial.print("getChipId=");			
			Serial.println(getChipId());	

			Serial.print("getChipId_int = ");
			Serial.println(getChipId_int());	

			uint64_t ChipId_int = getChipId_int();

			Serial.println("update..");
			sprintf(mc100live.xbuf, "update");
			hputs(0, 0, mc100live.xbuf, 0);
			convert_img_ws2812(CRGB::Blue, 0);
			FastLED.show();	
			delay(500);
			wifiupdate();

		}

		if ((ubk.s.data & ETHERNET_SET) != ETHERNET_SET)
			timeClient.begin();
		mc100live.server_mode = 20;			
	}


	if(!setup_flag)
	{
		Si446x_init();
		Si446x_RX(0);

		pinMode(RX358_DATA, INPUT_PULLDOWN);
		attachInterrupt(RX358_DATA, interruptFunction, CHANGE);
		Serial.println("Si4432");
	}

	timer = timerBegin(0, 80, true);                  //timer 0, div 80
	timerAttachInterrupt(timer, &resetModule, true);  //attach callback
	timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
	timerAlarmEnable(timer);
}
//-----------------------------------------------------------------------------
void read_key(void)		//2msec
{
	static  uint8_t  s2oncount, s3oncount, s1offcount, s2offcount;
	static  uint16_t s3offcount,s1oncount;

	if (!digitalRead(SW1)) {
		if (++s1oncount > ONCOUNT) {
			s1offcount = 0;
			s1flag = 1;
			if(s1oncount > UPDATECOUNT)
			{
				s1oncount--;
				longs1flag = 1;
			}
		}
	}
	else if (++s1offcount>ONCOUNT) {
		s1offcount--;
		s1oncount = 0;
		s1flag = 0;
		if(longs1flag)
		{
			longs1flag = 0;
			Serial.println("update....");
			sprintf(mc100live.xbuf, "update");
			// hputs(0, 0, mc100live.xbuf, 0);
			// convert_img_ws2812(CRGB::Blue, 0);
			// FastLED.show();	
			// delay(500);
			// if ((ubk.s.data & ETHERNET_SET) != ETHERNET_SET)		
			// 	wifiupdate();
		}
		//olds1flag = 0;
	}
	if (!digitalRead(SW2)) {
		if (++s2oncount>ONCOUNT) {
			s2oncount--;
			s2offcount = 0;
			s2flag = 1;
		}
	}
	else if (++s2offcount>LONCOUNT) {
		s2offcount--;
		s2oncount = 0;
		s2flag = 0;
		//olds2flag = 0;
	}


	if (digitalRead(PIR)) {
		if (++s3oncount>ONCOUNT) {
			s3oncount--;
			s3offcount = 0;
			s3flag = 1;

		}
	}
	else if (++s3offcount> LONCOUNT) {
		s3offcount--;
		s3oncount = 0;
		s3flag = 0;
		olds3flag = 0;
	}
}
//-----------------------------------------------------------------------------
uint8_t	check_eeprom(void)
{
	for (int i = 0;i < BKP_DR_NUMBER;i++)
	{
		ubk._b[i] = EEPROM.read(i);
	}
	//Serial.println("magicnum");
	//Serial.println(ubk.s.magicnum, HEX);
	//Serial.println(ubk.s.chksum, HEX);
	Serial.println(CRC16((BKP_DR_NUMBER - 2), ubk._b), HEX);
	if (ubk.s.magicnum != MAGIC_NUMBER)	return 0;
	if (ubk.s.chksum != CRC16((BKP_DR_NUMBER - 2), ubk._b)) return 0;
	return 1;
}
//-----------------------------------------------------------------------------
void eeprom_init()
{
	uint8_t	idx;
	Serial.println("start...");
	if (!EEPROM.begin(EEPROM_SIZE))
	{
		Serial.println("failed to initialise EEPROM"); 
    delay(1000000);
	}
	Serial.println(" bytes read from Flash . Values are:");
	EEPROM.get(0, ubk.s);
	Serial.print("devide ID :");
	Serial.println(ubk.s.deviceID);
	Serial.print("volume :");
	Serial.println(ubk.s.volume);
	Serial.print("night volume :");
	Serial.println(ubk.s.lvolume);

	Serial.print("comm_config; :");
	Serial.println(ubk.s.comm_config, HEX);

	Serial.print("ssid :");
	Serial.println(ubk.s.ssid);
	Serial.print("password :");
	idx = 0;
	Serial.print(ubk.s.password[idx++]);
	while (ubk.s.password[idx++])
		Serial.print("*");
	Serial.print(ubk.s.password[idx - 1]);
	Serial.println();
	Serial.print("serverName :");
	Serial.println(ubk.s.serverName);
	Serial.print("port :");
	Serial.println(ubk.s.port);
	Serial.print("station1 :");
	Serial.println(ubk.s.measurestation);
	Serial.print("station2 :");
	Serial.println(ubk.s.measurestation2);
	Serial.println();
	Serial.println("writing random n. in memory");
}
//-----------------------------------------------------------------------------
void	backdata_init()
{
	Serial.println("backdata_init");
	ubk.s.magicnum = MAGIC_NUMBER;
	ubk.s.comm_config = LAN_USE | DHCP_USE;
	ubk.s.deviceID = 12345678;
	ubk.s.lvolume = 1;
	ubk.s.volume = 3;
	ubk.s.port = 80;
	ubk.s.bright = 2;
	sprintf(ubk.s.ssid, "eztek");
	sprintf(ubk.s.password, "12341234");
	sprintf(ubk.s.serverName, "%s", "openapi.airkorea.or.kr");
	ubk.s.mac[0] = 0x12;
	ubk.s.mac[1] = 0x34;
	ubk.s.mac[2] = 0x56;
	ubk.s.mac[3] = 0x78;
	ubk.s.mac[4] = 0xAB;
	ubk.s.mac[5] = 0xCD;
	ubk.s.NightStart = 2000;
	ubk.s.NightEnd = 600;
	ubk.s.soundInterval = 600;
	ubk.s.dispSelect = BASE_DISP | MISE_DISP | GDBG_DISP | MISENUM_DISP | CHOMISENUM_DISP;
	flash_backup = 1;
}
//-----------------------------------------------------------------------------
uint16_t 	 CRC16(uint16_t wLength, const uint8_t *nData)
{
	static const uint16_t wCRCTable[] = {
		0X0000, 0XC0C1, 0XC181, 0X0140, 0XC301, 0X03C0, 0X0280, 0XC241,
		0XC601, 0X06C0, 0X0780, 0XC741, 0X0500, 0XC5C1, 0XC481, 0X0440,
		0XCC01, 0X0CC0, 0X0D80, 0XCD41, 0X0F00, 0XCFC1, 0XCE81, 0X0E40,
		0X0A00, 0XCAC1, 0XCB81, 0X0B40, 0XC901, 0X09C0, 0X0880, 0XC841,
		0XD801, 0X18C0, 0X1980, 0XD941, 0X1B00, 0XDBC1, 0XDA81, 0X1A40,
		0X1E00, 0XDEC1, 0XDF81, 0X1F40, 0XDD01, 0X1DC0, 0X1C80, 0XDC41,
		0X1400, 0XD4C1, 0XD581, 0X1540, 0XD701, 0X17C0, 0X1680, 0XD641,
		0XD201, 0X12C0, 0X1380, 0XD341, 0X1100, 0XD1C1, 0XD081, 0X1040,
		0XF001, 0X30C0, 0X3180, 0XF141, 0X3300, 0XF3C1, 0XF281, 0X3240,
		0X3600, 0XF6C1, 0XF781, 0X3740, 0XF501, 0X35C0, 0X3480, 0XF441,
		0X3C00, 0XFCC1, 0XFD81, 0X3D40, 0XFF01, 0X3FC0, 0X3E80, 0XFE41,
		0XFA01, 0X3AC0, 0X3B80, 0XFB41, 0X3900, 0XF9C1, 0XF881, 0X3840,
		0X2800, 0XE8C1, 0XE981, 0X2940, 0XEB01, 0X2BC0, 0X2A80, 0XEA41,
		0XEE01, 0X2EC0, 0X2F80, 0XEF41, 0X2D00, 0XEDC1, 0XEC81, 0X2C40,
		0XE401, 0X24C0, 0X2580, 0XE541, 0X2700, 0XE7C1, 0XE681, 0X2640,
		0X2200, 0XE2C1, 0XE381, 0X2340, 0XE101, 0X21C0, 0X2080, 0XE041,
		0XA001, 0X60C0, 0X6180, 0XA141, 0X6300, 0XA3C1, 0XA281, 0X6240,
		0X6600, 0XA6C1, 0XA781, 0X6740, 0XA501, 0X65C0, 0X6480, 0XA441,
		0X6C00, 0XACC1, 0XAD81, 0X6D40, 0XAF01, 0X6FC0, 0X6E80, 0XAE41,
		0XAA01, 0X6AC0, 0X6B80, 0XAB41, 0X6900, 0XA9C1, 0XA881, 0X6840,
		0X7800, 0XB8C1, 0XB981, 0X7940, 0XBB01, 0X7BC0, 0X7A80, 0XBA41,
		0XBE01, 0X7EC0, 0X7F80, 0XBF41, 0X7D00, 0XBDC1, 0XBC81, 0X7C40,
		0XB401, 0X74C0, 0X7580, 0XB541, 0X7700, 0XB7C1, 0XB681, 0X7640,
		0X7200, 0XB2C1, 0XB381, 0X7340, 0XB101, 0X71C0, 0X7080, 0XB041,
		0X5000, 0X90C1, 0X9181, 0X5140, 0X9301, 0X53C0, 0X5280, 0X9241,
		0X9601, 0X56C0, 0X5780, 0X9741, 0X5500, 0X95C1, 0X9481, 0X5440,
		0X9C01, 0X5CC0, 0X5D80, 0X9D41, 0X5F00, 0X9FC1, 0X9E81, 0X5E40,
		0X5A00, 0X9AC1, 0X9B81, 0X5B40, 0X9901, 0X59C0, 0X5880, 0X9841,
		0X8801, 0X48C0, 0X4980, 0X8941, 0X4B00, 0X8BC1, 0X8A81, 0X4A40,
		0X4E00, 0X8EC1, 0X8F81, 0X4F40, 0X8D01, 0X4DC0, 0X4C80, 0X8C41,
		0X4400, 0X84C1, 0X8581, 0X4540, 0X8701, 0X47C0, 0X4680, 0X8641,
		0X8201, 0X42C0, 0X4380, 0X8341, 0X4100, 0X81C1, 0X8081, 0X4040
	};

	uint8_t nTemp;
	uint16_t wCRCWord = 0xFFFF;

	while (wLength--)
	{
		nTemp = *nData++ ^ wCRCWord;
		wCRCWord >>= 8;
		wCRCWord ^= wCRCTable[nTemp];
	}
	return wCRCWord;
}



uint8_t		Char_put_Buf[5];
uint8_t		testbuf[IMG_SIZE] = { 0, };

//====== 占싼깍옙/占쏙옙占쏙옙 占싼깍옙占쏙옙 占쏙옙占쏙옙獨占� (占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙) ========================
uint8_t	hputch(uint8_t x, uint8_t y, uint8_t *text, uint32_t mode)
{
	uint16_t	k = 0, p = 0, bRet;

	for (int i = 0;i<5;i++) {
		bRet = Char_put_Buf[i];
		if (!bRet) continue;
		k = mc100live.tpos;
		for (int j = 0;j<8;j++) {
			if (bRet & 0x01)
			{
				if (mode)	testbuf[k] = 0x00;
				else
				{
					testbuf[k] = 0x01;
				}
			}
			else
			{
				if (mode)	testbuf[k] = 0x01;
				else
				{
					testbuf[k] = 0x00;
				}
			}
			bRet = bRet >> 1;
			k = k + 35;
		}
		 mc100live.tpos++;
	}
	 mc100live.tpos++;
	return  mc100live.tpos;
}
//====== 占쌍억옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙쨔占쏙옙謗占� 占쏙옙占쏙옙獨占� ================================
void	eng_put(char *keyin)
{
	uint8_t i;
	for (i = 0; i<5; i++) Char_put_Buf[i] = Font5x7[(((*keyin) - 0x20) * 5) + i];
}

//====== 占싼깍옙/占쏙옙占쏙옙 占쏙옙占쌘울옙 占쏙옙占쏙옙獨占� =============================================
uint8_t	hputs(uint8_t x, uint8_t y, char *s, uint32_t mode)
{
	uint8_t tposs;
	memset(testbuf, 0, IMG_SIZE);
	 mc100live.tpos = 35;
	while (*s)
	{
		eng_put(s);													/* 占쌍억옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙쨔占쏙옙謗占� 占쏙옙쨈占�. */
		tposs = hputch(x, y, Char_put_Buf, mode);		/* 占싼깍옙占쏙옙 占쏙옙占� */
		s++;																/* 占쌍억옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙占쌘뤄옙 占싱듸옙 */
	}
	 mc100live.xpos = tposs - 35 - 2;
	if ( mc100live.xpos > 34)   mc100live.xpos = 34;
	//Serial.print(tposs);
	//Serial.print("\t");
	//Serial.println(xpos);

	return  mc100live.xpos;
}
void FillLEDsFromPaletteColors(uint8_t colorIndex)
{
	uint8_t brightness = 255;

	for (int i = 0; i < NUM_LEDS; i++) {
		leds[i] = CRGB::Red;//ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
							//Serial.print(i);
							//Serial.print("-");
							//Serial.print(leds[i].raw[0]);
							//Serial.print("-");
							//Serial.print(leds[i].raw[1]);
							//Serial.print("-");
							//Serial.println(leds[i].raw[2]);
		colorIndex += 3;
	}
}

void color_fill(CRGB c)
{
	uint8_t brightness = 255;

	for (int i = 0; i < NUM_LEDS; i++) {
		leds[i] = c;//ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
					//Serial.print(i);
					//Serial.print("-");
					//Serial.print(leds[i].raw[0]);
					//Serial.print("-");
					//Serial.print(leds[i].raw[1]);
					//Serial.print("-");
					//Serial.println(leds[i].raw[2]);

	}
}

const uint8_t airstatus[315] =		//대기상태
{
	0,1,0,1,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,1,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,
	0,1,1,1,0,1,0,1,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,0,1,1,1,1,0,1,0,1,0,	
	0,1,0,1,0,0,0,0,1,0,0,0,1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,1,0,1,0,0,0,1,0,
	0,1,0,0,0,1,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,1,0,
	0,1,1,1,0,1,1,1,1,0,0,1,0,1,0,0,0,1,0,1,0,0,1,0,0,0,0,1,1,1,0,0,0,1,0,
	0,1,0,0,0,1,0,1,0,0,0,0,0,1,0,1,0,1,0,0,0,1,0,1,0,0,1,0,0,0,0,1,0,1,0,
	0,1,0,1,0,0,0,0,1,0,1,1,0,1,0,0,0,1,0,1,0,1,0,0,0,0,0,1,0,1,0,0,0,1,0,
	0,1,1,1,0,1,0,1,0,1,1,1,1,1,0,1,0,0,1,0,1,0,0,1,0,0,1,1,1,1,0,1,0,1,0,	
	0,1,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,	
};

const uint8_t misedustdisp[315] =		//미세먼지
{

	0,0,0,0,1,0,0,0,0,0,0,0,1,1,1,1,1,0,1,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,
	0,0,0,0,1,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0,0,0,1,0,0,0,1,0,1,0,0,0,0,
	0,0,0,0,1,0,1,0,0,0,1,0,1,0,0,0,0,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0,0,
	0,0,0,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,0,1,0,1,0,0,0,1,0,1,0,0,0,0,
	0,0,0,0,1,0,0,1,0,1,0,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,0,0,
	0,0,0,0,1,0,1,0,1,0,0,1,0,1,1,0,1,0,1,0,1,1,1,0,0,0,1,0,0,0,1,0,0,0,0,
	0,0,0,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,1,0,1,0,0,1,0,0,1,0,1,0,1,0,0,0,0,
	0,0,0,0,1,1,1,0,1,0,0,1,0,0,1,0,1,0,1,1,1,0,1,0,1,1,1,1,1,0,1,0,0,0,0,
	0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,
};

const uint8_t joeum[315] =		//좋음
{
	1,1,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,
	1,1,1,1,1,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
	0,1,1,1,0,0,0,0,1,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,1,0,0,0,1,
	0,1,1,1,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1

	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,1,1,
	//1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,1,1,
	//0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,1,1,1,1,
	//0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,1,0,0,0,0,1,1,1,0,
	//1,0,0,0,1,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,1,0
};

const uint8_t botong[315] =		//보통
{
	0,1,1,1,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,
	0,1,1,1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	0,0,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1,
	0,0,0,0,1,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1

	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,1,1,1,0,
	//1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,1,1,0,
	//1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,0,0,
	//1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,1,0,0,0,0,
	//1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1

};

const uint8_t nabbum[315] =		//나쁨
{

	1,1,1,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,
	1,1,1,1,1,0,0,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1,
	0,0,0,0,0,0,0,1,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1,
	1,0,1,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1,
	1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1

	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,1,1,1,1,
	//1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,1,0,0,0,1,1,1,1,1,
	//1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,1,0,0,0,0,0,0,0,
	//1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,1,0,1,
	//1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,1,0,1
};

const uint8_t ajunabbum[315] =		//아주나쁨
{
	1,1,1,1,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,1,0,0,0,0,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,1,1,1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,
	1,1,1,1,1,0,0,0,1,0,1,1,1,0,0,0,1,0,0,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1,
	0,0,0,0,0,0,0,1,1,0,0,0,1,0,1,1,1,1,1,0,1,1,1,0,1,0,1,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,1,1,1,
	1,0,1,0,1,0,0,0,1,0,0,0,1,0,0,1,1,1,0,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,0,1,1,1,0,1,0,1,0,1,0,0,0,1,0,1,0,0,0,1,0,0,0,1,1,1,1,1,
	1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,1,1,1,0,0,1,0,1,0,0,0,0,0,1,1,1,1,1,1,1

	//1,1,1,1,1,1,1,0,0,0,0,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,1,1,1,1,
	//1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,1,0,1,1,1,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,0,0,1,0,0,0,1,1,1,0,1,0,0,0,1,1,1,1,1,
	//1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,1,0,1,0,1,1,1,0,1,1,1,1,1,0,1,0,0,0,1,1,0,0,0,0,0,0,0,
	//1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1,0,1,0,1,0,1,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,1,0,1,0,1,0,1,0,0,1,1,1,0,0,1,0,0,0,1,0,0,0,1,0,1,0,1,
	//1,1,1,1,1,0,0,0,1,0,0,0,1,0,1,0,0,0,1,0,1,0,1,0,1,1,1,0,1,1,1,1,1,1,1,
	//1,1,1,1,1,1,1,0,0,0,0,0,1,0,1,0,0,1,1,1,0,0,0,0,0,0,1,0,0,0,1,0,1,0,1

};

const uint8_t chomise[315] =		//초미세
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,
	0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0
};

const uint8_t level1[315] =		//Level 1
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1
};
const uint8_t level2[315] =		//Level 2
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1
};
const uint8_t level3[315] =		//Level 3
{
	0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1
};
const uint8_t level4[315] =		//Level 4
{
	0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1
};

const uint8_t level5[315] =		//Level 5
{
	1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,
	1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,
	1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,1,1,1,1,1,	
	1,1,1,1,1,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
	1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1	
};

void fill_chomise(CRGB c)
{
	for (int i = 27;i < 35;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}
	for (int i = 35;i < 43;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}

	for (int i = 97;i < 105;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}
	for (int i = 105;i < 113;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}

	for (int i = 167;i < 175;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}
	for (int i = 175;i < 183;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}

	for (int i = 237;i < 245;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}
	for (int i = 245;i < 253;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}

	for (int i = 307;i < 315;i++) {
		if (chomise[i])
			leds[i] = c;
		else
			leds[i] = CRGB::Black;
	}
}

void color_airedust(CRGB c)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (airstatus[i])	leds[i] = c;
		else					leds[i] = CRGB::Black;
	}
}

void color_misedust(CRGB c)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (misedustdisp[i])	leds[i] = c;
		else					leds[i] = CRGB::Black;
	}
}


void color_joeum(void)
{  
	for (int i = 0; i < NUM_LEDS; i++) {
		if (joeum[i])   leds[i] = CRGB::Blue;
		else			leds[i] = CRGB::Black;
	}
}

void color_botong(void)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (botong[i])   leds[i] = CRGB::Green;
		else			leds[i] = CRGB::Black;
	}
}

void color_nabbum(void)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (nabbum[i])  leds[i] = CRGB::Yellow;
		else			leds[i] = CRGB::Black;
	}
}

void color_ajunabbum(void)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (ajunabbum[i])  leds[i] = CRGB::Red;
		else			   leds[i] = CRGB::Black;
	}
}

void color_level1(CRGB c)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (level1[i])  leds[i] = c;
		else			leds[i] = CRGB::Black;
	}
}
void color_level2(CRGB c)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (level2[i])  leds[i] = c;
		else			leds[i] = CRGB::Black;
	}
}
void color_level3(CRGB c)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (level3[i])  leds[i] = c;
		else			leds[i] = CRGB::Black;
	}
}
void color_level4(CRGB c)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (level4[i])  leds[i] = c;
		else			leds[i] = CRGB::Black;
	}
}

void color_level5(CRGB c)
{
	for (int i = 0; i < NUM_LEDS; i++) {
		if (level5[i])  leds[i] =c;
		else			leds[i] = CRGB::Black;
	}
}

void fill_level(uint8_t l, CRGB c)
{
	switch (l)
	{
	case	0:		color_level1(c);	break;
	case	1:		color_level2(c);	break;
	case	2:		color_level3(c);	break;
	case	3:		color_level4(c);	break;
	case	4:		color_level5(c);	break;

	}
}

void color_hangle(uint8_t c)
{
	switch (c)
	{
		case	0:	
					color_joeum();
					break;

		case	1:
					color_botong();
					break;

		case	2:
					color_nabbum();
					break;

		case	3:
					color_ajunabbum();
					break;
	}

}

//------------------------------------------------------------------------------
void  convert_img_ws2812(CRGB c, uint8_t mode)
{
	uint16_t	i = 0, j = 0;
	uint8_t		tmp32;
	for (i = 0;i < 8;i++)
	{
		if (i % 2)
		{
			for (j = 0;j < 17;j++)
			{
				tmp32 = testbuf[i * 35 + j];
				testbuf[i * 35 + j] = testbuf[i * 35 + 34 - j];
				testbuf[i * 35 + 34 - j] = tmp32;
			}
		}
	}

	for (i = 0;i < IMG_SIZE;i++)
	{
		//if (mode)
		//{
		//	if (!testbuf[i])	leds[IMG_SIZE - i - 1] = c;
		//	else
		//	{
		//		leds[IMG_SIZE - i - 1] = ~c;//CRGB::Black;
		//	}
		//}
		//else

		{
			if (testbuf[i])	leds[IMG_SIZE - i - 1] = c;
			else
			{
				leds[IMG_SIZE - i - 1] = CRGB::Black;
			}
			if (mc100live.station_mode)				leds[279] = c;
		}
	}
	if (!mode)	return;
	for (int i = 0;i < (mc100live.xpos - 7);i++) {
		leds[i] = leds[(34 - mc100live.xpos) + i];
		if (i != ((34 - mc100live.xpos) + i))
			leds[(34 - mc100live.xpos) + i] = CRGB::Black;

		leds[69 - i] = leds[69 - (34 - mc100live.xpos) - i];
		if ((69 - i) != (69 - (34 - mc100live.xpos) - i))
			leds[69 - (34 - mc100live.xpos) - i] = CRGB::Black;

		leds[70 + i] = leds[70 + (34 - mc100live.xpos) + i];
		if ((70 + i) != (70 + (34 - mc100live.xpos) + i))
			leds[70 + (34 - mc100live.xpos) + i] = CRGB::Black;

		leds[70 + 69 - i] = leds[70 + 69 - (34 - mc100live.xpos) - i];
		if ((70 + 69 - i) != (70 + 69 - (34 - mc100live.xpos) - i))
			leds[70 + 69 - (34 - mc100live.xpos) - i] = CRGB::Black;

		leds[140 + i] = leds[140 + (34 - mc100live.xpos) + i];
		if ((140 + i) != (140 + (34 - mc100live.xpos) + i))
			leds[140 + (34 - mc100live.xpos) + i] = CRGB::Black;

		leds[140 + 69 - i] = leds[140 + 69 - (34 - mc100live.xpos) - i];
		if ((140 + 69 - i) != (140 + 69 - (34 - mc100live.xpos) - i))
			leds[140 + 69 - (34 - mc100live.xpos) - i] = CRGB::Black;

		leds[210 + i] = leds[210 + (34 - mc100live.xpos) + i];
		if ((210 + i) != (210 + (34 - mc100live.xpos) + i))
			leds[210 + (34 - mc100live.xpos) + i] = CRGB::Black;
		leds[210 + 69 - i] = leds[210 + 69 - (34 - mc100live.xpos) - i];
		if ((210 + 69 - i) != (210 + 69 - (34 - mc100live.xpos) - i))
			leds[210 + 69 - (34 - mc100live.xpos) - i] = CRGB::Black;
	}
}
//-----------------------------------------------------------------------------
void display_info(void)
{
	int idx = 0;
	Serial.print("DeviceNum:");
	Serial.println(ubk.s.deviceID);
	Serial.print("IP:");
	Serial.print(ubk.s.ip[0]);
	Serial.print("\t");
	Serial.print(ubk.s.ip[1]);
	Serial.print("\t");
	Serial.print(ubk.s.ip[2]);
	Serial.print("\t");
	Serial.println(ubk.s.ip[3]);
	Serial.print("SubnetMask:");
	Serial.print(ubk.s.subnetmask[0]);
	Serial.print("\t");
	Serial.print(ubk.s.subnetmask[1]);
	Serial.print("\t");
	Serial.print(ubk.s.subnetmask[2]);
	Serial.print("\t");
	Serial.println(ubk.s.subnetmask[3]);
	Serial.print("Gateway:");
	Serial.print(ubk.s.gateway[0]);
	Serial.print("\t");
	Serial.print(ubk.s.gateway[1]);
	Serial.print("\t");
	Serial.print(ubk.s.gateway[2]);
	Serial.print("\t");
	Serial.println(ubk.s.gateway[3]);
	Serial.print("DNS:");
	Serial.print(ubk.s.dns[0]);
	Serial.print("\t");
	Serial.print(ubk.s.dns[1]);
	Serial.print("\t");
	Serial.print(ubk.s.dns[2]);
	Serial.print("\t");
	Serial.println(ubk.s.dns[3]);
	Serial.print("Port:");
	Serial.println(ubk.s.port);
	Serial.print("station1 :");
	Serial.println(ubk.s.measurestation);
	Serial.print("station2 :");
	Serial.println(ubk.s.measurestation2);
	if ((ubk.s.data & 0x0F) == DHCP_SET)	Serial.println("DHCP Enabled");
	else    								Serial.println("DHCP Disabled");
	if ((ubk.s.data & 0xF0) == WIFI_SET)	Serial.println("WIFI Enabled");
	else    								Serial.println("WIFI Disabled");

	Serial.print("MAC :");
	for (int i = 0;i < 6;i++)
	{
		Serial.print(mac[i], HEX);
		Serial.print(" ");
	}
	Serial.println();
	Serial.print("OFF Start :");
	Serial.println(ubk.s.NightStart);
	Serial.print("OFF End :");
	Serial.println(ubk.s.NightEnd);
	Serial.print("Sound delay :");

	Serial.print("soundInterval :");
	Serial.println(ubk.s.soundInterval);

	Serial.println(ubk.s.sounddelay);
	Serial.print("ssid :");
	Serial.println(ubk.s.ssid);
	Serial.print("password :");
	idx = 0;
	Serial.print(ubk.s.password[idx++]);
	while (ubk.s.password[idx++])
		Serial.print("*");
	Serial.print(ubk.s.password[idx - 1]);
	Serial.println();
	Serial.println("End!!!!!!!!!!!!");
}
//------------------------------------------------------------------------------
void check_serial(void)
{
	static  uint8_t     s1_mode, tmp;
	static  uint16_t    rlng, lng, bcc;
	if (!Serial)	return;

	while (Serial.available())
	{
		tmp = Serial.read();
		//Serial.println(s1_mode);
		switch (s1_mode)
		{
		case    0:      if (tmp == 0x02)  s1_mode++;
						else if (tmp == 0xF2)     s1_mode = 10;
						else if (tmp == '1')
						{

					String payload = "{""response"":{""body"":{""totalCount"":23,""items"":[{""so2Grade"":""1"",""coFlag"":null,""khaiValue"":""72"",""so2Value"":""0.004"",""coValue"":""0.4"",""pm25Flag"":null,""pm10Flag"":null,""pm10Value"":""87"",""o3Grade"":""1"",""khaiGrade"":""2"",""pm25Value"":""59"",""no2Flag"":null,""no2Grade"":""1"",""o3Flag"":null,""pm25Grade"":""2"",""so2Flag"":null,""dataTime"":""2023-05-03 08:00"",""coGrade"":""1"",""no2Value"":""0.026"",""pm10Grade"":""2"",""o3Value"":""0.021""}],""pageNo"":1,""numOfRows"":1},""header"":{""resultMsg"":""NORMAL_CODE"",""resultCode"":""00""}}}";
			
  				StaticJsonDocument<2048> doc;				
  				DeserializationError error = deserializeJson(doc, payload);					
					
					mc100live.khaiGrade = doc["response"]["body"]["items"][0]["khaiGrade"];
					mc100live.khaiValue = doc["response"]["body"]["items"][0]["khaiValue"];
					mc100live.pm10Value = doc["response"]["body"]["items"][0]["pm10Value"];
					mc100live.pm25Value = doc["response"]["body"]["items"][0]["pm25Value"];
					mc100live.pm10Flag = doc["response"]["body"]["items"][0]["pm10Flag"];
					mc100live.pm25Flag = doc["response"]["body"]["items"][0]["pm25Flag"];
					if(mc100live.khaiGrade)	mc100live.khaiGrade -= 1;


					Serial.print("khaiGrade = ");
					Serial.println(mc100live.khaiGrade);
					Serial.print("khaiValue = ");
					Serial.println(mc100live.khaiValue);
					Serial.print("pm10Value = ");
					Serial.println(mc100live.pm10Value);
					Serial.print("pm25Value = ");
					Serial.println(mc100live.pm25Value);
					Serial.print("pm10Flag = ");
					Serial.println(mc100live.pm10Flag);
					Serial.print("pm25Flag = ");
					Serial.println(mc100live.pm25Flag);
					datavalidate_flag = 1;


							// display_info();

						}
						else if (tmp == '2')		mc100live.server_mode = 20;
						else if (tmp == '3')		//WIFI SCAN
						{
							break;
						}
						rlng = 0;
						mc100live.xbuf[rlng++] = tmp;
						break;

		case    1:      lng = tmp;
						mc100live.xbuf[rlng++] = tmp;
						if (lng > 70)  s1_mode = 0;
						else          s1_mode++;
						break;

		case    2:      
            mc100live.xbuf[rlng++] = tmp;
						if (rlng >= (lng + 1))           s1_mode++;
						break;

		case    3:      
            if (tmp == 0x03)
						{
							mc100live.xbuf[rlng++] = tmp;
							s1_mode++;
						}
						else
							s1_mode = 0;
						break;

		case    4:      bcc = tmp;
						s1_mode++;
						break;

		case    5:      s1_mode = 0;
						bcc = bcc | (tmp << 8);

						//if (bcc == CRC16(rlng, rxdata))
						{
							if (mc100live.xbuf[0] == 0x02)
							{
								//for (int i = 0; i < rlng; i++)
								//	urxdata[i] = rxdata[i];
								analysis_data((uint8_t *)&mc100live.xbuf[2]);
							}
							else
							{
								//for (int i = 0; i < rlng; i++)
								//	urxdata[i] = rxdata[i];
								analysis_data((uint8_t *)&mc100live.xbuf[3]);
							}
						}
						break;

		case  10:       
            mc100live.xbuf[rlng++] = tmp;
						lng = tmp;
						s1_mode++;
						break;

		case  11:       
            mc100live.xbuf[rlng++] = tmp;
						lng = lng + (tmp << 8);
						if (lng > 258)       s1_mode = 0;
						else                s1_mode++;
						break;

		case  12:       
            mc100live.xbuf[rlng++] = tmp;
						if (rlng > (lng + 3))  s1_mode = 4;
						break;

		default:        s1_mode = 0;
						break;
		}
	}
}
//------------------------------------------------------------------------------
void key_control(void)		//100mmsec
{
	uint8_t	tmp[2];	
	if (s1flag) {
		if (!olds1flag) {
			olds1flag = 1;
		}
	}
	else if (olds1flag)
	{
		olds1flag = 0;
		if (setup_flag)		return;
		if (++ubk.s.volume > 4) ubk.s.volume = 0;
		tmp[0] = 0x02;
		write_fc1004(AUDIO_CON | SBC_WRITE, 1, tmp);

		tmp[0] = volref[ubk.s.volume];
		write_fc1004(VOL_CON | SBC_WRITE, 1, tmp);
		playsound_flag = 0;
		play_sound(mc100live.khaiGrade + 1);
		flash_backup = 1;
		fill_level(ubk.s.volume, CRGB::Green);
		FastLED.show();
		delay(500);
	}

	if (s2flag) {
		if (!olds2flag) {
			olds2flag = 1;
		}
	}
	else if (olds2flag)
	{
		olds2flag = 0;
		if (setup_flag)		return;
		if (++ubk.s.bright > 4)	ubk.s.bright = 0;
		FastLED.setBrightness(brightref[ubk.s.bright]);
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		flash_backup = 1;

		fill_level(ubk.s.bright, CRGB::Blue);
		FastLED.show();
		delay(500);
		//Serial.print("BRIGHT : ");
		//Serial.println(brightref[ubk.s.bright]);
	}

	if (++mc100live.soundInterval < (ubk.s.soundInterval * 10));
	else
		mc100live.soundInterval--;

	if (s3flag)
	{
		if (!olds3flag && datavalidate_flag)
		{
			olds3flag = 1;
			if((!mc100live.pm10Value) && (!mc100live.pm25Value))	play_sound(SOUND_BEEP);
			else
			{
				if (++mc100live.soundInterval < (ubk.s.soundInterval * 10));
				else
				{
					mc100live.soundInterval = 0;
					play_sound(mc100live.khaiGrade + 1);
				}
			}
		}
	}
	if (setup_flag)		return;
	if (test_flag)
	{
		datavalidate_flag = 1;
		return;
	}
}
//------------------------------------------------------------------------------
void display_control(void)		//500msec
{	
	static	uint8_t	wcnt;
	if ((ubk.s.data & ETHERNET_SET) != ETHERNET_SET)	
		timeClient.update();


	if(!datavalidate_flag)		return;
	if(++ wcnt % 8) 		return;

	switch(mc100live.display_mode)
	{
		case	MISENUM_DSIP_MODE:
			if (!mc100live.pm10Value)
			{
				sprintf(mc100live.xbuf, "    <  - -  >");
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(dust_level[mc100live.khaiGrade], 0);
			}
			else
			{
				sprintf(mc100live.xbuf, "        %3dug", mc100live.pm10Value);
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(dust_level[mc100live.khaiGrade], 1);
			}
			mc100live.display_mode = check_nextdisp(mc100live.display_mode, ubk.s.dispSelect);
			break;

		case MISE_DSIP_MODE:
			color_misedust(dust_level[mc100live.khaiGrade]);
			mc100live.display_mode = check_nextdisp(mc100live.display_mode, ubk.s.dispSelect);
			break;

		case	CHOMISENUM_DSIP_MODE:
			if (!mc100live.pm25Value)
			{
				sprintf(mc100live.xbuf, "    #  - -  #");
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(dust_level[mc100live.khaiGrade], 0);
			}
			else
			{
				sprintf(mc100live.xbuf, "        %3dug", mc100live.pm25Value);
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(dust_level[mc100live.khaiGrade], 1);
				fill_chomise(dust_level[mc100live.khaiGrade]);
			}
			mc100live.display_mode = check_nextdisp(mc100live.display_mode, ubk.s.dispSelect);
			break;

		case	FULL_DSIP_MODE:
			if ((!mc100live.pm25Value) && (!mc100live.pm10Value))
			{
				sprintf(mc100live.xbuf, "    >  = =  <");
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(dust_level[mc100live.khaiGrade], 0);
			}
			else
			{
				color_fill(dust_level[mc100live.khaiGrade]);
			}
			mc100live.display_mode = AIR_SATUS_MODE;//check_nextdisp(d_mode, ubk.s.dispSelect);
			break;

		case AIR_SATUS_MODE:
			color_airedust(dust_level[mc100live.khaiGrade]);
			mc100live.display_mode = check_nextdisp(mc100live.display_mode, ubk.s.dispSelect);
			break;

		case	GDNG_DSIP_MODE:
			if ((!mc100live.pm25Value) && (!mc100live.pm10Value))
			{
				sprintf(mc100live.xbuf, "    >  = =  <");
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(dust_level[mc100live.khaiGrade], 0);
			}
			else
			{
				color_hangle(mc100live.khaiGrade);
			}
			mc100live.display_mode = check_nextdisp(mc100live.display_mode, ubk.s.dispSelect);
			break;

		case CLOCK_DSIP_MODE:
			if ((ubk.s.data & ETHERNET_SET) != ETHERNET_SET)
			{
				sprintf(mc100live.xbuf, "      %02d:%02d", timeClient.getHours(), timeClient.getMinutes());
				//sprintf(mc100live.xbuf, "      %02d:%02d", 1, 1);
				hputs(0, 0, mc100live.xbuf, 0);
				convert_img_ws2812(dust_level[mc100live.khaiGrade], 0);
			}
			mc100live.display_mode = check_nextdisp(mc100live.display_mode, ubk.s.dispSelect);
			break;

		default:	
			mc100live.display_mode = check_nextdisp(mc100live.display_mode, ubk.s.dispSelect);
			break;
		}
		// FastLED.show();
}
//-----------------------------------------------------------------------------
uint8_t	check_nextdisp(uint8_t dmode, uint16_t sel)
{
	switch(mc100live.display_mode)
	{
		case	AIR_SATUS_MODE:
								if (ubk.s.dispSelect & GDBG_DISP)					return	GDNG_DSIP_MODE;
								if (ubk.s.dispSelect & MISE_DISP)					return	MISE_DSIP_MODE;
								if (ubk.s.dispSelect & MISENUM_DISP)			return	MISENUM_DSIP_MODE;
								if (ubk.s.dispSelect & CHOMISENUM_DISP)		return	CHOMISENUM_DSIP_MODE;
								if (ubk.s.dispSelect & CLOCK_DISP)				return	CLOCK_DSIP_MODE;
								break;

		case	MISE_DSIP_MODE:
								if (ubk.s.dispSelect & MISENUM_DISP)			return	MISENUM_DSIP_MODE;
								if (ubk.s.dispSelect & CHOMISENUM_DISP)			return	CHOMISENUM_DSIP_MODE;
								if (ubk.s.dispSelect & CLOCK_DISP)				return	CLOCK_DSIP_MODE;
								if (ubk.s.dispSelect & BASE_DISP)				return	FULL_DSIP_MODE;
								break;

		case	GDNG_DSIP_MODE:
								if (ubk.s.dispSelect & MISE_DISP)				return	MISE_DSIP_MODE;
								if (ubk.s.dispSelect & MISENUM_DISP)			return	MISENUM_DSIP_MODE;
								if (ubk.s.dispSelect & CLOCK_DISP)				return	CLOCK_DSIP_MODE;
								if (ubk.s.dispSelect & BASE_DISP)				return	FULL_DSIP_MODE;
								break;

		case	MISENUM_DSIP_MODE:
								if (ubk.s.dispSelect & CHOMISENUM_DISP)			return	CHOMISENUM_DSIP_MODE;
								if (ubk.s.dispSelect & CLOCK_DISP)				return	CLOCK_DSIP_MODE;
								if (ubk.s.dispSelect & BASE_DISP)				return	FULL_DSIP_MODE;
								if (ubk.s.dispSelect & GDBG_DISP)				return	GDNG_DSIP_MODE;
								if (ubk.s.dispSelect & MISENUM_DISP)			return	MISENUM_DSIP_MODE;
								break;

		case	CHOMISENUM_DSIP_MODE:
								if (ubk.s.dispSelect & CLOCK_DISP)				return	CLOCK_DSIP_MODE;
								if (ubk.s.dispSelect & BASE_DISP)				return	FULL_DSIP_MODE;
								if (ubk.s.dispSelect & GDBG_DISP)				return	GDNG_DSIP_MODE;
								if (ubk.s.dispSelect & MISENUM_DISP)			return	MISENUM_DSIP_MODE;
								break;

		case	CLOCK_DSIP_MODE:

								if (ubk.s.dispSelect & BASE_DISP)				return	FULL_DSIP_MODE;
								if (ubk.s.dispSelect & GDBG_DISP)				return	GDNG_DSIP_MODE;
								if (ubk.s.dispSelect & MISENUM_DISP)			return	MISENUM_DSIP_MODE;
								if (ubk.s.dispSelect & CHOMISENUM_DISP)			return	CHOMISENUM_DSIP_MODE;
								if (ubk.s.dispSelect & CLOCK_DISP)				return	CLOCK_DSIP_MODE;
								break;

		default:				return	FULL_DSIP_MODE;
								break;

	}
}
//------------------------------------------------------------------------------
void   analysis_data(uint8_t   *rxdata)
{
	uint32_t        ltime;
	uint16_t        i, j;
	uint8_t         buf[16], retstatus,n;
	struct  STINFO  *tst;

	static  union       ubk_struct  tubk;

	tst = (struct  STINFO *)rxdata;
	switch (tst->cmd)
	{
	case  0x5F:
		play_sound(SOUND_BEEP);
		if (test_flag)	test_flag = 0;
		else
		{
			test_flag = 1;
		}
		mc100live.oldcmd = '_';
		break;

	case  'c':
		backdata_init();
		flash_backup = 1;
		req_satstu_flag = 1;
		mc100live.oldcmd = 'c';
		break;

	case  'E':
		mc100live.fc_mode = ERASE_MODE;
		mc100live.oldcmd = tst->cmd;
		break;

	case  'M':
		mc100live.fc_mode = DATA_MODE;
		mc100live.oldcmd = tst->cmd;
		break;

	case  'm':
		mc100live.fc_mode = DFAT_MODE;
		mc100live.oldcmd = tst->cmd;
		break;

	case    'i':


		play_sound(SOUND_BEEP);
		mc100live.oldcmd = 'i';
		req_satstu_flag = 1;
		break;

	case	'*':
		//m_mode = 150;

		WiFi.mode(WIFI_STA);
		WiFi.disconnect();
		delay(100);
		Serial.println("scan start");

		// WiFi.scanNetworks will return the number of networks found
		n = WiFi.scanNetworks();
		Serial.println("scan done");
		if (n == 0) {
			Serial.println("no networks found");
		}
		else {
			Serial.print(n);
			Serial.println(" networks found");
			for (int i = 0; i < n; ++i) {
				// Print SSID and RSSI for each network found
				Serial.print(i + 1);
				Serial.print(": ");
				Serial.print(WiFi.SSID(i));
				Serial.print(" (");
				Serial.print(WiFi.RSSI(i));
				Serial.print(")");
				Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
				delay(10);
			}
		}
		Serial.println("");

		// Wait a bit before scanning again
		delay(5000);
		break;

	case    'R':
		display_info();
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		break;

	case    'T':
		ubk.s.NightStart		= (uint16_t)rxdata[1] + (uint16_t)rxdata[2] * 256;
		ubk.s.NightEnd			= (uint16_t)rxdata[5] + (uint16_t)rxdata[6] * 256;
		ubk.s.dispSelect		= (uint16_t)rxdata[9] + (uint16_t)rxdata[10] * 256;
		ubk.s.soundInterval		= (uint16_t)rxdata[13] + (uint16_t)rxdata[14] * 256;
		flash_backup = 1;
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		hputs(0, 0, "set T", 0);
		convert_img_ws2812(CRGB::Blue, 0);
		FastLED.show();
		delay(500);

		break;

	case '?':
		for (int i = 0; i < 6; i++)
			ubk.s.mac[i] = rxdata[1 + i];
		flash_backup = 1;
		play_sound(SOUND_BEEP);
		hputs(0, 0, "set M", 0);
		convert_img_ws2812(CRGB::Blue, 0);
		FastLED.show();
		delay(500);
		break;

	// case	'@':
	// 	play_sound(SOUND_BEEP);
	// 	hputs(0, 0, "set A", 0);
	// 	convert_img_ws2812(CRGB::Blue, 0);
	// 	FastLED.show();
	// 	misedust = rxdata[1];
	// 	ultramisedust = rxdata[2];

	// 	if (misedust == 0)			misedust = 50000;
	// 	if (ultramisedust == 0)		ultramisedust = 50000;

	// 	Serial.print("misedust : ");
	// 	Serial.println(misedust);
	// 	Serial.print("ultramisedust : ");
	// 	Serial.println(ultramisedust);

	// 	if (misedust == 50000)			zone = 10;
	// 	else if (misedust < 31)			zone = 0;
	// 	else if (misedust < 81)			zone = 1;
	// 	else if (misedust < 151)		zone = 2;
	// 	else												zone = 3;
	// 	if(!khaiGrade) 	zone = 10;
	// 	else						zone = khaiGrade - 1;

	// 	if (ultramisedust == 50000)		ultrazone = 10;
	// 	else if (ultramisedust < 16)    ultrazone = 0;
	// 	else if (ultramisedust < 36)    ultrazone = 1;
	// 	else if (ultramisedust < 76)	ultrazone = 2;
	// 	else                            ultrazone = 3;

	// 		if (zone == 10)
	// 		{
	// 			if (ultrazone < 10)
	// 			{
	// 				selzone = ultrazone;
	// 			}
	// 		}
	// 		else if (ultrazone == 10)
	// 		{
	// 			if (zone < 10)
	// 			{
	// 				selzone = zone;
	// 			}
	// 		}
	// 		else
	// 		{
	// 			if (ultrazone > zone)
	// 			{
	// 				selzone = ultrazone;
	// 			}
	// 			else
	// 			{
	// 				selzone = zone;
	// 			}
	// 		}

	// 	Serial.print("zone : ");
	// 	Serial.println(zone);
	// 	Serial.print("ultrazone : ");
	// 	Serial.println(ultrazone);
	// 	Serial.print("selzone : ");
	// 	Serial.println(selzone);
	// 	break;

	case    'Y':
		for (int i = 0; i < 31; i++)
			ubk.s.ssid[i] = rxdata[1 + i];
		ubk.s.ssid[31] = 0;
		flash_backup = 1;
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		hputs(0, 0, "set Y1", 0);
		convert_img_ws2812(CRGB::Blue, 0);
		FastLED.show();
		delay(500);
		break;

	case    'y':
		for (int i = 0; i < 31; i++)
			ubk.s.password[i] = rxdata[1 + i];
		ubk.s.password[31] = 0;
		flash_backup = 1;
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		hputs(0, 0, "set Y2", 0);
		convert_img_ws2812(CRGB::Blue, 0);
		FastLED.show();
		delay(500);
		break;

	case    'S':
		for (int i = 0; i < 31; i++)
			ubk.s.measurestation[i] = rxdata[1 + i];
		ubk.s.measurestation[31] = 0;
		flash_backup = 1;
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		hputs(0, 0, "set S1", 0);
		convert_img_ws2812(CRGB::Blue, 0);
		FastLED.show();
		delay(500);
		break;

	case    's':
		for (int i = 0; i < 31; i++)
			ubk.s.measurestation2[i] = rxdata[1 + i];
		ubk.s.measurestation2[31] = 0;
		flash_backup = 1;
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		hputs(0, 0, "set S2", 0);
		convert_img_ws2812(CRGB::Blue, 0);
		FastLED.show();
		delay(500);
		break;


	case  'W':
		ubk.s.ip[0] = rxdata[1];
		ubk.s.ip[1] = rxdata[2];
		ubk.s.ip[2] = rxdata[3];
		ubk.s.ip[3] = rxdata[4];
		ubk.s.subnetmask[0] = rxdata[5];
		ubk.s.subnetmask[1] = rxdata[6];
		ubk.s.subnetmask[2] = rxdata[7];
		ubk.s.subnetmask[3] = rxdata[8];
		ubk.s.gateway[0] = rxdata[9];
		ubk.s.gateway[1] = rxdata[10];
		ubk.s.gateway[2] = rxdata[11];
		ubk.s.gateway[3] = rxdata[12];
		ubk.s.dns[0] = rxdata[13];
		ubk.s.dns[1] = rxdata[14];
		ubk.s.dns[2] = rxdata[15];
		ubk.s.dns[3] = rxdata[16];
		ubk.s.deviceID = *(uint32_t *)&rxdata[17];

		ubk.s.port = (uint16_t)rxdata[22] * 256 + rxdata[21];
		ubk.s.data = rxdata[23];
		ubk.s.magicnum = MAGIC_NUMBER;
		//ubk.s.chksum = GetCheckSum(63, uc._b);
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		flash_backup = 1;
		hputs(0, 0, "set W", 0);
		convert_img_ws2812(CRGB::Blue, 0);
		FastLED.show();
		delay(500);
		break;

	case 'D':
		ubk.s.sounddelay = rxdata[1];
		playsound_flag = 0;
		play_sound(SOUND_BEEP);
		flash_backup = 1;
		hputs(0, 0, "set D", 0);
		convert_img_ws2812(CRGB::Blue, 0);
		FastLED.show();
		delay(500);
		break;
	}
}

//-----------------------------------------------------------------------------
void eeprom_datacontrol(void)		//10msec
{
	if (!flash_backup)	return;
	flash_backup = 0;
	//Serial.println("eeprom_datacontrol");
	detachInterrupt(RX358_DATA);

	ubk.s.magicnum = MAGIC_NUMBER;
	ubk.s.chksum = CRC16((BKP_DR_NUMBER - 2), ubk._b);
	//EEPROM.put(0, ubk.s);
	for (int i = 0;i < BKP_DR_NUMBER;i++)
	{
		EEPROM.write(i, ubk._b[i]);//ubk._b[i]);
		//Serial.print(ubk._b[i],HEX); Serial.print(" ");
	}
	EEPROM.commit();
	delay(100);
	attachInterrupt(RX358_DATA, interruptFunction, CHANGE);

	//Serial.println();
	//Serial.print(EEPROM_SIZE);
	//Serial.println(" bytes written on Flash . Values are:");
	////EEPROM.get(0, tmp.s);
	////interrupts();

	//for (int i = 0; i <EEPROM_SIZE; i++)
	//{
	//	Serial.print(byte(EEPROM.read(i)),HEX); Serial.print(" ");
	//}
	//Serial.println(); Serial.println("----------------------------------");
}
//------------------------------------------------------------------------------
void    push_status(void)
{
	static  uint8_t   seqble;
	uint8_t i;
	//return;
	if (req_satstu_flag)
	{
		req_satstu_flag = 0;
		switch (mc100live.oldcmd)
		{
		case  '0':
		case  '4':
		case  '9':
		case  '8':
		case  '7':
		case  'R':
		case  'P':
		case  'E':
		case  'V':
		case  'v':
		case  'I':
		case  'i':
		case  'g':
		case  'f':
		case  'y':
		case  'Y':
		case  '@':
		case  '#':
		case  '$':
			for (int i = 0; i < 26; i++)
				ust._b[i + 3] = ubk._b[i];
			ust.s.cmd = mc100live.oldcmd;
			break;

		case  'G':
		case  'F':
			for (int i = 0; i < 26; i++)
				ust._b[i + 3] = ubk._b[i];
			ust.s.cmd = mc100live.oldcmd;
			break;


		case  'm':
		case  'M':
			ust.s.cmd = mc100live.oldcmd;
			break;

		}
		ust.s.stx = 0x02;
		ust.s.num = 24;
		ust.s.etx = 0x03;
		ust.s.crc16 = CRC16(CMD_CRC_SIZE, ust._b);
		Serial.write(ust._b, 29);
	}
}

//-------------------------------------------------------------------------------------------------
void microdust_control(void)			//100msec
{
	EthernetClient httpe;
	HTTPClient	http;	
	static  uint16_t	wpos, timecnt, waittime;
	String		payload;
	char webdata[2048];
	char	temp[16], errcode;
	char	*pstr;

  timerWrite(timer, 0); //reset timer (feed watchdog)

	if (setup_flag)		return;

	switch(mc100live.server_mode)
	{
		case	0:
						if(++timecnt < 12000)			//20min		12000
							break;
						timecnt = 0;
						mc100live.server_mode = 20;
						break;

		case	20:
			Serial.println(ubk.s.serverName);
			wpos = 0;
			while (1)
			{
				if ((ubk.s.data & ETHERNET_SET) == ETHERNET_SET)
				{
					if (apikey[wpos])
					{								
						mc100live.xbuf[wpos] = apikey[wpos];				
						Serial.println(mc100live.xbuf[wpos++]);
					}
					else
					{
						mc100live.xbuf[wpos] = 0;				
						break;
					}
				}
				else
				{
					if (wifiapikey[wpos])
					{
						mc100live.xbuf[wpos] = wifiapikey[wpos];
						Serial.println(mc100live.xbuf[wpos++]);						
					}
					else
					{
						mc100live.xbuf[wpos] = 0;				
						break;
					}
				}
			}
			wpos = 0;
			while (ubk.s.measurestation[wpos])
			{
				sprintf(temp, "%%%02X", ubk.s.measurestation[wpos]);
				strcat(mc100live.xbuf, temp);
				wpos++;
			}
			strcat(mc100live.xbuf, "&dataTerm=DAILY&ver=1.0");
			if ((ubk.s.data & ETHERNET_SET) == ETHERNET_SET)
			{
				Serial.println(mc100live.xbuf);
				int statusCode = 	Rclient.get(mc100live.xbuf,&payload);
				Serial.print("statusCode = ");
				Serial.println(statusCode);
				if (statusCode == 200)
				{
					Serial.println(payload);        //DEBUG		
  				StaticJsonDocument<2048> doc;				
  				DeserializationError error = deserializeJson(doc, payload);					
					mc100live.khaiGrade = doc["response"]["body"]["items"][0]["khaiGrade"];
					mc100live.khaiValue = doc["response"]["body"]["items"][0]["khaiValue"];
					mc100live.pm10Value = doc["response"]["body"]["items"][0]["pm10Value"];
					mc100live.pm25Value = doc["response"]["body"]["items"][0]["pm25Value"];
					mc100live.pm10Flag = doc["response"]["body"]["items"][0]["pm10Flag"];
					mc100live.pm25Flag = doc["response"]["body"]["items"][0]["pm25Flag"];
					if(mc100live.khaiGrade)	mc100live.khaiGrade -= 1;
					datavalidate_flag = 1;
				}
				else
				{
					Serial.printf("ETHERNET GET... failed, error: %d\n", statusCode);
					mc100live.server_mode = 100;
					color_misedust(CRGB::White);
				}
				mc100live.server_mode = 0;
			}
			else
			{
				http.begin(mc100live.xbuf);
				//http.begin("http://openapi.airkorea.or.kr/openapi/services/rest/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?serviceKey=UTtJn%2FonwbUxkVwiHfSlxwn1q98%2BgWbmijJDK4Byn8wamaMMVXV4hAMnvIxLY3OJKZ%2FCmqSzX7aBIgfqvUjIUg%3D%3D&numOfRows=1&pageNo=1&stationName=%EC%9D%B8%EA%B3%84%EB%8F%99&dataTerm=DAILY&ver=1.3");
				Serial.println("SERVER REQ");
				Serial.println(mc100live.xbuf);        //DEBUG
				int httpCode = http.GET();
				if (httpCode > 0)
				{
					payload = http.getString();
					Serial.println(payload);        //DEBUG		
  				StaticJsonDocument<2048> doc;				
  				DeserializationError error = deserializeJson(doc, payload);					
					mc100live.khaiGrade = doc["response"]["body"]["items"][0]["khaiGrade"];
					mc100live.khaiValue = doc["response"]["body"]["items"][0]["khaiValue"];
					mc100live.pm10Value = doc["response"]["body"]["items"][0]["pm10Value"];
					mc100live.pm25Value = doc["response"]["body"]["items"][0]["pm25Value"];
					mc100live.pm10Flag = doc["response"]["body"]["items"][0]["pm10Flag"];
					mc100live.pm25Flag = doc["response"]["body"]["items"][0]["pm25Flag"];
					if(mc100live.khaiGrade)	mc100live.khaiGrade -= 1;
					datavalidate_flag = 1;

					// if (strstr(webdata, "<resultCode>00"))
					// {
						
					// }
				}
				else
				{
					Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
					color_misedust(CRGB::White);
					mc100live.server_mode = 100;
				}
				http.end();
				mc100live.server_mode = 0;
			}
			break;

		case	100:
				if(++timecnt < 1200)			//20min		12000
					break;
				timecnt = 0;
				mc100live.server_mode = 20;		
			break;			
	}
}
//-----------------------------------------------------------------------------
String getChipId()
{
  String ChipIdHex = String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  ChipIdHex += String((uint32_t)ESP.getEfuseMac(), HEX);
  return ChipIdHex;
}
//-----------------------------------------------------------------------------
uint64_t getChipId_int()
{
  return ESP.getEfuseMac();
}
void wifiupdate(void)
{

  String url = "http://otadrive.com/deviceapi/update?";
  url += "k=a4644fa3-1542-43c6-a4cb-6fea9946895d";
  url += "&v=1.0.0.37";
  url += "&s=" + getChipId();

  WiFiClient client;
  httpUpdate.update(client, url, "1.0.0.37");
}
//------------------------------------------------------------------------------
void checkplay_sound(void)		//100msec
{

	if (guidesound_flag)
	{
		if (mc100live.soundelaytime)
		{
			mc100live.soundelaytime--;
		}
		else
		{
			guidesound_flag = 0;
			play_sound(mc100live.khaiGrade+ 1);
		}
	}
	else
		mc100live.soundelaytime = 0;
}
//------------------------------------------------------------------------------
void  check_soundbusy(void)   //2msec
{
	uint8_t tmp[4];
	static uint16_t		wcnt;
	if (playsound_flag)
	{
		if (read_fc1004(SBC_CON | SBC_READ, 1, tmp) == 0x00)
		{
			if (++wcnt > 2000)
			{
				wcnt = 0;
				playsound_flag = 0;
			}
		}
		else
			wcnt = 0;
	}
	else
		wcnt = 0;
}
#if 0
http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getMsrstnAcctoRltmMesureDnsty?serviceKey=UTtJn%2FonwbUxkVwiHfSlxwn1q98%2BgWbmijJDK4Byn8wamaMMVXV4hAMnvIxLY3OJKZ%2FCmqSzX7aBIgfqvUjIUg%3D%3D&returnType=json&numOfRows=1&pageNo=1&stationName=%EB%B0%B1%EC%84%9D%EC%9D%8D&dataTerm=DAILY&ver=1.0
{"response":{"body":{"totalCount":23,"items":[{"so2Grade":"1","coFlag":null,"khaiValue":"72","so2Value":"0.004","coValue":"0.4","pm25Flag":null,"pm10Flag":null,"pm10Value":"87","o3Grade":"1","khaiGrade":"2","pm25Value":"59","no2Flag":null,"no2Grade":"1","o3Flag":null,"pm25Grade":"2","so2Flag":null,"dataTime":"2023-05-03 08:00","coGrade":"1","no2Value":"0.026","pm10Grade":"2","o3Value":"0.021"}],"pageNo":1,"numOfRows":1},"header":{"resultMsg":"NORMAL_CODE","resultCode":"00"}}}
khaiValue = {"body":{"totalCount":23,"items":[{"so2Grade":"1","coFlag":null,"khaiValue":"72","so2Value":"0.004","coValue":"0.4","pm25Flag":null,"pm10Flag":null,"pm10Value":"87","o3Grade":"1","khaiGrade":"2","pm25Value":"59","no2Flag":null,"no2Grade":"1","o3Flag":null,"pm25Grade":"2","so2Flag":null,"dataTime":"2023-05-03 08:00","coGrade":"1","no2Value":"0.026","pm10Grade":"2","o3Value":"0.021"}],"pageNo":1,"numOfRows":1},"header":{"resultMsg":"NORMAL_CODE","resultCode":"00"}}


#endif