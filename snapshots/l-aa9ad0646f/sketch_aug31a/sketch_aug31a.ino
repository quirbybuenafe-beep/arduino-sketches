// PocketCat32: complete single-tab Arduino sketch.
#include <Arduino.h>
// Classic ESP32-WROOM development board; NOT ESP32-C3/S3 pin mapping.
#ifndef USE_SSD1306
#define USE_SSD1306 0  // 0: SH1106, 1: SSD1306. Choose the actual controller.
#endif
#ifndef ENABLE_COMPANION
#define ENABLE_COMPANION 1  // Set 0 to exclude Wi-Fi and the phone dashboard.
#endif
constexpr uint8_t OLED_SDA = 21; // Original code used 21; its D23 comment was wrong.
constexpr uint8_t OLED_SCL = 22;
constexpr uint8_t PIN_UP = 16;
constexpr uint8_t PIN_DOWN = 17;
constexpr uint8_t PIN_LEFT = 18;
constexpr uint8_t PIN_RIGHT = 5;
constexpr uint8_t PIN_A = 32;
constexpr uint8_t PIN_B = 33;
constexpr uint8_t PIN_BUZZER = 26;  // Passive buzzer on GPIO26
constexpr uint32_t FRAME_MS = 40;
constexpr uint32_t SAVE_MS = 120000;
constexpr char AP_NAME[] = "PocketCat32";
constexpr char AP_PASSWORD[] = "pocketcat32"; // Change before sharing the device.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Preferences.h>
#include <esp_system.h>
#include <math.h>
#include <stddef.h>
#if USE_SSD1306
#include <Adafruit_SSD1306.h>
static Adafruit_SSD1306 display(128,64,&Wire,-1);
#else
#include <Adafruit_SH110X.h>
static Adafruit_SH1106G display(128,64,&Wire,-1);
#endif
#if ENABLE_COMPANION
#include <WiFi.h>
#include <WebServer.h>
// Static page kept in flash. No internet, cloud, account, or external libraries.
static const char COMPANION_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PocketCat32</title><style>
:root{color-scheme:dark;font-family:system-ui;background:#11151c;color:#eef2f8}
body{max-width:520px;margin:auto;padding:24px}h1{letter-spacing:-1px;margin-bottom:4px}
.sub{color:#98a9bd;font-size:14px}.card{background:#1b2330;border-radius:22px;padding:22px;margin:18px 0}
.cat{text-align:center;font-size:64px}.row{display:flex;justify-content:space-between;margin:14px 0 5px}
.track{height:8px;background:#303b4d;border-radius:9px;overflow:hidden}.fill{height:100%;background:#91dfc7;transition:width .4s}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}button{border:0;border-radius:14px;padding:15px;background:#91dfc7;color:#10251f;font-weight:700;font-size:15px}button:disabled{opacity:.4}
#notice{min-height:22px;color:#91dfc7}footer{font-size:12px;color:#98a9bd}</style>
<h1>PocketCat32</h1><div class="sub">Your tiny cat, a little closer.</div>
<div class="card"><div class="cat">🐈</div><h2 id="name">Connecting…</h2><div id="meta" class="sub"></div><div id="stats"></div></div>
<div class="card"><div class="grid"><button data-act="pet">Pet</button><button data-act="feed">Feed kibble</button><button data-act="wash">Wash</button><button data-act="sleep">Sleep / wake</button></div><p id="notice"></p></div>
<footer>Uses your ESP32's local hotspot. Keep playing on the OLED for games, quests, shopping, and settings. Hotspot closes after 10 minutes.</footer>
<script>
let busy=false;const labels=['Fullness','Joy','Energy','Cleanliness','Health','Bond'];
document.querySelector('#stats').innerHTML=labels.map((x,i)=>`<div class="row"><span>${x}</span><span id="v${i}">—</span></div><div class="track"><div class="fill" id="b${i}"></div></div>`).join('');
async function refresh(){try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error();const s=await r.json();document.querySelector('#name').textContent=s.name;document.querySelector('#meta').textContent=`${s.stage} · ${s.mood} · Day ${s.day} · ${s.coins} coins`;s.stats.forEach((v,i)=>{document.querySelector('#v'+i).textContent=v+'%';document.querySelector('#b'+i).style.width=v+'%'})}catch(e){document.querySelector('#notice').textContent='Connect to PocketCat32 Wi-Fi, then reopen this page.'}}
document.querySelectorAll('button').forEach(b=>b.onclick=async()=>{if(busy)return;busy=true;document.querySelectorAll('button').forEach(x=>x.disabled=true);try{const r=await fetch('/api/action',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'action='+encodeURIComponent(b.dataset.act)});document.querySelector('#notice').textContent=await r.text();await refresh()}catch(e){document.querySelector('#notice').textContent='Connection lost.'}finally{busy=false;document.querySelectorAll('button').forEach(x=>x.disabled=false)}});
refresh();setInterval(refresh,2500);
</script></html>
)HTML";

static WebServer server(80);
#endif

