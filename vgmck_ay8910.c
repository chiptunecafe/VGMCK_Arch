/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event;
  unsigned char a;
  unsigned char v;
  unsigned short d;
  unsigned short m;
} Event1;

static unsigned char ena[2];
static unsigned char vol;
static char dual=0;
static char spec=0;
static signed int*opt;
static signed int mul;

static inline void poke(int address,unsigned char data) {
  fputc(0xA0,outfp);
  fputc(address,outfp);
  fputc(data,outfp);
}

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
  info->clock_div=-opt['H'];
}

static void x_file_begin(void) {
  dual=(dual>2-spec);
}

static void x_file_end(void) {
  long k=ftell(outfp);
  long clock=opt['H'];
  fseek(outfp,0x74,SEEK_SET);
  fputc(clock&255,outfp); fputc((clock>>8)&255,outfp);
  fputc((clock>>16)&255,outfp); fputc(((!!dual)<<6)|(clock>>24)&255,outfp);
  fputc(opt['T'],outfp);
  fputc(opt['l']|(opt['s']<<1)|(opt['d']<<2)|(opt['r']<<3),outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int a=channel[e->chan].chip_sub;
  int b=channel[e->chan].chan_sub;
  int c=(a&b)|(b>2-spec);
  int d=a?2:b%(3-spec);
  switch(e->a) {
    case 0x20: // key off/on
      if(a) {
        poke(11|(c<<7),e->m&255);
        poke(12|(c<<7),e->m>>8);
      }
      poke(d|(c<<7)|8,e->v);
      poke((d<<1)|(c<<7),e->d&255);
      poke((d<<1)|(c<<7)|1,e->d>>8);
      break;
    case 0x21: // volume
      poke(d|(c<<7)|8,e->d);
      if(a && e->m) poke(13|(c<<7),e->m);
      break;
    case 0x22: // tone
      ena[c]&=~(9<<d);
      ena[c]|=((e->d&1)|((e->d&2)<<2))<<d;
      poke(7|(c<<7),ena[c]);
      if(a) poke(13|(c<<7),(e->d>>2)|8);
      break;
    default:
      poke(e->a^(c<<7),e->d);
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub;
  mul=0;
  vol=15;
  spec|=a;
  if(dual<b) dual=b;
  if(a && b) dual=6;
  info->note_bits=a?16:12;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  switch(command) {
    case MC_Volume:
      if(dyn && vol==value) return 0;
      e=malloc(sizeof(Event1));
      e->a=0x21;
      e->d=vol=value&15;
      e->m=0;
      return (Event*)e;
    case MC_Tone:
      e=malloc(sizeof(Event1));
      e->a=0x22;
      e->d=value;
      return (Event*)e;
    case MC_Multiply:
      vol=0x1F;
      mul=value;
      return 0;
    case MC_VolumeEnv:
      e=malloc(sizeof(Event1));
      e->a=0x21;
      e->d=vol=0x1F;
      e->m=value>0?13:9;
      mul=value*(value>0?-1:1);
      return (Event*)e;
    case MC_Sample:
      e=malloc(sizeof(Event1));
      e->a=0x06;
      e->d=value;
      return (Event*)e;
    default:
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->a=0x20;
  e->v=vol;
  e->d=note;
  if(channel[chan].chip_sub) {
    e->d>>=4;
    if(opt['S']<0) e->d>>=-opt['S'];
    if(mul>0) {
      e->m=(note*mul)>>6;
      if(opt['S']>0) e->m>>=opt['S'];
    } else {
      e->m=-mul;
    }
  }
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  return x_note_on(chan,note,oct,1);
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->a=0x20;
  e->v=0;
  e->d=0;
  e->m=0;
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  return x_note_off(chan,0,0);
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->a=address;
  e->d=value;
  return (Event*)e;
}

Auto_Constructor(
  .name="GI-AY",
  .chip_id=CH_AY8910,
  .basic_octave=1,
  .options={ ['H']=1789750, ['S']=1, ['l']=1 }
)
