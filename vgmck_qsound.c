/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event;
  int a;
  int d;
} Event1;

typedef struct Local_Sample {
  SampleLoader*s;
  signed short head[6]; // loop_start loop_end pitch_shift_bits pitch_clock fixed_pitch basic_octave
  long address;
} Local_Sample;

static signed int*opt;
static ChipDef*inf;
static int mru_sam=-1;
static Local_Sample*sam;
static long next_address=0;
static int vol[16];
static char key[16];
static unsigned char per[16];

static inline void qs_write(int a,int d) {
  fputc(0xC4,outfp);
  fputc((d>>8)&255,outfp);
  fputc(d&255,outfp);
  fputc(a,outfp);
}

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
  inf=info;
  sam=calloc(sizeof(Local_Sample),256);
}

static void x_file_begin(void) {
  char*buf;
  int x;
  long q;
  for(x=0;x<256;x++) {
    if(sam[x].s) {
      fputc(0x67,outfp);
      fputc(0x66,outfp);
      fputc(0x8F,outfp);
      q=sam[x].s->count+8;
      fputc((q>>0)&255,outfp); fputc((q>>8)&255,outfp); fputc((q>>16)&255,outfp); fputc((q>>24)&255,outfp);
      q=next_address; if(q&0xFFFF) q++;
      fputc((q>>0)&255,outfp); fputc((q>>8)&255,outfp); fputc((q>>16)&255,outfp); fputc((q>>24)&255,outfp);
      q=sam[x].address;
      fputc((q>>0)&255,outfp); fputc((q>>8)&255,outfp); fputc((q>>16)&255,outfp); fputc((q>>24)&255,outfp);
      buf=malloc(q=sam[x].s->count);
      sam_read(sam[x].s,buf,0,q);
      fwrite(buf,q,1,outfp);
      free(buf);
    }
  }
}

static void x_file_end(void) {
  long k=ftell(outfp);
  int x;
  long q;
  fseek(outfp,0xB4,SEEK_SET);
  q=opt['H'];
  fputc((q>>0)&255,outfp); fputc((q>>8)&255,outfp); fputc((q>>16)&255,outfp); fputc((q>>24)&255,outfp);
  for(x=0;x<256;x++) if(sam[x].s) sam_close(sam[x].s);
  free(sam);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  return;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int ch=channel[ev->chan].chan_sub;
  if(e->a<0) {
    switch(~e->a) {
      case 0: // Sample select
        qs_write(((ch-1)&15)<<3,(sam[e->d].address>>16)|0x8000);
        qs_write((ch<<3)|1,sam[e->d].address&0xFFFF);
        qs_write((ch<<3)|4,sam[e->d].head[1]-sam[e->d].head[0]);
        qs_write((ch<<3)|5,(sam[e->d].address+sam[e->d].head[1])&0xFFFF);
        if(per[ch]=(sam[e->d].head[3]<0 && sam[e->d].head[4])) qs_write((ch<<3)|2,sam[e->d].head[4]);
        break;
      case 1: // Volume
        if(vol[ch]!=e->d) {
          vol[ch]=e->d;
          if(key[ch]) qs_write((ch<<3)|6,e->d);
        }
        break;
      case 2: // Key off
        if(key[ch]) qs_write((ch<<3)|6,0);
        key[ch]=0;
        break;
      case 3: // Key on
      case 4: // Pitch change
        if(per[ch]) {
          qs_write((ch<<3)|4,e->d);
          qs_write((ch<<3)|5,e->d);
        } else {
          qs_write((ch<<3)|2,e->d);
        }
        if(e->a==~3) {
          qs_write((ch<<3)|6,vol[ch]);
          key[ch]=1;
        }
        break;
    }
  } else {
    qs_write(e->a,e->d);
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  mru_sam=-1;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  int ch=channel[chan].chan_sub;
  switch(command) {
    case MC_Sample:
      if(!sam[value].s) {
        sam[value].s=sam_open(value,opt['H'],-8);
        sam_header(sam[value].s,6,sam[value].head);
        if(!sam[value].head[1]) {
          if(sam[value].s->loop_mode) {
            sam[value].head[0]=sam[value].s->loop_start;
            sam[value].head[1]=sam[value].s->loop_end;
          } else {
            sam[value].head[0]=sam[value].head[1]=sam[value].s->count;
          }
        }
        if(!sam[value].head[2]) sam[value].head[2]=15;
        if(!sam[value].head[3]) sam[value].head[3]=sam[value].s->clock>>8;
        if(sam[value].head[3]>0 && !sam[value].head[5]) sam[value].head[5]=8;
        if(0xF0000&(next_address^(next_address+sam[value].s->count))) next_address=(next_address+0x10000)&0xFF0000;
        sam[value].address=next_address;
        next_address+=sam[value].s->count;
      }
      if(mru_sam==value) return 0;
      mru_sam=value;
      inf->clock_div=((int)(sam[value].head[3]))<<8;
      inf->note_bits=sam[value].head[2];
      inf->basic_octave=sam[value].head[5];
      figure_out_note_values(inf);
      e=malloc(sizeof(Event1));
      e->a=~0;
      e->d=value;
      return (Event*)e;
    case MC_Volume:
      e=malloc(sizeof(Event1));
      e->a=~1;
      e->d=value;
      return (Event*)e;
    case MC_Panning:
      e=malloc(sizeof(Event1));
      e->a=ch|0x80;
      e->d=value+0x0120;
      return (Event*)e;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->a=~3;
  e->d=note;
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->a=~4;
  e->d=note;
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->a=~2;
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->a=~2;
  return (Event*)e;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->a=address;
  e->d=value;
  return (Event*)e;
}

Auto_Constructor(
  .name="QSOUND",
  .chip_id=CH_QSound,
  .note_bits=16,
  .options={ ['H']=4000000 }
)
