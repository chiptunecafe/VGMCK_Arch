/*
  VGMCK (Video Game Music Compiler Kit) version 1.0
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

static unsigned long counter=0;

typedef struct Event1 {
  Event ev;
  unsigned long x;
} Event1;

static void x_chip_enable(ChipDef*info) {
  printf("DEBUG x=%u y=%u\n",info->options['x'],info->options['y']);
  info->clock_div=info->options['c'];
  info->note_bits=info->options['n'];
  info->basic_octave=info->options['o'];
}

static void x_file_begin(void) {
  puts("file_begin");
}

static void x_file_end(void) {
  puts("file_end");
}

static void x_loop_start(void) {
  puts("loop_start");
}

static void x_send(Event*ev) {
  printf("send %d,%lu,%lu\n",ev->chan,ev->time,((Event1*)ev)->x);
}

static void x_start_channel(ChipDef*info,int chan) {
  printf("start_channel %d,%d,%d\n",chan,channel[chan].chip_sub,channel[chan].chan_sub);
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
  Event1*e=malloc(sizeof(Event1));
  printf("%lu: set_macro %d,%d,%d,%d\n",e->x=counter++,chan,dyn,command,value);
  return (Event*)e;
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
  Event1*e=malloc(sizeof(Event1));
  printf("%lu: note_on %d,%ld,%d,%ld\n",e->x=counter++,chan,note,oct,dur);
  return (Event*)e;
}

static Event*x_note_change(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  printf("%lu: note_change %d,%ld,%d\n",e->x=counter++,chan,note,oct);
  return (Event*)e;
}

static Event*x_note_off(int chan,long note,int oct) {
  Event1*e=malloc(sizeof(Event1));
  printf("%lu: note_off %d,%ld,%d\n",e->x=counter++,chan,note,oct);
  return (Event*)e;
}

static Event*x_rest(int chan,long dur) {
  Event1*e=malloc(sizeof(Event1));
  printf("%lu: rest %d,%ld\n",e->x=counter++,chan,dur);
  return (Event*)e;
}

static Event*x_direct(int chan,int address,int value) {
  Event1*e=malloc(sizeof(Event1));
  printf("%lu: direct %d,%d,%d\n",e->x=counter++,chan,address,value);
  return (Event*)e;
}

Auto_Constructor(
  .name="DEBUG",
  .clock_div=1,
  .note_bits=16,
  .basic_octave=9,
  .options={ ['c']=1,['n']=16,['o']=9,['x']=42 }
)
