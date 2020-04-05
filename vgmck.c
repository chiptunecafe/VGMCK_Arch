/*
  VGMCK (Video Game Music Compiler Kit) version 1.1
  Licensed under GNU GPL v3 or later version.

  Should be compiled with GNU89 mode.
*/

#include "vgmck.h"
#include <limits.h>
#define GRAPHIC(x) (((x)&255)>' ')
#define TAU (6.283185307179586476925286766559005768394)

FILE*outfp;
ChipDef*chipsets;
ChanDef channel[52];
Event*events;
long total_samples=0;
MacroEnv macro_env[MAX_MACRO][256];

static char**gd3_text;
static int gd3_count;
static double note_freq[32];
static int note_letter[10]={9,11,0,2,4,5,7,0,0,0};
static long note_value[32];
static int octave_count=12;
static long loop_samples=0;
static signed long recording_rate=0;
static signed short volmod=0;
static signed char loopbase=0;
static unsigned char loopmod=0;
static char*textmac[128];
static char loop_on=0;
static long loop_point=0;
static int macro_use[MAX_MACRO];
static int framerate=735;
static double basefreq;
static long loop_offset;
static int note_off_event=0;
static int sample_list=-1;
static long fastforward=0;
static long portam[8];
static FILE*midifp;
static int debug_input_lines=0;

static const MacroDef macro_def[MAX_MACRO]={
  [MC_Volume]={"v","@v","@vr"},
  [MC_Panning]={"P","@P",""},
  [MC_Tone]={"@","@@",""},
  [MC_Option]={"","@x",""},
  [MC_Arpeggio]={"","@EN",""},
  [MC_Global]={"@G","",""},
  [MC_Multiply]={"M","@M",""},
  [MC_Waveform]={"@W","@W",""},
  [MC_ModWaveform]={"@WM","",""},
  [MC_VolumeEnv]={"ve","",""},
  [MC_Sample]={"@S","@S",""},
  [MC_SampleList]={"@SL","@SL",""},
  [MC_MIDI]={"","@MIDI",""}
};

static void insert_event(Event*ev) {
  Event**pp=&events;
  int q=0;
  ev->next=0;
  if(events) {
    for(;;) {
      if(!*pp || (q && ev->time<(*pp)->time)) {
        ev->next=*pp;
        *pp=ev;
        break;
      } else {
        q=ev->time>=(*pp)->time;
        pp=&(*pp)->next;
      }
    }
  } else {
    events=ev;
  }
}

static void add_gd3(int line,char*text) {
  if(gd3_count<=line) {
    int c=line+1;
    gd3_text=realloc(gd3_text,sizeof(char*)*c);
    while(gd3_count<c) gd3_text[gd3_count++]=0;
  }
  if(gd3_text[line]) {
    gd3_text[line]=realloc(gd3_text[line],strlen(gd3_text[line])+strlen(text)+2);
    strcat(gd3_text[line],"\n");
    strcat(gd3_text[line],text);
  } else {
    gd3_text[line]=strdup(text);
  }
}

static int lchan(char ch) {
  if(ch>='A' && ch<='Z') return (ch&31)-1;
  if(ch>='a' && ch<='z') return (ch&31)+25;
  return -1;
}

static long readnum(char**s) {
  long b=10;
  long m=1;
  long v=0;
  if(**s==',') ++*s;
  if(**s=='$') ++*s,b=16;
  if(**s=='+') ++*s,m=1;
  if(**s=='-') ++*s,m=-1;
  if(**s=='$') ++*s,b=16;
  while(**s) {
    if(**s>='0' && **s<='9') v=b*v+**s-'0';
    else if(b==16 && **s>='A' && **s<='F') v=b*v+**s+10-'A';
    else break;
    ++*s;
  }
  return m*v;
}

static inline void out32(unsigned long x) {
  fputc(x&255,outfp);
  fputc((x>>8)&255,outfp);
  fputc((x>>16)&255,outfp);
  fputc((x>>24)&255,outfp);
}

static inline void write_utf16(unsigned char*text) {
  // Warning: This is not compliant with Unicode standard!! But it is good enough to convert *valid* UTF-8 and CESU-8 to UTF-16.
  // (I do not want to add error checking and that kind of things; I do not need it to be compliant.)
  int utf_value=0;
  int utf_count=0;
  int ch;
  while(ch=*text++) {
    if(ch&128) {
      if(ch&64) {
        if(ch&32) {
          utf_value=ch&15;
          utf_count=(ch>>4)&3;
        } else {
          utf_value=ch&31;
          utf_count=1;
        }
      } else {
        utf_value=(utf_value<<6)|(ch&63);
        if(!--utf_count) {
          if(utf_value&0x1F0000) {
            // lead
            fputc((utf_value>>10)&255,outfp);
            fputc(0xD8|((utf_value>>18)&3),outfp);
            // trail
            fputc(utf_value&255,outfp);
            fputc(0xDC|((utf_value>>8)&3),outfp);
          } else {
            fputc(utf_value&255,outfp);
            fputc(utf_value>>8,outfp);
          }
        }
      }
    } else {
      fputc(ch,outfp);
      fputc(0,outfp);
    }
  }
}

