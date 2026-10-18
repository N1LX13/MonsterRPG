
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/pgmspace.h>
#include <string.h>

// ============================================================
// MONSTER RPG v0.9
// Arduino Micro | SSD1306 128x64 I2C | KY-023 joystick
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- HARDWARE ----------------
const int JOY_X = A0;
const int JOY_Y = A1;
const int JOY_SW = 4;

const bool INVERT_X = false;
const bool INVERT_Y = false;

const int JOY_LOW = 350;
const int JOY_HIGH = 670;

// ---------------- SETTINGS ----------------
const int MONSTER_COUNT = 16;
const int TILE_SIZE = 16;
const int EVOLUTION_LEVEL = 5;

const unsigned long MOVE_DELAY = 22;
const unsigned long MENU_DELAY = 180;
const unsigned long ENCOUNTER_COOLDOWN = 7000;
const unsigned long EVOLUTION_TIME = 2400;

// ---------------- STATES ----------------
enum GameState { WORLD, COLLECTION, BATTLE, EVOLUTION };
GameState gameState = WORLD;
GameState previousState = BATTLE;

// ---------------- SPRITES ----------------
const uint8_t sprite0[] PROGMEM = {0x18,0x3C,0x7E,0x5A,0x7E,0x3C,0x24,0x66};
const uint8_t sprite1[] PROGMEM = {0x24,0x5A,0x3C,0x7E,0xDB,0x7E,0x24,0x42};
const uint8_t sprite2[] PROGMEM = {0x18,0x3C,0x7E,0xDB,0x7E,0x24,0x5A,0x42};
const uint8_t sprite3[] PROGMEM = {0x3C,0x7E,0xDB,0xFF,0x7E,0x3C,0x24,0x42};
const uint8_t sprite4[] PROGMEM = {0x42,0xA5,0x5A,0x3C,0x7E,0x3C,0x24,0x42};
const uint8_t sprite5[] PROGMEM = {0x24,0x5A,0x7E,0xDB,0x7E,0x3C,0x5A,0x24};
const uint8_t sprite6[] PROGMEM = {0x3C,0x7E,0xDB,0xFF,0xFF,0x7E,0x24,0x66};
const uint8_t sprite7[] PROGMEM = {0x42,0xA5,0xDB,0x7E,0x3C,0x7E,0x24,0x42};
const uint8_t sprite8[] PROGMEM = {0x18,0x3C,0x7E,0xFF,0x7E,0x3C,0x5A,0x42};
const uint8_t sprite9[] PROGMEM = {0x24,0x7E,0xDB,0xFF,0x7E,0x3C,0x24,0x5A};
const uint8_t sprite10[] PROGMEM = {0x18,0x7E,0xDB,0xFF,0xDB,0x7E,0x3C,0x66};
const uint8_t sprite11[] PROGMEM = {0x42,0x24,0x7E,0xDB,0x7E,0x3C,0x66,0x42};
const uint8_t sprite12[] PROGMEM = {0x18,0x3C,0x7E,0xFF,0xDB,0x7E,0x3C,0x66};
const uint8_t sprite13[] PROGMEM = {0x42,0xA5,0x7E,0xDB,0x7E,0x3C,0x24,0x42};
const uint8_t sprite14[] PROGMEM = {0x18,0x3C,0x7E,0xDB,0xFF,0x7E,0x24,0x42};
const uint8_t sprite15[] PROGMEM = {0x24,0x5A,0x7E,0xFF,0x7E,0x3C,0x66,0x42};

const uint8_t* const monsterSprites[MONSTER_COUNT] PROGMEM = {
  sprite0,sprite1,sprite2,sprite3,sprite4,sprite5,sprite6,sprite7,
  sprite8,sprite9,sprite10,sprite11,sprite12,sprite13,sprite14,sprite15
};

// ---------------- MONSTER DATA ----------------
bool owned[MONSTER_COUNT] = {};
bool evolved[MONSTER_COUNT] = {};

uint8_t monsterLevel[MONSTER_COUNT];
uint8_t monsterXP[MONSTER_COUNT];

int monsterHP[MONSTER_COUNT];
int monsterMaxHP[MONSTER_COUNT];
int monsterAttack[MONSTER_COUNT];

int activeMonster = 0;
int ownedCount = 1;

// ---------------- WORLD ----------------
float playerX = 80;
float playerY = 80;

int cameraX = 0;
int cameraY = 0;

