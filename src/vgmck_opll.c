/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event;
  unsigned char a;
  unsigned char m;
  unsigned char d;
  unsigned char s;
} Event1;

static signed int*opt;
static char dual=0;
static char drum=0;
static unsigned char sus=0;
static short mem[2][64]={[0 ... 1]={[0 ... 63]=256}};

static void opll_put(int chip,int address,int mask,int data) {
  if(address&0x80) chip=address&0x40;
  address&=0x3F;
  data|=mem[chip][address]&mask;
  if(mem[chip][address]==data) return;
  fputc(chip?0xA1:0x51,outfp);
  fputc(address,outfp);
  fputc(data,outfp);
  mem[chip][address]=data;
}

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
}

static void x_file_begin(void) {
  if(drum && dual>=6) dual=6;
  else if(dual>=9) dual=9;
  else dual=127;
  if(!drum) {
    opll_put(0,0x0E,0x00,0x00);
    if(dual!=127) opll_put(1,0x0E,0x00,0x00);
  }
}

static void x_file_end(void) {
  long k=ftell(outfp);
  long clock=opt['H'];
  fseek(outfp,0x10,SEEK_SET);
  fputc(clock&255,outfp); fputc((clock>>8)&255,outfp);
  fputc((clock>>16)&255,outfp); fputc(((dual!=127)<<6)|(clock>>24)&255,outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int b=channel[e->chan].chip_sub;
  int c=(b&channel[e->chan].chan_sub)||channel[e->chan].chan_sub>=dual;
  int d=channel[e->chan].chan_sub%dual;
  int x;
  switch(e->a) {
    case 0xF0 ... 0xF7: // command selecting a register of this channel
      x=(e->a&7)<<4;
      if(b) {
        opll_put(c,x|6,e->m,e->d);
        opll_put(c,x|7,e->m,e->d);
        opll_put(c,x|8,e->m,e->d);
      } else {
        opll_put(c,x|d,e->m,e->d);
      }
      break;
    case 0xFD: // custom instrument select
      for(x=0;x<8;x++) opll_put(c,x,0,macro_env[MC_Option][e->d].data[x]);
      opll_put(c,0x30|d,0x0F,0x00);
      break;
    case 0xFE: // rhythm note on
      opll_put(c,0x16,0,e->m);
      opll_put(c,0x17,0,e->m);
      opll_put(c,0x18,0,e->m);
      opll_put(c,0x26,0,e->d);
      opll_put(c,0x27,0,e->d);
      opll_put(c,0x28,0,e->d);
      opll_put(c,0x0E,0x20,e->s);
      break;
    case 0xFF: // melody note on
      opll_put(c,0x10|d,0,e->m);
      opll_put(c,0x20|d,0,e->d);
      break;
    default: // direct register write
      opll_put(c,e->a,e->m,e->d);
      break;
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  info->clock_div=(opt['H']/9)*!channel[chan].chip_sub;
  sus=channel[chan].chip_sub<<5;
  drum|=channel[chan].chip_sub;
  if(dual<channel[chan].chan_sub) dual=channel[chan].chan_sub;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  switch(command) {
    case MC_Volume:
      e=malloc(sizeof(Event1));
      e->a=0xF3;
      e->m=0xF0;
      e->d=0x0F&~value;
      return (Event*)e;
    case MC_Tone:
    case MC_Sample:
      e=malloc(sizeof(Event1));
      if(channel[chan].chip_sub) {
        sus=value;
      } else if(value&~0x1F) {
        sus=0;
      } else {
        e->a=0xF3;
        e->m=0x0F;
        e->d=(value&15)<<4;
        sus=value&0x10;
      }
      return (Event*)e;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  if(sus&~0x1F) note=sus>>5;
  e->a=~channel[chan].chip_sub;
  e->m=note&255;
  e->d=(note>>8)|(oct<<1)|(0x10*!channel[chan].chip_sub)|(sus*channel[chan].chip_sub);
  e->s=sus&0x1F;
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  if(sus&~0x1F) note=sus>>5;
  e->a=~channel[chan].chip_sub;
  e->m=note&255;
  e->d=(note>>8)|(oct<<1)|(0x10*!channel[chan].chip_sub)|(sus*channel[chan].chip_sub);
  e->s=sus&0x1F;
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  if(channel[chan].chip_sub) {
    e->a=0x0E;
    e->m=0x00;
    e->d=0x20;
  } else {
    e->a=0xF2;
    e->m=0xEF;
    e->d=0x00;
  }
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  Event1*e=malloc(sizeof(Event1));
  if(channel[chan].chip_sub) {
    e->a=0x0E;
    e->m=0x00;
    e->d=0x20;
  } else {
    e->a=0xF2;
    e->m=0xEF;
    e->d=0x00;
  }
  return (Event*)e;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->a=address;
  e->m=0x00;
  e->d=value;
  return (Event*)e;
}

Auto_Constructor(
  .name="OPLL",
  .chip_id=CH_YM2413,
  .note_bits=-9,
  .basic_octave=7,
  .options={ ['H']=3579545 }
)
