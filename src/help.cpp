// Help screen: how to play, in seven pages. Text is word-wrapped at draw time.
#include "gfx.h"
#include "screen.h"

struct HelpPage {
  const char *title;
  const char *body;  // paragraphs separated by '\n'; each one gets a bullet
};

static const HelpPage PAGES[] = {
    {"HOW TO PLAY",
     "YOUR PET STARTS AS AN EGG. TAP IT TO WARM IT: IT HATCHES AFTER 10 TAPS OR 2 MINUTES.\n"
     "KEEP IT FED, HAPPY, RESTED AND CLEAN. IT GROWS FROM BABY TO CHILD TO TEEN, THEN EVOLVES INTO AN ADULT.\n"
     "BOTTOM BAR: HOME SHOWS YOUR PET, MENU OPENS EVERYTHING ELSE, CLOCK IS THE CLOCK AND ALARM.\n"
     "IN THE MENU PICK A ROOM: FEED, PLAY, CLEAN, BAG OR SLEEP. TAP THE ROUND ICONS AT THE BOTTOM RIGHT OF A ROOM TO USE THEM.\n"
     "TAP YOUR PET TO PET IT. THE ARROW AT THE TOP LEFT GOES BACK."},
    {"THE STATS",
     "FOOD (APPLE): HOW FULL IT IS. IT DRAINS ALL DAY.\n"
     "JOY (HEART): HOW HAPPY IT IS. PLAY AND TREATS RAISE IT.\n"
     "ENERGY (BOLT): HOW RESTED IT IS. SLEEP RESTORES IT.\n"
     "THE BARS TURN RED WHEN LOW. WORDS UNDER THE BARS TELL YOU WHAT IS WRONG.\n"
     "DISCIPLINE AND BOND ARE HIDDEN: HOW WELL BEHAVED IT IS AND HOW CLOSE YOU ARE.\n"
     "MISTAKES COUNT CALLS YOU ANSWERED TOO LATE.\n"
     "THE STATS SCREEN IN THE MENU SHOWS EVERYTHING AND SAYS WHAT TO DO NEXT."},
    {"FIXING PROBLEMS",
     "HUNGRY: OPEN FEED AND GIVE FOOD. RICE IS FREE.\n"
     "UNHAPPY OR SAD: OPEN PLAY. TREATS LIKE DANGO AND CAKE CHEER IT UP TOO.\n"
     "SLEEPY OR EXHAUSTED: OPEN SLEEP AND PUT IT TO BED.\n"
     "TOO DIRTY: OPEN CLEAN AND SWEEP. 3 PILES FOR 15 MINUTES MAKE IT SICK.\n"
     "FEELS ILL: GIVE MEDICINE (BUY IT IN THE SHOP) WITHIN 90 MINUTES OR IT FADES AWAY.\n"
     "GRUMPY: TAP THE PET WITHIN 5 MINUTES TO SCOLD IT.\n"
     "ANSWER A CALL WITHIN 15 MINUTES TO AVOID A MISTAKE."},
    {"EVOLUTION",
     "BABY LASTS 30 MINUTES, CHILD 6 HOURS, TEEN 24 HOURS. THEN IT EVOLVES.\n"
     "THE FORM DEPENDS ON YOUR MISTAKES, DISCIPLINE, BOND, JOY AND ENERGY AT THAT MOMENT.\n"
     "FEW MISTAKES AND HIGH DISCIPLINE GIVE THE BEST FORMS, LIKE THE KITSUNE.\n"
     "A HAPPY, BONDED PET CAN BECOME A MOON RABBIT. A NEGLECTED ONE MAY BECOME A SERPENT.\n"
     "RAISE DISCIPLINE BY SCOLDING TANTRUMS, USING SCROLLS AND TRAINING.\n"
     "THE DEX SHOWS A HINT FOR EACH OF THE 12 FORMS."},
    {"GAMES AND MISSIONS",
     "OPEN PLAY TO FIND SHURIKEN, SHELL AND MISSION.\n"
     "SHURIKEN: TAP TO THROW WHEN THE TARGET WILL BE IN FRONT OF YOUR PET. 5 THROWS, BETTER AIM PAYS MORE.\n"
     "SHELL: WATCH WHICH CLOUD HIDES THE PET, THEN TAP IT AFTER THE SHUFFLE.\n"
     "GAMES EARN RYO AND JOY BUT COST ENERGY.\n"
     "MISSION: YOUR PET LEAVES FOR 5 TO 30 MINUTES AND COMES BACK WITH RYO AND BONUSES. IT NEEDS TO BE HEALTHY AND NOT TIRED."},
    {"SHOP AND RYO",
     "RYO IS THE MONEY. EARN IT BY PLAYING, CLEANING, SCOLDING, EVOLVING, GAMES AND MISSIONS.\n"
     "THE SHOP SELLS 12 FOODS, CHAKRA, MEDICINE AND SCROLLS. USE THE FOOD AND ITEMS TABS AND BACK / NEXT.\n"
     "BIG MEALS FILL FOOD, TREATS RAISE JOY, MATCHA RESTORES ENERGY.\n"
     "THE BAG HOLDS UP TO 9 OF EACH. ONLY FOODS YOU OWN SHOW IN THE FEED LIST."},
    {"DAY AND NIGHT",
     "AT NIGHT (21:00 TO 06:00) YOUR PET SLEEPS. IF YOU WAKE IT, IT STAYS UP UNTIL MORNING.\n"
     "IN A DARK ROOM IT GOES TO BED TOO, IF THE LIGHT SENSOR WORKS.\n"
     "SET THE REAL TIME ON THE CLOCK SCREEN, OR USE WI-FI TIME SYNC.\n"
     "THE ALARM WAKES THE PET AND BEEPS FOR A MINUTE. TAP TO STOP IT.\n"
     "THE SCREEN DIMS WHEN IT IS DARK, LATE OR THE PET SLEEPS. TOUCH TO BRIGHTEN IT."},
};
static const int PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);

