#include <Arduino.h>

/* ================= USB (MIDI + CDC) ================= */
#include "USB.h"
#include "USBMIDI.h"
#include "USBCDC.h"
USBMIDI MIDI;
USBCDC  CAT;

/* ================= DISPLAY ================= */
#include <Arduino_GFX_Library.h>
#define TFT_BL 1
Arduino_DataBus *bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
Arduino_GFX *gfx = new Arduino_NV3041A(bus, GFX_NOT_DEFINED, 0, true);

#define W 480
#define H 272

/* ================= TOUCH ================= */
#include <bb_captouch.h>
#include <Wire.h>
BBCapTouch touch;
#define TOUCH_SDA 8
#define TOUCH_SCL 4
#define TOUCH_INT 3
#define TOUCH_RST 38
#define TOUCH_CYD_543 1

/* ================= ENCODER VFO ================= */
#define ENC_VFO_A 17
#define ENC_VFO_B 18
volatile int encVfoPos = 0;
int lastEncVfoPos = 0;

/* ================= MIDI MAP ================= */
static const uint8_t MIDI_CH = 1;

// Encoder = wheel (relativo)
static const uint8_t CC_WHEEL = 15;     // valor 64±1

// STEP = botões NOTE (como as telas)
static const uint8_t NOTE_STEP_MINUS = 120;
static const uint8_t NOTE_STEP_PLUS  = 121;

/* ================= COLORS ================= */
#define BLACK      0x0000
#define WHITE      0xFFFF
#define GREY       0x7BEF
#define DARKGREY   0x4208
#define LIGHT_BLUE 0x7E3F

/* ================= TOP BAR NAV ================= */
#define TOPBAR_H   40
#define NAV_W      44
#define NAV_H      30
#define NAV_Y      6
#define NAV_PREV_X 6
#define NAV_NEXT_X (W - NAV_W - 6)
#define NAV_HIT_PAD 10

/* ================= COMMAND GRID (12 = 4x3) ================= */
#define CMD_COLS 4
#define CMD_ROWS 3
#define CMD_COUNT (CMD_COLS * CMD_ROWS)
#define CMD_GAP 10
int cmdW, cmdH, cmdStartX, cmdStartY;

/* ================= PAGES ================= */
#define PAGE_VFO   0
#define PAGE_MODES 1
#define NUM_PAGES  11
int currentPage = 0;

struct BtnDef { const char* label; uint8_t note; };
struct PageDef { const char* title; BtnDef btn[CMD_COUNT]; };

