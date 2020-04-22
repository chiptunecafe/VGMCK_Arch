/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event;
  unsigned short a;
  unsigned short d;
} Event1;

static char nor=0;
static char sup=0;
static char dual=0;
static unsigned char assign[12]={0,1,4,5,8,9,12,13,14,10,6,2};
static short*mem;
static unsigned char vol[12]={[0 ... 11]=127};
static unsigned char pan[12]={[0 ... 11]=0xC0};
static signed short**oper;
static signed int*opt;

static void opn2_put(int address,int data) {
  if((mem[address]!=data || ((address&0xA0)==0xA0)) && (dual || !(address&0x200))) {
    mem[address]=data;
    fputc((address&0x200?0xA2:0x52)|((address&0x100)>>8),outfp);
    fputc(address&0xFF,outfp);
    fputc(data&0xFF,outfp);
  }
}

static void update_oper(int mo,int ch) {
  int i,j,k;
  int ad=((assign[ch]&12)<<5)|(assign[ch]&3);
  signed short*d=oper[ch];
  int aff[4]={0,0,0,16};
  int alg=d[28]&7;
  if(alg>3) aff[2]=16;
  if(alg>4) aff[1]=16;
  if(alg==7) aff[0]=16;
  for(i=0;i<4;i++) {
    if(mo) aff[i]=d[i*3+32];
    for(j=0;j<7;j++) {
      k=d[i*7+j];
      if(j==1) {
        k+=(vol[ch]*aff[i])>>4;
        if(k<0) k=0;
        if(k>127) k=127;
      }
      opn2_put(ad|(i<<2)|((j+3)<<4),k);
    }
  }
  opn2_put(ad|0xB0,d[28]);
  opn2_put(ad|0xB4,d[29]|pan[ch]);
}

static void update_note(int mo,int ch,int note) {
  int ad=((assign[ch]&12)<<5)|(assign[ch]&3);
  int h;
  if(mo) {
    signed short*d=oper[ch];
    int i;
    for(i=0;i<4;i++) {
      h=(d[i*3+31]?:note)|(d[i*3+30]<<11);
      opn2_put((ad|0xA4)+i,h>>8);
      opn2_put((ad|0xA0)+i,h&255);
      ad|=4;
    }
  } else {
    opn2_put(ad|0xA4,note>>8);
    opn2_put(ad|0xA0,note&255);
  }
}

static void x_chip_enable(ChipDef*info) {
  memset(mem=malloc(0x400*sizeof(short)),255,0x400*sizeof(short));
  oper=malloc(12*sizeof(signed short*));
  opt=info->options;
  info->clock_div=opt['H'];
}

#define ASSIGN(x) assign[i++]=x
static void x_file_begin(void) {
  int i=0;
  dual=(sup>2 || nor>6-sup);
  ASSIGN(0); ASSIGN(1); if(sup<1) ASSIGN(2);
  ASSIGN(4); ASSIGN(5); if(sup<2) ASSIGN(6);
  ASSIGN(8); ASSIGN(9); if(sup<3) ASSIGN(10);
  ASSIGN(12); ASSIGN(13); if(sup<4) ASSIGN(14);
  for(i=0;i<1;i++) {
    opn2_put((i<<9)|0x27,(i<sup)<<6);
    opn2_put((i<<9)|0x2B,0x00);
  }
  for(i=0;i<12;i++) oper[i]=macro_env[MC_Option][i].data;
}

static void x_file_end(void) {
  long k=ftell(outfp);
  long clock=opt['H'];
  fseek(outfp,0x2C,SEEK_SET);
  fputc(clock&255,outfp); fputc((clock>>8)&255,outfp);
  fputc((clock>>16)&255,outfp); fputc((dual<<6)|(clock>>24)&255,outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int cs=channel[e->chan].chan_sub;
  int mo=channel[e->chan].chip_sub;
  int ch=mo?12-cs:cs;
  switch(e->a>>12) {
    case 0: // write
      opn2_put(e->a&0x3FF,e->d);
      break;
    case 1: // write global
      opn2_put(e->a&0x3FF,e->d);
      opn2_put((e->a&0x3FF)|0x100,e->d);
      opn2_put((e->a&0x3FF)|0x200,e->d);
      opn2_put((e->a&0x3FF)|0x300,e->d);
      break;
    case 2: // note off
      opn2_put(((assign[ch]&8)<<5)|0x28,assign[ch]&7);
      break;
    case 3: // note on
      update_note(mo,ch,e->d);
      opn2_put(((assign[ch]&8)<<5)|0x28,0xF0|assign[ch]&0xF7);
      break;
    case 4: // note change
      update_note(mo,ch,e->d);
      break;
    case 5: // set operators
      oper[ch]=macro_env[MC_Option][e->d].data;
      update_oper(mo,ch);
      break;
    case 6: // set volume
      vol[ch]=e->d;
      update_oper(mo,ch);
      break;
    case 7: // set panning
      pan[ch]=e->d;
      update_oper(mo,ch);
      break;
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  int x=channel[chan].chip_sub;
  int y=channel[chan].chan_sub+1;
  if(x) {
    if(y>sup) sup=y;
  } else {
    if(y>nor) nor=y;
  }
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  switch(command) {
    case MC_Volume:
      e=malloc(sizeof(Event1));
      e->a=0x6000;
      e->d=value^127;
      return (Event*)e;
    case MC_Panning:
      e=malloc(sizeof(Event1));
      e->a=0x7000;
      e->d=value<0?0x80:(value>0?0x40:0xC0);
      return (Event*)e;
    case MC_Tone:
      e=malloc(sizeof(Event1));
      e->a=0x5000;
      e->d=value&255;
      return (Event*)e;
    case MC_Global:
      e=malloc(sizeof(Event1));
      e->a=0x1022;
      e->d=value;
      return (Event*)e;
    default:
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->a=0x3000;
  e->d=note|(oct<<11);
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->a=0x4000;
  e->d=note|(oct<<11);
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->a=0x2000;
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
  .name="OPN2",
  .chip_id=CH_YM2612,
  .basic_octave=7,
  .note_bits=-11,
  .options={ ['H']=7670454 }
)
