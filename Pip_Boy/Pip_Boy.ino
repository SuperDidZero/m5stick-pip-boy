/*
  Miniature Pip-Boy (by SuperDidZero)  -  M5StickS3
  --------------------------------------------------------------
  Tabs:  STAT (you + time and date) | COMMS (LinkedIn) | BASE (website) | RADIO (Instagram)
  Front button (BtnA) = next tab      Side button (BtnB) = previous tab
  Shake = full-screen RAD counter (Wi-Fi Geiger), shake again to go back
  Time and date come from a known Wi-Fi at boot; with none, they are simply not shown.
  All lettering is drawn as LCD segments (see "LCD segment lettering").
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
enum { T_STAT, T_COMMS, T_BASE, T_RADIO, T_COUNT };
const char* TAB_NAME[T_COUNT] = { "STAT", "COMMS", "BASE", "RADIO" };
int tab = T_STAT;
bool bootWifiOk = false;          // did Wi-Fi connect at boot (for the clock)
bool radScreen  = false;          // shake toggles the full-screen RAD counter
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

// ---------- LCD segment lettering ----------
// Every word and number is drawn as a segmented LCD display, like an old digital watch.
// Letters use 14 segments, digits look like a 7-segment clock. Big digits can show the
// faint "unlit" segments behind them (ghost), like a real LCD.
#define C_GHOST 0x02A0   // unlit segment shade, clearly visible but well below the lit green
enum { SA=1, SB=2, SC=4, SD=8, SE=16, SF=32, SG1=64, SG2=128,
       SH=256, SI=512, SJ=1024, SK=2048, SL=4096, SM=8192 };
const uint16_t SEG7ALL = SA|SB|SC|SD|SE|SF|SG1|SG2;

uint16_t segMask(char c){
  if(c>='a' && c<='z') c -= 32;
  switch(c){
    case '0': return SA|SB|SC|SD|SE|SF;
    case '1': return SB|SC;
    case '2': return SA|SB|SG1|SG2|SE|SD;
    case '3': return SA|SB|SC|SD|SG1|SG2;
    case '4': return SF|SG1|SG2|SB|SC;
    case '5': return SA|SF|SG1|SG2|SC|SD;
    case '6': return SA|SF|SE|SD|SC|SG1|SG2;
    case '7': return SA|SB|SC;
    case '8': return SEG7ALL;
    case '9': return SA|SB|SC|SD|SF|SG1|SG2;
    case 'A': return SA|SB|SC|SE|SF|SG1|SG2;
    case 'B': return SA|SB|SC|SD|SG2|SI|SL;
    case 'C': return SA|SD|SE|SF;
    case 'D': return SA|SB|SC|SD|SI|SL;
    case 'E': return SA|SD|SE|SF|SG1;
    case 'F': return SA|SE|SF|SG1;
    case 'G': return SA|SC|SD|SE|SF|SG2;
    case 'H': return SB|SC|SE|SF|SG1|SG2;
    case 'I': return SA|SD|SI|SL;
    case 'J': return SB|SC|SD|SE;
    case 'K': return SE|SF|SG1|SJ|SM;
    case 'L': return SD|SE|SF;
    case 'M': return SB|SC|SE|SF|SH|SJ;
    case 'N': return SB|SC|SE|SF|SH|SM;
    case 'O': return SA|SB|SC|SD|SE|SF;
    case 'P': return SA|SB|SE|SF|SG1|SG2;
    case 'Q': return SA|SB|SC|SD|SE|SF|SM;
    case 'R': return SA|SB|SE|SF|SG1|SG2|SM;
    case 'S': return SA|SC|SD|SF|SG1|SG2;
    case 'T': return SA|SI|SL;
    case 'U': return SB|SC|SD|SE|SF;
    case 'V': return SE|SF|SK|SJ;
    case 'W': return SB|SC|SE|SF|SK|SM;
    case 'X': return SH|SJ|SK|SM;
    case 'Y': return SH|SJ|SL;
    case 'Z': return SA|SD|SJ|SK;
    case '-': return SG1|SG2;
    case '/': return SJ|SK;
    case '%': return SJ|SK|SF|SC;
    case '@': return SA|SB|SD|SE|SF|SG2|SI;
    case '[': return SA|SD|SE|SF;
    case ']': return SA|SB|SC|SD;
    case '_': return SD;
    default:  return 0;               // space and anything unknown
  }
}

// small text has no room for diagonal strokes, so those letters use simpler shapes
uint16_t segMaskSmall(char c){
  if(c>='a' && c<='z') c -= 32;
  switch(c){
    case 'M': return SA|SB|SC|SE|SF|SI;
    case 'W': return SB|SC|SD|SE|SF|SL;
    case 'N': return SA|SB|SC|SE|SF;
    case 'K': return SF|SE|SG1|SG2|SC;
    case 'V': return SB|SC|SD|SE|SF;
    case 'X': return SB|SC|SE|SF|SG1|SG2;
    case 'Y': return SF|SG1|SG2|SB|SC|SD;
    case 'Z': return SA|SB|SG1|SG2|SE|SD;
    default:  return segMask(c);
  }
}

// one bevelled horizontal / vertical segment, or a diagonal line
void segH(int x0, int x1, int yc, int t, uint16_t col){
  if(x1 < x0) return;
  int hf = t/2;
  if(t < 3 || x1 - x0 < t){ canvas.fillRect(x0, yc-hf, x1-x0+1, t, col); return; }
  canvas.fillRect(x0+hf, yc-hf, x1-x0+1-2*hf, t, col);
  canvas.fillTriangle(x0, yc, x0+hf, yc-hf, x0+hf, yc+hf, col);
  canvas.fillTriangle(x1, yc, x1-hf, yc-hf, x1-hf, yc+hf, col);
}
void segV(int xc, int y0, int y1, int t, uint16_t col){
  if(y1 < y0) return;
  int hf = t/2;
  if(t < 3 || y1 - y0 < t){ canvas.fillRect(xc-hf, y0, t, y1-y0+1, col); return; }
  canvas.fillRect(xc-hf, y0+hf, t, y1-y0+1-2*hf, col);
  canvas.fillTriangle(xc, y0, xc-hf, y0+hf, xc+hf, y0+hf, col);
  canvas.fillTriangle(xc, y1, xc-hf, y1-hf, xc+hf, y1-hf, col);
}
void segD(int x0, int y0, int x1, int y1, int t, uint16_t col){
  int r = (t > 1) ? t/2 : 0;
  for(int k=-r; k<=r; k++) canvas.drawLine(x0+k, y0, x1+k, y1, col);
}

void segDraw(int x, int y, int w, int h, int t, uint16_t m, uint16_t col){
  int g  = (t > 2) ? 1 : 0;           // little gap between segments, like a real LCD
  int hf = t/2;
  int xL = x + hf, xR = x + w - 1 - hf, xC = x + w/2;
  int yT = y + hf, yB = y + h - 1 - hf, yM = y + h/2;
  if(m & SA)  segH(xL+g+1, xR-g-1, yT, t, col);
  if(m & SD)  segH(xL+g+1, xR-g-1, yB, t, col);
  if(m & SG1) segH(xL+g+1, (m & SG2) ? xC-g : xC, yM, t, col);
  if(m & SG2) segH((m & SG1) ? xC+g : xC, xR-g-1, yM, t, col);
  if(m & SF)  segV(xL, yT+g+1, yM-g-1, t, col);
  if(m & SB)  segV(xR, yT+g+1, yM-g-1, t, col);
  if(m & SE)  segV(xL, yM+g+1, yB-g-1, t, col);
  if(m & SC)  segV(xR, yM+g+1, yB-g-1, t, col);
  if(m & SI)  segV(xC, yT+g+1, yM-g-1, t, col);
  if(m & SL)  segV(xC, yM+g+1, yB-g-1, t, col);
  if(m & SH)  segD(xL+t, yT+t, xC-1, yM-1-hf, t, col);
  if(m & SJ)  segD(xR-t, yT+t, xC+1, yM-1-hf, t, col);
  if(m & SK)  segD(xL+t, yB-t, xC-1, yM+1+hf, t, col);
  if(m & SM)  segD(xR-t, yB-t, xC+1, yM+1+hf, t, col);
}

int segCharW(char c, int w, int t){ return (c==':' || c=='.') ? t + 2 : w; }
int segGap(int h){ return (h >= 14) ? 3 : 1; }

int segWidth(const char* s, int h, int w, int t){
  int tot = 0, n = 0;
  for(const char* p=s; *p; p++){ tot += segCharW(*p, w, t); n++; }
  return n ? tot + (n-1)*segGap(h) : 0;
}

// align: 0 = left, 1 = centre, 2 = right of x.  ghost = show unlit 7-segment digits behind.
int segText(const char* s, int x, int y, int h, int w, int t, int align, uint16_t col, bool ghost){
  int tw = segWidth(s, h, w, t);
  if(align == 1) x -= tw/2; else if(align == 2) x -= tw;
  int gap = segGap(h);
  for(const char* p=s; *p; p++){
    char c = *p;
    int cw = segCharW(c, w, t);
    if(c == ':'){
      canvas.fillRect(x+1, y + h/4,       t, t, col);
      canvas.fillRect(x+1, y + 3*h/4 - t, t, t, col);
    } else if(c == '.'){
      canvas.fillRect(x+1, y + h - t, t, t, col);
    } else if(c == '%'){
      canvas.drawLine(x, y + h - 1, x + w - 1, y, col);
      canvas.fillRect(x, y, 2, 2, col);
      canvas.fillRect(x + w - 2, y + h - 2, 2, 2, col);
    } else {
      if(ghost && ((c >= '0' && c <= '9') || c == ' ')) segDraw(x, y, w, h, t, SEG7ALL, C_GHOST);
      segDraw(x, y, w, h, t, (w < 8) ? segMaskSmall(c) : segMask(c), col);
    }
    x += cw + gap;
  }
  return tw;
}
// small text helpers: tiny (tabs, labels) and line text
#define TXT_H 9
#define TXT_W 5
int segSmall(const char* s, int x, int y, int align, uint16_t col){ return segText(s, x, y, TXT_H, TXT_W, 1, align, col, false); }

// ---------- geiger ----------
int   gLevel = 0, gCount = 0;   // gLevel 0..12 from strongest signal; gCount = networks seen
uint32_t gNextClick = 0, gNextScan = 0;
bool  scanning = false;

void updateGeiger(){
  if(!radScreen) return;
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

// ---------- shake opens the RAD counter ----------
float lastMag = 1.0; uint32_t shakeLock = 0;
void checkShake(){
  float ax, ay, az;
  if(M5.Imu.getAccel(&ax, &ay, &az)){
    float mag = sqrtf(ax*ax + ay*ay + az*az);
    if(fabs(mag - lastMag) > 0.7 && millis() > shakeLock){
      radScreen = !radScreen;                // shake flips the full-screen RAD counter
      shakeLock = millis() + 700;
      if(radScreen){ stopMelody(); gNextScan = 0; gLevel = 0; }
      beep(radScreen ? 1200 : 700, 45);
    }
    lastMag = mag;
  }
}

// ---------- drawing ----------
void drawTabs(){
  int total = 0, w[T_COUNT];
  for(int i=0;i<T_COUNT;i++){ w[i] = segWidth(TAB_NAME[i], TXT_H, TXT_W, 1); total += w[i]; }
  int gap = (135 - 4 - total) / (T_COUNT - 1);
  int x = 2;
  for(int i=0;i<T_COUNT;i++){
    if(i==tab){ canvas.fillRect(x-2, 3, w[i]+4, 14, C_GRN); segSmall(TAB_NAME[i], x, 5, 0, C_BG); }
    else segSmall(TAB_NAME[i], x, 5, 0, C_GRN);
    x += w[i];
    if(i < T_COUNT-1){ canvas.drawFastVLine(x + gap/2, 4, 12, C_DIM); x += gap; }
  }
  canvas.drawFastHLine(0, 20, 135, C_GRN);
}

void drawHP(){
  int lvl = M5.Power.getBatteryLevel();
  if(lvl < 0) lvl = 100;
  canvas.drawFastHLine(0, 218, 135, C_GRN);
  segSmall("HP", 5, 223, 0, C_GRN);
  canvas.drawRect(24, 222, 72, 10, C_GRN);
  int w = (70 * lvl) / 100;
  canvas.fillRect(25, 223, w, 8, C_GRN);
  char pc[12]; sprintf(pc, "%3d%%", constrain(lvl, 0, 100));
  segText(pc, 132, 223, TXT_H, TXT_W, 1, 2, C_GRN, true);
}

// healthy idle: random poses
uint32_t beatStart = 0; bool thumbBeat = false;
int idleState = 0; uint32_t idleUntil = 0;

void drawFigure(){
  int lvl = M5.Power.getBatteryLevel(); if(lvl < 0) lvl = 100;
  int bob = (int)(sinf(millis()/300.0) * 3);   // gentle up/down
  segSmall(HANDLE, 67, 22, 1, C_GRN);
  struct tm tnow;                              // time + date, only once a known Wi-Fi gave the time
  if(getLocalTime(&tnow, 0)){
    char tb[12]; sprintf(tb, "%02d:%02d", tnow.tm_hour, tnow.tm_min);
    char db[24]; sprintf(db, "%02d.%02d.%04d", tnow.tm_mday, tnow.tm_mon + 1, 1900 + tnow.tm_year);
    segText(tb, 67, 33, 22, 16, 3, 1, C_GRN, true);    // big LCD clock with unlit segments behind
    segText(db, 67, 57, 7, 4, 1, 1, C_GRN, true);             // date in numbers: day.month.year
  }                                            // no time: leave the space empty
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
      canvas.pushImage((135-sw)/2, 65+bob, sw, sh, s, KEY);
    } else if(idleState == 2){
      canvas.pushImage((135-THUMB_W)/2, 65+bob, THUMB_W, THUMB_H, SPR_THUMB, KEY);
    } else if(idleState == 3){
      canvas.pushImage((135-WAVE_W)/2, 65+bob, WAVE_W, WAVE_H, SPR_WAVE, KEY);
    } else {
      canvas.pushImage((135-HEALTHY_W)/2, 65+bob, HEALTHY_W, HEALTHY_H, SPR_HEALTHY, KEY);
    }
  } else if(lvl >= 20){
    int sway = (int)(sinf(millis()/450.0) * 2);
    canvas.pushImage((135-TIRED_W)/2+sway, 66+bob, TIRED_W, TIRED_H, SPR_TIRED, KEY);
  } else {
    int breathe = (int)(sinf(millis()/700.0) * 2);
    canvas.pushImage((135-DOWN_W)/2, 90+breathe, DOWN_W, DOWN_H, SPR_DOWN, KEY);
  }
}

void drawQR(const char* url, const char* title, const char* sub){
  segSmall(title, 67, 25, 1, C_GRN);
  int dim = 120;                               // bigger squares, easier to scan
  int ox = (135 - dim)/2, oy = 50;             // nudged down a bit, no white border
  canvas.qrcode(url, ox, oy, dim, 5);
  segSmall(sub, 67, oy + dim + 8, 1, C_GRN);
}

void drawGeiger(){
  segSmall("RAD COUNTER", 67, 24, 1, C_GRN);
  int cx=67, cy=120, r=46;
  for(int a=180;a<=360;a+=10){
    float ra=a*PI/180.0;
    canvas.drawLine(cx+cos(ra)*(r-4), cy+sin(ra)*(r-4), cx+cos(ra)*r, cy+sin(ra)*r, C_DIM);
  }
  float na = (180 + (gLevel*180.0/12.0)) * PI/180.0;   // needle
  canvas.drawLine(cx, cy, cx+cos(na)*(r-8), cy+sin(na)*(r-8), C_GRN);
  canvas.fillCircle(cx, cy, 3, C_GRN);
  segSmall("WIFI SIGNAL", 67, 128, 1, C_DIM);
  // readings as LCD digits with the unlit segments shaded, fixed width like a real meter
  char cps[8];  sprintf(cps,  "%03d", constrain(gLevel*8, 0, 999));
  char nets[8]; sprintf(nets, "%02d",  constrain(gCount, 0, 99));
  int nx = 67 - 30;
  segText(cps,  nx, 140, 18, 11, 2, 0, C_GRN, true);  segSmall("CPS",  nx + 44, 149, 0, C_DIM);
  segText(nets, nx, 164, 18, 11, 2, 0, C_GRN, true);  segSmall("NETS", nx + 44, 173, 0, C_DIM);
  // live click bars
  for(int i=0;i<8;i++){
    int h = 3 + (gLevel? random(2, gLevel+3) : 1);
    canvas.fillRect(24 + i*12, 205 - h, 6, h, C_GRN);
  }
}

void drawRadScreen(){
  canvas.drawRect(0, 0, 135, 240, C_GRN);
  drawGeiger();
  segSmall("SHAKE TO GO BACK", 67, 222, 1, C_GRN);
}

void render(){
  canvas.fillSprite(C_BG);
  if(radScreen){ drawRadScreen(); canvas.pushSprite(0, 0); return; }
  drawTabs();
  switch(tab){
    case T_STAT:  drawFigure(); break;
    case T_COMMS: drawQR(URL_LINKEDIN,  "COMMS",  "LinkedIn"); break;
    case T_BASE:  drawQR(URL_WEBSITE,   "BASE",   "PC Build PT"); break;
    case T_RADIO: drawQR(URL_INSTAGRAM, "RADIO",  HANDLE); break;
  }
  drawHP();
  canvas.pushSprite(0, 0);
}

void onEnterTab(){
  stopMelody();
  if(tab==T_COMMS) startMelody(M_BACH,  sizeof(M_BACH)/sizeof(Note));
  if(tab==T_BASE)  startMelody(M_BASE,  sizeof(M_BASE)/sizeof(Note));
  if(tab==T_RADIO) startMelody(M_STARS, sizeof(M_STARS)/sizeof(Note));
}

void setup(){
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(0);        // GPIO pins up, big button at the bottom. If upside down, try 2.
  M5.Speaker.begin();
  M5.Speaker.setVolume(140);
  WiFi.mode(WIFI_STA);
  // grab the real time once at boot from a known Wi-Fi, then run the clock locally.
  // The StickS3 has no battery-backed clock, so with no known Wi-Fi at boot no time is shown.
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
  if(M5.BtnA.wasPressed() && !radScreen){ tab = (tab + 1) % T_COUNT; beep(1046, 30); onEnterTab(); }
  if(M5.BtnB.wasPressed() && !radScreen){ tab = (tab - 1 + T_COUNT) % T_COUNT; beep(880, 30); onEnterTab(); }
  checkShake();
  updateMelody();
  updateGeiger();
  render();
  delay(16);
}