unsigned long lastMove = 0;
unsigned long lastEncounter = 0;

int lastGrassX = -999;
int lastGrassY = -999;

// ---------------- BATTLE ----------------
int enemyMonster = 0;
int enemyLevel = 1;
int enemyHP = 0;
int enemyMaxHP = 0;

int battleAction = 0;
bool magicUsed = false;
bool enemyDefeated = false;

// ---------------- MENUS ----------------
int collectionCursor = 0;
unsigned long lastMenuMove = 0;
unsigned long lastBattleMove = 0;

// ---------------- EVOLUTION ----------------
int evolvingMonster = 0;
unsigned long evolutionStarted = 0;

// ---------------- MESSAGE ----------------
char message[22] = "WELCOME!";

// ============================================================
// SETUP
// ============================================================

void setup() {
  pinMode(JOY_SW, INPUT_PULLUP);
  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    while (true) {}
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(8, 15);
  display.println(F("MONSTER"));
  display.setCursor(26, 37);
  display.println(F("RPG!"));
  display.display();
  delay(1000);

  initializeMonsters();
  randomSeed(analogRead(A2) ^ micros());

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Loading world..."));
  display.display();
  delay(300);
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
  bool pressed = readButton();

  switch (gameState) {
    case WORLD:
      updateWorld(pressed);
      drawWorld();
      break;

    case COLLECTION:
      updateCollection(pressed);
      drawCollection();
      break;

    case BATTLE:
      updateBattle(pressed);
      drawBattle();
      break;

    case EVOLUTION:
      drawEvolution();
      if (millis() - evolutionStarted >= EVOLUTION_TIME) {
        gameState = previousState;
      }
      break;
  }
}

// ============================================================
// INPUT
// ============================================================

bool readButton() {
  static bool lastReading = HIGH;
  static bool stableState = HIGH;
  static unsigned long changedAt = 0;

  bool reading = digitalRead(JOY_SW);

  if (reading != lastReading) {
    lastReading = reading;
    changedAt = millis();
  }

  if (millis() - changedAt >= 30 && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) return true;
  }

  return false;
}

int joyX() {
  int v = analogRead(JOY_X);
  if (INVERT_X) v = 1023 - v;
  if (v < JOY_LOW) return -1;
  if (v > JOY_HIGH) return 1;
  return 0;
}

int joyY() {
  int v = analogRead(JOY_Y);
  if (INVERT_Y) v = 1023 - v;
  if (v < JOY_LOW) return -1;
  if (v > JOY_HIGH) return 1;
  return 0;
}

// ============================================================
// MONSTER INITIALIZATION
// ============================================================

void initializeMonsters() {
  for (int i = 0; i < MONSTER_COUNT; i++) {
    owned[i] = false;
    evolved[i] = false;
    monsterLevel[i] = 1;
    monsterXP[i] = 0;
    monsterMaxHP[i] = 20 + (i % 4) * 2;
    monsterHP[i] = monsterMaxHP[i];
    monsterAttack[i] = 4 + (i % 4);
  }

  owned[0] = true;
  activeMonster = 0;
  ownedCount = 1;
}

// ============================================================
// MONSTER NAMES
// ============================================================

void printMonsterName(int i) {
  if (evolved[i]) printEvolvedName(i);
  else printBaseName(i);
}

void printBaseName(int i) {
  switch (i) {
    case 0: display.print(F("EMBERCUB")); break;
    case 1: display.print(F("MOSSBIT")); break;
    case 2: display.print(F("VOLTFIN")); break;
    case 3: display.print(F("DUSTLING")); break;
    case 4: display.print(F("FROSTFOX")); break;
    case 5: display.print(F("BLOOMBAT")); break;
    case 6: display.print(F("ROCKODILE")); break;
    case 7: display.print(F("SHADOWKIT")); break;
    case 8: display.print(F("AQUADRAKE")); break;
    case 9: display.print(F("MUSHFROG")); break;
    case 10: display.print(F("CRYSTABUG")); break;
    case 11: display.print(F("CLOUDRAM")); break;
    case 12: display.print(F("MAGMANEW")); break;
    case 13: display.print(F("MOONOWL")); break;
    case 14: display.print(F("VINESERP")); break;
    default: display.print(F("STARHARE")); break;
  }
}

