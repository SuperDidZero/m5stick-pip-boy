/*
  SUPER's Miniature Pip-Boy  -  M5StickS3
  --------------------------------------------------------------
  Tabs:  STAT (you) | COMMS (LinkedIn) | BASE (website) | RADIO (Instagram) | RAD (Geiger)
  Front button (BtnA) = next tab      Side button (BtnB) = previous tab
  Shake = mute / unmute sound
  Battery drives the figure: >50% healthy (walks, winks, thumbs up),
  20-50% tired, under 20% knocked out on the floor.

  Needs: M5Unified library only. The QR codes are drawn by M5GFX itself,
  so no separate QR library is required.
  The figure art lives in sprites.h (same folder as this file).

  --- To reuse this sketch, swap these three links for your own ---
*/
#include <M5Unified.h>
#include <WiFi.h>
#include "sprites.h"

// >>> your public links (safe to share) <<<
const char* URL_INSTAGRAM = "https://instagram.com/YOUR_HANDLE";                 // RADIO tab
const char* URL_LINKEDIN  = "https://linkedin.com/in/YOUR_LINKEDIN"; // COMMS tab
const char* URL_WEBSITE   = "https://YOUR-WEBSITE.example";                           // BASE tab
const char* HANDLE        = "@YOUR_HANDLE";

// ---------- colours (green CRT) ----------
#define C_BG   0x0160   // dark green-black
#define C_DIM  0x3DC9   // dim green
#define C_GRN  0x9FEB   // bright phosphor green
uint16_t KEY = SPR_KEY; // sprite transparent colour

M5Canvas canvas(&M5.Display);

// ---------- tabs ----------
enum { T_STAT, T_COMMS, T_BASE, T_RADIO, T_RAD, T_COUNT };
const char* TAB_NAME[T_COUNT] = { "STAT", "COMMS", "BASE", "RADIO", "RAD" };
int tab = T_STAT;

// ---------- sound ----------
bool muted = false;
struct Note { uint16_t f, d; };
// RADIO = "Stars and Stripes Forever" (Sousa, public domain, real Fallout Enclave Radio track)
const Note M_STARS[] = {{392,180},{523,360},{659,180},{523,180},{392,360},{440,180},{698,360},{659,180},{587,180},{523,480}};
// COMMS = Bach-style prelude arpeggio (Agatha's Station, public domain), simplified for the buzzer
const Note M_BACH[]  = {{262,150},{330,150},{392,150},{523,150},{659,150},{392,150},{523,150},{659,150},{262,150},{349,150},{440,150},{523,300}};
// BASE  = second public-domain classical phrase, simplified
const Note M_BASE[]  = {{330,200},{392,200},{523,300},{494,200},{440,200},{392,300},{330,200},{392,200},{523,420}};
const Note* mel = nullptr; int melLen = 0, melIdx = 0; uint32_t melNext = 0;

void startMelody(const Note* m, int n){ mel=m; melLen=n; melIdx=0; melNext=millis(); }
void stopMelody(){ mel=nullptr; }
void updateMelody(){
  if(!mel) return;
  if(melIdx>=melLen){ mel=nullptr; return; }
  if(millis()>=melNext){
    if(!muted) M5.Speaker.tone(mel[melIdx].f, mel[melIdx].d);
    melNext = millis() + mel[melIdx].d + 25;
    melIdx++;
  }
}
void beep(uint16_t f, uint16_t d){ if(!muted) M5.Speaker.tone(f,d); }

// ---------- geiger ----------
int   gLevel = 0;            // 0..12 signal density
uint32_t gNextClick = 0, gNextScan = 0;
bool  scanning = false;

void updateGeiger(){
  if(tab != T_RAD) return;
  // kick off an async wifi scan every ~3s
  if(!scanning && millis() > gNextScan){
    WiFi.scanNetworks(true, true);   // async, show hidden
    scanning = true;
  }
  int res = WiFi.scanComplete();
  if(scanning && res >= 0){
    int lvl = res; if(lvl>12) lvl=12;
    gLevel = lvl;
    WiFi.scanDelete();
    scanning = false;
    gNextScan = millis() + 3000;
  }
  // click rate from level
  if(millis() > gNextClick){
    int interval = map(gLevel, 0, 12, 900, 90);
    gNextClick = millis() + interval;
    beep(1600, 6);
  }
}

