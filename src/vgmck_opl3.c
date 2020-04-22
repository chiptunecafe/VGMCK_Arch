/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

static const unsigned char chop[9]={0,1,2,8,9,10,16,17,18};
static const unsigned char fop[4]={0,3,8,11};
static const unsigned char inst[4]={0x5E,0x5F,0xAE,0xAF};

typedef struct Event1 {
  Event;
  unsigned short a; // bit8 and bit9 select port/instance, bit10 for specials
  unsigned short d;
} Event1;

static signed int*opt;
static unsigned char a2op[36]; // bit6=second port
static unsigned char a4op[12]; // bit7=second chip instance
static char use[3]={0,0,0}; // two-ops, four-ops, rhythm
static char dual=0;
static unsigned char drum[2];
static unsigned short sam[2];
static unsigned short tone=0xC000;

static inline void poke(int id,int addr,int data) {
  if((id&2) && !dual) return;
  fputc(inst[id&3],outfp);
  fputc(addr,outfp);
  fputc(data,outfp);
}

static void poke_chan(int ch,int addr,int data) {
  if((ch&15)==15) {
    poke(ch>>6,addr|6,data);
    poke(ch>>6,addr|7,data);
    poke(ch>>6,addr|8,data);
  } else {
    poke(ch>>6,addr|(ch&15),data);
  }
}

static void poke_oper(int ch,int op,int addr,int data) {
  if((ch&15)==15) {
    poke(ch>>6,op+addr+16,data);
  } else {
    poke(ch>>6,chop[ch&15]+fop[op&3]+addr,data);
  }
}

static void instrument(int sub,int ch,int patch,int data) {
  signed short*mac=macro_env[MC_Option][data&255].data;
  int alg,fb,vol,op,pan,x;
  op=(sub+1)<<1;
  fb=mac[op*5];
  alg=(fb>>4)&3;
  fb&=7;
  vol=(data>>8)&0x3F;
  pan=(data>>10)&0x30;
  while(op--) {
    if(patch) {
      poke_oper(ch,op,0x20,mac[op*5]);
      if(mac[op*5+4]&0x10) poke_oper(ch,op,0x40,mac[op*5+1]);
      poke_oper(ch,op,0x60,mac[op*5+2]);
      poke_oper(ch,op,0x80,mac[op*5+3]);
      poke_oper(ch,op,0xE0,mac[op*5+4]&0x07);
    }
    if(!(mac[op*5+4]&0x10)) {
      x=(mac[op*5+1]&0x3F)+vol;
      if(x>63) x=63;
      poke_oper(ch,op,0x40,x|(mac[op*5+1]&0xC0));
    }
  }
  if(sub==1) poke_chan(ch+3,0xC0,alg>>1);
  x=fb<<1;
  x|=alg&1;
  poke_chan(ch,0xC0,x|pan);
}

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
  info->clock_div=opt['H']/9;
}

#define ASSIGN2(x) a2op[a2++]=x
#define ASSIGN4(x) a4op[a4++]=x
static void x_file_begin(void) {
  int a2=0;
  int a4=0;
  int x;
  // Assignment of operators and dual chips
  dual=use[2]>>1;
  ASSIGN2(0x46),ASSIGN2(0x47),ASSIGN2(0x48);
  if(use[2]<1) ASSIGN2(0x06),ASSIGN2(0x07),ASSIGN2(0x08);
  if(use[1]<1) ASSIGN2(0x00),ASSIGN2(0x03); else ASSIGN4(0x00);
  if(use[1]<2) ASSIGN2(0x01),ASSIGN2(0x04); else ASSIGN4(0x01);
  if(use[1]<3) ASSIGN2(0x02),ASSIGN2(0x05); else ASSIGN4(0x02);
  if(use[1]<4) ASSIGN2(0x40),ASSIGN2(0x43); else ASSIGN4(0x40);
  if(use[1]<5) ASSIGN2(0x41),ASSIGN2(0x44); else ASSIGN4(0x41);
  if(use[1]<6) ASSIGN2(0x42),ASSIGN2(0x45); else ASSIGN4(0x42);
  if(use[1]<7) ASSIGN2(0x80),ASSIGN2(0x83); else ASSIGN4(0x80);
  if(use[1]<8) ASSIGN2(0x81),ASSIGN2(0x84); else ASSIGN4(0x81);
  if(use[1]<9) ASSIGN2(0x82),ASSIGN2(0x85); else ASSIGN4(0x82);
  if(use[1]<10) ASSIGN2(0xC0),ASSIGN2(0xC3); else ASSIGN4(0xC0);
  if(use[1]<11) ASSIGN2(0xC1),ASSIGN2(0xC4); else ASSIGN4(0xC1);
  if(use[1]<12) ASSIGN2(0xC2),ASSIGN2(0xC5); else ASSIGN4(0xC2);
  ASSIGN2(0xC6),ASSIGN2(0xC7),ASSIGN2(0xC8);
  if(use[2]<2) ASSIGN2(0x86),ASSIGN2(0x87),ASSIGN2(0x88);
  for(x=0;x<use[0];x++) if(a2op[x]&0x80) dual=1;
  if(use[1]>6) dual=1;
  // Initialize
  poke(0,0x01,0x20);
  poke(1,0x05,0x01);
  poke(2,0x01,0x20);
  poke(3,0x05,0x01);
  poke(1,0x04,((1<<use[1])-1)&0x3F);
  poke(3,0x04,use[1]>6?(1<<(use[1]-6))-1:0);
}
#undef ASSIGN2
#undef ASSIGN4

