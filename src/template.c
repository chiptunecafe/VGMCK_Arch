/*
  VGMCK (Video Game Music Compiler Kit) version ?.?
  Licensed under GNU GPL v3 or later version.
*/

#include "vgmck.h"

typedef struct Event1 {
  Event ev;
} Event1;

static void x_chip_enable(ChipDef*info) {
}

static void x_file_begin(void) {
}

static void x_file_end(void) {
}

static void x_loop_start(void) {
}

static void x_send(Event*ev) {
}

static void x_start_channel(ChipDef*info,int chan) {
}

static Event*x_set_macro(int chan,int dyn,int command,signed short value) {
}

static Event*x_note_on(int chan,long note,int oct,long dur) {
}

static Event*x_note_change(int chan,long note,int oct) {
}

static Event*x_note_off(int chan,long note,int oct) {
}

static Event*x_rest(int chan,long dur) {
}

static Event*x_direct(int chan,int address,int value) {
}

Auto_Constructor(
  .name="???",
  .options={ ['H']=0 }
)
