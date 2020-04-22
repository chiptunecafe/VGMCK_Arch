/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event;
  int address;
  int value;
  int period;
} Event1;

static unsigned char enable[2]={0,0};
static unsigned char dutyvol[2][2]={{0x30,0x30},{0x30,0x30}};
static int dual=0;
static long clock;

static void x_chip_enable(ChipDef*info) {
  clock=info->options['H'];
}

static void x_file_begin(void) {
  return;
}

static void x_file_end(void) {
  long k=ftell(outfp);
  fseek(outfp,0x84,SEEK_SET);
  fputc(clock&255,outfp); fputc((clock>>8)&255,outfp);
  fputc((clock>>16)&255,outfp); fputc(((!!dual)<<6)|(clock>>24)&255,outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int a=channel[e->chan].chip_sub;
  int b=channel[e->chan].chan_sub;
  int c=b>!a;
  int d=a+(b&!a)+!!a;
  dual|=c;
  if(e->address<0) {
    switch(~e->address) {
      case 0: // note off
        fputc(0xB4,outfp);
        fputc((c<<7)|0x15,outfp);
        fputc(enable[c]&=~e->value,outfp);
        break;
      case 1: // note on
        if(a==1) {
          fputc(0xB4,outfp);
          fputc((c<<7)|0x08,outfp);
          fputc(0xFF,outfp);
        }
        fputc(0xB4,outfp);
        fputc((c<<7)|0x15,outfp);
        fputc(enable[c]|=e->value,outfp);
        //fallthrough
      case 2: // note change
        fputc(0xB4,outfp);
        fputc((c<<7)|(d<<2)|2,outfp);
        fputc(e->period&255,outfp);
        fputc(0xB4,outfp);
        fputc((c<<7)|(d<<2)|3,outfp);
        fputc((e->period>>8)|0xF8,outfp);
        break;
      case 3: // duty/volume of square channels
        fputc(0xB4,outfp);
        fputc((c<<7)|((b&1)<<2),outfp);
        fputc(dutyvol[c][b&1]=((dutyvol[c][b&1]&e->value)|e->period),outfp);
        break;
    }
  } else {
    fputc(0xB4,outfp);
    fputc(e->address,outfp);
    fputc(e->value,outfp);
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  info->clock_div=channel[chan].chip_sub==2?0:-clock;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub;
  Event1*e;
  switch(command) {
    case MC_Volume:
      if(a==0) {
        e=malloc(sizeof(Event1));
        e->address=~3;
        e->value=0xF0;
        e->period=value;
        return (Event*)e;
      } else if(a==2) {
        e=malloc(sizeof(Event1));
        e->address=(b<<7)|0x0C;
        e->value=0x30|value;
        return (Event*)e;
      }
      return 0;
    case MC_Tone:
      if(!a) {
        e=malloc(sizeof(Event1));
        e->address=~3;
        e->value=0x3F;
        e->period=value<<6;
        return (Event*)e;
      }
      return 0;
    default:
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub;
  Event1*e=malloc(sizeof(Event1));
  e->address=~1;
  e->value=1<<(a+(b&!a)+!!a);
  if(a==2) note|=oct<<7; else note--;
  e->period=note;
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub;
  Event1*e=malloc(sizeof(Event1));
  e->address=~2;
  e->value=1<<(a+(b&!a)+!!a);
  if(a==2) note|=oct<<7; else note--;
  e->period=note;
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub;
  Event1*e=malloc(sizeof(Event1));
  e->address=~0;
  e->value=0x1F^(1<<(a+(b&!a)+!!a));
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  return 0;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->address=address;
  e->value=value;
  return (Event*)e;
}

Auto_Constructor(
  .name="FAMICOM",
  .chip_id=CH_NES_APU,
  .note_bits=11,
  .basic_octave=2,
  .options={ ['H']=1789772 }
)