static inline void write_gd3(void) {
  int i;
  long o=ftell(outfp)-0x14;
  long O;
  if(!gd3_count) return;
  fseek(outfp,0x14,SEEK_SET);
  fputc((o>>0)&255,outfp); fputc((o>>8)&255,outfp); fputc((o>>16)&255,outfp); fputc((o>>24)&255,outfp);
  fseek(outfp,o+=0x14,SEEK_SET);
  fwrite("Gd3 \0\1\0\0'''",1,12,outfp); o+=8;
  for(i=0;i<gd3_count;i++) {
    if(gd3_text[i]) write_utf16(gd3_text[i]);
    fputc(0,outfp); fputc(0,outfp);
  }
  for(;i<11;i++) {
    fputc(0,outfp); fputc(0,outfp);
  }
  i=((O=ftell(outfp))-o)-4;
  fseek(outfp,o,SEEK_SET);
  fputc((i>>0)&255,outfp); fputc((i>>8)&255,outfp); fputc((i>>16)&255,outfp); fputc((i>>24)&255,outfp);
  fseek(outfp,O,SEEK_SET);
}

static void write_delay(unsigned long dur) {
  if(fastforward>dur) {
    fastforward-=dur;
    return;
  } else if(fastforward) {
    dur-=fastforward;
    fastforward=0;
  }
  while(dur) {
    if((dur>=735 && dur<=751) || dur==1470 || dur==1617 || (dur>=65536 && dur<=67152)) {
      fputc(0x62,outfp);
      dur-=735;
    } else if((dur>=882 && dur<=898) || dur==1764 || (dur>=67153 && dur<=67299)) {
      fputc(0x63,outfp);
      dur-=882;
    } else if(dur<=16) {
      fputc(dur+0x6F,outfp);
      break;
    } else if(dur<=32) {
      fputc(0x7F,outfp);
      dur-=16;
    } else if(dur<=65535) {
      fputc(0x61,outfp); fputc(dur&255,outfp); fputc(dur>>8,outfp);
      break;
    } else {
      fputc(0x61,outfp); fputc(0xFF,outfp); fputc(0xFF,outfp);
      dur-=65535;
    }
  }
}

static inline void write_vgm_header(void) {
  int i;

  fputc('V',outfp); fputc('g',outfp); fputc('m',outfp); fputc(' ',outfp);
  out32(0); // EoF offset
  out32(VGM_VERSION);
  out32(0);

  out32(0);
  out32(0); // GD3 offset
  out32(total_samples-fastforward);
  out32(0); // Loop offset

  out32((total_samples-fastforward)-loop_point);
  out32(recording_rate);
  out32(0);
  out32(0);

  out32(0);
  out32((VGM_MAX_HEADER<<2)-0x34);
  out32(0);
  out32(0);

  out32(0);
  out32(0);
  out32(0);
  out32(0);

  out32(0);
  out32(0);
  out32(0);
  out32(0);

  out32(0);
  out32(0);
  out32(0);
  out32(0);

  out32(0);
  out32(0);
  out32(0);
  fputc((volmod==-64?-63:volmod)&255,outfp); fputc(0,outfp); fputc(loopbase&255,outfp); fputc(loopmod&255,outfp);

  for(i=32;i<VGM_MAX_HEADER;i++) out32(0);
}

static inline void write_eof_loop(void) {
  long x=ftell(outfp);
  fseek(outfp,4,SEEK_SET);
  out32(x);
  fseek(outfp,28,SEEK_SET);
  out32(loop_offset-28);
}

static inline void parse_chip_enable(char*cmd,char*par) {
  ChipDef*rec=chipsets;
  int x,y,z;
  while(rec) {
    if(!strcmp(cmd,rec->name)) {
      rec->used=1;
      y=z=0;
      while(*par) {
        if(*par==' ') break;
        if(*par==',') {
          y++;
          z=0;
        } else if(*par=='_') {
          z++;
        }
        x=lchan(*par++);
        if(x!=-1) {
          channel[x].chip=rec;
          channel[x].chip_sub=y;
          channel[x].chan_sub=z++;
          channel[x].text=strdup("");
        }
      }
      par+=!!*par;
      x=0;
      while(*par) {
        if(*par==' ') {
          x=0;
        } else if(*par=='+') {
          rec->options[par[1]&127]=1;
        } else if(*par=='-') {
          rec->options[par[1]&127]=0;
        } else if(*par=='=') {
          par++;
          rec->options[x]=readnum(&par);
          par--;
          x=0;
        } else if(*par==':' && x=='o') {
          par++;
          rec->basic_octave=readnum(&par);
          par--;
          x=0;
        } else if(*par==':' && x=='N') {
          par++;
          rec->chip_id=readnum(&par);
          par--;
          x=0;
        } else {
          x=*par&127;
        }
        par++;
      }
      rec->chip_enable(rec);
      return;
    }
    rec=rec->next;
  }
}

static inline void parse_scale(char*s) {
  int x=0;
  while(*s) {
    if(*s>='a' && *s<='j') {
      note_letter[*s-'a']=x++;
    } else if (*s=='.') {
      x++;
    }
    s++;
  }
  octave_count=x;
}

static inline void parse_just_intonation(char*s) {
  int x,y,z;
  for(x=0;x<octave_count;x++) {
    y=readnum(&s);
    z=readnum(&s);
    note_freq[x]=((double)y)/(double)z;
  }
}

static inline void make_equal_temperament(void) {
  int x;
  for(x=0;x<octave_count;x++) note_freq[x]=pow(2.0,((double)x)/(double)octave_count);
}

static void read_input(FILE*inputfp);