void printEvolvedName(int i) {
  switch (i) {
    case 0: display.print(F("EMBERLION")); break;
    case 1: display.print(F("MOSSBEAST")); break;
    case 2: display.print(F("STORMFIN")); break;
    case 3: display.print(F("DUNETITAN")); break;
    case 4: display.print(F("GLACIERFOX")); break;
    case 5: display.print(F("BLOOMWING")); break;
    case 6: display.print(F("TERRACROC")); break;
    case 7: display.print(F("NIGHTPANTHER")); break;
    case 8: display.print(F("TIDEDRAKE")); break;
    case 9: display.print(F("SPORETOAD")); break;
    case 10: display.print(F("CRYSTALORD")); break;
    case 11: display.print(F("SKYRAM")); break;
    case 12: display.print(F("INFERNOVA")); break;
    case 13: display.print(F("ECLIPSEOWL")); break;
    case 14: display.print(F("THORNCOIL")); break;
    default: display.print(F("STARHOPPER")); break;
  }
}

// ============================================================
// WORLD MAP
// ============================================================

unsigned long tileHash(int x, int y) {
  unsigned long h = (unsigned long)x * 374761393UL;
  h ^= (unsigned long)y * 668265263UL;
  h = (h ^ (h >> 13)) * 1274126177UL;
  return h ^ (h >> 16);
}

// 0 grass, 1 tree, 2 flowers, 3 path,
// 4 tall grass, 5 water, 6 stones

int tileAt(int tx, int ty) {
  int pathX = 3 + ((ty / 5) % 4);
  if (abs(tx - pathX) <= 1) return 3;

  if (tx >= 10 && tx <= 13 && ty >= 4 && ty <= 7) {
    if (tx == 10 || tx == 13 || ty == 4 || ty == 7) return 0;
    return 5;
  }

  int r = tileHash(tx, ty) % 100;
  if (r < 9) return 1;
  if (r < 15) return 2;
  if (r < 32) return 4;
  if (r < 36) return 6;
  return 0;
}

bool solidTile(int tile) {
  return tile == 1 || tile == 5;
}

bool canStand(float x, float y) {
  int x1 = (int)(x / TILE_SIZE);
  int x2 = (int)((x + 7) / TILE_SIZE);
  int y1 = (int)(y / TILE_SIZE);
  int y2 = (int)((y + 7) / TILE_SIZE);

  return !solidTile(tileAt(x1, y1)) &&
         !solidTile(tileAt(x2, y1)) &&
         !solidTile(tileAt(x1, y2)) &&
         !solidTile(tileAt(x2, y2));
}

// ============================================================
// WORLD UPDATE
// ============================================================

void updateWorld(bool pressed) {
  if (pressed) {
    collectionCursor = activeMonster;
    gameState = COLLECTION;
    return;
  }

  if (millis() - lastMove >= MOVE_DELAY) {
    int dx = joyX();
    int dy = joyY();

    float nx = playerX + dx * 1.6;
    float ny = playerY + dy * 1.6;

    if (canStand(nx, playerY)) playerX = nx;
    if (canStand(playerX, ny)) playerY = ny;

    lastMove = millis();
  }

  checkEncounter();
}

void checkEncounter() {
  int tx = (int)((playerX + 4) / TILE_SIZE);
  int ty = (int)((playerY + 4) / TILE_SIZE);

  if (tx == lastGrassX && ty == lastGrassY) return;

  lastGrassX = tx;
  lastGrassY = ty;

  if (tileAt(tx, ty) != 4) return;
  if (millis() - lastEncounter < ENCOUNTER_COOLDOWN) return;

  if (random(100) < 25) startBattle();
}

void startBattle() {
  enemyMonster = random(MONSTER_COUNT);
  enemyLevel = random(1, 4);
  enemyMaxHP = 15 + enemyLevel * 4 + random(0, 7);
  enemyHP = enemyMaxHP;

  magicUsed = false;
  enemyDefeated = false;
  battleAction = 0;

  lastEncounter = millis();
  gameState = BATTLE;
  setMessage("WILD MONSTER!");
}

// ============================================================
// WORLD DRAWING
// ============================================================

