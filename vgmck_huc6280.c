/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event ev;
  int type;
  int addr;
  long val;
} Event1;

static const int maxsub[3]={6,1,2};

static signed int*opt;
static int dual=0;
static int usesub[3]={0,0,0};
static int assign[12][2];
static int memory[12][10]={[0 ... 11]={[0 ... 9]=-1}};
static char memw[12][10];
static int mult[2]={4,4};
static int wave[2][6]={[0 ... 1]={[0 ... 5]=-1}};
static int fixfm[2]={0,0};

static void mem_write(int chip,int chan,int addr,int val) {
  int k;
  chan*=(k=(addr>=2 && addr<=7));
  if(memory[chip*6+chan][addr]!=val || addr==6 || memw[chip*6+chan][addr]) {
    memw[chip*6+chan][addr]=0;
    if(k && memory[chip*6][0]!=chan) mem_write(chip,0,0,chan);
    memory[chip*6+chan][addr]=val;
    fputc(0xB9,outfp);
    fputc((chip<<7)|addr,outfp);
    fputc(val,outfp);
  }
}

static void x_chip_enable(ChipDef*info) {
  opt=info->options;
}

#define Assign(x,y) (assign[i][0]=x),(assign[i++][1]=y)
static void x_file_begin(void) {
  int i=0;
  dual|=(usesub[0]>6-(usesub[1]*2+usesub[2]));
  Assign(0,2),Assign(0,3);
  if(usesub[1]<1) Assign(0,0),Assign(0,1);
  if(usesub[2]<1) Assign(0,4);
  if(usesub[2]<2) Assign(0,5);
  Assign(1,2),Assign(1,3);
  if(usesub[1]<2) Assign(1,0),Assign(1,1);
  if(usesub[2]<3) Assign(1,4);
  if(usesub[2]<4) Assign(1,5);
  for(i=0;i<=dual;i++) {
    mem_write(i,0,1,0xFF);
    mem_write(i,0,9,(usesub[1]>i)<<7);
    mem_write(i,0,4,0);
    mem_write(i,0,5,0xFF);
    mem_write(i,1,4,0);
    mem_write(i,1,5,0xFF);
    mem_write(i,2,4,0);
    mem_write(i,2,5,0xFF);
    mem_write(i,3,4,0);
    mem_write(i,3,5,0xFF);
    mem_write(i,4,4,0);
    mem_write(i,4,5,0xFF);
    mem_write(i,4,7,(usesub[2]<=i*2)<<7);
    mem_write(i,5,4,0);
    mem_write(i,5,5,0xFF);
    mem_write(i,5,7,(usesub[2]<=i*2+1)<<7);
  }
}
#undef Assign

static void x_file_end(void) {
  long x=ftell(outfp);
  fseek(outfp,0xA4,SEEK_SET);
  fputc(opt['H']&255,outfp); fputc((opt['H']>>8)&255,outfp);
  fputc((opt['H']>>16)&255,outfp); fputc(((!!dual)<<6)|(opt['H']>>24)&255,outfp);
  fseek(outfp,x,SEEK_SET);
}

static void x_loop_start(void) {
  int i,j;
  for(i=0;i<12;i++) for(j=0;j<10;j++) memw[i][j]=1;
}