static void x_file_end(void) {
  long k=ftell(outfp);
  fseek(outfp,0x5C,SEEK_SET);
  fputc(opt['H']&255,outfp); fputc((opt['H']>>8)&255,outfp);
  fputc((opt['H']>>16)&255,outfp); fputc(((!!dual)<<6)|(opt['H']>>24)&255,outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int a=channel[e->chan].chip_sub;
  int b=channel[e->chan].chan_sub;
  int c=a==2?15|(b<<7):(a?a4op:a2op)[b];
  if(e->a&0x400) {
    switch(e->a&7) {
      case 0: // note on/off/change
        poke_chan(c,0xA0,e->d&255);
        poke_chan(c,0xB0,e->d>>8);
        break;
      case 1: // rhythm on
        if(sam[b]>>5) e->d=sam[b]>>5;
        poke_chan(c,0xA0,e->d&255);
        poke_chan(c,0xB0,e->d>>8);
        poke(b<<1,0xBD,drum[b]=(sam[b]&0x1F)|0x20|(drum[b]&0xC0));
        break;
      case 2: // rhythm off
        poke(b<<1,0xBD,drum[b]&=0xE0);
        break;
      case 3: // volume/panning
        instrument(a,c,0,e->d);
        break;
      case 4: // rhythm set
        sam[b]=e->d;
        break;
      case 5: // instrument set
        instrument(a,c,1,e->d);
        break;
      case 6: // global
        drum[0]&=0x3F;
        poke(0,0xBD,drum[0]|=(e->d&3)<<6);
        drum[1]&=0x3F;
        poke(2,0xBD,drum[1]|=(e->d&3)<<6);
        poke(0,0x08,(e->d&12)<<4);
        poke(2,0x08,(e->d&12)<<4);
        break;
    }
  } else {
    poke(e->a>>8,e->a&255,e->d);
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  int a=channel[chan].chip_sub;
  int b=channel[chan].chan_sub+1;
  if(use[a]<b) use[a]=b;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  switch(command) {
    case MC_Volume:
      e=malloc(sizeof(Event1));
      e->a=0x403;
      e->d=tone=(tone&~0x3F00)|((63&~value)<<8);
      return (Event*)e;
    case MC_Panning:
      e=malloc(sizeof(Event1));
      e->a=0x403;
      value=value<0?0x4000:(value>0?0x8000:0xC000);
      e->d=tone=(tone&~0xC000)|value;
      return (Event*)e;
    case MC_Tone:
      e=malloc(sizeof(Event1));
      e->a=0x405;
      e->d=tone=(tone&~0xFF)|(value&255);
      return (Event*)e;
    case MC_Global:
      e=malloc(sizeof(Event1));
      e->a=0x406;
      e->d=value;
      return (Event*)e;
    case MC_Sample:
      e=malloc(sizeof(Event1));
      e->a=0x404;
      e->d=value;
      return (Event*)e;
    default:
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  int a=channel[chan].chip_sub;
  Event1*e=malloc(sizeof(Event1));
  e->a=0x400|(a==2);
  e->d=note|(oct<<10)|((a!=2)<<13);
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  int a=channel[chan].chip_sub;
  Event1*e=malloc(sizeof(Event1));
  e->a=0x400|(a==2);
  e->d=note|(oct<<10)|((a!=2)<<13);
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  int a=channel[chan].chip_sub;
  Event1*e=malloc(sizeof(Event1));
  e->a=0x400|((a==2)<<1);
  e->d=note|(oct<<10);
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
  .name="OPL3",
  .chip_id=CH_YMF262,
  .note_bits=-10,
  .basic_octave=0,
  .options={ ['H']=14318180 }
)
