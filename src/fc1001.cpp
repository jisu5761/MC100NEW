#include <Arduino.h>
#include <FastLED.h>
#include "mytype.h"
#include "portdef.h"
#include "fc1001.h"
#include "proc.h"
//------------------------------------------------------------------------------
void  sdiodelay(uint8_t c)
{
	volatile static  uint8_t  i;
	for (i = 0; i < c; i++);
}
//------------------------------------------------------------------------------
void  sclk_cmd(void)
{
	SSCK_HIGH;
	sdiodelay(10);
	SSCK_LOW;
	sdiodelay(10);
}
//------------------------------------------------------------------------------
void    write_fc1004(uint8_t addr, uint8_t lng, uint8_t *buf)
{
	uint8_t i, dummy;
	SCSB_SEL_LOW;
	SCSB_LOW;

	SSDATI_LOW;
	SSCK_LOW;
	dummy = addr;
	for (i = 0; i < 8; i++)
	{
		if (dummy & 0x80)    SSDATI_HIGH;
		else                SSDATI_LOW;
		sclk_cmd();
		dummy <<= 1;
	}
	dummy = buf[0];
	for (i = 0; i < 8; i++)
	{
		if (dummy & 0x80)    SSDATI_HIGH;
		else                SSDATI_LOW;
		sclk_cmd();
		dummy <<= 1;
	}

	SCSB_HIGH;
}
//------------------------------------------------------------------------------
uint8_t    read_fc1004(uint8_t addr, uint8_t lng, uint8_t *buf)
{
	uint8_t i, dummy;
	SCSB_SEL_LOW;
	SCSB_LOW;

	SSDATI_LOW;
	SSCK_LOW;
	dummy = addr;
	for (i = 0; i < 8; i++)
	{
		if (dummy & 0x80)    SSDATI_HIGH;
		else                SSDATI_LOW;
		sclk_cmd();
		dummy <<= 1;
	}
	dummy = 0;
	for (i = 0; i < 8; i++)
	{
		dummy *= 2;
		if (SSDATO_IN)    dummy += 1;
		sclk_cmd();
	}
	SCSB_HIGH;
	return  dummy;
}

//------------------------------------------------------------------------------
void    play_sound(uint8_t  no)
{
	if (ubk.s.volume == 0)	return;
	if (playsound_flag)		return;
	if (offtime_flag)		return;

	write_fc1004(DEC_CHAP | SBC_WRITE, 1, &no);
	playsound_flag = 1;
}
//------------------------------------------------------------------------------
uint8_t crc_check(void)
{
	uint8_t tmp[4];
	tmp[0] = 0x01;
	write_fc1004(SF_CRC | SBC_WRITE, 1, tmp);
	do {
		tmp[0] = read_fc1004(SF_CRC | SBC_READ, 1, tmp);
	} while (tmp[0] == 0x01);
	tmp[0] = read_fc1004(CRC_RESULT | SBC_READ, 1, tmp);
	return  tmp[0];
}
//------------------------------------------------------------------------------
uint8_t    spi_write(uint16_t   lng, uint8_t *buf)
{
	uint16_t i, j, dummy;
	for (i = 0; i < lng; i++)
	{
		dummy = buf[i];
		for (j = 0; j < 8; j++)
		{
			if (dummy & 0x80)    SSDATI_HIGH;
			else                SSDATI_LOW;
			sclk_cmd();
			dummy <<= 1;
		}
	}
	return 0;
}
//------------------------------------------------------------------------------
uint8_t    spi_read(uint16_t   lng, uint8_t *buf)
{
	uint16_t i, j, dummy = 0;

	for (i = 0; i < lng; i++)
	{
		dummy = 0;
		for (j = 0; j < 8; j++)
		{
			dummy *= 2;
			if (SSDATO_IN)    dummy += 1;
			sclk_cmd();
		}
		buf[i] = dummy;
	}
	return  dummy;
}
//------------------------------------------------------------------------------
void    SFspi_start(void)
{
	SCSB_LOW;
	SCSB_SEL_HIGH;
}
//------------------------------------------------------------------------------
void    SFspi_finish(void)
{
	SCSB_HIGH;
	SCSB_SEL_LOW;
}
//------------------------------------------------------------------------------
uint8_t    SFwrite_enable(void)
{
	uint8_t tmp[16];
	SFspi_start();
	tmp[0] = 0x06;
	spi_write(1, tmp);
	SFspi_finish();
	SFbusy_check();
	return 0;
}
//------------------------------------------------------------------------------
uint8_t    SFwrite_disable(void)
{
	uint8_t tmp[16];
	SFspi_start();
	tmp[0] = 0x04;
	spi_write(1, tmp);
	SFspi_finish();
	SFbusy_check();
	return 0;
}
//------------------------------------------------------------------------------
uint8_t    SFread_status_register(void)
{
	uint8_t tmp[4];

	SFspi_start();
	tmp[0] = F_RDSR;
	spi_write(1, tmp);
	spi_read(1, tmp);
	SFspi_finish();
	return tmp[0];
}
uint8_t Rdata;
//------------------------------------------------------------------------------
void SFbusy_check(void)
{
	volatile  uint8_t c;

	do {
		for (c = 0; c < 200; c++);
		Rdata = SFread_status_register();
		//__HAL_IWDG_RELOAD_COUNTER(&hiwdg);    
	} while ((Rdata&F_WIP));
}