static inline void parse_global_command(char*cmd) {
  char*par=cmd;
  while(GRAPHIC(*par)) par++;
  if(*par) *par++=0;
  while(!GRAPHIC(*par)) par++;

  if(!strcmp(cmd,"COMPOSER")) {
    add_gd3(6,par);
    add_gd3(7,par);
  }

  if(!strcmp(cmd,"COMPOSER-E")) {
    add_gd3(6,par);
  }

  if(!strcmp(cmd,"COMPOSER-J")) {
    add_gd3(7,par);
  }

  if(!strcmp(cmd,"DATE")) {
    add_gd3(8,par);
  }

  if(!strcmp(cmd,"DEBUG-INPUT-LINES")) {
    debug_input_lines=readnum(&par);
  }

  if(!strcmp(cmd,"EQUAL-TEMPERAMENT")) {
    make_equal_temperament();
  }

  if(cmd[0]=='E' && cmd[1]=='X' && cmd[2]=='-') {
    parse_chip_enable(cmd+3,par);
  }

  if(!strcmp(cmd,"GAME")) {
    add_gd3(2,par);
    add_gd3(3,par);
  }

  if(!strcmp(cmd,"GAME-E")) {
    add_gd3(2,par);
  }

  if(!strcmp(cmd,"GAME-J")) {
    add_gd3(3,par);
  }

  if(!strcmp(cmd,"INCLUDE")) {
    FILE*fp1=fopen(par,"r");
    if(fp1) {
      read_input(fp1);
      fclose(fp1);
    }
  }

  if(!strcmp(cmd,"JUST-INTONATION")) {
    parse_just_intonation(par);
  }

  if(!strcmp(cmd,"LOOP-BASE")) {
    loopbase=readnum(&par);
  }

  if(!strcmp(cmd,"LOOP-MODIFIER")) {
    loopmod=readnum(&par);
  }

  if(!strcmp(cmd,"NOTES")) {
    add_gd3(10,par);
  }

  if(!strcmp(cmd,"PITCH-CHANGE")) {
    basefreq=readnum(&par)*10.0;
  }

  if(!strcmp(cmd,"PROGRAMER") || !strcmp(cmd,"PROGRAMMER")) {
    add_gd3(9,par);
  }

  if(!strcmp(cmd,"RATE")) {
    recording_rate=readnum(&par);
    if(recording_rate<0) {
      framerate=44100/-recording_rate;
      recording_rate=0;
    } else if(recording_rate>0) {
      framerate=44100/recording_rate;
    }
  }

  if(!strcmp(cmd,"SCALE")) {
    parse_scale(par);
  }

  if(!strcmp(cmd,"SYSTEM")) {
    add_gd3(4,par);
    add_gd3(5,par);
  }

  if(!strcmp(cmd,"SYSTEM-E")) {
    add_gd3(4,par);
  }

  if(!strcmp(cmd,"SYSTEM-J")) {
    add_gd3(5,par);
  }

  if(cmd[0]=='T' && cmd[1]=='E' && cmd[2]=='X' && cmd[3]=='T') {
    add_gd3(strtol(cmd+4,0,0),par);
  }

  if(!strcmp(cmd,"TITLE")) {
    add_gd3(0,par);
    add_gd3(1,par);
  }

  if(!strcmp(cmd,"TITLE-E")) {
    add_gd3(0,par);
  }

  if(!strcmp(cmd,"TITLE-J")) {
    add_gd3(1,par);
  }

  if(!strcmp(cmd,"VOLUME")) {
    volmod=readnum(&par);
  }
}

static inline void parse_envelope(char*cmd) {
  static int mac=-1;
  static int env=0;
  static int block=0;
  static int rep=1;
  static int brep[32];
  static int bst[32];
  int x,y,z,q;
  char buf[8]={0,0,0,0,0,0,0,0};
  if(*cmd=='@') {
    block=0;
    rep=1;
    for(x=0;x<7 && *cmd;x++) {
      if(*cmd>='@' && *cmd!='{') buf[x]=*cmd++; else break;
    }
    mac=-1;
    for(x=0;x<MAX_MACRO;x++) {
      if(!strcmp(buf,macro_def[x].dyn)) mac=x;
    }
    if(mac==-1) return;
    env=readnum(&cmd)&255;
    macro_env[mac][env].loopstart=-1;
    macro_env[mac][env].loopend=0;
  }
  if(mac==-1) return;
  for(;;) {
    while(*cmd && *cmd<=' ') cmd++;
    if((*cmd>='0' && *cmd<='9') || *cmd=='-' || *cmd=='+' || *cmd=='$') {
      if(macro_env[mac][env].loopend>=MAX_MACROENV) return;
      x=readnum(&cmd);
      for(y=0;y<rep;y++) macro_env[mac][env].data[macro_env[mac][env].loopend++]=x;
    } else if(*cmd=='|') {
      macro_env[mac][env].loopstart=macro_env[mac][env].loopend;
      cmd++;
    } else if(*cmd=='\'') {
      cmd++;
      rep=readnum(&cmd);
    } else if(*cmd==',' && cmd[1]>='a' && cmd[1]<='j') {
      x=note_letter[*++cmd-'a']-macro_env[mac][env].loopend;
      for(;;) {
        if(*cmd=='+') x++;
        else if(*cmd=='-') x--;
        else break;
        cmd++;
      }
      x+=readnum(&cmd)*octave_count;
      y=macro_env[mac][env].loopend-1;
      while(x--) macro_env[mac][env].data[macro_env[mac][env].loopend++]=macro_env[mac][env].data[y];
    } else if(*cmd=='=' || *cmd=='{' || *cmd==',') {
      cmd++;
    } else if(*cmd=='[') {
      brep[block]=rep;
      bst[block]=macro_env[mac][env].loopend;
      block++;
      cmd++;
    } else if(*cmd==']' && block) {
      cmd++;
      z=readnum(&cmd);
      y=macro_env[mac][env].loopend;
      block--;
      while(--z>0) for(x=bst[block];x<y;x++) macro_env[mac][env].data[macro_env[mac][env].loopend++]=macro_env[mac][env].data[x];
      rep=brep[block];
    } else if(*cmd=='"') {
      x=0;
      cmd++;
      while(*cmd && *cmd!='"' && x<63) macro_env[mac][env].text[x++]=*cmd++;
      macro_env[mac][env].text[x++]=0;
      cmd++;
    } else if(*cmd==':') {
      z=0;
      while(*cmd==':') z++,cmd++;
      x=readnum(&cmd);
      y=macro_env[mac][env].data[macro_env[mac][env].loopend-1];
      z*=x>y?1:-1;
      while(x!=y) {
        y+=z;
        for(q=0;q<rep;q++) macro_env[mac][env].data[macro_env[mac][env].loopend++]=y;
      }
    } else {
      return;
    }
  }
}