namespace {
enum Key : uint8_t { UP, DOWN, LEFT, RIGHT, A, B, KEY_COUNT };
enum Screen : uint8_t { HOME, MENU, FOOD, CARE, GAMES, SHOP, STATS, QUESTS, SETTINGS, NAMES, RESET, MINI, RESULT, EVENT, ACTION };
enum Anim : uint8_t { IDLE, EATING, HEARTS, BATH, HEAL, CELEBRATE, SWEEP, TRICK, NAP, WAKE };
enum GameType : uint8_t { FISH, DASH, MEMORY, TIMING };
const uint8_t keyPins[] = {PIN_UP,PIN_DOWN,PIN_LEFT,PIN_RIGHT,PIN_A,PIN_B};
const char *const catNames[] = {"Mochi","Luna","Miso","Neko","Tofu","Oreo","Mango","Pip"};
const char *const stageNames[] = {"Kitten","Junior","Adult","Senior"};
const char *const personalities[] = {"Curious","Foodie","Playful","Dreamer"};
const char *const menuNames[] = {"Food & treats","Care & sleep","Mini-games","Shop & room","Cat journal","Daily quests","Settings"};
const char *const foodNames[] = {"Kibble","Fish dinner","Milk bowl","Sweet treat"};
const char *const careNames[] = {"Pet & cuddle","Wash cat","Clean litter","Sleep / wake","Medicine","Train trick","Vet rescue (free)"};
const char *const gameNames[] = {"Fish catcher","Rooftop dash","Memory paws","Perfect pounce"};
const char *const shopNames[] = {"Kibble x3","Fish x2","Milk x2","Treat x2","Medicine","Bow collar","Cozy bed","Window plant","Cat tree","Star scarf"};
const uint16_t prices[] = {12,20,16,18,30,70,85,95,130,180};
const char *const achievementNames[] = {"First meal","Clean paws","First victory","Best friends","Junior cat","Adult cat","Rich kitty","Trick master","Perfect memory","Room collector","Quest champion","Senior cat"};
constexpr uint32_t MAGIC = 0x50435432;
constexpr uint16_t VERSION = 1;
struct SaveData {
  uint32_t magic;
  uint16_t version, bytes;
  uint32_t ageMinutes, worldMinutes, totalGames, totalFeeds, totalWashes;
  uint32_t achievements, dayId;
  uint16_t coins, xp, level, streak;
  uint8_t full, joy, energy, clean, health, bond;
  uint8_t name, personality, asleep, sick, poop, sound, speed, contrast;
  uint8_t food[4], medicine, owned, accessory, tricks;
  uint8_t questFeed, questWash, questPlay, questClaimed, weather;
  uint32_t rescueMinute;
  uint8_t reserved[5];
  uint32_t checksum;
};
SaveData pet{};
Preferences prefs;
bool storageReady=false, dirty=false, oledReady=false;
Screen screen=HOME;
uint8_t selected=0, statsPage=0;
uint32_t nowMs=0, frameAt=0, simAt=0, saveAt=0, animAt=0, noticeUntil=0;
uint32_t resetAt=0, lastHomeMove=0, nextIdle=0, beepUntil=0;
uint32_t cuddleAt=0, trainAt=0, eventAt=0;
bool hasCuddled=false, hasTrained=false;
uint8_t eventKind=0, eventSelection=0;
uint8_t anim=IDLE, idleStyle=0, phase=0;
Screen animationReturn=HOME;
uint8_t animationSelection=0;
const uint16_t animationLengths[]={0,3200,3200,4200,3000,2800,3000,3200,2800,2200};
int catX=43, targetX=43;
char noticeText[22]="";
bool hotspot=false;
uint32_t hotspotAt=0;
struct Button {
  bool raw=false, held=false, pressed=false, repeat=false;
  uint32_t changed=0, repeatAt=0;
} keys[KEY_COUNT];
struct Particle { int16_t x,y; int8_t vx,vy; uint8_t life, type; } particles[18]{};
struct Drop { int16_t x,y; uint8_t kind; bool active; } drops[8]{};
struct MiniState {
  GameType type=FISH;
  uint32_t started=0, stepAt=0, spawnAt=0, phaseAt=0;
  int16_t player=60, score=0, lives=3, jump=0, obstacle=140;
  uint8_t sequence[12]{}, round=1, input=0, shown=0, mode=0, flash=255;
  uint8_t attempts=0, combo=0;
  int16_t timing=0, direction=1;
  uint16_t reward=0;
} mg;

bool due(uint32_t now, uint32_t deadline);
bool due(uint32_t now, uint32_t deadline) { return int32_t(now-deadline)>=0; }
int bound(int value,int low,int high);
int bound(int value,int low,int high) { return value<low?low:(value>high?high:value); }
void change(uint8_t &value,int delta);
void change(uint8_t &value,int delta) { value=uint8_t(bound(int(value)+delta,0,100)); dirty=true; }
void coins(int delta);
void coins(int delta) { pet.coins=uint16_t(bound(int(pet.coins)+delta,0,9999)); dirty=true; }
void text(int x,int y,const char *str);
void text(int x,int y,const char *str) { display.setCursor(x,y); display.print(str); }
void number(int x,int y,int value);
void number(int x,int y,int value) { display.setCursor(x,y); display.print(value); }
void center(int y,const char *str);
void center(int y,const char *str) { text((128-int(strlen(str))*6)/2,y,str); }
void sound(uint16_t freq,uint16_t duration);
void sound(uint16_t freq,uint16_t duration=70) {
  if (!pet.sound) return;
  tone(PIN_BUZZER,freq); beepUntil=nowMs+duration;
}
void notify(const char *message,uint16_t frequency);
void notify(const char *message,uint16_t frequency=900) {
  snprintf(noticeText,sizeof(noticeText),"%s",message);
  noticeUntil=nowMs+2200; sound(frequency);
}
uint32_t hashSave(const SaveData &s);
uint32_t hashSave(const SaveData &s) {
  const uint8_t *data=reinterpret_cast<const uint8_t*>(&s);
  uint32_t h=2166136261UL;
  for(size_t i=0;i<offsetof(SaveData,checksum);++i) { h^=data[i]; h*=16777619UL; }
  return h;
}
uint8_t stage();
uint8_t stage() { return pet.ageMinutes<1440?0:(pet.ageMinutes<4320?1:(pet.ageMinutes<28800?2:3)); }
const char *mood();
const char *mood() {
  if(pet.sick) return "Feeling sick";
  if(pet.asleep) return "Sleeping";
  if(pet.health<25) return "Needs care";
  if(pet.full<25) return "Hungry";
  if(pet.energy<20) return "Sleepy";
  if(pet.clean<25) return "Messy";
  if(pet.joy<30) return "Lonely";
  if(pet.bond>75 && pet.joy>75) return "Loves you!";
  return pet.joy>65?"Happy":"Content";
}
void defaults();
void defaults() {
  anim=IDLE;
  memset(particles,0,sizeof(particles));
  memset(&pet,0,sizeof(pet)); pet.magic=MAGIC; pet.version=VERSION; pet.bytes=sizeof(pet);
  pet.worldMinutes=8*60; pet.dayId=0; pet.coins=60; pet.level=1;
  pet.full=75; pet.joy=75; pet.energy=90; pet.clean=90; pet.health=100; pet.bond=15;
  pet.personality=esp_random()%4; pet.sound=1; pet.contrast=180;
  pet.rescueMinute=UINT32_MAX;
  pet.food[0]=6; pet.food[1]=2; pet.food[2]=2; pet.food[3]=2; pet.medicine=1;
  dirty=true;
}
bool validSave(const SaveData &s);
bool validSave(const SaveData &s) {
  if(s.magic!=MAGIC || s.version!=VERSION || s.bytes!=sizeof(s) || s.checksum!=hashSave(s)) return false;
  if(s.full>100 || s.joy>100 || s.energy>100 || s.clean>100 || s.health>100 || s.bond>100) return false;
  if(s.name>=8 || s.personality>=4 || s.speed>2 || s.poop>4 || s.weather>2 || s.accessory>2) return false;
  if(s.sound>1 || s.asleep>1 || s.sick>1 || s.level<1 || s.level>99 || s.coins>9999 || s.tricks>10) return false;
  return true;
}
bool save(bool force);
bool save(bool force=false) {
  if(!dirty && !force) return true;
  if(!storageReady) return false;
  pet.checksum=hashSave(pet);
  if(prefs.putBytes("pet",&pet,sizeof(pet))!=sizeof(pet)) {
    Serial.println("Save failed; previous NVS record retained."); return false;
  }
  dirty=false; saveAt=nowMs; return true;
}
void load();
void load() {
  storageReady=prefs.begin("pocketcat",false);
  SaveData candidate{};
  if(storageReady && prefs.getBytesLength("pet")==sizeof(candidate) &&
     prefs.getBytes("pet",&candidate,sizeof(candidate))==sizeof(candidate) && validSave(candidate)) {
    pet=candidate;
  } else { defaults(); save(true); }
  // Clock advances only while powered: no guessed offline neglect without an RTC.
}
void unlock(uint8_t bit);
void unlock(uint8_t bit) {
  if(!(pet.achievements&(1UL<<bit))) { pet.achievements|=1UL<<bit; coins(10); dirty=true; }
}
void addXP(uint16_t amount);
void addXP(uint16_t amount) {
  pet.xp+=amount;
  while(pet.level<99 && pet.xp>=pet.level*30) { pet.xp-=pet.level*30; ++pet.level; coins(12); }
  if(pet.level==99) pet.xp=0;
  dirty=true;
}
void burst(uint8_t type,int x,int y);
void burst(uint8_t type,int x=60,int y=30) {
  for(auto &p:particles) {
    if(p.life) continue;
    p.x=x+random(-15,16); p.y=y+random(-5,6); p.vx=random(-1,2); p.vy=-1;
    p.life=random(12,28); p.type=type;
  }
}
void animate(Anim value);
void animate(Anim value) {
  if(screen!=ACTION) { animationReturn=screen; animationSelection=selected; }
  anim=value; animAt=nowMs; screen=ACTION;
  targetX=catX; // Keep the cat in place throughout the interaction.
  memset(particles,0,sizeof(particles));
  if(value==HEARTS || value==CELEBRATE || value==HEAL) burst(value==HEARTS?1:0,64,27);
}
void endAnimation();
void endAnimation() {
  anim=IDLE; screen=animationReturn; selected=animationSelection;
}
bool awake();
bool awake() {
  if(pet.asleep) { notify("Wake cat first",300); return false; }
  return true;
}
bool feed(uint8_t choice);
bool feed(uint8_t choice) {
  if(!awake()) return false;
  if(!pet.food[choice]) { notify("Buy food in shop",300); return false; }
  if(pet.full>=97) { notify("Already full!",300); return false; }
  --pet.food[choice];
  const int gain[]={23,38,18,10}, joy[]={2,8,5,14};
  change(pet.full,gain[choice]); change(pet.joy,joy[choice]); change(pet.bond,2);
  if(choice==3 && pet.clean<40) change(pet.health,-3);
  ++pet.totalFeeds; pet.questFeed=bound(pet.questFeed+1,0,3); addXP(4); unlock(0);
  animate(EATING); notify("Nom nom!",1100); return true;
}
bool care(uint8_t choice);
bool care(uint8_t choice) {
  if(choice!=3 && choice!=6 && !awake()) return false;
  switch(choice) {
    case 0:
      if(hasCuddled && nowMs-cuddleAt<6000) { notify("Enjoying your cuddle"); return false; }
      cuddleAt=nowMs; hasCuddled=true;
      change(pet.joy,9); change(pet.bond,4); addXP(2); animate(HEARTS); notify("Purrrr..."); break;
    case 1:
      if(pet.clean>94) { notify("Already sparkling"); return false; }
      change(pet.clean,32); change(pet.joy,-2); ++pet.totalWashes;
      pet.questWash=1; unlock(1); addXP(5); animate(BATH); notify("Fresh little paws"); break;
    case 2:
      if(!pet.poop) { notify("Litter is clean"); return false; }
      pet.poop=0; change(pet.clean,12); pet.questWash=1; addXP(3); animate(SWEEP); notify("Litter cleaned"); break;
    case 3:
      pet.asleep=!pet.asleep; dirty=true; animate(pet.asleep?NAP:WAKE); notify(pet.asleep?"Sweet dreams":"Good morning!"); break;
    case 4:
      if(!pet.sick && pet.health>85) { notify("Cat is healthy"); return false; }
      if(!pet.medicine) { notify("Buy medicine first",300); return false; }
      --pet.medicine; pet.sick=0; change(pet.health,35); animate(HEAL); notify("Feeling better!"); break;
    case 5:
      if(hasTrained && nowMs-trainAt<15000) { notify("Short practice break"); return false; }
      if(pet.energy<20 || pet.full<20 || pet.sick) { notify("Needs rest & food",300); return false; }
      if(pet.tricks>=10) { notify("All tricks learned!"); return false; }
      trainAt=nowMs; hasTrained=true;
      change(pet.energy,-8); change(pet.full,-4); change(pet.bond,3); addXP(10);
      animate(TRICK);
      if(random(100)<35+pet.bond/2) { ++pet.tricks; notify("New trick learned!"); }
      else notify("Practice makes purr");
      if(pet.tricks>=5) unlock(7); break;
    case 6:
      if(pet.health>25 && pet.full>10 && !pet.sick) { notify("Rescue is for crisis"); return false; }
      if(pet.rescueMinute!=UINT32_MAX && pet.ageMinutes-pet.rescueMinute<360) { notify("Vet: rest & recover"); return false; }
      pet.rescueMinute=pet.ageMinutes; pet.sick=0; pet.asleep=0; pet.poop=0;
      change(pet.health,50); change(pet.full,40); change(pet.clean,45); change(pet.energy,30);
      if(pet.food[0]<3) pet.food[0]=3;
      notify("Vet rescued your cat"); animate(HEAL); break;
  }
  if(pet.bond>=90) unlock(3);
  dirty=true; return true;
}
void simulateMinute();
void simulateMinute() {
  ++pet.ageMinutes; ++pet.worldMinutes; dirty=true;
  uint32_t day=pet.worldMinutes/1440;
  if(day!=pet.dayId) {
    if(pet.questClaimed) pet.streak=bound(pet.streak+1,0,999); else pet.streak=0;
    pet.dayId=day; pet.questFeed=pet.questWash=pet.questPlay=pet.questClaimed=0;
    pet.weather=random(3); coins(8);
  }
  if(pet.asleep && pet.worldMinutes%2==0) change(pet.energy,(pet.owned&2)?3:2);
  if(pet.asleep && pet.energy>=100) { pet.asleep=0; notify("Cat woke refreshed"); }
  if(pet.worldMinutes%10) return;
  change(pet.full,-(pet.asleep?1:(pet.personality==1?3:2)));
  change(pet.clean,-(1+pet.poop));
  if(!pet.asleep) {
    change(pet.energy,-(pet.personality==3?3:2));
    change(pet.joy,-(pet.personality==2?3:2));
  }
  if(pet.owned&4) change(pet.joy,1);
  if(pet.owned&8 && !pet.asleep) change(pet.joy,1);
  if(pet.worldMinutes%120==0 && pet.full>15) pet.poop=bound(pet.poop+1,0,4);
  if(pet.clean<20 && !pet.sick && random(100)<10) { pet.sick=1; notify("Cat needs medicine",240); }
  int stress=(pet.full<15)+(pet.energy<10)+(pet.clean<15)+(pet.joy<10)+int(pet.sick);
  if(stress) change(pet.health,-stress*2);
  else if(pet.full>45 && pet.clean>45 && !pet.sick) change(pet.health,1);
  // Compassionate design: critical health allows recovery; no permanent death.
  if(pet.health==0) { pet.health=1; pet.sick=1; }
  if(pet.joy<15) change(pet.bond,-1);
  if(stage()>=1) unlock(4);
  if(stage()>=2) unlock(5);
  if(stage()>=3) unlock(11);
  if(pet.coins>=500) unlock(6);
}
void readKeys();
void readKeys() {
  for(uint8_t i=0;i<KEY_COUNT;++i) {
    Button &k=keys[i]; bool raw=digitalRead(keyPins[i])==LOW;
    k.pressed=k.repeat=false;
    if(raw!=k.raw) { k.raw=raw; k.changed=nowMs; }
    if(raw!=k.held && nowMs-k.changed>=22) {
      k.held=raw;
      if(raw) { k.pressed=k.repeat=true; k.repeatAt=nowMs+320; }
    }
    if(k.held && due(nowMs,k.repeatAt)) { k.repeat=true; k.repeatAt=nowMs+110; }
  }
}
void go(Screen next);
void go(Screen next) { screen=next; selected=0; resetAt=0; sound(700,30); }
void finishMini(bool won);
void finishMini(bool won) {
  mg.reward=won?bound(mg.score/2+8,8,60):bound(mg.score/4,0,15);
  coins(mg.reward); addXP(won?15:4); change(pet.joy,won?16:5); change(pet.energy,-8); change(pet.full,-4);
  ++pet.totalGames;
  if(won) { pet.questPlay=1; unlock(2); change(pet.bond,3); burst(0); }
  if(mg.type==MEMORY && won) unlock(8);
  screen=RESULT; selected=won; sound(won?1400:330,200); save();
}
void startMini(uint8_t type);
void startMini(uint8_t type) {
  if(!awake()) return;
  if(pet.energy<15 || pet.full<10 || pet.sick) { notify("Care for cat first",300); return; }
  mg=MiniState{}; mg.type=GameType(type); mg.started=mg.stepAt=mg.spawnAt=mg.phaseAt=nowMs;
  for(uint8_t &v:mg.sequence) v=random(4);
  memset(drops,0,sizeof(drops)); screen=MINI;
}
void tickMini();
void tickMini() {
  if(keys[B].pressed) { go(GAMES); return; } // Aborted games grant no rewards.
  uint32_t elapsed=nowMs-mg.started;
  if(mg.type==FISH) {
    if(keys[LEFT].repeat) mg.player=bound(mg.player-10,8,119);
    if(keys[RIGHT].repeat) mg.player=bound(mg.player+10,8,119);
    if(nowMs-mg.spawnAt>=bound(720-int(elapsed/160),330,720)) {
      mg.spawnAt=nowMs;
      for(auto &d:drops) if(!d.active) { d={int16_t(random(8,120)),12,uint8_t(random(5)==0),true}; break; }
    }
    if(nowMs-mg.stepAt>=70) {
      mg.stepAt=nowMs;
      for(auto &d:drops) if(d.active) {
        d.y+=2+int(elapsed/12000);
        if(d.y>=51) {
          bool caught=abs(d.x-mg.player)<10;
          if(caught) { if(d.kind) { --mg.lives; sound(200); } else { mg.score+=4; sound(950,25); } }
          else if(!d.kind) --mg.lives;
          d.active=false;
        }
      }
    }
    if(mg.lives<=0 || elapsed>=30000) finishMini(mg.lives>0 && mg.score>=16);
  } else if(mg.type==DASH) {
    if((keys[A].pressed || keys[UP].pressed) && mg.jump==0) { mg.jump=1; sound(780,35); }
    if(nowMs-mg.stepAt>=40) {
      mg.stepAt=nowMs;
      if(mg.jump) { ++mg.jump; if(mg.jump>24) mg.jump=0; }
      mg.obstacle-=3+bound(mg.score/20,0,3);
      if(mg.obstacle<=33 && mg.obstacle>24) {
        int height=mg.jump?int(sinf((mg.jump-1)*PI/24)*20):0;
        if(height<12) { --mg.lives; mg.obstacle=random(138,188); sound(220); }
      }
      if(mg.obstacle<-8) { mg.obstacle=random(138,188); mg.score+=5; }
    }
    if(mg.lives<=0 || elapsed>=30000) finishMini(mg.lives>0 && mg.score>=15);
  } else if(mg.type==MEMORY) {
    if(mg.mode==0) {
      uint32_t slot=(nowMs-mg.phaseAt)/650;
      if(slot>=mg.round) { mg.mode=1; mg.input=0; mg.flash=255; mg.phaseAt=nowMs; }
      else mg.flash=(nowMs-mg.phaseAt)%650<420?mg.sequence[slot]:255;
    } else if(mg.mode==1) {
      if(nowMs-mg.phaseAt>=10000) { finishMini(false); return; }
      for(uint8_t i=0;i<4;++i) if(keys[i].pressed) {
        mg.flash=i; mg.phaseAt=nowMs;
        if(i!=mg.sequence[mg.input]) { finishMini(false); return; }
        ++mg.input; sound(600+i*160);
        if(mg.input>=mg.round) {
          mg.score+=mg.round*3; ++mg.round;
          if(mg.round>8) { finishMini(true); return; }
          mg.mode=2; mg.phaseAt=nowMs; mg.flash=255;
        }
      }
      if(nowMs-mg.phaseAt>220) mg.flash=255;
    } else if(nowMs-mg.phaseAt>=900) { mg.mode=0; mg.phaseAt=nowMs; }
  } else {
    if(nowMs-mg.stepAt>=30) {
      mg.stepAt=nowMs; mg.timing+=mg.direction*(3+mg.combo/2);
      if(mg.timing>=100) { mg.timing=100; mg.direction=-1; }
      if(mg.timing<=0) { mg.timing=0; mg.direction=1; }
    }
    if(keys[A].pressed) {
      ++mg.attempts; int distance=abs(mg.timing-50);
      if(distance<=9) { mg.score+=10+mg.combo*2; ++mg.combo; sound(1300); }
      else if(distance<=20) { mg.score+=4; mg.combo=0; sound(850); }
      else { mg.combo=0; sound(220); }
      if(mg.attempts>=10) finishMini(mg.score>=35);
    }
    if(elapsed>=30000 && screen==MINI) finishMini(mg.score>=35);
  }
}
void buy(uint8_t item);
void buy(uint8_t item) {
  uint8_t bit=item>=5?uint8_t(1<<(item-5)):0;
  if(bit && (pet.owned&bit)) {
    if(item==5) pet.accessory=pet.accessory==1?0:1;
    else if(item==9) pet.accessory=pet.accessory==2?0:2;
    dirty=true; notify("Already owned / set"); return;
  }
  if(pet.coins<prices[item]) { notify("Need more coins",300); return; }
  if(item<4 && pet.food[item]>96) { notify("Food bag is full"); return; }
  if(item==4 && pet.medicine>=99) { notify("Medicine bag full"); return; }
  coins(-int(prices[item]));
  if(item<4) pet.food[item]+=item==0?3:2;
  else if(item==4) ++pet.medicine;
  else { pet.owned|=bit; if(item==5) pet.accessory=1; if(item==9) pet.accessory=2; }
  if((pet.owned&14)==14) unlock(9);
  notify("Purchased!"); save();
}
void claimQuest();
void claimQuest() {
  if(pet.questClaimed) { notify("Claimed. Next day!"); return; }
  if(pet.questFeed<3 || !pet.questWash || !pet.questPlay) { notify("Finish all 3 quests"); return; }
  pet.questClaimed=1; coins(35+bound(pet.streak,0,10)*3); addXP(20); unlock(10);
  notify("Quest bonus claimed"); animate(CELEBRATE); save();
}
void resolveEvent();
void resolveEvent() {
  if(eventKind==0) { // A neighbor brings a gift or shares playtime.
    if(eventSelection==0) { coins(12); notify("Neighbor left coins!"); }
    else { change(pet.joy,12); change(pet.bond,3); notify("Made a new friend!"); }
  } else if(eventKind==1) {
    if(eventSelection==0 && pet.food[3]) { --pet.food[3]; change(pet.bond,8); addXP(8); notify("Shared your treat!"); }
    else { change(pet.joy,6); notify("A friendly wave!"); }
  } else {
    if(eventSelection==0) { change(pet.energy,-5); coins(random(4,16)); notify("Found pocket coins!"); }
    else { change(pet.energy,6); notify("A cozy window nap"); }
  }
  screen=HOME; animate(HEARTS); dirty=true;
}
void stopHotspot();
void stopHotspot() {
#if ENABLE_COMPANION
  server.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
#endif
  hotspot=false;
}
void startHotspot();
void startHotspot() {
#if ENABLE_COMPANION
  WiFi.mode(WIFI_AP);
  if(!WiFi.softAP(AP_NAME,AP_PASSWORD,1,false,2)) { WiFi.mode(WIFI_OFF); notify("Wi-Fi start failed",300); return; }
  hotspot=true; hotspotAt=nowMs; server.begin(); notify("Open 192.168.4.1");
#else
  notify("Wi-Fi not compiled");
#endif
}
void handleControls();
void handleControls() {
  if(screen==ACTION) { if(keys[B].pressed) endAnimation(); return; }
  if(screen==EVENT) {
    if(keys[LEFT].pressed || keys[RIGHT].pressed) eventSelection=!eventSelection;
    if(keys[A].pressed) resolveEvent();
    if(keys[B].pressed || nowMs-eventAt>20000) { screen=HOME; }
    return;
  }
  if(screen==MINI) { tickMini(); return; }
  if(screen==RESULT) { if(keys[A].pressed || keys[B].pressed) go(GAMES); return; }
  if(screen==HOME) {
    if(keys[A].pressed) go(MENU);
    else if(keys[B].pressed) { care(0); }
    else if(keys[UP].pressed) go(STATS);
    else if(keys[DOWN].pressed) go(QUESTS);
    else if(!pet.asleep) {
      if(keys[LEFT].repeat) { targetX=bound(targetX-7,28,95); lastHomeMove=nowMs; }
      if(keys[RIGHT].repeat) { targetX=bound(targetX+7,28,95); lastHomeMove=nowMs; }
    }
    return;
  }
  if(screen==RESET) {
    if(keys[B].pressed) { go(SETTINGS); return; }
    if(keys[A].held) {
      if(!resetAt) resetAt=nowMs;
      if(nowMs-resetAt>=3000) { defaults(); save(true); go(HOME); notify("Meet your new cat!"); }
    } else resetAt=0;
    return;
  }
  if(keys[B].pressed) { if(screen==MENU) go(HOME); else if(screen==NAMES) go(SETTINGS); else go(MENU); save(); return; }
  if(screen==STATS) {
    if(keys[RIGHT].pressed || keys[DOWN].pressed) statsPage=(statsPage+1)%4;
    if(keys[LEFT].pressed || keys[UP].pressed) statsPage=(statsPage+3)%4;
    return;
  }
  if(screen==QUESTS) { if(keys[A].pressed) claimQuest(); return; }
  uint8_t count=screen==MENU?7:(screen==FOOD?4:(screen==CARE?7:(screen==GAMES?4:(screen==SHOP?10:(screen==NAMES?8:7)))));
  if(keys[UP].repeat || keys[LEFT].repeat) { selected=(selected+count-1)%count; sound(600,20); }
  if(keys[DOWN].repeat || keys[RIGHT].repeat) { selected=(selected+1)%count; sound(600,20); }
  if(!keys[A].pressed) return;
  if(screen==MENU) { const Screen pages[]={FOOD,CARE,GAMES,SHOP,STATS,QUESTS,SETTINGS}; go(pages[selected]); }
  else if(screen==FOOD) feed(selected);
  else if(screen==CARE) care(selected);
  else if(screen==GAMES) startMini(selected);
  else if(screen==SHOP) buy(selected);
  else if(screen==NAMES) { pet.name=selected; dirty=true; notify("Name updated!"); save(); }
  else if(screen==SETTINGS) {
    switch(selected) {
      case 0: pet.sound=!pet.sound; if(!pet.sound) noTone(PIN_BUZZER); dirty=true; break;
      case 1: pet.speed=(pet.speed+1)%3; simAt=nowMs; dirty=true; break;
      case 2: pet.contrast=pet.contrast==60?180:(pet.contrast==180?255:60);
#if USE_SSD1306
        display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(pet.contrast);
#else
        display.setContrast(pet.contrast);
#endif
        dirty=true; break;
      case 3: go(NAMES); break;
      case 4: if(hotspot) { stopHotspot(); notify("Hotspot off"); } else startHotspot(); break;
      case 5: notify(save(true)?"Progress saved":"SAVE FAILED",storageReady?1000:250); break;
      case 6: go(RESET); break;
    }
  }
}

void heart(int x,int y);
void heart(int x,int y) { display.fillCircle(x-2,y,2,1); display.fillCircle(x+2,y,2,1); display.fillTriangle(x-4,y,x+4,y,x,y+5,1); }
void drawCat(int x,int floorY,int scale);
void drawCat(int x,int floorY,int scale=1) {
  int bob=(!pet.asleep && catX!=targetX)?((phase/3)%2):0;
  uint32_t t=nowMs-animAt;
  if(anim==EATING) bob=-int((t/180)%2)*2;
  if(anim==HEARTS) bob=int((t/250)%2);
  if(anim==BATH && t>3000) bob=int((t/70)%3)-1;
  if(anim==WAKE) bob=int((t/180)%2);
  int y=floorY-16*scale-bob;
  if(pet.asleep) {
    display.fillRoundRect(x-13,y+6,27,11,5,1);
    display.fillTriangle(x-12,y+9,x-10,y+2,x-5,y+9,1);
    display.drawLine(x-9,y+12,x-5,y+12,0); display.drawLine(x+3,y+12,x+7,y+12,0);
    text(x+16,y-5+((phase/12)%3),"z"); return;
  }
  // Procedural pixel sprite: face, ears, body, paws, striped tail, accessories.
  display.fillRoundRect(x-9*scale,y+8*scale,19*scale,12*scale,4*scale,1);
  display.fillRoundRect(x-12*scale,y-2*scale,25*scale,18*scale,5*scale,1);
  display.fillTriangle(x-12*scale,y+2*scale,x-10*scale,y-8*scale,x-3*scale,y-2*scale,1);
  display.fillTriangle(x+12*scale,y+2*scale,x+10*scale,y-8*scale,x+3*scale,y-2*scale,1);
  display.drawLine(x-10*scale,y-1*scale,x-9*scale,y-4*scale,0);
  display.drawLine(x+10*scale,y-1*scale,x+9*scale,y-4*scale,0);
  bool blink=phase%80>=76 || anim==HEARTS || (anim==BATH && t>3000) || (anim==EATING && (t/180)%2);
  int eyeY=y+5*scale;
  if(blink) { display.drawFastHLine(x-7*scale,eyeY,4*scale,0); display.drawFastHLine(x+3*scale,eyeY,4*scale,0); }
  else {
    display.fillRect(x-6*scale,eyeY,2*scale,3*scale,0); display.fillRect(x+4*scale,eyeY,2*scale,3*scale,0);
    if(pet.joy<25 || pet.sick) { display.drawLine(x-7*scale,eyeY-2,x-4*scale,eyeY,0); display.drawLine(x+4*scale,eyeY,x+7*scale,eyeY-2,0); }
  }
  display.fillTriangle(x-scale,y+9*scale,x+scale,y+9*scale,x,y+10*scale,0);
  display.drawLine(x,y+10*scale,x-2*scale,y+12*scale,0); display.drawLine(x,y+10*scale,x+2*scale,y+12*scale,0);
  if(anim==EATING && (t/140)%2) display.fillRect(x-2,y+11*scale,5,3,0);
  display.drawFastHLine(x-11*scale,y+9*scale,3*scale,0); display.drawFastHLine(x+8*scale,y+9*scale,3*scale,0);
  display.fillRect(x-8*scale,floorY,6*scale,3,1); display.fillRect(x+3*scale,floorY,6*scale,3,1);
  int tail=phase/6%2?3:-2;
  display.drawLine(x+9*scale,y+17*scale,x+16*scale,y+12*scale+tail,1);
  display.drawLine(x+10*scale,y+17*scale,x+17*scale,y+12*scale+tail,1);
  if(stage()>=2) { display.drawFastVLine(x-2,y-1,4,0); display.drawFastVLine(x+2,y-1,4,0); }
  if(stage()==1) display.drawFastHLine(x-4,y,9,0);
  if(stage()==3) { display.drawLine(x-14,y+7,x-9,y+9,1); display.drawLine(x+14,y+7,x+9,y+9,1); }
  if(pet.accessory==1) { display.fillTriangle(x-5,y+16,x,y+18,x-5,y+20,0); display.fillTriangle(x+5,y+16,x,y+18,x+5,y+20,0); }
  if(pet.accessory==2) { display.drawFastHLine(x-8,y+16,16,0); display.drawFastVLine(x+6,y+16,7,0); }
  if(pet.sick) { text(x-21,y-6,"+"); }
}
void header(const char *title);
void header(const char *title) { text(0,0,title); display.drawFastHLine(0,9,128,1); }
void footer(const char *hint);
void footer(const char *hint) { display.drawFastHLine(0,54,128,1); text(0,56,hint); }
void bar(int x,int y,int w,uint8_t value);
void bar(int x,int y,int w,uint8_t value) {
  display.drawRect(x,y,w,5,1); if(value) display.fillRect(x+1,y+1,(w-2)*value/100,3,1);
}
void drawRoom();
void drawRoom() {
  bool night=(pet.worldMinutes%1440)/60>=20 || (pet.worldMinutes%1440)/60<6;
  display.drawRoundRect(92,13,29,22,2,1); display.drawFastVLine(106,13,22,1); display.drawFastHLine(92,24,29,1);
  if(night) { display.fillCircle(99,19,3,1); display.fillCircle(101,18,3,0); display.drawPixel(113,18,1); }
  else if(pet.weather==0) display.drawCircle(99,19,3,1);
  else {
    display.fillCircle(99,19,3,1); display.fillCircle(103,20,3,1);
    if(pet.weather==2) for(int i=0;i<3;++i) display.drawLine(97+i*7,27+(phase%3),95+i*7,30+(phase%3),1);
  }
  display.drawFastHLine(0,48,128,1);
  if(pet.owned&2) { display.drawRoundRect(4,41,26,10,3,1); display.drawFastHLine(8,44,17,1); }
  if(pet.owned&4) { display.drawRect(113,42,9,6,1); display.drawLine(117,42,117,35,1); display.fillTriangle(117,38,110,34,115,40,1); }
  if(pet.owned&8) { display.drawFastVLine(8,27,20,1); display.drawFastHLine(0,27,20,1); display.drawFastHLine(1,36,17,1); }
  for(int i=0;i<pet.poop;++i) { int x=76+i*8; display.fillTriangle(x-3,47,x+3,47,x,42,1); }
  if(!pet.asleep && idleStyle==2 && anim==IDLE) { display.drawCircle(70,46,3,1); }
}
void drawHome();
void drawHome() {
  text(0,0,catNames[pet.name]); text(66,0,"$"); number(72,0,pet.coins);
  if(hotspot) text(114,0,"W");
  display.drawFastHLine(0,9,128,1); drawRoom();
  int jump=(anim==CELEBRATE)?int(abs(sinf(phase*0.35f))*7):0;
  drawCat(catX,45-jump);
  if(anim==HEARTS) heart(catX+19,23-(phase%5));
  if(pet.full<25) text(3,15,"FOOD!"); else if(pet.energy<20) text(3,15,"REST!");
  text(0,56,mood());
  bar(95,56,30,pet.health);
}
void drawAction();
void drawAction() {
  const char *labels[]={"","NOM NOM!","PURRR...","BUBBLE BATH","GET WELL SOON","HAPPY CAT!","LITTER CLEANUP","TRICK PRACTICE","SWEET DREAMS","GOOD MORNING"};
  uint32_t t=nowMs-animAt;
  uint16_t duration=animationLengths[anim];
  header(labels[anim]);
  int x=64, floor=44;
  int jump=(anim==CELEBRATE || anim==TRICK)?int(abs(sinf(t*0.006f))*7):0;
  if(anim==BATH) {
    // Shower and falling water draw behind the cat.
    if(t<3000) {
      display.drawLine(91,12,91,18,1);
      display.drawLine(82,18,98,18,1);
      for(int i=0;i<5;++i) {
        int y=20+((int(t/35)+i*7)%24);
        display.drawLine(83+i*3,y,83+i*3-1,y+2,1);
      }
    }
  }
  drawCat(x,floor-jump);
  if(anim==EATING) {
    // Kibble shrinks in the bowl while crumbs move toward the mouth.
    display.fillRect(48,43,33,9,0);
    display.drawRoundRect(49,44,31,8,3,1);
    display.drawFastHLine(52,46,25,1);
    int pieces=bound(7-int(t/450),0,7);
    for(int i=0;i<pieces;++i) display.fillRect(53+i*3,43-(i%2),2,2,1);
    int step=(t/90)%8;
    display.drawPixel(61+step/2,43-step,1);
    if((t/180)%2) text(88,25,"yum");
  } else if(anim==HEARTS) {
    // A hand strokes from ear to ear, with relaxed eyes and rising hearts.
    int hx=49+int((sinf(t*0.004f)+1)*11);
    display.fillRoundRect(hx,14,15,5,2,1);
    display.drawRect(hx+10,10,5,6,1);
    for(int i=0;i<3;++i) display.drawFastVLine(hx+2+i*3,12,5,1);
    for(int i=0;i<3;++i) heart(36+i*29,39-int((t/65+i*9)%24));
    display.drawLine(52,43,47,39,1); display.drawLine(76,43,81,39,1);
  } else if(anim==BATH) {
    // Tub masks the lower body; bubbles rise; a sponge scrubs, then cat shakes.
    display.fillRoundRect(42,39,45,13,4,0);
    display.drawRoundRect(42,39,45,13,4,1);
    display.drawFastHLine(45,42,39,1);
    for(int i=0;i<7;++i) {
      int bx=37+i*8,by=39-int((t/65+i*5)%19);
      display.drawCircle(bx,by,1+(i%3),1);
    }
    if(t<3000) {
      int sx=49+int((sinf(t*0.009f)+1)*10);
      display.fillRoundRect(sx,33,13,5,2,1);
      display.drawPixel(sx+3,35,0); display.drawPixel(sx+8,35,0);
    } else {
      for(int i=0;i<6;++i) {
        int side=i%2?1:-1, spread=17+(t/90+i*3)%10;
        display.drawLine(x+side*spread,22+i*4,x+side*(spread+2),23+i*4,1);
      }
    }
  } else if(anim==HEAL) {
    for(int i=0;i<3;++i) {
      int px=36+i*28,py=34-int((t/80+i*7)%19);
      display.fillRect(px-3,py,7,2,1); display.fillRect(px,py-2,2,7,1);
    }
    if(t<1600) {
      int px=95-bound(int(t/65),0,27);
      display.drawRoundRect(px,32,9,5,2,1); display.drawFastVLine(px+4,32,5,1);
    }
  } else if(anim==SWEEP) {
    int sx=38+int((sinf(t*0.006f)+1)*20);
    display.drawRect(32,44,62,8,1);
    display.drawLine(sx,31,sx+5,46,1);
    display.fillTriangle(sx+5,44,sx-1,49,sx+12,49,1);
    for(int i=0;i<5;++i) if(t<2200) display.drawPixel(38+i*11,47+(i%2),1);
    display.drawRect(102,39,13,13,1); display.drawFastHLine(100,37,17,1);
  } else if(anim==TRICK || anim==CELEBRATE) {
    if(anim==TRICK) { display.drawLine(54,36-jump,45,29-jump,1); display.drawLine(74,36-jump,83,29-jump,1); }
    for(int i=0;i<6;++i) {
      int px=25+i*16,py=15+int((t/55+i*7)%31);
      display.drawFastHLine(px-2,py,5,1); display.drawFastVLine(px,py-2,5,1);
    }
  } else if(anim==NAP) {
    display.drawRoundRect(45,42,39,10,3,1);
    display.drawFastHLine(47,46,35,1);
    text(91,18-int((t/200)%3),"Z"); text(103,12,"z");
  } else if(anim==WAKE) {
    display.drawLine(51,41,44,34,1); display.drawLine(77,41,84,34,1);
    for(int i=0;i<3;++i) display.drawLine(88+i*4,20,90+i*4,17,1);
  }
  // Put feedback below the sprite so the action stays visible.
  if(noticeText[0]) center(56,noticeText);
  int progress=duration?bound(int(t*128/duration),0,128):128;
  display.drawFastHLine(0,53,progress,1);
}
void drawList(const char *title,const char *const *items,uint8_t count);
void drawList(const char *title,const char *const *items,uint8_t count) {
  header(title);
  int start=bound(int(selected)-1,0,count>3?count-3:0);
  for(int i=0;i<3 && start+i<count;++i) {
    int index=start+i,y=13+i*13;
    if(index==selected) { display.fillRoundRect(0,y-2,128,12,2,1); display.setTextColor(0); }
    text(4,y,items[index]);
    if(screen==FOOD) number(111,y,pet.food[index]);
    display.setTextColor(1);
  }
  if(screen==SHOP) {
    char line[22]; bool owned=selected>=5 && (pet.owned&(1<<(selected-5)));
    snprintf(line,sizeof(line),owned?"A: equip  B: back":"$%u  A:buy B:back",prices[selected]); footer(line);
  } else footer("A:select       B:back");
}
void drawStats();
void drawStats() {
  char title[22]; snprintf(title,sizeof(title),"CAT JOURNAL       %u/4",statsPage+1); header(title);
  if(statsPage==0 || statsPage==1) {
    const char *labels[]={"Fullness","Joy","Energy","Clean","Health","Bond"};
    uint8_t values[]={pet.full,pet.joy,pet.energy,pet.clean,pet.health,pet.bond};
    for(int row=0;row<3;++row) { int i=statsPage*3+row,y=13+row*13; text(0,y,labels[i]); bar(52,y+1,48,values[i]); number(106,y,values[i]); }
  } else if(statsPage==2) {
    text(0,13,stageNames[stage()]); text(60,13,personalities[pet.personality]);
    text(0,26,"Age days:"); number(60,26,pet.ageMinutes/1440);
    text(0,39,"Level:"); number(36,39,pet.level); text(59,39,"Tricks:"); number(104,39,pet.tricks);
  } else {
    uint8_t count=0; for(int i=0;i<12;++i) if(pet.achievements&(1UL<<i)) ++count;
    text(0,13,"Badges:"); number(48,13,count); text(66,13,"/12");
    text(0,26,"Games:"); number(42,26,pet.totalGames);
    uint8_t badge=(nowMs/2500)%12;
    text(0,39,(pet.achievements&(1UL<<badge))?"+":"-"); text(10,39,achievementNames[badge]);
  }
  footer("<>:page        B:back");
}
void drawQuests();
void drawQuests() {
  header("DAILY QUESTS"); char line[22];
  snprintf(line,sizeof(line),"%c Feed meals  %u/3",pet.questFeed>=3?'+':'-',pet.questFeed); text(0,13,line);
  text(0,26,pet.questWash?"+ Wash / clean litter":"- Wash / clean litter");
  text(0,39,pet.questPlay?"+ Win a mini-game":"- Win a mini-game");
  footer(pet.questClaimed?"CLAIMED        B:back":"A:claim bonus  B:back");
}
void drawSettings();
void drawSettings() {
  char lines[7][22]; const char *ptrs[7];
  snprintf(lines[0],22,"Sound: %s",pet.sound?"ON":"OFF");
  const char *speeds[]={"Normal","Fast x4","Demo x12"};
  snprintf(lines[1],22,"Time: %s",speeds[pet.speed]);
  snprintf(lines[2],22,"Brightness: %s",pet.contrast==60?"LOW":pet.contrast==180?"MID":"HIGH");
  snprintf(lines[3],22,"Cat name: %s",catNames[pet.name]);
  snprintf(lines[4],22,"Phone Wi-Fi: %s",hotspot?"ON":"OFF");
  snprintf(lines[5],22,"Save progress"); snprintf(lines[6],22,"New cat / reset");
  for(int i=0;i<7;++i) ptrs[i]=lines[i]; drawList("SETTINGS",ptrs,7);
}
void arrow(int x,int y,uint8_t direction,bool filled);
void arrow(int x,int y,uint8_t direction,bool filled) {
  int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
  int tipX=x+dx[direction]*5,tipY=y+dy[direction]*5;
  int baseX=x-dx[direction]*4,baseY=y-dy[direction]*4;
  if(filled) display.fillTriangle(tipX,tipY,baseX+dy[direction]*4,baseY-dx[direction]*4,baseX-dy[direction]*4,baseY+dx[direction]*4,1);
  else display.drawTriangle(tipX,tipY,baseX+dy[direction]*4,baseY-dx[direction]*4,baseX-dy[direction]*4,baseY+dx[direction]*4,1);
}
void drawMini();
void drawMini() {
  text(0,0,"S:"); number(12,0,mg.score);
  if(mg.type!=MEMORY) { text(53,0,"T:"); number(65,0,bound(30-int((nowMs-mg.started)/1000),0,30)); }
  if(mg.type==FISH || mg.type==DASH) { text(92,0,"HP:"); number(110,0,mg.lives); }
  display.drawFastHLine(0,9,128,1);
  if(mg.type==FISH) {
    for(auto &d:drops) if(d.active) {
      if(d.kind) { display.drawLine(d.x-3,d.y-3,d.x+3,d.y+3,1); display.drawLine(d.x-3,d.y+3,d.x+3,d.y-3,1); }
      else { display.fillEllipse(d.x,d.y,4,2,1); display.fillTriangle(d.x+3,d.y,d.x+6,d.y-3,d.x+6,d.y+3,1); }
    }
    display.drawRoundRect(mg.player-9,49,19,6,2,1); footer("<>:catch fish  B:quit");
  } else if(mg.type==DASH) {
    int height=mg.jump?int(sinf((mg.jump-1)*PI/24)*20):0;
    // Compact independent runner sprite; pet needs don't change its jump pose.
    int y=45-height; display.fillRoundRect(23,y-11,12,11,2,1);
    display.fillTriangle(23,y-9,24,y-15,28,y-10,1); display.fillTriangle(35,y-9,34,y-15,30,y-10,1);
    display.drawPixel(31,y-7,0); display.drawLine(35,y-2,40,y-5,1);
    display.fillRect(mg.obstacle,36,8,12,1); display.drawFastHLine(0,49,128,1);
    footer("A/UP:jump     B:quit");
  } else if(mg.type==MEMORY) {
    text(5,13,mg.mode==0?"WATCH":"REPEAT"); text(81,13,"R:"); number(93,13,mg.round);
    const int ax[]={64,64,42,86},ay[]={27,45,36,36};
    for(uint8_t i=0;i<4;++i) arrow(ax[i],ay[i],i,mg.flash==i);
    footer("D-pad:repeat  B:quit");
  } else {
    text(0,14,"POUNCES:"); number(54,14,mg.attempts); text(70,14,"/10");
    display.drawRect(9,30,110,14,1); display.fillRect(54,31,20,12,1);
    int x=10+mg.timing*106/100; display.drawFastVLine(x,26,22,1); display.drawFastVLine(x+1,26,22,1);
    text(16,47,"Hit center zone!"); footer("A:pounce      B:quit");
  }
}
void draw();
void draw() {
  display.clearDisplay(); display.setTextColor(1); display.setTextSize(1); display.setTextWrap(false);
  switch(screen) {
    case ACTION: drawAction(); break;
    case HOME: drawHome(); break;
    case MENU: drawList("POCKETCAT MENU",menuNames,7); break;
    case FOOD: drawList("FOOD BAG",foodNames,4); break;
    case CARE: drawList("CARE & CUDDLES",careNames,7); break;
    case GAMES: drawList("ARCADE",gameNames,4); break;
    case SHOP: drawList("SHOP & ROOM",shopNames,10); break;
    case STATS: drawStats(); break;
    case QUESTS: drawQuests(); break;
    case SETTINGS: drawSettings(); break;
    case NAMES: drawList("CHOOSE CAT NAME",catNames,8); break;
    case RESET:
      header("ADOPT A NEW CAT?"); center(15,"Erases this cat."); center(29,"Hold A 3 seconds");
      bar(20,43,88,resetAt?bound((nowMs-resetAt)/30,0,100):0); footer("B:cancel"); break;
    case MINI: drawMini(); break;
    case RESULT:
      header(selected?"PURRFECT VICTORY!":"GOOD PRACTICE!");
      text(15,18,"Score:"); number(64,18,mg.score); text(15,32,"Coins:"); number(64,32,mg.reward);
      footer("A/B:back to arcade"); break;
    case EVENT: {
      const char *titles[]={"A NEIGHBOR VISITS","A STRAY CAT VISITS","MYSTERY IN THE YARD"};
      const char *left[]={"Gift","Treat","Explore"}; const char *right[]={"Play","Wave","Rest"};
      header(titles[eventKind]); center(17,"A little adventure!");
      text(7,36,left[eventKind]); text(77,36,right[eventKind]);
      display.drawRoundRect(eventSelection?72:2,32,54,15,2,1); footer("<>:choose A:do B:skip"); break;
    }
  }
  if(screen==HOME || screen==RESULT) for(auto &p:particles) if(p.life) {
    if(p.type) heart(p.x,p.y); else { display.drawPixel(p.x,p.y,1); display.drawPixel(p.x+1,p.y+1,1); }
  }
  if(noticeUntil && !due(nowMs,noticeUntil) && screen!=MINI && screen!=RESET && screen!=ACTION) {
    display.fillRoundRect(0,21,128,20,3,0); display.drawRoundRect(0,21,128,20,3,1); center(27,noticeText);
  }
  display.display();
}
void visualTick();
void visualTick() {
  ++phase;
  if(screen==ACTION && nowMs-animAt>=animationLengths[anim]) endAnimation();
  if(screen==HOME && !pet.asleep && anim==IDLE && nowMs-lastHomeMove>4000 && due(nowMs,nextIdle)) {
    idleStyle=random(3); targetX=random(32,91); nextIdle=nowMs+random(5000,10000);
  }
  if(!pet.asleep) catX+=catX<targetX?1:(catX>targetX?-1:0);
  for(auto &p:particles) if(p.life) { --p.life; p.x+=p.vx; p.y+=p.vy; }
}
#if ENABLE_COMPANION
void setupCompanion();
void setupCompanion() {
  server.on("/",HTTP_GET,[](){ server.send_P(200,"text/html",COMPANION_HTML); });
  server.on("/api/status",HTTP_GET,[](){
    char json[440]; snprintf(json,sizeof(json),
      "{\"name\":\"%s\",\"stage\":\"%s\",\"mood\":\"%s\",\"day\":%lu,\"coins\":%u,\"stats\":[%u,%u,%u,%u,%u,%u]}",
      catNames[pet.name],stageNames[stage()],mood(),(unsigned long)(pet.ageMinutes/1440+1),pet.coins,
      pet.full,pet.joy,pet.energy,pet.clean,pet.health,pet.bond);
    server.sendHeader("Cache-Control","no-store"); server.send(200,"application/json",json);
  });
  server.on("/api/action",HTTP_POST,[](){
    if(screen==MINI || screen==RESET || screen==EVENT || screen==ACTION) { server.send(409,"text/plain","Finish the current game or dialog first."); return; }
    // Reject foreign origins; companion controls originate at this AP address.
    String origin=server.header("Origin");
    if(origin.length() && origin!="http://192.168.4.1") { server.send(403,"text/plain","Use the local dashboard."); return; }
    static uint32_t actionAt=0;
    if(nowMs-actionAt<700) { server.send(429,"text/plain","One moment..."); return; }
    actionAt=nowMs; String action=server.arg("action"); bool ok=false;
    if(action=="pet") ok=care(0);
    else if(action=="feed") ok=feed(0);
    else if(action=="wash") ok=care(1);
    else if(action=="sleep") ok=care(3);
    else { server.send(400,"text/plain","Unknown action."); return; }
    server.send(ok?200:409,"text/plain",noticeText);
  });
  const char *headers[]={"Origin"}; server.collectHeaders(headers,1);
  server.onNotFound([](){ server.send(404,"text/plain","Open http://192.168.4.1/"); });
}
#endif
} // namespace