//------------------------------------------------------------------------------
uint8_t     read_fc1004id(void)
{
	uint8_t tmp[16], i;
	SFspi_start();
	tmp[0] = 0x9F;
	spi_write(1, tmp);
	spi_read(3, tmp);
	//    for(i = 0;i < 5;i ++)
	//       str[i] = tmp[i];
	SFspi_finish();

	// SFwrite_disable();
	return  0;
}
//------------------------------------------------------------------------------
void SFWrite_status_register(uint8_t buf)
{
	uint8_t tmp[4];
	SFwrite_enable();
	SFspi_start();
	tmp[0] = F_WRSR;
	tmp[1] = buf;
	spi_write(2, tmp);
	SFspi_finish(); // Stop
	SFbusy_check();
}
//------------------------------------------------------------------------------
uint8_t    erase_fc1004(void)
{
	uint8_t tmp[6];
	SFWrite_status_register(0x02);
	SFwrite_enable();
	SFspi_start();
	tmp[0] = F_CE;
	spi_write(1, tmp);
	SFspi_finish();

	SFbusy_check();
	return 0;

}
//------------------------------------------------------------------------------
uint8_t    program_fc1004(uint32_t  addr, uint16_t  lng, uint8_t  *buf)
{

	uint8_t     tmp[4];

	SFwrite_enable();
	SFspi_start();
	tmp[0] = F_PP;
	tmp[1] = addr >> 16;
	tmp[2] = addr >> 8;
	tmp[3] = addr;

	spi_write(4, tmp);
	spi_write(256, buf);
	SFspi_finish();
	SFWrite_status_register(0x3C);
	SFbusy_check();
	return  0;
}

//------------------------------------------------------------------------------
uint8_t    dump_fc1004(uint32_t  addr, uint16_t  lng, uint8_t  *buf)
{
	uint8_t tmp[16], i;
	SFspi_start();
	tmp[0] = F_READ;
	tmp[1] = addr >> 16;
	tmp[2] = addr >> 8;
	tmp[3] = addr;
	spi_write(4, tmp);
	spi_read(lng, buf);
	SFspi_finish();

	return  0;
}
//------------------------------------------------------------------------------
void  fc1004_control(void)      //10msec
{
	static          uint32_t    addrs;
	uint8_t buf[2], retstatus;
	switch (mc100live.fc_mode)
	{
	case  IDLE_MODE:
		break;

	case  ERASE_MODE:
		buf[0] = 0x02;
		write_fc1004(AUDIO_CON | SBC_WRITE, 1, buf);
		retstatus = erase_fc1004();
		req_satstu_flag = 1;
		addrs = 0;
		mc100live.fc_mode = IDLE_MODE;
		break;

	case  DATA_MODE:
		program_fc1004(addrs, 256, (uint8_t *)&mc100live.xbuf[4]);
		addrs += 256;
		req_satstu_flag = 1;
		mc100live.fc_mode = IDLE_MODE;
		break;

	case    DFAT_MODE:
		if (addrs < 0xFFC00) addrs = 0xFFC00;
		program_fc1004(addrs, 256, (uint8_t *)&mc100live.xbuf[4]);
		addrs += 256;
		req_satstu_flag = 1;
		mc100live.fc_mode = IDLE_MODE;
		break;
	}
}