static inline void parse_channel(char*cmd) {
  char buf[8192];
  char*p=cmd;
  char*b=buf;
  int x;
  while(*p>' ') p++;
  while(*p) {
    if(*p==';') {
      break;
    } else if(*p=='*') {
      strcpy(b,textmac[p[1]]);
      while(*++b);
      p++;
      p+=!!*p;
    } else {
      *b++=*p++;
    }
  }
  *b++=0;
  while(*cmd) {
    x=lchan(*cmd++);
    if(x==-1) break;
    if(!channel[x].text) {
      fprintf(stderr,"Channel %c not declared before use.\n",cmd[-1]);
      return;
    }
    strcat(channel[x].text=realloc(channel[x].text,strlen(channel[x].text)+(b-buf)),buf);
  }
}

static void read_input(FILE*inputfp) {
  static char buf[2048];
  char*bend;
  char*bstr;
  while(!feof(inputfp) && fgets(buf,2048,inputfp) && *buf) {
    bend=buf+strlen(buf)-1;
    while(*bend && !GRAPHIC(*bend)) *bend--=0;
    bstr=buf;
    while(*bstr&0x80) bstr++; // strip UTF-8 "byte order mark"
    while(*bstr && !GRAPHIC(*bstr)) bstr++;
    if(debug_input_lines) puts(bstr);
    switch(*bstr) {
      case '"':
        add_gd3(10,bstr+1);
        break;
      case '#':
        if(!strcmp(bstr,"#EOF")) return;
        parse_global_command(bstr+1);
        break;
      case '*':
        free(textmac[bstr[1]]);
        if(bend>bstr+2) textmac[bstr[1]]=strdup(bstr+2);
        break;
      case '@':
      case '-':
      case '+':
      case '$':
      case '[':
      case ']':
      case '{':
      case ',':
      case '|':
      case '0' ... '9':
        parse_envelope(bstr);
        break;
      case 'A' ... 'Z':
      case 'a' ... 'z':
        parse_channel(bstr);
        break;
      default:
        break;
    }
  }
}

void figure_out_note_values(ChipDef*chip) {
  int i;
  int b=chip->note_bits;
  int c=chip->clock_div<0;
  int q=chip->clock_div*(c?-1:1);
  unsigned long long v;
  unsigned long long w=0;
  double d;
  long j;
  static unsigned long long u[32];
  if(!q) return;
  if(b<0) b=-b;
  j=(-1)<<b;
  for(i=0;i<32;i++) {
    d=note_freq[i]*basefreq+0.000001;
    if(c) v=(((unsigned long long)q)<<24)/d;
    else v=((unsigned long long)d)*(((unsigned long long)q)<<22);
    u[i]=v;
    w|=v;
  }
  while(w&j) {
    w>>=1;
    for(i=0;i<32;i++) u[i]>>=1;
  }
  for(i=0;i<32;i++) note_value[i]=u[i];
}

static inline long calc_note_len(int tempo,int len,int dots) {
  long k=10584000/len;
  long j=k;
  while(dots--) k+=(j>>=1);
  return k/tempo;
}

static long readlen(char**t,int tempo) {
  int x=readnum(t);
  int d=0;
  while(**t=='.') d++,++*t;
  return calc_note_len(tempo,x,d);
}

static void read_note(char**t,int tempo,long*len,int*note) {
  long len2=*len;
  int x;
  int d=0;
  if(*note>=0) while(**t) {
    if(**t=='+') ++*note;
    else if(**t=='-') --*note;
    else if(**t=='\'') *note+=octave_count;
    else break;
    ++*t;
  }
  x=readnum(t);
  while(**t=='.') d++,++*t;
  if(x) {
    *len=calc_note_len(tempo,x,d);
  } else {
    while(d--) *len+=(len2>>=1);
  }
}