// ---------- shake to mute ----------
float lastMag = 1.0; uint32_t shakeLock = 0;
void checkShake(){
  float ax, ay, az;
  if(M5.Imu.getAccel(&ax, &ay, &az)){
    float mag = sqrtf(ax*ax + ay*ay + az*az);
    if(fabs(mag - lastMag) > 1.2 && millis() > shakeLock){
      muted = !muted;
      shakeLock = millis() + 800;
      if(!muted) M5.Speaker.tone(880, 60);
    }
    lastMag = mag;
  }
}

// ---------- drawing ----------
void drawTabs(){
  canvas.setTextSize(1);
  canvas.setTextDatum(top_left);
  int gap = 2, total = 0, w[T_COUNT];
  for(int i=0;i<T_COUNT;i++){ w[i] = canvas.textWidth(TAB_NAME[i]); total += w[i]; }
  total += gap*(T_COUNT-1);
  int x = (135 - total)/2; if(x < 0) x = 0;
  for(int i=0;i<T_COUNT;i++){
    if(i==tab){ canvas.fillRect(x-1, 3, w[i]+1, 13, C_GRN); canvas.setTextColor(C_BG); }
    else canvas.setTextColor(C_GRN);
    canvas.drawString(TAB_NAME[i], x, 5);
    x += w[i];
    if(i < T_COUNT-1){ canvas.drawFastVLine(x + gap/2, 4, 11, C_DIM); x += gap; }
  }
  canvas.drawFastHLine(0, 20, 135, C_GRN);     // divider, moved down a touch
}

void drawHP(){
  int lvl = M5.Power.getBatteryLevel();
  if(lvl < 0) lvl = 100;
  canvas.drawFastHLine(0, 218, 135, C_GRN);
  canvas.setTextColor(C_GRN); canvas.setTextDatum(top_left); canvas.setTextSize(1);
  canvas.drawString("HP", 5, 223);
  canvas.drawRect(24, 222, 72, 10, C_GRN);
  int w = (70 * lvl) / 100;
  canvas.fillRect(25, 223, w, 8, C_GRN);
  canvas.setTextDatum(top_right);
  canvas.drawString(String(lvl) + "%", 132, 223);
  canvas.setTextDatum(middle_center); canvas.setTextColor(C_DIM);
  if(muted) canvas.drawString("[muted]", 67, 213);
}

// healthy animation timing
uint32_t beatStart = 0; bool thumbBeat = false;

void drawFigure(){
  int lvl = M5.Power.getBatteryLevel(); if(lvl < 0) lvl = 100;
  int bob = (int)(sinf(millis()/300.0) * 3);   // gentle up/down
  canvas.setTextColor(C_GRN); canvas.setTextDatum(top_center); canvas.setTextSize(1);
  canvas.drawString(HANDLE, 67, 24);           // name right under the tab line
  if(lvl > 50){
    // every 4s, pause the walk for a thumbs up + wink (~1.3s)
    if(millis() - beatStart > 4000){ thumbBeat = true; beatStart = millis(); }
    if(thumbBeat && millis() - beatStart > 1300) thumbBeat = false;
    if(thumbBeat){
      canvas.pushImage((135-THUMB_W)/2, 46+bob, THUMB_W, THUMB_H, SPR_THUMB, KEY);
    } else {                                   // 2-frame walk cycle, mirror = opposite step
      bool leg = (millis()/260) % 2;
      if(leg) canvas.pushImage((135-WALK_W)/2,  46+bob, WALK_W,  WALK_H,  SPR_WALK,  KEY);
      else    canvas.pushImage((135-WALKF_W)/2, 46+bob, WALKF_W, WALKF_H, SPR_WALKF, KEY);
    }
  } else if(lvl >= 20){
    int sway = (int)(sinf(millis()/450.0) * 2);
    canvas.pushImage((135-TIRED_W)/2+sway, 48+bob, TIRED_W, TIRED_H, SPR_TIRED, KEY);
  } else {
    int breathe = (int)(sinf(millis()/700.0) * 2);
    canvas.pushImage((135-DOWN_W)/2, 86+breathe, DOWN_W, DOWN_H, SPR_DOWN, KEY);
  }
}