static const int TEXT_X = 22, TEXT_W = 202, LINE_H = 10, PARA_GAP = 3;
static const int BODY_Y = 50, BODY_H = 220;
static int pageNo = 0;

static void drawPage() {
  drawGradientBg(R_SKY);
  blitSprite(&tft, ROOMUI_FRAMES[ROOMUI_BACK], 4, 4);
  gradPanel(&tft, 54, 8, 182, 34, R_BUTTER, 5, 0);
  drawTextC(&tft, FONT_CHUNK, PAGES[pageNo].title, 145, 25, 1, C_WHITE, C_OUT, TXT_OUTLINE);

  gradPanel(&tft, 4, BODY_Y, 232, BODY_H, R_CREAM, 5, 0);
  const char *p = PAGES[pageNo].body;
  int y = BODY_Y + 10;
  while (*p) {
    // one paragraph: a bullet, then the text wrapped word by word
    const char *end = strchr(p, '\n');
    int plen = end ? end - p : strlen(p);
    tft.fillRect(TEXT_X - 9, y + 2, 4, 4, C_OUT);
    char line[48];
    int ll = 0, lineW = 0;
    int i = 0;
    while (i <= plen) {
      int j = i;
      while (j < plen && p[j] != ' ') j++;  // the next word is p[i..j)
      char word[40];
      int wl = min(j - i, 39);
      memcpy(word, p + i, wl);
      word[wl] = 0;
      int ww = textWidth(FONT_SILK, word, 1);
      int sp = ll ? textWidth(FONT_SILK, " ", 1) : 0;
      if (ll && lineW + sp + ww > TEXT_W) {  // does not fit: flush the line
        line[ll] = 0;
        drawTextC(&tft, FONT_SILK, line, TEXT_X, y + 4, 1, C_TEXT, C_WHITE, TXT_LEFTX | TXT_SHADOW);
        y += LINE_H;
        ll = 0;
        lineW = 0;
        sp = 0;
      }
      if (ll) line[ll++] = ' ';
      memcpy(line + ll, word, wl);
      ll += wl;
      lineW += sp + ww;
      i = j + 1;
    }
    if (ll) {
      line[ll] = 0;
      drawTextC(&tft, FONT_SILK, line, TEXT_X, y + 4, 1, C_TEXT, C_WHITE, TXT_LEFTX | TXT_SHADOW);
      y += LINE_H;
    }
    y += PARA_GAP;
    p += plen + (end ? 1 : 0);
  }
  pageNavDraw(pageNo, PAGE_COUNT);
}

static void helpEnter() {
  pageNo = 0;
  drawPage();
}

static void helpUpdate() {}

static void helpTouch(int x, int y) {
  if (x < 52 && y < 52) {
    gotoScreen(SCR_MENU);
    return;
  }
  int nav = pageNavHit(x, y, pageNo, PAGE_COUNT);
  if (nav >= 0) {
    pageNo += nav == 0 ? -1 : 1;
    fadeThrough(drawPage);
  }
}

const Screen HELP_SCREEN = {helpEnter, helpUpdate, helpTouch};