static inline int calc_portamento(ChipDef*chip,int prate,int oldnote,int newnote,long gap,int count,long*out) {
  long cldiv=chip->clock_div;
  int boct=chip->basic_octave;
  int dir=newnote>oldnote?1:-1;
  int od=chip->note_bits<0;
  if(portam[0]==0) {
    // Amiga
    int o1=oldnote/octave_count;
    int o=od?0:(cldiv<0?o1-boct:boct-o1);
    int n=oldnote%octave_count;
    long oldv=cldiv?(note_value[n]>>o):n;
    long newv;
    o1=oldnote/octave_count;
    o=od?0:(cldiv<0?o1-boct:boct-o1);
    n=oldnote%octave_count;
    newv=cldiv?(note_value[n]>>o):n;
    if(portam[2]) {
      long x=oldv+dir;
      int y=0;
      while(x!=newv && count--) {
        out[y++]=x;
        x+=dir*portam[2];
        if(dir==1 && x>=newv) break;
        if(dir==-1 && x<=newv) break;
      }
      return y;
    } else {
      int c=gap/prate;
      int i=0;
      while(i<c) i++,out[i]=((newv-oldv)*i+c/2)/c+oldv;
      return c;
    }
  } else if(portam[0]==1) {
    // Glissando
    if(portam[2]) {
      int x=oldnote;
      int y=0;
      while(x!=newnote && count--) {
        int o1=x/octave_count;
        int o=od?0:(cldiv<0?o1-boct:boct-o1);
        int n=x%octave_count;
        out[y++]=cldiv?(note_value[n]>>o):n;
        x+=dir*portam[2];
        if(dir==1 && x>=newnote) break;
        if(dir==-1 && x<=newnote) break;
      }
      return y;
    } else {
      int c=gap/prate;
      int i=0;
      if(c>count) c=count;
      while(i<c) {
        int x=((newnote-oldnote)*i+c/2)/c+oldnote;
        int o1=x/octave_count;
        int o=od?0:(cldiv<0?o1-boct:boct-o1);
        int n=x%octave_count;
        out[i++]=cldiv?(note_value[n]>>o):n;
      }
      return c;
    }
  }
}

static void send_note(int chan,ChipDef*chip,long time,int note,long dur,long detune,long quantize,unsigned char kind) {
  Event*e;
  static int m[MAX_MACRO];
  static int oldnote;
  int i;
  int newnote=note;
  if(kind&1) quantize=0;
  if(kind&8) {
    int o1=note/octave_count;
    int prate=(portam[1]*framerate)>>1?:1;
    long ct=time;
    int c=quantize/prate;
    long*m=malloc(c*sizeof(long));
    time-=quantize;
    dur+=quantize;
    c=calc_portamento(chip,prate,oldnote,newnote,quantize,c,m);
    for(i=0;i<c;i++) {
      if(e=chip->note_change(chan,m[i],o1)) {
        e->chan=chan;
        e->time=time;
        insert_event(e);
      }
      time+=prate;
      dur-=prate;
    }
    free(m);
  }
  if(note==-1) {
    if(e=chip->rest(chan,dur)) {
      e->chan=chan;
      e->time=time;
      insert_event(e);
    }
  } else if(note>=0) {
    int o1=note/octave_count;
    int o=chip->note_bits<0?0:(chip->clock_div<0?(o1-chip->basic_octave):(chip->basic_octave-o1));
    int n=note%octave_count;
    long v=chip->clock_div?((note_value[n]>>o)-detune):n;
    long d=dur-quantize;
    long t=time;
    if(sample_list!=-1) {
      if(e=chip->set_macro(chan,1,MC_Sample,macro_env[MC_SampleList][sample_list].data[note])) {
        e->chan=chan;
        e->time=time;
        insert_event(e);
      }
    }
    if(d<0) d=0;
    if(note_off_event==1 && !(kind&12) && (e=chip->note_off(chan,v,o1))) {
      e->chan=chan;
      e->time=time;
      insert_event(e);
    }
    if(kind&12) e=chip->note_change(chan,v,o1);
    else e=chip->note_on(chan,v,o1,d);
    if(e) {
      e->chan=chan;
      e->time=time;
      insert_event(e);
    }
    for(i=0;i<MAX_MACRO;i++) m[i]=0;
    while(t<time+d) {
      for(i=0;i<MAX_MACRO;i++) {
        if(macro_use[i]!=-1 && m[i]!=-1) {
          if(i==MC_Arpeggio) {
            if(macro_env[i][macro_use[i]].data[m[i]]) {
              note+=macro_env[i][macro_use[i]].data[m[i]];
              o1=note/octave_count;
              o=chip->note_bits<0?0:(chip->clock_div<0?(o1-chip->basic_octave):(chip->basic_octave-o1));
              n=note%octave_count;
              v=chip->clock_div?((note_value[n]>>o)-detune):n;
              if(e=chip->note_change(chan,v,o1)) {
                e->chan=chan;
                e->time=t;
                insert_event(e);
              }
            }
          } else {
            if(e=chip->set_macro(chan,1,i,macro_env[i][macro_use[i]].data[m[i]])) {
              e->chan=chan;
              e->time=t;
              insert_event(e);
            }
          }
          if(++m[i]==macro_env[i][macro_use[i]].loopend) m[i]=macro_env[i][macro_use[i]].loopstart;
        }
      }
      t+=framerate;
    }
    if(note_off_event==0 && !(kind&3) && (e=chip->note_off(chan,v,o1))) {
      e->chan=chan;
      e->time=time+d;
      insert_event(e);
    }
    oldnote=note;
  }
}

