/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event;
  unsigned char a;
  unsigned short d;
  unsigned char m;
} Event1;

static signed short mul;
static signed int*opt;
static unsigned char audc;
static unsigned char audctl=0;
static char stat[3][4];
static char ass[4]={0,2,4,6};

static inline void poke(int address,int data) {
  fputc(0xBB,outfp);
  fputc(address,outfp);
  fputc(data,outfp);
}

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
  audctl=opt['c']|(opt['p']<<7);
}

static void x_file_begin(void) {
  poke(0,0x00);
  poke(1,0xF0);
  poke(2,0x00);
  poke(3,0xF0);
  poke(4,0x00);
  poke(5,0xF0);
  poke(6,0x00);
  poke(7,0xF0);
  poke(8,audctl);
}

static void x_file_end(void) {
  long k=ftell(outfp);
  long clock=opt['H'];
  fseek(outfp,0xB0,SEEK_SET);
  fputc(clock&255,outfp); fputc((clock>>8)&255,outfp);
  fputc((clock>>16)&255,outfp); fputc((clock>>24)&255,outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int c=channel[e->chan].chip_sub;
  int d=channel[e->chan].chan_sub;
  int a=c?d<<(c^3):ass[d];
  switch(e->a) {
    case 0xFD: // volume/distortion setting
      stat[c][d]&=0x10;
      stat[c][d]|=e->d;
      if(!(stat[c][d]&0x10)) poke(a|1,stat[c][d]);
      break;
    case 0xFE: // key on
      poke(a,e->d&0xFF);
      if(c==1) poke(a|2,e->d>>8);
      if(c==2) poke(a|4,e->m);
      if(stat[c][d]&0x10) poke(a|1,stat[c][d]&=0xEF);
      break;
    case 0xFF: // key off
      if(stat[c][d]&0x10) break;
      stat[c][d]|=0x10;
      poke(a|1,0xF0);
      poke(a,0x00);
      if(c==1) poke(a|2,0x00);
      break;
    default:
      poke(e->a,e->d);
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  int c=channel[chan].chip_sub;
  int d=channel[chan].chan_sub;
  audc=0x00;
  stat[c][d]=0x10;
  if(c==1) {
    info->clock_div=-opt['H'];
    info->note_bits=16;
    ass[0]=4,ass[1]=6,audctl|=0x10>>d;
  } else {
    info->clock_div=-opt['H']/(opt['c']?114:28);
    info->note_bits=8;
    if(c==2) ass[0]=2,ass[1]=6,audctl|=0x04>>d;
  }
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  switch(command) {
    case MC_Volume:
      if((audc&0x0F)!=value) {
        e=malloc(sizeof(Event1));
        e->a=0xFD;
        e->d=audc=(audc&0xE0)|value;
        return (Event*)e;
      }
      return 0;
    case MC_Tone:
      if(audc>>5!=value) {
        e=malloc(sizeof(Event1));
        e->a=0xFD;
        e->d=audc=(audc&0x0F)|(value<<5);
        return (Event*)e;
      }
      return 0;
    case MC_Multiply:
      mul=value;
      return 0;
    default:
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->a=0xFE;
  e->d=note-(channel[chan].chip_sub==1?7:1);
  if(channel[chan].chip_sub==1) e->d=(e->d<<8)|(e->d>>8);
  if(channel[chan].chip_sub==2) {
    int x;
    if(mul>0) x=note*mul-1;
    else if(mul<0) x=(note/-mul)-1;
    else x=0x40;
    if(x<0) x=0;
    if(x>255) x=255;
    e->m=x;
  }
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->a=0xFE;
  e->d=note-(channel[chan].chip_sub==1?7:1);
  if(channel[chan].chip_sub==1) e->d=(e->d<<8)|(e->d>>8);
  if(channel[chan].chip_sub==2) {
    int x;
    if(mul>0) x=note*mul-1;
    else if(mul<0) x=(note/-mul)-1;
    else x=0x40;
    if(x<0) x=0;
    if(x>255) x=255;
    e->m=opt['x']?mul:x;
  }
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->a=0xFF;
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  return 0;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->a=address;
  e->d=value;
  return (Event*)e;
}

Auto_Constructor(
  .name="POKEY",
  .chip_id=CH_Pokey,
  .basic_octave=2,
  .options={ ['H']=1789772 }
)