void gameSetup();
void gameSetup() {
  Serial.begin(115200); nowMs=millis(); randomSeed(esp_random());
  for(uint8_t pin:keyPins) pinMode(pin,INPUT_PULLUP);
  pinMode(PIN_BUZZER,OUTPUT); digitalWrite(PIN_BUZZER,LOW);
  load();
  Wire.begin(OLED_SDA,OLED_SCL); Wire.setClock(400000); Wire.setTimeOut(40);
  uint8_t address=0;
  for(uint8_t candidate:{uint8_t(0x3C),uint8_t(0x3D)}) {
    Wire.beginTransmission(candidate); if(Wire.endTransmission()==0) { address=candidate; break; }
  }
  if(!address) { Serial.printf("OLED missing. SDA=%u SCL=%u. Check 3V3/GND.\n",OLED_SDA,OLED_SCL); return; }
#if USE_SSD1306
  oledReady=display.begin(SSD1306_SWITCHCAPVCC,address);
  if(oledReady) { display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(pet.contrast); }
#else
  oledReady=display.begin(address,true); if(oledReady) display.setContrast(pet.contrast);
#endif
  if(!oledReady) { Serial.println("OLED initialization failed. Check controller setting."); return; }
  display.clearDisplay(); display.setTextColor(1); display.setTextSize(1); display.setTextWrap(false);
  center(8,"POCKETCAT32"); drawCat(64,47); center(56,"A:menu  B:cuddle"); display.display();
#if ENABLE_COMPANION
  WiFi.mode(WIFI_OFF); setupCompanion();
#endif
  nowMs=millis(); simAt=frameAt=saveAt=nowMs; nextIdle=nowMs+5000;
  Serial.printf("PocketCat32 ready: OLED 0x%02X, save %u bytes, free heap %u\n",address,unsigned(sizeof(pet)),unsigned(ESP.getFreeHeap()));
  if(!storageReady) notify("NVS SAVE UNAVAILABLE",250);
}
void gameLoop();
void gameLoop() {
  nowMs=millis();
  if(!oledReady) { delay(5); return; }
  if(beepUntil && due(nowMs,beepUntil)) { noTone(PIN_BUZZER); beepUntil=0; }
  readKeys(); handleControls();
  const uint16_t minuteMs[]={1000,250,83}; uint16_t interval=minuteMs[pet.speed];
  // Limited catch-up avoids freezing the UI following a slow Wi-Fi client.
  uint8_t steps=0;
  while(nowMs-simAt>=interval && steps<30) { simAt+=interval; simulateMinute(); ++steps; }
  if(nowMs-simAt>30000) simAt=nowMs;
  if(nowMs-saveAt>=SAVE_MS) { save(); saveAt=nowMs; }
  // Random events only interrupt the home scene, at most once per 3 minutes.
  if(screen==HOME && !pet.asleep && !pet.sick && nowMs-eventAt>=180000) {
    eventAt=nowMs;
    if(random(100)<65) { eventKind=random(3); eventSelection=0; screen=EVENT; sound(1200,130); }
  }
#if ENABLE_COMPANION
  if(hotspot) { server.handleClient(); if(nowMs-hotspotAt>=600000) { stopHotspot(); notify("Hotspot timed out"); } }
#endif
  if(nowMs-frameAt>=FRAME_MS) { frameAt=nowMs; visualTick(); draw(); }
  delay(1); // Yield to ESP32 background tasks and watchdog.
}

void setup();
void loop();
void setup() { gameSetup(); }
void loop() { gameLoop(); }