#define SENDNOTE if(curlen && !(phase=(phase+1)%phase_count))send_note(chan,chip,time,curnote,curlen,detune,quantize,kind);time+=curlen;curlen=0;kind<<=2
static inline void parse_music(int chan) {
  int transpose=0;
  char*t=channel[chan].text;
  ChipDef*chip=channel[chan].chip;
  int cs=channel[chan].chip_sub;
  int octave=0;
  long time=0;
  int x,y;
  long z;
  int tempo=120;
  long len=calc_note_len(120,4,0);
  int curnote=-1;
  long curlen=0;
  char name[8];
  char*p;
  char*q;
  Event*e;
  char*loopb[128];
  char*loope[128];
  int loopc[128];
  int loop=-1;
  long detune=0;
  long quantize=0;
  int phase=0;
  int phase_count=1;
  unsigned char kind=0;

  if(!chip) {
    fprintf(stderr,"Not a channel: %d\n",chan);
    return;
  }
  note_off_event=0;
  sample_list=-1;
  channel[chan].loop_point=-1;
  for(x=0;x<MAX_MACRO;x++) macro_use[x]=-1;
  chip->start_channel(chip,chan);
  figure_out_note_values(chip);
  while(*t) {
    if(*t>='a' && *t<='j') {
      SENDNOTE;
      curnote=octave*octave_count+note_letter[*t-'a']+transpose;
      curlen=len;
      t++;
      read_note(&t,tempo,&curlen,&curnote);
    } else if(*t=='r') {
      SENDNOTE;
      curlen=len;
      t++;
      read_note(&t,tempo,&curlen,&curnote);
      curnote=-1;
    } else if(*t=='w') {
      SENDNOTE;
      curlen=len;
      t++;
      read_note(&t,tempo,&curlen,&curnote);
      curnote=-2;
    } else if(*t=='n') {
      SENDNOTE;
      t++;
      curnote=readnum(&t)+transpose;
      curlen=len;
      read_note(&t,tempo,&curlen,&curnote);
    } else if(*t=='l') {
      t++;
      len=readlen(&t,tempo);
    } else if(*t=='^') {
      t++;
      z=len;
      read_note(&t,tempo,&z,&y);
      curlen+=z;
    } else if(*t=='&') {
      t++;
      kind|=1;
    } else if(*t=='/') {
      t++;
      kind|=2;
    } else if(*t=='o') {
      t++;
      octave=readnum(&t);
    } else if(*t=='>') {
      t++;
      octave++;
    } else if(*t=='<') {
      t++;
      octave--;
    } else if(*t=='t') {
      t++;
      tempo=readnum(&t);
    } else if(*t=='D') {
      SENDNOTE;
      t++;
      detune=readnum(&t);
    } else if(*t=='K') {
      SENDNOTE;
      t++;
      transpose=readnum(&t);
    } else if(*t=='!') {
      break;
    } else if(*t=='L') {
      SENDNOTE;
      channel[chan].loop_point=time;
      loop_on=1;
      loop_point=time;
      t++;
    } else if(*t=='@' && t[1]=='q') {
      SENDNOTE;
      t+=2;
      quantize=readnum(&t)*framerate;
      quantize-=readnum(&t);
    } else if(*t=='[' && loop<127) {
      loop++;
      loopb[loop]=++t;
      loope[loop]=0;
      loopc[loop]=0;
    } else if(*t==']' && loop>=0) {
      loope[loop]=t++;
      x=readnum(&t);
      if(++loopc[loop]<x) t=loopb[loop]; else loop--;
    } else if(*t=='\\' && loop>=0) {
      t=loope[loop]?:t+1;
    } else if(*t=='?') {
      t+=2;
      x=lchan(t[-1]);
      if(chan!=x && t[-1]!='.') while(*t && *t!='?') t++;
    } else if(*t=='E' && t[1]=='N' && t[2]=='O' && t[3]=='F') {
      SENDNOTE;
      t+=4;
      macro_use[MC_Arpeggio]=-1;
    } else if(*t=='E' && t[1]=='N') {
      SENDNOTE;
      t+=2;
      macro_use[MC_Arpeggio]=readnum(&t);
    } else if(*t=='x') {
      SENDNOTE;
      t++;
      x=readnum(&t);
      y=readnum(&t);
      e=chip->direct(chan,x,y);
      if(e) {
        e->chan=chan;
        e->time=time;
        insert_event(e);
      }
    } else if(*t=='y') {
      SENDNOTE;
      t++;
      x=readnum(&t);
      e=malloc(sizeof(Event)+1);
      e->chan=-1;
      e->time=time;
      e->_data[0]=x;
      insert_event(e);
    } else if(*t=='{') {
      t++;
      len*=2;
      len/=3;
    } else if(*t=='}') {
      t++;
      len*=3;
      len/=2;
    } else if(*t=='N' && t[1]=='O' && t[2]=='E') {
      SENDNOTE;
      t+=3;
      note_off_event=readnum(&t);
    } else if(*t=='@' && t[1]=='[') {
      SENDNOTE;
      t+=2;
      phase=phase_count=0;
      while(*t && *t!=']') {
        if(lchan(*t++)==chan) phase=phase_count;
        phase_count++;
      }
      phase_count+=!!phase_count;
    } else if(*t=='@' && t[1]=='!') {
      SENDNOTE;
      t+=2;
      fastforward=time-readnum(&t)*framerate;
    } else if(*t=='@' && t[1]=='w') {
      SENDNOTE;
      t+=2;
      x=readnum(&t);
      y=readnum(&t);
      time+=(x*framerate)>>y;
    } else if(*t=='@' && t[1]=='/') {
      t+=2;
      for(x=0;x<8;x++) portam[x]=readnum(&t);
    } else if(*t>='@') {
      SENDNOTE;
      for(x=0;x<7 && *t;x++) {
        if(*t>='@') name[x]=*t++; else break;
      }
      name[x]=0;
      y=readnum(&t);
      for(x=0;x<MAX_MACRO;x++) {
        if(!strcmp(name,macro_def[x].stat)) {
          macro_use[x]=-1;
          e=chip->set_macro(chan,0,x,y);
          if(e) {
            e->chan=chan;
            e->time=time;
            insert_event(e);
          }
          break;
        }
        if(!strcmp(name,macro_def[x].dyn)) {
          macro_use[x]=y&255;
          break;
        }
      }
    } else {
      t++; // meaningless commands/spaces
    }
  }
  SENDNOTE;
  channel[chan].duration=time;
  if(total_samples<time) total_samples=time;
  printf("|  %c  |  %8lu  |  %8lu  |\n",chan%26+(chan>=26?'a':'A'),time,loop_point);
}

