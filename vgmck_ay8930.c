/*
  VGMCK (Video Game Music Compiler Kit) version 0.8
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

static const unsigned char enh[6]={0x0B,0x10,0x12,0x8B,0x90,0x92};
static const unsigned char enm[6]={0x0D,0x14,0x15,0x8D,0x94,0x95};

typedef struct Event1 {
  Event;
  unsigned char a;
  unsigned char v;
  unsigned short d;
  unsigned short m;
} Event1;

static unsigned char ena[2];
static unsigned char enva[2];
static unsigned char bank[2];
static unsigned char vol;
static char dual=0;
static signed int*opt;
static signed int mul;

static inline void poke(int address,unsigned char data) {
  if((address&15)==13) {
    data|=(address&0x10)|0xA0;
    enva[address>>7]=data;
  } else if(bank[address>>7]!=(address&0x10)) {
    fputc(0xA0,outfp);
    fputc(0x0D|address&0x8D,outfp);
    fputc(enva[address>>7],outfp);
  }
  bank[address>>7]=(address&0x10);
  fputc(0xA0,outfp);
  fputc(address&0x8F,outfp);
  fputc(data,outfp);
}

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
  info->clock_div=-opt['H'];
}

static void x_file_begin(void) {
  poke(0x0D,0xA0);
  if(dual) poke(0x8D,0xA0);
}

static void x_file_end(void) {
  long k=ftell(outfp);
  long clock=opt['H'];
  fseek(outfp,0x74,SEEK_SET);
  fputc(clock&255,outfp); fputc((clock>>8)&255,outfp);
  fputc((clock>>16)&255,outfp); fputc((dual<<6)|(clock>>24)&255,outfp);
  fputc(3,outfp); // type 3 = AY8930
  fputc(opt['l']|(opt['s']<<1)|(opt['d']<<2)|(opt['r']<<3),outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int b=channel[e->chan].chan_sub;
  int c=b/3;
  int d=b%3;
  switch(e->a) {
    case 0x20: // key off/on
      poke(enh[b],e->m&255);
      poke(enh[b]+1,e->m>>8);
      poke(d|(c<<7)|8,e->v);
      poke((d<<1)|(c<<7),e->d&255);
      poke((d<<1)|(c<<7)|1,e->d>>8);
      break;
    case 0x21: // volume
      poke(d|(c<<7)|8,e->d);
      if(e->m) poke(enm[b],e->m);
      break;
    case 0x22: // tone
      ena[c]&=~(9<<d);
      ena[c]|=((e->d&1)|((e->d&2)<<2))<<d;
      poke(7|(c<<7),ena[c]);
      poke(enm[b],((e->d>>2)|8)&15);
      poke((c<<7)|0x16+d,(e->d>>5));
      break;
    default:
      poke(e->a,e->d);
      poke(e->a|0x80,e->d);
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  dual|=channel[chan].chan_sub/3;
  mul=0;
  vol=31;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  switch(command) {
    case MC_Volume:
      if(dyn && vol==value) return 0;
      e=malloc(sizeof(Event1));
      e->a=0x21;
      e->d=vol=value&31;
      e->m=0;
      return (Event*)e;
    case MC_Tone:
      e=malloc(sizeof(Event1));
      e->a=0x22;
      e->d=value;
      return (Event*)e;
    case MC_Multiply:
      vol=0x3F;
      mul=value;
      return 0;
    case MC_VolumeEnv:
      e=malloc(sizeof(Event1));
      e->a=0x21;
      e->d=vol=0x3F;
      e->m=value>0?13:9;
      mul=value*(value>0?-1:1);
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
  if(opt['S']<0) e->d>>=-opt['S'];
  if(mul>0) {
    e->m=(note*mul)>>6;
    if(opt['S']>0) e->m>>=opt['S'];
  } else {
    e->m=-mul;
  }
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  return x_note_on(0,note,0,0);
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
  .name="AY8930",
  .chip_id=CH_AY8910,
  .basic_octave=0,
  .note_bits=16,
  .options={ ['H']=1789750, ['l']=1 }
)
