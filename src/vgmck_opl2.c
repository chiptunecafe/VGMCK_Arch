/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

static const int oper[9]={0,1,2,8,9,10,16,17,18};

static int memory[2][256]={{[0 ... 255]=-1},{[0 ... 255]=-1}};
static signed int*opt;
static int dual=0;
static int subc[2]={0,0};
static int instr[6][18];
static int vol[6][18];

typedef struct Event1 {
  Event;
  int type;
  long data1;
  long data2;
} Event1;

static void write_opl(int chip,int address,int value) {
  if(memory[chip][address]!=value) {
    memory[chip][address]=value;
    fputc(chip?0xAA:0x5A,outfp);
    fputc(address,outfp);
    fputc(value,outfp);
  }
}

static inline void set_opl(int chip,int address,int mask,int set) {
  write_opl(chip,address,(memory[chip][address]&~mask)|(set&mask));
}

static void set_instrument(int c,int ch,int o,int i,int v) {
  signed short*d=macro_env[MC_Option][i].data;
  int s=(o&7)/3; // is second operator
  int h=((memory[c][0xBD]&0x20) && o>16) || (d[10]&1) || s;
  v+=d[s|2]&0x3F;
  if(v>63) v=63;
  write_opl(c,o|0x20,d[s]);
  write_opl(c,o|0x40,h?((d[s|2]&0xC0)|v):d[s|2]);
  write_opl(c,o|0x60,d[s|4]);
  write_opl(c,o|0x80,d[s|6]);
  write_opl(c,o|0xE0,d[s|8]);
  if(!s) write_opl(c,ch|0xC0,d[10]);
}

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
  info->clock_div=opt['H']/9;
}

static void x_file_begin(void) {
  int i,j;
  dual=subc[1]==2?6:9;
  if(subc[1]<2 && subc[0]<=dual) dual=0;
  for(i=0;i<=!!dual;i++) {
    write_opl(i,0x01,0x20);
    write_opl(i,0x08,0x00);
    for(j=0x20;j<0xB9;j++) write_opl(i,j,0);
    write_opl(i,0xBD,(subc[1]>i)<<5);
  }
}

static void x_file_end(void) {
  long k=ftell(outfp);
  fseek(outfp,0x50,SEEK_SET);
  fputc(opt['H']&255,outfp); fputc((opt['H']>>8)&255,outfp);
  fputc((opt['H']>>16)&255,outfp); fputc(((!!dual)<<6)|(opt['H']>>24)&255,outfp);
  fseek(outfp,k,SEEK_SET);
}

static void x_loop_start(void) {
  int i;
  for(i=0xA0;i<0xB0;i++) memory[0][i]=memory[1][i]=-1;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int a=channel[e->chan].chip_sub;
  int b=channel[e->chan].chan_sub;
  int c=a?b:(dual && b>=dual);
  int d=a?:(dual?b%dual:b);
  switch(e->type) {
    case 0: // direct
      write_opl(c,e->data1,e->data2);
      break;
    case 1: // note on
      if(a) d=11-a;
      if(a==1 || a==2) {
        write_opl(c,0xA7,e->data1);
        write_opl(c,0xB7,e->data2+(macro_env[MC_Option][instr[a][b]].data[11]<<2));
        d=8;
      }
      write_opl(c,0xA0|d,e->data1);
      write_opl(c,0xB0|d,e->data2);
      if(a) set_opl(c,0xBD,1<<(a-1),0xFF);
      break;
    case 2: // note off
      if(a) set_opl(c,0xBD,1<<(a-1),0);
      else set_opl(c,0xB0|d,0x20,0);
      break;
    case 3: // instrument
      instr[a][b]=e->data1;
      //fallthrough
    case 4: // volume
      if(e->type==4) vol[a][b]=e->data1;
      if(a==1 || a==2) {
        // Hat/Cymbal
        set_instrument(c,7,17,instr[a][b],vol[a][b]);
        set_instrument(c,8,21,instr[a][b],vol[a][b]);
      } else if(a==3) {
        // Tom
        set_instrument(c,8,18,instr[a][b],vol[a][b]);
      } else if(a==4) {
        // SD
        set_instrument(c,7,17,instr[a][b],vol[a][b]);
        set_instrument(c,7,20,instr[a][b],vol[a][b]);
      } else if(a==5) {
        // BD
        set_instrument(c,6,16,instr[a][b],vol[a][b]);
        set_instrument(c,6,19,instr[a][b],vol[a][b]);
      } else if(a==0) {
        // Melody
        set_instrument(c,d,oper[d],instr[a][b],vol[a][b]);
        set_instrument(c,d,oper[d]+3,instr[a][b],vol[a][b]);
      }
      break;
    case 5: // global setting
      write_opl(c,0xBD,(memory[c][0xBD]&0x3F)|e->data1);
      write_opl(c,0x08,e->data2);
      break;
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  int x=!!channel[chan].chip_sub;
  int y=channel[chan].chan_sub;
  if(subc[x]<=y) subc[x]=y+1;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  switch(command) {
    case MC_Volume:
      e=malloc(sizeof(Event1));
      e->type=4;
      e->data1=63&~value;
      return (Event*)e;
    case MC_Tone:
      e=malloc(sizeof(Event1));
      e->type=3;
      e->data1=value&255;
      return (Event*)e;
    case MC_Global:
      e=malloc(sizeof(Event1));
      e->type=5;
      e->data1=(value&3)<<6;
      e->data2=(value&12)<<4;
      return (Event*)e;
    default:
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->type=1;
  e->data1=note&255;
  e->data2=(note>>8)|(oct<<2)|((!channel[chan].chip_sub)<<5);
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  return x_note_on(chan,note,oct,0);
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->type=2;
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->type=2;
  return (Event*)e;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->type=0;
  e->data1=address;
  e->data2=value;
  return (Event*)e;
}

Auto_Constructor(
  .name="OPL2",
  .chip_id=CH_YM3812,
  .note_bits=-10,
  .basic_octave=7,
  .options={ ['H']=3579545 }
)