static inline void make_event_out(void) {
  Event*e=events;
  ChipDef*x=chipsets;
  long time=0;
  while(e) {
    if(loop_on && loop_point>=time && loop_point<=e->time) {
      write_delay(loop_point-time);
      loop_offset=ftell(outfp);
      time=loop_point;
      while(x) {
        if(x->used) x->loop_start();
        x=x->next;
      }
      loop_on=0;
    }
    write_delay(e->time-time);
    time=e->time;
    if(e->chan==-1) {
      fputc(e->_data[0],outfp);
    } else {
      channel[e->chan].chip->send(e);
    }
    e=e->next;
  }
  write_delay(total_samples-time);
  fputc(0x66,outfp);
}

static void chipsets_begin_end(int which) {
  ChipDef*x=chipsets;
  while(x) {
    if(x->used) (which?x->file_begin:x->file_end)();
    x=x->next;
  }
}

static inline void help_list_chips(void) {
  ChipDef*x=chipsets;
  while(x) {
    puts(x->name);
    x=x->next;
  }
}

int main(int argc,char**argv) {
  int i;
  if(argc<2) {
    fprintf(stderr,"VGMCK v" VERSION "\nusage: vgmck {output} < {input}\n");
    return 1;
  } else if(argv[1][0]=='-') {
    switch(argv[1][1]) {
      case 'L':
        help_list_chips();
        break;
    }
    return 0;
  }
  for(i=0;i<12;i++) note_freq[i]=pow(2.0,((double)i)/12.0);
  for(i=12;i<32;i++) note_freq[i]=1.99999;
  basefreq=3520.0*pow(2.0,3.0/12.0);
  read_input(stdin);
  for(i=0;i<52;i++) if(channel[i].chip && channel[i].text) parse_music(i);
  outfp=fopen(argv[1],"wb");
  if(!outfp) return 1;
  write_vgm_header();
  chipsets_begin_end(1);
  make_event_out();
  chipsets_begin_end(0);
  write_gd3();
  write_eof_loop();
  fclose(outfp);
  return 0;
}

static void*i_sam_D(MacroEnv*env,long*count) {
  short*out=malloc(sizeof(short)*(*count=env->loopend-env->loopstart));
  memcpy(out,env->data+env->loopstart,sizeof(short)*(env->loopend-env->loopstart));
  return out;
}

static void*i_sam_d(MacroEnv*env,long*count) {
  char*out=malloc(*count=env->loopend-env->loopstart);
  char*p=out;
  int x=env->loopstart;
  while(x<env->loopend) *p++=env->data[x++];
  return out;
}

static void*i_sam_P(MacroEnv*env,long*count) {
  short*out;
  int a,b,x,y=0;
  for(x=env->loopstart;x<env->loopend;x+=2) y+=env->data[x];
  out=malloc(sizeof(short)*(*count=y));
  y=0;
  for(x=env->loopstart;x<env->loopend;x+=2) {
    a=env->data[x]; b=env->data[x+1];
    while(a--) out[y++]=b;
  }
  return out;
}

static void*i_sam_p(MacroEnv*env,long*count) {
  char*out;
  int a,b,x,y=0;
  for(x=env->loopstart;x<env->loopend;x+=2) y+=env->data[x];
  out=malloc(*count=y);
  y=0;
  for(x=env->loopstart;x<env->loopend;x+=2) {
    a=env->data[x]; b=env->data[x+1];
    while(a--) out[y++]=b;
  }
  return out;
}

static void*i_sam_S(MacroEnv*env,long*count) {
  int x=env->data[env->loopstart];
  int y,z;
  double a,b;
  short*out=malloc(sizeof(short)*(*count=x=x<0?-x:x));
  memset(out,0,sizeof(short)*x);
  for(y=env->loopstart+1;y<=env->loopend;y+=2) {
    a=(double)env->data[y];
    b=TAU/((double)env->data[y+1]);
    for(z=0;z<x;z++) out[z]+=(short)(sin(b*(double)z)*a);
  }
  if(env->data[env->loopstart]>0) for(y=0;y<x;y++) out[y]^=0x8000;
  return out;
}

