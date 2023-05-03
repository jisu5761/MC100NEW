#ifndef   __PORT_DEF__
#define   __PORT_DEF__

#define  RX358_DATA     15
#define  SW1            39
#define  SW2            34
#define  PIR            36
#define  SSDATO         32
#define  SSCK           25
#define  SSDATI         33
#define  SCSB           26
#define  SCSB_SEL       27
#define	 LED_PIN		    13
#define	 N358_PIN		    12

#define  STLEDPIN		    4
#define  W5500RST		    16
#define  SI446X_SDN		  14

#define   SSDATO_IN                   digitalRead(SSDATO)

#define   SSCK_HIGH                   digitalWrite(SSCK , HIGH)
#define   SSCK_LOW                    digitalWrite(SSCK , LOW)

#define   SSDATI_HIGH                 digitalWrite(SSDATI , HIGH)
#define   SSDATI_LOW                  digitalWrite(SSDATI , LOW)

#define   SCSB_HIGH                   digitalWrite(SCSB , HIGH)
#define   SCSB_LOW                    digitalWrite(SCSB , LOW)

#define   SCSB_SEL_HIGH               digitalWrite(SCSB_SEL , HIGH)
#define   SCSB_SEL_LOW                digitalWrite(SCSB_SEL , LOW)

#define   N358_HIGH					          digitalWrite(N358_PIN , HIGH)
#define   N358_LOW					          digitalWrite(N358_PIN , LOW)

#define   SI446X_SDN_HIGH             digitalWrite(SI446X_SDN , HIGH)
#define   SI446X_SDN_LOW              digitalWrite(SI446X_SDN , LOW)

#endif