void drawWorld() {
  display.clearDisplay();

  cameraX = (int)playerX - 60;
  cameraY = (int)playerY - 28;

  if (cameraX < 0) cameraX = 0;
  if (cameraY < 0) cameraY = 0;

  int startTX = cameraX / TILE_SIZE;
  int startTY = cameraY / TILE_SIZE;

  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 9; col++) {
      int tx = startTX + col;
      int ty = startTY + row;
      drawTile(tileAt(tx, ty),
               tx * TILE_SIZE - cameraX,
               ty * TILE_SIZE - cameraY);
    }
  }

  int px = (int)playerX - cameraX;
  int py = (int)playerY - cameraY;

  display.fillCircle(px + 4, py + 3, 3, SSD1306_WHITE);
  display.fillRect(px + 1, py + 5, 6, 4, SSD1306_WHITE);
  display.drawPixel(px + 3, py + 3, SSD1306_BLACK);
  display.drawPixel(px + 5, py + 3, SSD1306_BLACK);

  display.display();
}

void drawTile(int tile, int x, int y) {
  switch (tile) {
    case 0:
      display.drawPixel(x + 3, y + 5, SSD1306_WHITE);
      display.drawPixel(x + 12, y + 11, SSD1306_WHITE);
      break;
    case 1:
      display.fillTriangle(x + 8, y + 1, x + 1, y + 13,
                           x + 15, y + 13, SSD1306_WHITE);
      display.fillRect(x + 6, y + 12, 4, 4, SSD1306_WHITE);
      break;
    case 2:
      display.drawPixel(x + 5, y + 5, SSD1306_WHITE);
      display.drawPixel(x + 4, y + 5, SSD1306_WHITE);
      display.drawPixel(x + 6, y + 5, SSD1306_WHITE);
      display.drawPixel(x + 5, y + 4, SSD1306_WHITE);
      display.drawPixel(x + 5, y + 6, SSD1306_WHITE);
      display.drawPixel(x + 12, y + 11, SSD1306_WHITE);
      break;
    case 3:
      display.drawPixel(x + 3, y + 4, SSD1306_WHITE);
      display.drawPixel(x + 10, y + 10, SSD1306_WHITE);
      break;
    case 4:
      display.drawLine(x + 3, y + 15, x + 5, y + 4, SSD1306_WHITE);
      display.drawLine(x + 7, y + 15, x + 8, y + 2, SSD1306_WHITE);
      display.drawLine(x + 11, y + 15, x + 13, y + 5, SSD1306_WHITE);
      break;
    case 5:
      display.drawLine(x + 2, y + 5, x + 7, y + 5, SSD1306_WHITE);
      display.drawLine(x + 8, y + 11, x + 13, y + 11, SSD1306_WHITE);
      break;
    case 6:
      display.drawCircle(x + 8, y + 8, 3, SSD1306_WHITE);
      break;
  }
}

// ============================================================
// COLLECTION MENU - EACH OWNED MONSTER ONCE
// ============================================================

void updateCollection(bool pressed) {
  if (millis() - lastMenuMove >= MENU_DELAY) {
    int dx = joyX();
    int dy = joyY();

    if (dx < 0 || dy < 0) {
      moveCollection(-1);
      lastMenuMove = millis();
    } else if (dx > 0 || dy > 0) {
      moveCollection(1);
      lastMenuMove = millis();
    }
  }

  if (pressed && owned[collectionCursor]) {
    activeMonster = collectionCursor;
    gameState = WORLD;
  }
}

void moveCollection(int dir) {
  for (int tries = 0; tries < MONSTER_COUNT; tries++) {
    collectionCursor += dir;

    if (collectionCursor < 0) collectionCursor = MONSTER_COUNT - 1;
    if (collectionCursor >= MONSTER_COUNT) collectionCursor = 0;

    if (owned[collectionCursor]) return;
  }
}

void drawCollection() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("MONSTERS "));
  display.print(ownedCount);
  display.print('/');
  display.print(MONSTER_COUNT);

  // Find selected monster's position in the owned-only list.
  int selectedPosition = 0;
  for (int i = 0; i < collectionCursor; i++) {
    if (owned[i]) selectedPosition++;
  }

  int startPosition = selectedPosition - 1;
  if (startPosition < 0) startPosition = 0;

  int row = 0;
  int ownedPosition = 0;

  for (int i = 0; i < MONSTER_COUNT && row < 3; i++) {
    if (!owned[i]) continue;

    if (ownedPosition >= startPosition) {
      int y = 14 + row * 13;

      if (i == collectionCursor) {
        display.fillRect(0, y - 1, 128, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }

      display.setCursor(2, y);
      display.print(i == activeMonster ? '*' : ' ');
      printMonsterName(i);

      display.setCursor(102, y);
      display.print('L');
      display.print(monsterLevel[i]);

      row++;
    }

    ownedPosition++;
  }

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 55);
  display.print(F("MOVE / PRESS: SELECT"));
  display.display();
}