void drawQR(const char* url, const char* title, const char* sub){
  canvas.setTextColor(C_GRN); canvas.setTextDatum(top_center); canvas.setTextSize(1);
  canvas.drawString(title, 67, 23);
  int dim = 120;                               // bigger squares, easier to scan
  int ox = (135 - dim)/2, oy = 50;             // nudged down a bit, no white border
  canvas.qrcode(url, ox, oy, dim, 5);
  canvas.setTextColor(C_GRN); canvas.setTextDatum(top_center);
  canvas.drawString(sub, 67, oy + dim + 6);
}

void drawGeiger(){
  canvas.setTextColor(C_GRN); canvas.setTextDatum(top_center); canvas.setTextSize(1);
  canvas.drawString("RAD COUNTER", 67, 24);
  int cx=67, cy=120, r=46;
  for(int a=180;a<=360;a+=10){
    float ra=a*PI/180.0;
    canvas.drawLine(cx+cos(ra)*(r-4), cy+sin(ra)*(r-4), cx+cos(ra)*r, cy+sin(ra)*r, C_DIM);
  }
  float na = (180 + (gLevel*180.0/12.0)) * PI/180.0;   // needle
  canvas.drawLine(cx, cy, cx+cos(na)*(r-8), cy+sin(na)*(r-8), C_GRN);
  canvas.fillCircle(cx, cy, 3, C_GRN);
  canvas.setTextColor(C_DIM);
  canvas.drawString("WIFI SIGNAL", 67, 128);
  canvas.drawString(String(gLevel*8) + " cps", 67, 140);
  // live click bars
  for(int i=0;i<8;i++){
    int h = 3 + (gLevel? random(2, gLevel+3) : 1);
    canvas.fillRect(24 + i*12, 175 - h, 6, h, C_GRN);
  }
}

void render(){
  canvas.fillSprite(C_BG);
  drawTabs();
  switch(tab){
    case T_STAT:  drawFigure(); break;
    case T_COMMS: drawQR(URL_LINKEDIN,  "COMMS",  "LinkedIn"); break;
    case T_BASE:  drawQR(URL_WEBSITE,   "BASE",   "PC Build PT"); break;
    case T_RADIO: drawQR(URL_INSTAGRAM, "RADIO",  HANDLE); break;
    case T_RAD:   drawGeiger(); break;
  }
  drawHP();
  canvas.pushSprite(0, 0);
}

void onEnterTab(){
  stopMelody();
  if(tab==T_COMMS) startMelody(M_BACH,  sizeof(M_BACH)/sizeof(Note));
  if(tab==T_BASE)  startMelody(M_BASE,  sizeof(M_BASE)/sizeof(Note));
  if(tab==T_RADIO) startMelody(M_STARS, sizeof(M_STARS)/sizeof(Note));
  if(tab==T_RAD){ gNextScan=0; gLevel=0; }
}

void setup(){
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(0);        // GPIO pins up, big button at the bottom. If upside down, try 2.
  M5.Speaker.begin();
  M5.Speaker.setVolume(140);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();                // station mode, used only to scan on the RAD tab
  canvas.setColorDepth(16);
  canvas.createSprite(135, 240);
  randomSeed(esp_random());
  beep(523, 60); beep(784, 80);     // soft boot blips
  onEnterTab();
}

void loop(){
  M5.update();
  if(M5.BtnA.wasPressed()){ tab = (tab + 1) % T_COUNT; beep(1046, 30); onEnterTab(); }
  if(M5.BtnB.wasPressed()){ tab = (tab - 1 + T_COUNT) % T_COUNT; beep(880, 30); onEnterTab(); }
  checkShake();
  updateMelody();
  updateGeiger();
  render();
  delay(16);
}
