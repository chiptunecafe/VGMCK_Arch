/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event;
  int type;
  long value;
} Event1;

static signed int*opt;
static int vol[4]={-1,-1,-1,-1};
static int pan[4]={0,0,0,0};
static long tone[4]={0,0,0,0};
static int noteon[4]={0,0,0,0};
static int noise=-1;

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
  info->clock_div=-opt['H'];
}

static void x_file_begin(void) {
  return;
}

static void x_file_end(void) {
  long k=ftell(outfp);
  fseek(outfp,0x0C,SEEK_SET);
  fputc(opt['H']&255,outfp); fputc((opt['H']>>8)&255,outfp);
  fputc((opt['H']>>16)&255,outfp); fputc(0xC0|(opt['H']>>24)&255,outfp);
  fseek(outfp,0x28,SEEK_SET);
  fputc(opt['F'],outfp);
  fseek(outfp,0x2A,SEEK_SET);
  fputc(opt['S'],outfp);
  fputc(opt['f']|(opt['n']<<1)|((!opt['s'])<<2)|((!opt['d'])<<3),outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_start_channel(ChipDef*info,int chan) {
  return;
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int a=channel[e->chan].chip_sub;
  int b=channel[e->chan].chan_sub;
  int c=a?3:b&3;
  long v=e->value;
  int x;
  switch(e->type) {
    case 0: // direct write
      fputc(a?0x30:0x50,outfp);
      fputc(v,outfp);
      break;
    case 1: // stereo
      pan[c]=v;
      if(noteon[c]) goto volset;
      break;
    case 2: // volume
      vol[c]=v;
      if(noteon[c]) goto volset;
      break;
    case 3: // note on/change
      if(tone[c]!=v) {
        tone[c]=v;
        fputc(a?0x30:0x50,outfp);
        fputc((tone[c]&0x0F)|0x80|(a?0xC0:(c<<5)),outfp);
        fputc(a?0x30:0x50,outfp);
        fputc(tone[c]>>4,outfp);
      }
      if(noteon[c]) break;
      noteon[c]=1;
      volset:
      fputc(0x50,outfp);
      x=vol[c]-(pan[c]>0?pan[c]:0);
      fputc(0x9F^(x<0?0:x)|(c<<5),outfp); // left
      fputc(0x30,outfp);
      x=vol[c]+(pan[c]<0?pan[c]:0);
      fputc(0x9F^(x<0?0:x)|(c<<5),outfp); // right
      break;
    case 4: // note off
      if(noteon[c]) {
        noteon[c]=0;
        fputc(0x50,outfp);
        fputc(0x9F|(c<<5),outfp); // left
        fputc(0x30,outfp);
        fputc(0x9F|(c<<5),outfp); // right
      }
      break;
    case 5: // tone
      if(a && noise!=v) {
        noise=v;
        fputc(0x30,outfp);
        fputc(0xE3|(v<<2),outfp);
      }
      break;
  }
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e=malloc(sizeof(Event1));
  e->value=value;
  switch(command) {
    case MC_Tone:
      e->type=5;
      return (Event*)e;
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
  .name="NGP",
  .chip_id=CH_SN76489,
  .note_bits=10,
  .options={ ['F']=9,['H']=3072000,['S']=16,['d']=1,['f']=0,['n']=0,['s']=1 }
)