// ============================================================
// BATTLE UPDATE
// ============================================================

void updateBattle(bool pressed) {
  if (millis() - lastBattleMove >= MENU_DELAY) {
    int dx = joyX();
    int dy = joyY();

    if (dx < 0 || dy < 0) {
      battleAction--;
      if (battleAction < 0) battleAction = 3;
      lastBattleMove = millis();
    } else if (dx > 0 || dy > 0) {
      battleAction++;
      if (battleAction > 3) battleAction = 0;
      lastBattleMove = millis();
    }
  }

  if (pressed) chooseBattleAction();
}

void chooseBattleAction() {
  if (battleAction == 0) {
    if (enemyHP <= 0) {
      setMessage("CHOOSE CATCH!");
      return;
    }

    int damage = monsterAttack[activeMonster] +
                 monsterLevel[activeMonster] + random(1, 4);

    enemyHP -= damage;
    if (enemyHP < 0) enemyHP = 0;

    if (enemyHP == 0) defeatEnemy();
    else enemyTurn();
  }
  else if (battleAction == 1) {
    if (magicUsed) {
      setMessage("MAGIC USED!");
      return;
    }

    if (enemyHP <= 0) {
      setMessage("CHOOSE CATCH!");
      return;
    }

    magicUsed = true;

    int damage = monsterAttack[activeMonster] * 2 +
                 monsterLevel[activeMonster] + 3 + random(0, 4);

    enemyHP -= damage;
    if (enemyHP < 0) enemyHP = 0;

    if (enemyHP == 0) defeatEnemy();
    else enemyTurn();
  }
  else if (battleAction == 2) {
    if (enemyHP > 0) {
      setMessage("DEFEAT IT FIRST!");
      return;
    }

    if (!owned[enemyMonster]) {
      owned[enemyMonster] = true;
      ownedCount++;

      monsterLevel[enemyMonster] = enemyLevel;
      monsterXP[enemyMonster] = 0;
      monsterMaxHP[enemyMonster] = enemyMaxHP;
      monsterHP[enemyMonster] = enemyMaxHP;
      monsterAttack[enemyMonster] = 4 + enemyLevel;

      setMessage("MONSTER CAUGHT!");
    } else {
      setMessage("ALREADY OWNED!");
    }

    gameState = WORLD;
  }
  else {
    if (random(100) < 80) {
      setMessage("GOT AWAY!");
      gameState = WORLD;
    } else {
      setMessage("CANT ESCAPE!");
      enemyTurn();
    }
  }
}

// ============================================================
// EXPERIENCE AND ENEMY TURN
// ============================================================

void defeatEnemy() {
  if (enemyDefeated) return;
  enemyDefeated = true;

  monsterXP[activeMonster]++;

  if (monsterXP[activeMonster] >= 3) {
    monsterXP[activeMonster] = 0;
    monsterLevel[activeMonster]++;

    monsterMaxHP[activeMonster] += 3;
    monsterHP[activeMonster] = monsterMaxHP[activeMonster];
    monsterAttack[activeMonster]++;

    setMessage("LEVEL UP!");

    if (monsterLevel[activeMonster] >= EVOLUTION_LEVEL &&
        !evolved[activeMonster]) {
      beginEvolution(activeMonster);
    }
  } else {
    setMessage("ENEMY DEFEATED!");
  }
}

void enemyTurn() {
  if (enemyHP <= 0) return;

  int damage = 2 + enemyLevel + random(0, 3);
  monsterHP[activeMonster] -= damage;

  if (monsterHP[activeMonster] <= 0) {
    monsterHP[activeMonster] = 0;

    int replacement = findHealthyMonster();

    if (replacement >= 0) {
      activeMonster = replacement;
      setMessage("NEXT MONSTER!");
    } else {
      monsterHP[activeMonster] = monsterMaxHP[activeMonster];
      gameState = WORLD;
      setMessage("TEAM RESTED!");
    }
  }
}

int findHealthyMonster() {
  for (int i = 0; i < MONSTER_COUNT; i++) {
    if (owned[i] && monsterHP[i] > 0) return i;
  }
  return -1;
}

// ============================================================
// EVOLUTION
// ============================================================

