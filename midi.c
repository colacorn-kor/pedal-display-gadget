/* ============================================================================
 *  midi.c  —  MIDI 1.0 바이트 스트림 파서 (31250 baud UART)
 *  러닝 스테이터스 + 리얼타임 바이트 인터리브 처리. PC/CC/Note/Clock/SPP.
 * ========================================================================== */
#include "midi.h"

static uint8_t s_status=0, s_d1=0;
static int     s_need=0, s_have=0;

void midi_reset(void){ s_status=0; s_d1=0; s_need=0; s_have=0; }

static int data_len(uint8_t st){
    uint8_t hi=st&0xF0;
    if(hi==0xC0||hi==0xD0) return 1;                 /* PC, ChanPressure */
    if(hi==0x80||hi==0x90||hi==0xA0||hi==0xB0||hi==0xE0) return 2;
    return 0;
}

void midi_feed(uint8_t b){
    if(b>=0xF8){                                      /* 리얼타임(러닝상태 불변) */
        midi_msg_t m={0};
        if(b==0xF8)m.type=MIDI_CLOCK; else if(b==0xFA)m.type=MIDI_START;
        else if(b==0xFB)m.type=MIDI_CONTINUE; else if(b==0xFC)m.type=MIDI_STOP;
        else return;
        midi_on_message(&m); return;
    }
    if(b&0x80){                                       /* 스테이터스 바이트 */
        if(b==0xF2){ s_status=0xF2; s_need=2; s_have=0; return; }  /* SPP */
        if(b>=0xF0){ s_status=0; s_need=0; return; }              /* 기타 시스템공통 무시 */
        s_status=b; s_need=data_len(b); s_have=0; return;
    }
    /* 데이터 바이트 */
    if(s_status==0xF2){                               /* SPP: 14-bit */
        if(s_have==0){ s_d1=b; s_have=1; }
        else { midi_msg_t m={.type=MIDI_SPP,.pos14=(uint16_t)((b<<7)|s_d1)};
               midi_on_message(&m);
               /* System Common messages never establish running status. */
               s_status=0; s_need=0; s_have=0; }
        return;
    }
    if(!s_status) return;
    if(s_need==1){                                    /* PC */
        if((s_status&0xF0)==0xC0){
            midi_msg_t m={.type=MIDI_PC,.ch=(uint8_t)(s_status&0x0F),.d1=b};
            midi_on_message(&m);
        }
        return;                                       /* 러닝 스테이터스 유지 */
    }
    if(s_have==0){ s_d1=b; s_have=1; return; }        /* 첫 데이터 */
    midi_msg_t m={.ch=(uint8_t)(s_status&0x0F),.d1=s_d1,.d2=b};
    uint8_t hi=s_status&0xF0;
    if(hi==0x90) m.type=(b==0)?MIDI_NOTE_OFF:MIDI_NOTE_ON;   /* vel0 = off */
    else if(hi==0x80) m.type=MIDI_NOTE_OFF;
    else if(hi==0xB0) m.type=MIDI_CC;
    else m.type=MIDI_NONE;
    if(m.type!=MIDI_NONE) midi_on_message(&m);
    s_have=0;                                          /* 러닝 스테이터스 유지 */
}
