/*
  Miniature Pip-Boy (by SuperDidZero)  -  M5StickS3
  --------------------------------------------------------------
  Tabs:  STAT (you) | COMMS (LinkedIn) | BASE (website) | RADIO (Instagram) | RAD (Geiger)
  Front button (BtnA) = next tab      Side button (BtnB) = previous tab
  Shake = full-screen Pip-Boy clock (shake again to go back)
  Battery drives the figure: >50% healthy (walks, winks, thumbs up),
  20-50% tired, under 20% knocked out on the floor.

  Needs: M5Unified library only. The QR codes are drawn by M5GFX itself,
  so no separate QR library is required.
  The figure art lives in sprites.h (same folder as this file).

  --- To reuse this sketch, swap these three links for your own ---
*/
#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>
#include "sprites.h"

// >>> your public links (safe to share) <<<
const char* URL_INSTAGRAM = "https://instagram.com/YOUR_HANDLE";                  // RADIO tab
const char* URL_LINKEDIN  = "https://linkedin.com/in/YOUR_LINKEDIN";               // COMMS tab
const char* URL_WEBSITE   = "https://YOUR-WEBSITE.example";                        // BASE tab
const char* HANDLE        = "@YOUR_HANDLE";

// Wi-Fi is used once at boot only, to fetch the clock time (NTP). GitHub copy keeps placeholders.
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASS     = "YOUR_WIFI_PASSWORD";

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
bool bootWifiOk = false;          // did Wi-Fi connect at boot (for the clock)
bool timeScreen = false;          // shake toggles the full Pip-Boy clock screen
bool timeSynced = false;          // has NTP set the clock yet
bool wifiGaveUp = false;          // stopped trying NTP (so the radio can't drain the battery)

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
int   gLevel = 0, gCount = 0;   // gLevel 0..12 from strongest signal; gCount = networks seen
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
    int best = -100;
    for(int i=0;i<res;i++){ int r = WiFi.RSSI(i); if(r > best) best = r; }
    // strongest signal: ~ -90 dBm (far/weak) up to ~ -35 dBm (right next to a router)
    gLevel = (res > 0) ? map(constrain(best, -90, -35), -90, -35, 0, 12) : 0;
    gCount = res;
    WiFi.scanDelete();
    scanning = false;
    gNextScan = millis() + 2500;
  }
  // click rate follows the signal strength
  if(millis() > gNextClick){
    int interval = map(gLevel, 0, 12, 900, 70);
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
    if(fabs(mag - lastMag) > 0.7 && millis() > shakeLock){
      timeScreen = !timeScreen;              // shake flips the full Pip-Boy clock screen
      shakeLock = millis() + 700;
      beep(timeScreen ? 1200 : 700, 45);
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

// healthy idle: random poses
uint32_t beatStart = 0; bool thumbBeat = false;
int idleState = 0; uint32_t idleUntil = 0;

void drawFigure(){
  int lvl = M5.Power.getBatteryLevel(); if(lvl < 0) lvl = 100;
  int bob = (int)(sinf(millis()/300.0) * 3);   // gentle up/down
  canvas.setTextColor(C_GRN); canvas.setTextDatum(top_center); canvas.setTextSize(1);
  canvas.drawString(HANDLE, 67, 22);
  struct tm tnow;                              // boot clock (details live on the shake screen)
  canvas.setTextDatum(top_center); canvas.setTextSize(2);
  if(getLocalTime(&tnow, 0)){
    char tb[6]; sprintf(tb, "%02d:%02d", tnow.tm_hour, tnow.tm_min);
    canvas.setTextColor(C_GRN); canvas.drawString(tb, 67, 30);
  } else {
    canvas.setTextColor(C_DIM); canvas.drawString("--:--", 67, 30);
  }
  canvas.setTextSize(1); canvas.setTextColor(C_GRN);
  if(lvl > 50){
    // random idle: walk, stop, thumbs-up/wink, wave
    if(millis() > idleUntil){
      int r = random(100);
      if(r < 45)      { idleState = 0; idleUntil = millis() + random(1800, 4000); } // walk
      else if(r < 65) { idleState = 1; idleUntil = millis() + random(1200, 2600); } // stand
      else if(r < 80) { idleState = 2; idleUntil = millis() + random(1100, 1800); } // thumbs + wink
      else            { idleState = 3; idleUntil = millis() + random(1200, 2200); } // wave
    }
    if(idleState == 0){                        // walking, mirror = opposite step
      bool leg = (millis()/260) % 2;
      const uint16_t* s = leg ? SPR_WALK : SPR_WALKF;
      int16_t sw = leg ? WALK_W : WALKF_W, sh = leg ? WALK_H : WALKF_H;
      canvas.pushImage((135-sw)/2, 52+bob, sw, sh, s, KEY);
    } else if(idleState == 2){
      canvas.pushImage((135-THUMB_W)/2, 52+bob, THUMB_W, THUMB_H, SPR_THUMB, KEY);
    } else if(idleState == 3){
      canvas.pushImage((135-WAVE_W)/2, 52+bob, WAVE_W, WAVE_H, SPR_WAVE, KEY);
    } else {
      canvas.pushImage((135-HEALTHY_W)/2, 52+bob, HEALTHY_W, HEALTHY_H, SPR_HEALTHY, KEY);
    }
  } else if(lvl >= 20){
    int sway = (int)(sinf(millis()/450.0) * 2);
    canvas.pushImage((135-TIRED_W)/2+sway, 54+bob, TIRED_W, TIRED_H, SPR_TIRED, KEY);
  } else {
    int breathe = (int)(sinf(millis()/700.0) * 2);
    canvas.pushImage((135-DOWN_W)/2, 90+breathe, DOWN_W, DOWN_H, SPR_DOWN, KEY);
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
  canvas.drawString(String(gCount) + " nets", 67, 150);
  // live click bars
  for(int i=0;i<8;i++){
    int h = 3 + (gLevel? random(2, gLevel+3) : 1);
    canvas.fillRect(24 + i*12, 175 - h, 6, h, C_GRN);
  }
}

void drawTimeScreen(){
  struct tm t; bool ok = getLocalTime(&t, 0);
  canvas.drawRect(0, 0, 135, 240, C_GRN);
  canvas.setTextColor(C_GRN); canvas.setTextDatum(top_center); canvas.setTextSize(1);
  canvas.drawString("PIP-BOY CLOCK", 67, 10);
  canvas.drawFastHLine(10, 22, 115, C_DIM);
  char hhmm[6]; if(ok) sprintf(hhmm, "%02d:%02d", t.tm_hour, t.tm_min); else strcpy(hhmm, "--:--");
  canvas.setTextDatum(middle_center); canvas.setTextSize(4);
  canvas.drawString(hhmm, 67, 72);
  if(ok){ char ss[5]; sprintf(ss, ":%02d", t.tm_sec); canvas.setTextSize(2); canvas.drawString(ss, 67, 104); }
  canvas.setTextDatum(top_center); canvas.setTextSize(1);
  const char* days[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  const char* mons[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  if(ok){ char dl[28]; sprintf(dl, "%s  %02d %s %04d", days[t.tm_wday], t.tm_mday, mons[t.tm_mon], 1900+t.tm_year); canvas.drawString(dl, 67, 130); }
  else  { canvas.setTextColor(C_DIM); canvas.drawString(bootWifiOk ? "no time signal" : "no wifi at boot", 67, 130); canvas.setTextColor(C_GRN); }
  canvas.setTextColor(C_DIM);
  char rad[22]; sprintf(rad, "RADS   %d cps", gLevel*8); canvas.drawString(rad, 67, 150);
  canvas.drawString("STATUS   NOMINAL", 67, 162);
  canvas.setTextColor(C_GRN); canvas.setTextDatum(bottom_center);
  canvas.drawString("shake to go back", 67, 232);
}

void render(){
  canvas.fillSprite(C_BG);
  if(timeScreen){ drawTimeScreen(); canvas.pushSprite(0, 0); return; }
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
  // grab the real time once at boot, then run the clock locally (no battery-backed RTC)
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for(int i=0; i<30 && WiFi.status()!=WL_CONNECTED; i++) delay(300);   // up to ~9s to join
  bootWifiOk = (WiFi.status()==WL_CONNECTED);
  if(bootWifiOk){
    configTzTime("WET0WEST,M3.5.0/1,M10.5.0", "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    struct tm t; for(int i=0; i<20 && !getLocalTime(&t, 0); i++) delay(300);   // ~6s at boot
    if(getLocalTime(&t, 0)){ timeSynced = true; WiFi.disconnect(); }
    // if not synced yet, leave Wi-Fi on; loop() keeps trying NTP, then disconnects once it lands
  } else {
    WiFi.disconnect();
  }
  canvas.setColorDepth(16);
  canvas.createSprite(135, 240);
  randomSeed(esp_random());
  beep(523, 60); beep(784, 80);     // soft boot blips
  onEnterTab();
}

void loop(){
  M5.update();
  // keep trying NTP until the clock lands, then drop Wi-Fi.
  // give up after ~40s so a blocked time server can't keep the radio on and drain the battery.
  if(bootWifiOk && !timeSynced && !wifiGaveUp){
    struct tm t;
    if(getLocalTime(&t, 0)){ timeSynced = true; WiFi.disconnect(); }
    else if(millis() > 40000){ WiFi.disconnect(); wifiGaveUp = true; }
  }
  if(M5.BtnA.wasPressed()){ tab = (tab + 1) % T_COUNT; beep(1046, 30); onEnterTab(); }
  if(M5.BtnB.wasPressed()){ tab = (tab - 1 + T_COUNT) % T_COUNT; beep(880, 30); onEnterTab(); }
  checkShake();
  updateMelody();
  updateGeiger();
  render();
  delay(16);
}