void beginEvolution(int i) {
  if (evolved[i]) return;

  evolvingMonster = i;
  evolved[i] = true;

  monsterMaxHP[i] += 12;
  monsterHP[i] = monsterMaxHP[i];
  monsterAttack[i] += 4;

  previousState = BATTLE;
  evolutionStarted = millis();
  gameState = EVOLUTION;
}

void drawEvolution() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(27, 1);
  display.print(F("EVOLUTION!"));

  display.drawRect(8, 13, 112, 35, SSD1306_WHITE);

  drawMonsterSprite(evolvingMonster, 24, 21, false);

  display.setCursor(49, 29);
  display.print(F(">>"));

  drawMonsterSprite(evolvingMonster, 88, 21, true);

  if ((millis() / 150) % 2 == 0) {
    display.drawPixel(17, 18, SSD1306_WHITE);
    display.drawPixel(110, 17, SSD1306_WHITE);
    display.drawPixel(64, 16, SSD1306_WHITE);
    display.drawPixel(64, 44, SSD1306_WHITE);
  }

  display.setCursor(0, 51);
  printMonsterName(evolvingMonster);
  display.setCursor(0, 57);
  display.print(F("EVOLVED!"));

  display.display();
}

// ============================================================
// SPRITE DRAWING
// ============================================================

void drawMonsterSprite(int i, int x, int y, bool evolvedForm) {
  const uint8_t* bitmap =
    (const uint8_t*)pgm_read_ptr(&monsterSprites[i]);

  for (int row = 0; row < 8; row++) {
    uint8_t bits = pgm_read_byte(bitmap + row);

    for (int col = 0; col < 8; col++) {
      if (bits & (0x80 >> col)) {
        display.fillRect(x + col * 2, y + row * 2,
                         2, 2, SSD1306_WHITE);
      }
    }
  }

  if (evolvedForm) {
    display.drawPixel(x + 3, y - 1, SSD1306_WHITE);
    display.drawPixel(x + 12, y - 1, SSD1306_WHITE);
    display.drawPixel(x + 7, y - 2, SSD1306_WHITE);
    display.drawPixel(x + 16, y + 8, SSD1306_WHITE);
    display.drawPixel(x - 1, y + 8, SSD1306_WHITE);
  }
}

// ============================================================
// BATTLE DISPLAY
// ============================================================

void drawBattle() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  printMonsterName(enemyMonster);

  display.setCursor(0, 9);
  display.print(F("HP:"));
  drawHealthBar(22, 10, 45, enemyHP, enemyMaxHP);

  drawMonsterSprite(enemyMonster, 91, 2, evolved[enemyMonster]);

  display.setCursor(0, 22);
  printMonsterName(activeMonster);

  display.setCursor(0, 31);
  display.print(F("HP:"));
  drawHealthBar(22, 32, 45,
                monsterHP[activeMonster],
                monsterMaxHP[activeMonster]);

  display.setCursor(0, 42);
  display.print(F("LV"));
  display.print(monsterLevel[activeMonster]);

  display.setCursor(30, 42);
  display.print(F("XP "));
  display.print(monsterXP[activeMonster]);
  display.print(F("/3"));

  const int actionX[4] = {0, 32, 64, 96};

  for (int i = 0; i < 4; i++) {
    if (i == battleAction) {
      display.fillRect(actionX[i], 53, 31, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.drawRect(actionX[i], 53, 31, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(actionX[i] + 3, 55);

    if (i == 1 && magicUsed) display.print(F("USED"));
    else if (i == 0) display.print(F("ATK"));
    else if (i == 1) display.print(F("MAG"));
    else if (i == 2) display.print(F("CATCH"));
    else display.print(F("RUN"));
  }

  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void drawHealthBar(int x, int y, int width, int hp, int maxHP) {
  display.drawRect(x, y, width, 6, SSD1306_WHITE);
  if (maxHP <= 0) return;

  int fillWidth = hp * (width - 2) / maxHP;
  if (fillWidth < 0) fillWidth = 0;
  if (fillWidth > width - 2) fillWidth = width - 2;

  display.fillRect(x + 1, y + 1, fillWidth, 4, SSD1306_WHITE);
}

// ============================================================
// MESSAGE HELPER
// ============================================================

void setMessage(const char* text) {
  strncpy(message, text, sizeof(message) - 1);
  message[sizeof(message) - 1] = '\0';
}