PageDef pages[NUM_PAGES] = {
  { "VFO",   { {"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0} } },

  { "MODES", { // CAT MDn; (aqui sem MIDI)
      {"LSB",0}, {"USB",0}, {"CW",0},  {"FM",0},
      {"AM",0},  {"FSK",0}, {"CWR",0}, {"FSKR",0},
      {"",0}, {"",0}, {"",0}, {"",0}
    }},

  { "MENUS", {
      {"Main Menu",111}, {"Memory Menu",112}, {"Mode Menu",113}, {"Noise Menu",114},
      {"Band Menu",115}, {"TX Menu",116},     {"RX Menu",117},   {"Display Menu",118},
      {"",0}, {"",0}, {"",0}, {"",0}
    }},

  { "FILTERS", { // 12 (redistribuído)
      {"AGC",21}, {"AGC Menu",22}, {"NB",23}, {"NR",24},
      {"ANF",25}, {"CW Peak",26},  {"APF",27},{"Squelch",28},
      {"Filter -",31}, {"Filter +",32}, {"Filter Cut",33}, {"Filter Def",34}
    }},

  { "FILTERS 2", { // sobra
      {"RX Flt Menu",35}, {"Binaural",36}, {"Notch",37}, {"Notch Auto",38},
      {"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0}
    }},

  { "BANDS 1", { // 12 (redistribuído)
      {"6m",41}, {"10m",42}, {"12m",43}, {"15m",44},
      {"17m",45},{"20m",46}, {"22m",47}, {"26m",48},
      {"30m",49},{"40m",50}, {"60m",51}, {"80m",53}
    }},

  { "BANDS 2", { // sobra
      {"160m",54},{"2m",55},{"70cm",56},{"70m",52},
      {"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0},{"",0}
    }},

  { "VFO/TUNE", {
      {"VFO Menu",81},{"VFO Step -",82},{"VFO Step +",83},{"Tune",84},
      {"Tune Full",85},{"Tune Mem",86},{"CTUN",87},{"Lock",88},
      {"",0},{"",0},{"",0},{"",0}
    }},

  { "RIT/XIT", {
      {"RIT Clear",71},{"RIT On/Off",72},{"RIT -",73},{"RIT +",74},
      {"XIT On/Off",75},{"RIT XIT",76},{"Split",77},{"A/B",78},
      {"",0},{"",0},{"",0},{"",0}
    }},

  { "RX/DIV", {
      {"RX Menu",91},{"RX1",92},{"RX2",93},{"DIV On/Off",94},
      {"DIV Menu",95},{"Swap RX",96},{"RF Gain",97},{"AF Gain",98},
      {"",0},{"",0},{"",0},{"",0}
    }},

  { "TX/AUDIO", {
      {"MOX",101},{"Compressor",102},{"VOX On/Off",103},{"CTUN",104},
      {"Mute",105},{"Mute RX1",106},{"Power",107},{"Mic Gain",108},
      {"",0},{"",0},{"",0},{"",0}
    }},
};

/* ================= VFO/CAT STATE ================= */
uint64_t vfoAHz=0, vfoBHz=0;
bool catAlive=false;
uint32_t lastCatRxMs=0;
const uint32_t CAT_TIMEOUT_MS = 1200;

int selectedVfo=0; // 0=A 1=B

// step local só pra tela
static const uint32_t stepTable[] = {1,10,50,100,250,500,1000,5000,10000,100000};
static const int STEP_N = (int)(sizeof(stepTable)/sizeof(stepTable[0]));
int stepIndex = 6;
uint32_t stepHz = 1000;

// modo via CAT MD;
char modeStr[8] = "LSB";
char lastModeStr[8] = "";

// dirty
uint64_t lastShownHz = 0;
uint32_t lastShownStep = 0;
int lastShownVfoSel = -1;
bool lastShownCatAlive = false;

/* ================= VFO BUTTONS (4) ================= */
static const int VFO_BTN_W = 90;
static const int VFO_BTN_H = 36;
static const int VFO_BTN_Y = 228;
static const int VFO_BTN_GAP = 10;
static const int VFO_BTN_ROW_W = (4*VFO_BTN_W + 3*VFO_BTN_GAP);
static const int VFO_BTN_X0 = (W - VFO_BTN_ROW_W)/2;

static const int VFO_BTN_STEP_MINUS_X = VFO_BTN_X0 + 0*(VFO_BTN_W+VFO_BTN_GAP);
static const int VFO_BTN_VFOSEL_X     = VFO_BTN_X0 + 1*(VFO_BTN_W+VFO_BTN_GAP);
static const int VFO_BTN_MODE_X       = VFO_BTN_X0 + 2*(VFO_BTN_W+VFO_BTN_GAP);
static const int VFO_BTN_STEP_PLUS_X  = VFO_BTN_X0 + 3*(VFO_BTN_W+VFO_BTN_GAP);

/* ================= Dynamic regions ================= */
static const int VFO_FREQ_X = 20;
static const int VFO_FREQ_Y = 138;
static const int VFO_FREQ_W = W - 40;
static const int VFO_FREQ_H = 60;

static const int VFO_STEP_Y = 205;
static const int VFO_STEP_W = W;
static const int VFO_STEP_H = 24;

/* ================= TOUCH (1 ação por toque) ================= */
bool touchLocked=false;
uint8_t noTouchStreak=0;
const uint8_t NO_TOUCH_REQUIRED=6;

// botões das telas (noteOn while pressed)
int activeCmdBtn=-1;

// botões especiais da tela VFO (step/mode/vfo)
enum SpecialPress { SP_NONE=0, SP_STEP_MINUS, SP_STEP_PLUS };
SpecialPress activeSpecial = SP_NONE;

// nav debounce
static uint32_t lastNavMs = 0;
static const uint32_t NAV_DEBOUNCE_MS = 250;

/* ================= CAT IO ================= */
uint32_t lastCatPollMs=0;
const uint32_t CAT_POLL_MS=200;
char catLine[64];
uint8_t catPos=0;

/* ================= Utils ================= */
static inline bool inRect(int x,int y,int rx,int ry,int rw,int rh){
  return x>=rx && x<=rx+rw && y>=ry && y<=ry+rh;
}
static inline bool isDigit(char c){ return (c>='0'&&c<='9'); }

String fmtHzDots(uint64_t hz){
  if(hz==0) return String("");
  char buf[32];
  sprintf(buf, "%llu", (unsigned long long)hz);
  String s(buf), out;
  int n=s.length();
  for(int i=0;i<n;i++){
    out+=s[i];
    int left=n-i-1;
    if(left>0 && (left%3)==0) out+='.';
  }
  return out;
}

void catSend(const char* cmd){ CAT.print(cmd); }

static inline void sendNoteOn(uint8_t note){
  if(note) MIDI.noteOn(note, 127, MIDI_CH);
}
static inline void sendNoteOff(uint8_t note){
  if(note) MIDI.noteOff(note, 0, MIDI_CH);
}

/* ================= Drawing ================= */
void drawTopBar(const char* title){
  gfx->fillRect(0,0,W,TOPBAR_H,BLACK);
  gfx->drawFastHLine(0, TOPBAR_H-1, W, GREY);

  gfx->fillRoundRect(NAV_PREV_X, NAV_Y, NAV_W, NAV_H, 8, GREY);
  gfx->fillRoundRect(NAV_NEXT_X, NAV_Y, NAV_W, NAV_H, 8, GREY);

  gfx->setTextColor(BLACK);
  gfx->setTextSize(3);
  gfx->setCursor(NAV_PREV_X+16, NAV_Y+4); gfx->print("<");
  gfx->setCursor(NAV_NEXT_X+16, NAV_Y+4); gfx->print(">");

  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  String t=title?String(title):"";
  int tw=t.length()*12;
  int cx=(W-tw)/2;
  if(cx < (NAV_PREV_X+NAV_W+8)) cx = (NAV_PREV_X+NAV_W+8);
  if(cx+tw > (NAV_NEXT_X-8)) cx = (NAV_NEXT_X-8)-tw;
  if(cx<0) cx=0;
  gfx->setCursor(cx,12);
  gfx->print(t);
}

void computeGrid(){
  int usableW=W-(CMD_GAP*2);
  cmdW=(usableW-(CMD_GAP*(CMD_COLS-1)))/CMD_COLS;
  int usableH=H-TOPBAR_H-(CMD_GAP*2);
  cmdH=(usableH-(CMD_GAP*(CMD_ROWS-1)))/CMD_ROWS;
  cmdStartX=CMD_GAP;
  cmdStartY=TOPBAR_H+CMD_GAP;
}

void drawCmdButton(int idx, bool pressed){
  int col=idx%CMD_COLS, row=idx/CMD_COLS;
  int x=cmdStartX + col*(cmdW+CMD_GAP);
  int y=cmdStartY + row*(cmdH+CMD_GAP);

  uint16_t fill = pressed?DARKGREY:GREY;
  gfx->fillRoundRect(x,y,cmdW,cmdH,10,fill);
  gfx->drawRoundRect(x,y,cmdW,cmdH,10,BLACK);

  String label=pages[currentPage].btn[idx].label;
  label.trim();
  if(!label.length()) return;

  // quebra linha por espaço
  String words[12];
  int n=0, start=0;
  for(int i=0;i<=label.length();i++){
    if(i==label.length() || label.charAt(i)==' '){
      if(i>start && n<12) words[n++]=label.substring(start,i);
      start=i+1;
    }
  }

  gfx->setTextColor(BLACK);
  gfx->setTextSize(2);

  int lineH=18;
  int totalH=n*lineH;
  int y0=y + (cmdH-totalH)/2;

  for(int i=0;i<n;i++){
    int tw=words[i].length()*12;
    gfx->setCursor(x+(cmdW-tw)/2, y0+i*lineH);
    gfx->print(words[i]);
  }
}

int getTouchedCmdButton(int x,int y){
  if(currentPage==PAGE_VFO) return -1;
  for(int i=0;i<CMD_COUNT;i++){
    int col=i%CMD_COLS, row=i/CMD_COLS;
    int bx=cmdStartX+col*(cmdW+CMD_GAP);
    int by=cmdStartY+row*(cmdH+CMD_GAP);
    if(inRect(x,y,bx,by,cmdW,cmdH)) return i;
  }
  return -1;
}

/* ================= VFO UI ================= */
void drawVfoButtonsStatic(){
  gfx->fillRoundRect(VFO_BTN_STEP_MINUS_X, VFO_BTN_Y, VFO_BTN_W, VFO_BTN_H, 8, GREY);
  gfx->fillRoundRect(VFO_BTN_VFOSEL_X,     VFO_BTN_Y, VFO_BTN_W, VFO_BTN_H, 8, GREY);
  gfx->fillRoundRect(VFO_BTN_MODE_X,       VFO_BTN_Y, VFO_BTN_W, VFO_BTN_H, 8, GREY);
  gfx->fillRoundRect(VFO_BTN_STEP_PLUS_X,  VFO_BTN_Y, VFO_BTN_W, VFO_BTN_H, 8, GREY);

  gfx->setTextColor(BLACK);
  gfx->setTextSize(2);

  gfx->setCursor(VFO_BTN_STEP_MINUS_X+18, VFO_BTN_Y+6);  gfx->print("STEP");
  gfx->setCursor(VFO_BTN_STEP_MINUS_X+34, VFO_BTN_Y+22); gfx->print("-");

  gfx->setCursor(VFO_BTN_VFOSEL_X+24, VFO_BTN_Y+6); gfx->print("VFO");

  gfx->setCursor(VFO_BTN_MODE_X+18, VFO_BTN_Y+6); gfx->print("MOD");

  gfx->setCursor(VFO_BTN_STEP_PLUS_X+18,  VFO_BTN_Y+6);  gfx->print("STEP");
  gfx->setCursor(VFO_BTN_STEP_PLUS_X+34,  VFO_BTN_Y+22); gfx->print("+");
}

void vfoDrawVfoSelButton(int sel){
  gfx->fillRect(VFO_BTN_VFOSEL_X+20, VFO_BTN_Y+20, VFO_BTN_W-40, 16, GREY);
  gfx->setTextColor(BLACK);
  gfx->setTextSize(2);
  gfx->setCursor(VFO_BTN_VFOSEL_X+40, VFO_BTN_Y+22);
  gfx->print(sel==0?"A":"B");
}

void vfoDrawModeButton(){
  gfx->fillRect(VFO_BTN_MODE_X+10, VFO_BTN_Y+20, VFO_BTN_W-20, 16, GREY);
  gfx->setTextColor(BLACK);
  gfx->setTextSize(2);

  String m = String(modeStr);
  int tw = m.length()*12;
  int x = VFO_BTN_MODE_X + (VFO_BTN_W - tw)/2;
  if(x < VFO_BTN_MODE_X+10) x = VFO_BTN_MODE_X+10;
  gfx->setCursor(x, VFO_BTN_Y+22);
  gfx->print(m);
}

void vfoDrawFreqArea(const String& text, bool alive){
  gfx->fillRect(VFO_FREQ_X, VFO_FREQ_Y, VFO_FREQ_W, VFO_FREQ_H, BLACK);
  if(!alive || !text.length()) return;
  gfx->setTextColor(LIGHT_BLUE);
  gfx->setTextSize(5);
  gfx->setCursor(VFO_FREQ_X, VFO_FREQ_Y);
  gfx->print(text);
}

void vfoDrawStepArea(uint32_t step){
  gfx->fillRect(0, VFO_STEP_Y, VFO_STEP_W, VFO_STEP_H, BLACK);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  String st="STEP "+String(step)+"Hz";
  gfx->setCursor((W - (int)st.length()*12)/2, VFO_STEP_Y);
  gfx->print(st);
}

void drawVfoStatic(){
  gfx->fillScreen(BLACK);
  drawTopBar("VFO");

  gfx->drawRect(10, TOPBAR_H + 8, W-20, 70, WHITE);
  int baseY = TOPBAR_H + 8 + 70;
  for(int i=0;i<W-40;i+=6){
    int hh=(i%30)?8:22;
    gfx->drawFastVLine(20+i, baseY - hh - 4, hh, WHITE);
  }

  drawVfoButtonsStatic();

  lastShownHz = 0;
  lastShownStep = 0;
  lastShownVfoSel = -1;
  lastShownCatAlive = !catAlive;
  strcpy(lastModeStr, "");
}

void vfoUpdateDynamic(){
  bool alive = catAlive && (millis() - lastCatRxMs) <= CAT_TIMEOUT_MS;
  uint64_t hz = (selectedVfo==0)?vfoAHz:vfoBHz;

  if(strcmp(modeStr, lastModeStr)!=0){
    vfoDrawModeButton();
    strcpy(lastModeStr, modeStr);
  }

  if(alive != lastShownCatAlive || hz != lastShownHz){
    String f = alive ? fmtHzDots(hz) : String("");
    vfoDrawFreqArea(f, alive);
    lastShownHz = hz;
    lastShownCatAlive = alive;
  }

  if(stepHz != lastShownStep){
    vfoDrawStepArea(stepHz);
    lastShownStep = stepHz;
  }

  if(selectedVfo != lastShownVfoSel){
    vfoDrawVfoSelButton(selectedVfo);
    lastShownVfoSel = selectedVfo;
  }
}

/* ================= MODE via CAT ================= */
void catSetModeByLabel(const char* label){
  char cmd[8] = "MD1;";
  if(!strcmp(label,"LSB")) strcpy(cmd,"MD1;");
  else if(!strcmp(label,"USB")) strcpy(cmd,"MD2;");
  else if(!strcmp(label,"CW"))  strcpy(cmd,"MD3;");
  else if(!strcmp(label,"FM"))  strcpy(cmd,"MD4;");
  else if(!strcmp(label,"AM"))  strcpy(cmd,"MD5;");
  else if(!strcmp(label,"FSK")) strcpy(cmd,"MD6;");
  else if(!strcmp(label,"CWR")) strcpy(cmd,"MD7;");
  else if(!strcmp(label,"FSKR"))strcpy(cmd,"MD9;");

  catSend(cmd);
  strncpy(modeStr, label, sizeof(modeStr)-1);
  modeStr[sizeof(modeStr)-1]=0;
}

/* ================= CAT parse/poll ================= */
bool parseFAFB(const char* s){
  if(!(s[0]=='F' && (s[1]=='A' || s[1]=='B'))) return false;
  uint64_t hz=0; int i=2, digits=0;
  while(s[i] && s[i]!=';'){
    if(!isDigit(s[i])) return false;
    hz = hz*10ULL + (uint64_t)(s[i]-'0');
    digits++; i++;
  }
  if(digits<4) return false;
  if(s[1]=='A') vfoAHz=hz; else vfoBHz=hz;
  catAlive=true; lastCatRxMs=millis();
  return true;
}
bool parseMD(const char* s){
  if(!(s[0]=='M' && s[1]=='D')) return false;
  char m=s[2];
  if(!isDigit(m)) return false;

  switch(m){
    case '1': strcpy(modeStr,"LSB"); break;
    case '2': strcpy(modeStr,"USB"); break;
    case '3': strcpy(modeStr,"CW");  break;
    case '4': strcpy(modeStr,"FM");  break;
    case '5': strcpy(modeStr,"AM");  break;
    case '6': strcpy(modeStr,"FSK"); break;
    case '7': strcpy(modeStr,"CWR"); break;
    case '9': strcpy(modeStr,"FSKR");break;
    default:  strcpy(modeStr,"");    break;
  }
  catAlive=true; lastCatRxMs=millis();
  return true;
}

void catPoll(){
  uint32_t now=millis();
  if(now-lastCatPollMs < CAT_POLL_MS) return;
  lastCatPollMs=now;
  catSend(selectedVfo==0 ? "FA;" : "FB;");
  catSend("MD;");
}

void catRead(){
  while(CAT.available()){
    char c=(char)CAT.read();
    if(c=='\r'||c=='\n') continue;

    if(catPos >= sizeof(catLine)-1) catPos=0;
    catLine[catPos++]=c;
    catLine[catPos]=0;

    if(c!=';') continue;
    const char* s=catLine;

    parseFAFB(s);
    parseMD(s);

    catPos=0;
  }

  if(catAlive && (millis()-lastCatRxMs)>CAT_TIMEOUT_MS){
    catAlive=false;
  }

  if(currentPage==PAGE_VFO) vfoUpdateDynamic();
}

/* ================= Encoder ================= */
void IRAM_ATTR encVfoISR(){ encVfoPos += digitalRead(ENC_VFO_B) ? -1 : 1; }
void handleEncoder(){
  if(encVfoPos != lastEncVfoPos){
    int dir=(encVfoPos>lastEncVfoPos)?1:-1;
    MIDI.controlChange(CC_WHEEL, 64+dir, MIDI_CH); // wheel só aqui
    lastEncVfoPos=encVfoPos;
  }
}

/* ================= STEP local + NOTE like other buttons ================= */
void stepMinusPress(){
  if(stepIndex>0) stepIndex--;
  stepHz = stepTable[stepIndex];
  vfoUpdateDynamic();
  sendNoteOn(NOTE_STEP_MINUS);
}
void stepPlusPress(){
  if(stepIndex<STEP_N-1) stepIndex++;
  stepHz = stepTable[stepIndex];
  vfoUpdateDynamic();
  sendNoteOn(NOTE_STEP_PLUS);
}
void stepMinusRelease(){ sendNoteOff(NOTE_STEP_MINUS); }
void stepPlusRelease(){  sendNoteOff(NOTE_STEP_PLUS);  }

/* ================= Touch ================= */
void drawButtonsPageStatic(){
  gfx->fillScreen(BLACK);
  drawTopBar(pages[currentPage].title);
  computeGrid();
  for(int i=0;i<CMD_COUNT;i++) drawCmdButton(i,false);
}

void releaseActive(){
  // solta botões das telas
  if(activeCmdBtn >= 0){
    uint8_t n = pages[currentPage].btn[activeCmdBtn].note;
    sendNoteOff(n);
    drawCmdButton(activeCmdBtn, false);
    activeCmdBtn = -1;
  }
  // solta especiais do VFO
  if(activeSpecial == SP_STEP_MINUS) stepMinusRelease();
  if(activeSpecial == SP_STEP_PLUS)  stepPlusRelease();
  activeSpecial = SP_NONE;
}

void handleTouch(){
  TOUCHINFO ti;
  bool has = touch.getSamples(&ti) && ti.count > 0;

  if(!has){
    if(noTouchStreak < 255) noTouchStreak++;
    if(noTouchStreak >= NO_TOUCH_REQUIRED){
      if(touchLocked){
        touchLocked = false;
        releaseActive();
      }
    }
    return;
  }

  noTouchStreak = 0;
  if(touchLocked) return;      // 1 ação por toque
  touchLocked = true;

  int x = ti.x[0];
  int y = ti.y[0];
  if(x < 0 || x >= W || y < 0 || y >= H) return;

  // NAV
  uint32_t now = millis();
  bool prevHit = inRect(x,y, NAV_PREV_X - NAV_HIT_PAD, NAV_Y - NAV_HIT_PAD,
                        NAV_W + 2*NAV_HIT_PAD, NAV_H + 2*NAV_HIT_PAD);
  bool nextHit = inRect(x,y, NAV_NEXT_X - NAV_HIT_PAD, NAV_Y - NAV_HIT_PAD,
                        NAV_W + 2*NAV_HIT_PAD, NAV_H + 2*NAV_HIT_PAD);
  if(prevHit || nextHit){
    if(now - lastNavMs < NAV_DEBOUNCE_MS) return;
    lastNavMs = now;

    currentPage = prevHit
      ? (currentPage - 1 + NUM_PAGES) % NUM_PAGES
      : (currentPage + 1) % NUM_PAGES;

    // desenha página
    if(currentPage==PAGE_VFO){
      drawVfoStatic();
      vfoUpdateDynamic();
    } else {
      drawButtonsPageStatic();
    }
    return;
  }

  // VFO page
  if(currentPage==PAGE_VFO){
    if(inRect(x,y,VFO_BTN_STEP_MINUS_X,VFO_BTN_Y,VFO_BTN_W,VFO_BTN_H)){
      activeSpecial = SP_STEP_MINUS;
      // efeito visual
      gfx->fillRoundRect(VFO_BTN_STEP_MINUS_X, VFO_BTN_Y, VFO_BTN_W, VFO_BTN_H, 8, DARKGREY);
      gfx->setTextColor(BLACK); gfx->setTextSize(2);
      gfx->setCursor(VFO_BTN_STEP_MINUS_X+18, VFO_BTN_Y+6);  gfx->print("STEP");
      gfx->setCursor(VFO_BTN_STEP_MINUS_X+34, VFO_BTN_Y+22); gfx->print("-");
      stepMinusPress();
      return;
    }
    if(inRect(x,y,VFO_BTN_STEP_PLUS_X, VFO_BTN_Y,VFO_BTN_W,VFO_BTN_H)){
      activeSpecial = SP_STEP_PLUS;
      gfx->fillRoundRect(VFO_BTN_STEP_PLUS_X, VFO_BTN_Y, VFO_BTN_W, VFO_BTN_H, 8, DARKGREY);
      gfx->setTextColor(BLACK); gfx->setTextSize(2);
      gfx->setCursor(VFO_BTN_STEP_PLUS_X+18,  VFO_BTN_Y+6);  gfx->print("STEP");
      gfx->setCursor(VFO_BTN_STEP_PLUS_X+34,  VFO_BTN_Y+22); gfx->print("+");
      stepPlusPress();
      return;
    }

    if(inRect(x,y,VFO_BTN_VFOSEL_X,VFO_BTN_Y,VFO_BTN_W,VFO_BTN_H)){
      selectedVfo = (selectedVfo==0)?1:0;
      catSend(selectedVfo==0 ? "FR0;" : "FR1;");
      vfoUpdateDynamic();
      return;
    }

    if(inRect(x,y,VFO_BTN_MODE_X,VFO_BTN_Y,VFO_BTN_W,VFO_BTN_H)){
      currentPage = PAGE_MODES;
      drawButtonsPageStatic();
      return;
    }
    return;
  }

  // MODES page (CAT)
  if(currentPage==PAGE_MODES){
    int b = getTouchedCmdButton(x,y);
    if(b>=0){
      drawCmdButton(b,true);
      const char* lbl = pages[PAGE_MODES].btn[b].label;
      catSetModeByLabel(lbl);
      currentPage = PAGE_VFO;
      drawVfoStatic();
      vfoUpdateDynamic();
    }
    return;
  }

  // demais páginas: NOTE igual antes
  int b = getTouchedCmdButton(x,y);
  if(b>=0){
    const char* lbl = pages[currentPage].btn[b].label;
    uint8_t n = pages[currentPage].btn[b].note;
    if(lbl && lbl[0] && n){
      activeCmdBtn = b;
      drawCmdButton(b,true);
      sendNoteOn(n);
    }
  }
}

/* ================= Setup / Loop ================= */
void setup(){
  MIDI.begin();
  CAT.begin(115200);
  USB.begin();

  gfx->begin();
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  touch.init(TOUCH_CYD_543, TOUCH_RST, TOUCH_INT);

  pinMode(ENC_VFO_A, INPUT_PULLUP);
  pinMode(ENC_VFO_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_VFO_A), encVfoISR, CHANGE);

  stepHz = stepTable[stepIndex];

  drawVfoStatic();
  vfoUpdateDynamic();
}

void loop(){
  catPoll();
  catRead();

  handleTouch();
  handleEncoder();

  delay(5);
}
