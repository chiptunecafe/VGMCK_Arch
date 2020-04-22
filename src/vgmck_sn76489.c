/*
  VGMCK (Video Game Music Compiler Kit) version 1.1
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event;
  int type;
  long value;
} Event1;

static signed int*opt;
static int stereo[2]={0xFF,0xFF};
static int dual=0;
static int vol[2][4];
static long tone[2][4];
static int noteon[2][4];
static int ltone[2]={-1,-1};

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
  info->clock_div=-opt['H'];
}

static void x_file_begin(void) {
  int i,j;
  for(i=0;i<2;i++) for(j=0;j<4;j++) vol[i][j]=tone[i][j]=-1;
}

static void x_file_end(void) {
  long k=ftell(outfp);
  fseek(outfp,0x0C,SEEK_SET);
  fputc(opt['H']&255,outfp); fputc((opt['H']>>8)&255,outfp);
  fputc((opt['H']>>16)&255,outfp); fputc((dual<<6)|(opt['H']>>24)&255,outfp);
  fseek(outfp,0x28,SEEK_SET);
  fputc(opt['F'],outfp);
  fseek(outfp,0x2A,SEEK_SET);
  fputc(opt['S'],outfp);
  fputc(opt['f']|(opt['n']<<1)|((!opt['s'])<<2)|((!opt['d'])<<3),outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_start_channel(ChipDef*info,int chan) {
  info->clock_div=-opt['H']*!channel[chan].chip_sub;
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int a=channel[e->chan].chip_sub;
  int b=channel[e->chan].chan_sub;
  int c=b>=(a?1:3);
  int d=a?3:(b%3);
  int x;
  dual|=c;
  if(a>1 || b>5) return;
  switch(e->type) {
    case 0: // direct write
      fputc(c?0x30:0x50,outfp);
      fputc(e->value,outfp);
      ltone[c]=-1;
      break;
    case 1: // stereo
      x=stereo[c];
      x&=~(0x11<<d);
      if(e->value<=0) x|=0x10<<d;
      if(e->value>=0) x|=0x01<<d;
      if(x!=stereo[c]) {
        fputc(c?0x3F:0x4F,outfp);
        fputc(e->value,outfp);
        stereo[c]=x;
        ltone[c]=-1;
      }
      break;
    case 2: // volume
      x=e->value;
      if(noteon[c][d] && x!=vol[c][d]) {
        fputc(c?0x30:0x50,outfp);
        fputc(0x9F^x|(d<<5),outfp);
        ltone[c]=-1;
      }
      vol[c][d]=x;
      break;
    case 3: // note on/change
      if(vol[c][d] && !noteon[c][d]) {
        fputc(c?0x30:0x50,outfp);
        fputc(0x9F^vol[c][d]|(d<<5),outfp);
        ltone[c]=-1;
      }
      noteon[c][d]=1;
      if(((e->value^tone[c][d])&15) || (e->value!=tone[c][d] && ltone[c]!=d)) {
        fputc(c?0x30:0x50,outfp);
        fputc(0x80|(e->value&15)|(d<<5),outfp);
        ltone[c]=d;
      }
      if(ltone[c]==d && !a) {
        fputc(c?0x30:0x50,outfp);
        fputc(e->value>>4,outfp);
      }
      tone[c][d]=e->value;
      break;
    case 4: // note off
      if(noteon[c][d] && vol[c][d]) {
        fputc(c?0x30:0x50,outfp);
        fputc(0x9F|(d<<5),outfp);
        ltone[c]=-1;
      }
      noteon[c][d]=0;
      break;
  }
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e=malloc(sizeof(Event1));
  e->value=value;
  switch(command) {
    case MC_Volume:
      e->type=2;
      return (Event*)e;
    case MC_Panning:
      e->type=1;
      return (Event*)e;
    default:
      free(e);
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->type=3;
  e->value=note;
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->type=3;
  e->value=note;
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->type=4;
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->type=4;
  return (Event*)e;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->type=0;
  e->value=address;
  return (Event*)e;
}

Auto_Constructor(
  .name="PSG",
  .chip_id=CH_SN76489,
  .note_bits=10,
  .options={ ['F']=9,['H']=3579545,['S']=16,['d']=1,['f']=0,['n']=0,['s']=1 }
)
