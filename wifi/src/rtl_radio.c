/* SPDX-License-Identifier: MIT
 * Checked transport adapter for ISC-licensed NetBSD RTL8192C routines/tables.
 * EFUSE is only read, never programmed. First transport error stops writes.
 */
#include "rtl_radio.h"
#include <string.h>
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef __aligned
#define __aligned(n) __attribute__((aligned(n)))
#endif
#define __BIT(n) (1u<<(n))
#define __arraycount(a) (sizeof(a)/sizeof((a)[0]))
#define URTWN_RIDX_COUNT 28
#define URTWN_CHIP_92C 1
#define URTWN_CHIP_92C_1T2R 2
#define URTWN_CHIP_UMC_A_CUT 8
#define URTWN_CHIP_88E 16
#define URTWN_CHIP_92EU 32
#define URTWN_FLAG_CCK_HIPWR 1
#define IEEE80211_HTINFO_2NDCHAN_NONE 0
#define ISSET(a,b) ((a)&(b))
#define SET(a,b) ((a)|=(b))
#include "../third_party/urtwn-reference/if_urtwnreg.h"
#include "../third_party/urtwn-reference/if_urtwn_data.h"
typedef unsigned int u_int;
struct urtwn_softc {
    RtlIo *io; RtlDelayUs delay; void *ctx; RtlRadioReport *r;
    int error; unsigned chip, board_type, regulatory, nrxchains, ntxchains, sc_flags;
    uint8_t pa_setting, r88e_rom[256];
    struct r92c_rom rom; uint32_t rf_chnlbw[2];
};
int rtl_radio_write_allowed(unsigned a,unsigned n,const uint8_t *p){
    if(!p||(n!=1&&n!=2&&n!=4))return 0;
    /* No writes to EEPROM, EFUSE programming voltage or firmware RAM. */
    if(a==0x30)return n==4 && !(p[3]&0x80);
    if(a>=0x100&&a+n<=0x1000)return 1;
    if(a>=0xfe40&&a+n<=0xfe43)return 1;
    if(a==0xfe55&&n==1)return !(p[0]&R92C_USB_SPECIAL_OPTION_AGG_EN);
    return ((a==0x24||a==0x28)&&n==4)||(a==0&&(n==2))||(a==2&&(n==1||n==2))||(a==8&&n==2)||
        ((a==0x15||a==0x16||a==0x1f||a==0x22||a==0x25||a==0x28||a==0x29)&&n==1);
}
static uint32_t rd(struct urtwn_softc *s,unsigned a,unsigned n){
    uint32_t v=0;if(s->error)return 0;
    s->r->transfers++;s->r->last_address=a;
    s->error=rtl_read_register(s->io,a,n,&v);return v;
}
static void wr(struct urtwn_softc *s,unsigned a,unsigned n,uint32_t v){
    uint8_t p[4];RtlRequest q;unsigned i;int e;if(s->error)return;
    for(i=0;i<n;i++)p[i]=(uint8_t)(v>>(8*i));
    if(!rtl_radio_write_allowed(a,n,p)){s->error=RTL_E_ARGUMENT;return;}
    q.type=0x40;q.request=5;q.value=a;q.request_index=0;q.length=n;
    s->r->transfers++;s->r->last_address=a;e=s->io->control(s->io->context,&q,p);
    if(e!=(int)n)s->error=e<0?e:RTL_E_SHORT;
}
#define urtwn_read_1(s,a) rd(s,a,1)
#define urtwn_read_2(s,a) rd(s,a,2)
#define urtwn_read_4(s,a) rd(s,a,4)
#define urtwn_bb_read(s,a) rd(s,a,4)
#define urtwn_write_1(s,a,v) wr(s,a,1,v)
#define urtwn_write_2(s,a,v) wr(s,a,2,v)
#define urtwn_write_4(s,a,v) wr(s,a,4,v)
#define urtwn_bb_write(s,a,v) wr(s,a,4,v)
#define DELAY(us) do{if(!sc->error)sc->delay(sc->ctx,us);}while(0)
#define urtwn_delay_ms(s,ms) do{if(!(s)->error)(s)->delay((s)->ctx,(ms)*1000);}while(0)
#include "rtl_radio_netbsd.inc"
static void mod(struct urtwn_softc *s,unsigned a,unsigned n,uint32_t clear,uint32_t set){
    uint32_t v=rd(s,a,n);wr(s,a,n,(v&~clear)|set);
}
static uint8_t efuse(struct urtwn_softc *s,unsigned a){
    unsigned i;uint32_t v;if(a>=512){s->error=RTL_E_ARGUMENT;return 255;}
    v=rd(s,0x30,4);wr(s,0x30,4,(v&~0x8003ff00u)|(a<<8));
    for(i=0;i<100&&!s->error;i++){
        v=rd(s,0x30,4);if(v&0x80000000u)return (uint8_t)v;
        s->delay(s->ctx,5);
    }
    if(!s->error)s->error=RTL_E_TIMEOUT;
    return 255;
}
static void context(struct urtwn_softc *s,RtlIo *io,RtlDelayUs delay,void *ctx,RtlRadioReport *r){
    memset(s,0,sizeof(*s));s->io=io;s->delay=delay;s->ctx=ctx;s->r=r;
    memcpy(&s->rom,r->rom,sizeof(s->rom));s->chip=URTWN_CHIP_92C;
    s->board_type=r->board;s->regulatory=s->rom.rf_opt1&7;
    s->nrxchains=s->ntxchains=2;s->pa_setting=r->pa_setting;
    memcpy(s->rf_chnlbw,r->rf_channel,sizeof(r->rf_channel));
}
int rtl_radio_channel(RtlIo *io,RtlDelayUs delay,void *ctx,RtlRadioReport *r,unsigned ch){
    struct urtwn_softc s;unsigned i;uint16_t power[28];
    if(!io||!io->control||!delay||!r||ch<1||ch>13)return RTL_E_ARGUMENT;
    context(&s,io,delay,ctx,r);r->stage=RTL_RADIO_CHANNEL;
    for(i=0;i<2;i++){
        urtwn_get_txpower(&s,i,ch,0,power);urtwn_write_txpower(&s,i,power);
        urtwn_rf_write(&s,i,R92C_RF_CHNLBW,(s.rf_chnlbw[i]&~0xfffu)|ch|R92C_RF_CHNLBW_BW20);
    }
    mod(&s,R92C_BWOPMODE,1,0,R92C_BWOPMODE_20MHZ);
    mod(&s,R92C_FPGA0_RFMOD,4,R92C_RFMOD_40MHZ,0);
    mod(&s,R92C_FPGA1_RFMOD,4,R92C_RFMOD_40MHZ,0);
    mod(&s,R92C_FPGA0_ANAPARAM2,4,0,R92C_FPGA0_ANAPARAM2_CBW20);
    if(!s.error){r->channel=ch;r->stage=RTL_RADIO_READY;}
    return r->result=s.error;
}
int rtl_radio_bssid(RtlIo *io,RtlDelayUs delay,void *ctx,RtlRadioReport *r,const uint8_t bssid[6]){
    struct urtwn_softc s;unsigned i;
    if(!io||!io->control||!delay||!r||(bssid&&(bssid[0]&1)))return RTL_E_ARGUMENT;
    context(&s,io,delay,ctx,r);
    if(bssid){
        for(i=0;i<6;i++)wr(&s,R92C_BSSID+i,1,bssid[i]);
        /* Leaving survey: resume all transmit queues and reject unrelated
         * BSS traffic before it can fill the full-speed USB receive queue. */
        wr(&s,R92C_TXPAUSE,1,0);
        mod(&s,R92C_RCR,4,R92C_RCR_AAP,R92C_RCR_CBSSID_DATA|R92C_RCR_CBSSID_BCN);
    }else{
        /* A new survey must see beacons from every BSS again. */
        mod(&s,R92C_RCR,4,R92C_RCR_CBSSID_DATA|R92C_RCR_CBSSID_BCN,R92C_RCR_AAP);
    }
    return r->result=s.error;
}
int rtl_radio_init(RtlIo *io,RtlDelayUs delay,void *ctx,RtlRadioReport *r){
    struct urtwn_softc s;unsigned a=0,i,j;uint8_t h,*rom;uint32_t cfg;
    if(!io||!io->control||!delay||!r)return RTL_E_ARGUMENT;
    memset(r,0,sizeof(*r));context(&s,io,delay,ctx,r);r->stage=RTL_RADIO_ROM;
    cfg=rd(&s,0xf0,4);
    if(s.error)return r->result=s.error;
    if(!(cfg&0x08000000u)||(cfg&0x00800000u))return r->result=RTL_BOOT_UNSUPPORTED;
    /* This first physical port targets two-transmit-chain RTL8192CU boards.
     * Reject cut-down bonding instead of applying the wrong RF power tables. */
    cfg=rd(&s,R92C_HPON_FSM,4);
    if(s.error)return r->result=s.error;
    if(MS(cfg,R92C_HPON_FSM_CHIP_BONDING_ID)==R92C_HPON_FSM_CHIP_BONDING_ID_92C_1T2R)
        return r->result=RTL_BOOT_UNSUPPORTED;
    mod(&s,0,2,0,0x8000);mod(&s,2,2,0,0x1000);mod(&s,8,2,0,0x22);
    rom=(uint8_t*)&s.rom;memset(rom,255,128);
    while(a<512&&!s.error){
        h=efuse(&s,a++);if(h==255)break;
        for(i=0;i<4&&!s.error;i++)if(!(h&(1<<i))){
            if(a>510){s.error=RTL_RADIO_TRUNCATED_ROM;break;}
            rom[(h>>4)*8+i*2]=efuse(&s,a++);
            rom[(h>>4)*8+i*2+1]=efuse(&s,a++);
        }
    }
    /* Retain decoded bytes even on validation failure for field diagnostics.
     * rtl8xxxu rtl8192cu_parse_efuse validates 0x8129, NOT the chip name 8192.
     * Decode explicitly so this check does not depend on host endianness. */
    memcpy(r->rom,rom,128);memcpy(r->mac,s.rom.macaddr,6);
    if(s.error)return r->result=s.error;
    if(rom[0]!=0x29||rom[1]!=0x81)return r->result=RTL_RADIO_BAD_ID;
    if((s.rom.macaddr[0]&1)||!memcmp(s.rom.macaddr,"\0\0\0\0\0\0",6))
        return r->result=RTL_RADIO_BAD_MAC;
    for(i=0;i<2;i++)for(j=0;j<3;j++)
        if(s.rom.cck_tx_pwr[i][j]>63||s.rom.ht40_1s_tx_pwr[i][j]>63)
            return r->result=RTL_RADIO_BAD_POWER;
    s.board_type=s.rom.rf_opt1>>5;s.regulatory=s.rom.rf_opt1&7;
    if(s.board_type>4||s.regulatory>3)return r->result=RTL_BOOT_UNSUPPORTED;
    s.pa_setting=efuse(&s,0x1fa);if(s.error)return r->result=s.error;
    memcpy(r->rom,&s.rom,128);memcpy(r->mac,s.rom.macaddr,6);
    r->pa_setting=s.pa_setting;r->board=s.board_type;
    r->stage=RTL_RADIO_MAC;
    for(i=0;i<__arraycount(rtl8192cu_mac);i++)wr(&s,rtl8192cu_mac[i].reg,1,rtl8192cu_mac[i].val);
    for(i=0;i<6;i++)wr(&s,R92C_MACID+i,1,r->mac[i]);
    wr(&s,R92C_RX_DRVINFO_SZ,1,4);
    mod(&s,R92C_CR,4,R92C_CR_NETTYPE_M,SM(R92C_CR_NETTYPE,R92C_CR_NETTYPE_INFRA));
    urtwn_rxfilter_init(&s);urtwn_edca_init(&s);
    mod(&s,R92C_RRSR,4,R92C_RRSR_RATE_BITMAP_M,1);
    wr(&s,R92C_RL,2,0x3030);wr(&s,R92C_ACKTO,1,0x40);
    /* Software CCMP and sequence counters; do not let hardware decrypt twice. */
    wr(&s,R92C_SECCFG,1,0);wr(&s,R92C_HWSEQ_CTRL,1,0);
    mod(&s,R92C_TRXDMA_CTRL,1,R92C_TRXDMA_CTRL_RXDMA_AGG_EN,0);
    /* USB aggregation has its own switch; disabling RX DMA aggregation
     * alone does not clear a setting inherited from a previous host. */
    mod(&s,R92C_USB_SPECIAL_OPTION,1,R92C_USB_SPECIAL_OPTION_AGG_EN,0);
    if(s.error)return r->result=s.error;
    r->stage=RTL_RADIO_BB;urtwn_bb_init(&s);
    if(s.error)return r->result=s.error;
    r->stage=RTL_RADIO_RF;urtwn_rf_init(&s);
    if(s.error)return r->result=s.error;
    memcpy(r->rf_channel,s.rf_chnlbw,sizeof(r->rf_channel));
    mod(&s,R92C_FPGA0_RFMOD,4,0,R92C_RFMOD_CCK_EN|R92C_RFMOD_OFDM_EN);
    r->stage=RTL_RADIO_CALIBRATE;urtwn_lc_calib(&s);urtwn_pa_bias_init(&s);
    wr(&s,0xfe40,1,0xe0);wr(&s,0xfe41,1,0x8d);wr(&s,0xfe42,1,0x80);
    wr(&s,0x20c,4,0xfd0320);
    if(s.error)return r->result=s.error;
    return rtl_radio_channel(io,delay,ctx,r,1);
}