static void*i_sam_s(MacroEnv*env,long*count) {
  int x=env->data[env->loopstart];
  int y,z;
  double a,b;
  char*out=malloc(*count=x=x<0?-x:x);
  memset(out,0,x);
  for(y=env->loopstart+1;y<=env->loopend;y+=2) {
    a=(double)env->data[y];
    b=TAU/((double)env->data[y+1]);
    for(z=0;z<x;z++) out[z]+=(char)(sin(b*(double)z)*a);
  }
  if(env->data[env->loopstart]>0) for(y=0;y<x;y++) out[y]^=0x80;
  return out;
}

typedef void*(*i_sam_func)(MacroEnv*,long*);
static i_sam_func i_sam_list[128]={
  ['D']=i_sam_D,
  ['P']=i_sam_P,
  ['S']=i_sam_S,
  ['d']=i_sam_d,
  ['p']=i_sam_p,
  ['s']=i_sam_s
};

SampleLoader*sam_open(int id,long clock,int bits) {
  SampleLoader*sam=malloc(sizeof(SampleLoader));
  char*name=macro_env[MC_Sample][id].text;
  int x;
  long y;
  sam->id=id;
  sam->fp=0;
  sam->bit_conv=bits;
  sam->bit_file=bits;
  sam->head_start=0;
  sam->endian=0;
  sam->clock=clock;
  if(macro_env[MC_Sample][id].loopstart==-1) sam->header_size=macro_env[MC_Sample][id].loopend;
  else sam->header_size=macro_env[MC_Sample][id].loopstart;
  if(*name && *name!='#') {
    sam->fp=fopen(name+(*name=='?'),"rb");
    if(!sam->fp) {
      free(sam);
      return 0;
    }
    if(macro_env[MC_Sample][id].loopend) {
      sam->data_start=0;
      fseek(sam->fp,0,SEEK_END);
      sam->count=ftell(sam->fp);
    } else {
      x=(signed char)fgetc(sam->fp);
      if(x) sam->bit_file=x;
      if(!bits) sam->bit_conv=x;
      if(sam->loop_mode=fgetc(sam->fp)) {
        sam->loop_start=fgetc(sam->fp);
        sam->loop_start|=fgetc(sam->fp)<<8;
        sam->loop_end=fgetc(sam->fp);
        sam->loop_end|=fgetc(sam->fp)<<8;
      }
      y=fgetc(sam->fp);
      y|=fgetc(sam->fp)<<8;
      y|=fgetc(sam->fp)<<16;
      sam->clock=y?:clock;
      sam->header_size=fgetc(sam->fp);
      sam->head_start=ftell(sam->fp);
      sam->data_start=ftell(sam->fp)+(sam->header_size<<1);
      fseek(sam->fp,0,SEEK_END);
      sam->count=ftell(sam->fp)-sam->data_start;
    }
    clearerr(sam->fp);
    rewind(sam->fp);
    sam->count>>=(sam->bit_file==16 || sam->bit_file==-16);
  } else if(*name=='#' && macro_env[MC_Sample][id].loopstart!=-1 && i_sam_list[name[1]&127]) {
    sam->data=i_sam_list[name[1]&127](macro_env[MC_Sample]+id,&sam->count);
  } else {
    sam->data=0;
  }
  return sam;
}

void sam_close(SampleLoader*sam) {
  if(sam->fp) fclose(sam->fp);
  else if(sam->data) free(sam->data);
  free(sam);
}

void sam_read(SampleLoader*sam,void*dest,int start,int count) {
  int w1=(sam->bit_file==16 || sam->bit_file==-16);
  int w2=(sam->bit_conv==16 || sam->bit_conv==-16);
  int su=((sam->bit_file^sam->bit_conv)<0 && sam->bit_file)<<(w2?15:7); // sign conversion
  if(sam->fp) fseek(sam->fp,sam->data_start+(start<<w1),SEEK_SET);
  if(!sam->fp) {
    int s=w2?sizeof(signed short):1;
    memcpy(dest,((char*)sam->data)+s*start,s*count);
  } else if(w1 && w2 && sam->endian) {
    unsigned short*d=dest;
    int x;
    while(count--) {
      x=fgetc(sam->fp)<<8; x|=fgetc(sam->fp);
      *d++=x^su;
    }
  } else if(w1 && w2) {
    unsigned short*d=dest;
    int x;
    while(count--) {
      x=fgetc(sam->fp); x|=fgetc(sam->fp)<<8;
      *d++=x^su;
    }
  } else if(w1 && sam->endian) {
    unsigned char*d=dest;
    while(count--) {
      *d++=fgetc(sam->fp)^su; fgetc(sam->fp);
    }
  } else if(w1) {
    unsigned char*d=dest;
    while(count--) {
      fgetc(sam->fp); *d++=fgetc(sam->fp)^su;
    }
  } else if(w2) {
    unsigned short*d=dest;
    int x;
    su^=(sam->bit_file<0)<<7;
    while(count--) {
      x=fgetc(sam->fp);
      *d++=(x|(x<<8))^su;
    }
  } else {
    unsigned char*d=dest;
    while(count--) *d++=fgetc(sam->fp)^su;
  }
}

void sam_header(SampleLoader*sam,int max,signed short*data) {
  int x;
  if(sam->header_size<max) max=sam->header_size;
  if(sam->head_start) {
    fseek(sam->fp,sam->head_start,SEEK_SET);
    for(x=0;x<max;x++) {
      data[x]=fgetc(sam->fp);
      data[x]|=fgetc(sam->fp)<<8;
    }
  } else {
    memcpy(data,macro_env[MC_Sample][sam->id].data,max*sizeof(signed short));
  }
}