static void x_send(Event*ev) {
  Event1*e=(Event1*)ev;
  int cs=channel[ev->chan].chip_sub;
  int ca=channel[ev->chan].chan_sub;
  int chip=cs?ca/cs:assign[ca][0];
  int chan=cs?(cs==2?(4|ca&5):0):assign[ca][1];
  int i;
  long v=e->val;
  long w=v;
  switch(e->type) {
    case 0: // Direct
      mem_write(chip,e->addr>>4,e->addr&15,v);
      break;
    case 1: // Rest
      mem_write(chip,chan,4,(memory[chip*6+chan][4]&0x1F)|0x40);
      break;
    case 2: // Note
      note_retrig:
      if(cs==1) {
        if(fixfm[chip]) v=256;
        i=mult[chip];
        if(i&1) v>>=2;
        else if(i&2) v>>=1,i>>=1;
        else i>>=2;
        mem_write(chip,0,8,i);
        mem_write(chip,1,2,v&0xFF);
        mem_write(chip,1,3,v>>8);
      }
      if(cs==2) {
        mem_write(chip,chan,7,v|0x80);
      } else {
        mem_write(chip,chan,2,w&0xFF);
        mem_write(chip,chan,3,w>>8);
      }
      mem_write(chip,chan,4,(memory[chip*6+chan][4]&0x1F)|0x80);
      break;
    case 3: // Volume
      mem_write(chip,chan,4,(memory[chip*6+chan][4]&0xC0)|v);
      break;
    case 4: // Stereo
      mem_write(chip,chan,5,v);
      break;
    case 5: // Tone (FM)
      mem_write(chip,0,9,v&3);
      fixfm[chip]=v>>2;
      break;
    case 6: // Multiplier (FM)
      mult[ca]=v;
      w=v=(memory[chip*6][3]<<8)|memory[chip*6][2];
      if(memory[chip*6][4]&0x80) goto note_retrig;
      break;
    case 7: // Modulator waveform (FM)
      chan=1;
      //fallthrough
    case 8: // Carrier waveform
      if(wave[chip][chan]!=v) {
        wave[chip][chan]=v;
        mem_write(chip,chan,4,memory[chip*6+chan][4]&0x1F);
        w=macro_env[MC_Waveform][v].loopend-1;
        for(i=0;i<32;i++) mem_write(chip,chan,6,macro_env[MC_Waveform][v].data[i]&w);
      }
      break;
    case 9: // Global stereo
      mem_write(0,0,1,v);
      mem_write(dual,0,1,v);
      break;
  }
}

static void x_start_channel(ChipDef*info,int chan) {
  info->clock_div=-opt['H']*(channel[chan].chip_sub!=2);
  dual|=(channel[chan].chan_sub>=maxsub[channel[chan].chip_sub]);
  if(usesub[channel[chan].chip_sub]<=channel[chan].chan_sub) usesub[channel[chan].chip_sub]=channel[chan].chan_sub+1;
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e;
  switch(command) {
    case MC_Volume:
      e=malloc(sizeof(Event1));
      e->type=3;
      e->val=value&31;
      return (Event*)e;
    case MC_Panning:
      e=malloc(sizeof(Event1));
      e->type=4;
      e->val=0xFF^(value<0?-value:value<<4);
      return (Event*)e;
    case MC_Tone:
      if(channel[chan].chip_sub==1) {
        e=malloc(sizeof(Event1));
        e->type=5;
        e->val=value;
        return (Event*)e;
      }
      break;
    case MC_Multiply:
      if(channel[chan].chip_sub==1) {
        e=malloc(sizeof(Event1));
        e->type=6;
        e->val=value;
        return (Event*)e;
      }
      break;
    case MC_ModWaveform:
      if(channel[chan].chip_sub==1) {
        e=malloc(sizeof(Event1));
        e->type=7;
        e->val=value;
        return (Event*)e;
      }
      break;
    case MC_Waveform:
      e=malloc(sizeof(Event1));
      e->type=8;
      e->val=value;
      return (Event*)e;
    case MC_Global:
      e=malloc(sizeof(Event1));
      e->type=9;
      e->val=value;
      return (Event*)e;
    default:
      return 0;
  }
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->type=2;
  e->val=note;
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->type=2;
  e->val=note;
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  e->type=1;
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  Event1*e=malloc(sizeof(Event1));
  e->type=1;
  return (Event*)e;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  e->type=0;
  e->addr=address;
  e->val=value;
  return (Event*)e;
}

Auto_Constructor(
  .name="PCENGINE",
  .note_bits=12,
  .basic_octave=0,
  .options={ ['H']=3579545 }
)
