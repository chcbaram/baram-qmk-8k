#include "cli.h"
#include "cli_gui.h"


#ifdef _USE_HW_CLI_GUI




#define CHARSET_G0      0
#define CHARSET_G1      1


static uint16_t cli_gui_w = CLI_GUI_WIDTH;
static uint16_t cli_gui_h = CLI_GUI_HEIGHT;


static uint8_t cli_gui_cury = 0xff;    
static uint8_t cli_gui_curx = 0xff;    
static bool    cli_gui_is_init = false;

static uint8_t cli_gui_scrl_start = 0;   
static uint8_t cli_gui_scrl_end   = CLI_GUI_HEIGHT - 1;

static char line_buf[1024];




static void initScreen(int16_t w, int16_t h)
{

  cli_gui_w = w;
  cli_gui_h = h;

  cliPrintf(SEQ_LOAD_G1);

  cliGui()->showCursor(false);
  cliGui()->setAttr(A_NORMAL);
  cliGui()->move(0, 0);
  cliGui()->clear();
  cli_gui_is_init = true;
}

static void closeScreen(void)
{  
  cliGui()->setAttr(A_NORMAL);
  cliGui()->showCursor(true);

  cliGui()->addPrintf("close");
  cliGui()->clear();
}

static uint32_t getWidth(void)
{
  return cli_gui_w;
}

static uint32_t getHeight(void)
{
  return cli_gui_h;
}

static void setAttr(uint16_t attr)
{
  static uint16_t cur_attr = 0xff;     
  uint8_t idx = 0;

  if (attr != cur_attr)
  {
    cliPrintf(SEQ_ATTRSET);

    idx = (attr & F_COLOR) >> 8;

    if (idx >= 1 && idx <= 8)
    {
      cliPrintf(SEQ_ATTRSET_FCOLOR);
      cliPutch(idx - 1 + '0');
    }

    idx = (attr & B_COLOR) >> 12;

    if (idx >= 1 && idx <= 8)
    {
      cliPrintf(SEQ_ATTRSET_BCOLOR);
      cliPutch(idx - 1 + '0');
    }

    if (attr & A_REVERSE)
    {
      cliPrintf(SEQ_ATTRSET_REVERSE);
    }
    if (attr & A_UNDERLINE)
    {
      cliPrintf(SEQ_ATTRSET_UNDERLINE);
    }
    if (attr & A_BLINK)
    {
      cliPrintf(SEQ_ATTRSET_BLINK);
    }
    if (attr & A_BOLD)
    {
      cliPrintf(SEQ_ATTRSET_BOLD);
    }
    if (attr & A_DIM)
    {
      cliPrintf(SEQ_ATTRSET_DIM);
    }
    cliPutch('m');
    cur_attr = attr;
  }
}

static void clear(void)
{
  cliPrintf(SEQ_CLEAR);
}

static void guiMove(uint8_t x, uint8_t y)
{
  cliPrintf("%s%d;%dH", SEQ_CSI, y+1,x+1);
}

static void move(uint8_t x, uint8_t y)
{
  if (cli_gui_cury != y || cli_gui_curx != x)
  {
    cli_gui_cury = y;
    cli_gui_curx = x;
    guiMove(x, y);
  }
}



static void addCh_Or_InsCh (uint8_t ch, bool insert)
{
  static uint8_t  charset = 0xff;
  static uint8_t  insert_mode = false;

  if (ch >= 0x80 && ch <= 0x9F)
  {
    if (charset != CHARSET_G1)
    {
      cliPutch('\016');         
      charset = CHARSET_G1;
    }
    ch -= 0x20;                 
  }
  else
  {
    if (charset != CHARSET_G0)
    {
      cliPutch('\017');         
      charset = CHARSET_G0;
    }
  }

  if (insert)
  {
    if (! insert_mode)
    {
      cliPrintf(SEQ_INSERT_MODE);
      insert_mode = true;
    }
  }
  else
  {
    if (insert_mode)
    {
      cliPrintf(SEQ_REPLACE_MODE);
      insert_mode = false;
    }
  }

  cliPutch(ch);
  cli_gui_curx++;
}

static void addChar(uint8_t ch)
{
  addCh_Or_InsCh (ch, false);
}

static void drawBoxLine(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *title)
{
  uint8_t line;
  uint8_t col;

  uint32_t title_pos;
  uint32_t title_len;

  title_len = strlen(title);
  if (title_len > 0)
  {
    title_pos = 0;

    if (w > title_len)
      title_pos = (w - title_len)/2;
  }

  cliGui()->move(x, y);
  cliGui()->addChar(ACS_ULCORNER);
  if (title_len == 0)
  {
    for (col = 0; col < w - 2; col++)
    {
      cliGui()->addChar(ACS_HLINE);
    }
    cliGui()->addChar(ACS_URCORNER);
  }
  else
  {
    for (col = 0; col < title_pos - 1; col++)
    {
      cliGui()->addChar(ACS_HLINE);
    }
    cliGui()->addPrintf(title);  
    for (col = 0; col < w - title_pos - title_len - 1; col++)
    {
      cliGui()->addChar(ACS_HLINE);
    }

    cliGui()->addChar(ACS_URCORNER);    
  }
  for (line = 0; line < h - 2; line++)
  {
    cliGui()->move(x, line + y + 1);
    cliGui()->addChar(ACS_VLINE);
    cliGui()->move(x + w - 1, line + y + 1);
    cliGui()->addChar(ACS_VLINE);
  }

  cliGui()->move (x, y + h - 1);
  cliGui()->addChar(ACS_LLCORNER);
  for (col = 0; col < w - 2; col++)
  {
    cliGui()->addChar(ACS_HLINE);
  }
  cliGui()->addChar(ACS_LRCORNER);

}

static void drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *title)
{
  uint8_t line;
  uint8_t col;

  uint32_t title_pos;
  uint32_t title_len;

  title_len = strlen(title);
  if (title_len > 0)
  {
    title_pos = 0;

    if (w > title_len)
      title_pos = (w - title_len)/2;
  }

  cliGui()->move(x, y);
  cliGui()->addChar('+');
  if (title_len == 0)
  {
    for (col = 0; col < w - 2; col++)
    {
      cliGui()->addChar('-');
    }
    cliGui()->addChar('+');
  }
  else
  {
    for (col = 0; col < title_pos - 1; col++)
    {
      cliGui()->addChar('|');
    }
    cliGui()->addPrintf(title);  
    for (col = 0; col < w - title_pos - title_len - 1; col++)
    {
      cliGui()->addChar('-');
    }

    cliGui()->addChar('+');    
  }
  for (line = 0; line < h - 2; line++)
  {
    cliGui()->move(x, line + y + 1);
    cliGui()->addChar('|');
    cliGui()->move(x + w - 1, line + y + 1);
    cliGui()->addChar('|');
  }

  cliGui()->move (x, y + h - 1);
  cliGui()->addChar('+');
  for (col = 0; col < w - 2; col++)
  {
    cliGui()->addChar('-');
  }
  cliGui()->addChar('+');

}

static void eraseBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
  uint8_t line;
  uint8_t col;


  cliGui()->move(x, y);
  cliGui()->addChar(' ');
  for (col = 0; col < w - 2; col++)
  {
    cliGui()->addChar(' ');
  }
  cliGui()->addChar(' ');
  for (line = 0; line < h - 2; line++)
  {
    cliGui()->move(x, line + y + 1);
    cliGui()->addChar(' ');
    cliGui()->move(x + w - 1, line + y + 1);
    cliGui()->addChar(' ');
  }

  cliGui()->move (x, y + h - 1);
  cliGui()->addChar(' ');
  for (col = 0; col < w - 2; col++)
  {
    cliGui()->addChar(' ');
  }
  cliGui()->addChar(' ');

}

static void addStr(const char * str)
{
  while (*str)
  {
    addCh_Or_InsCh(*str++, false);
  }
}

static void showCursor(bool visibility)
{
    cliPrintf(SEQ_CURSOR_VIS);

  if (visibility == false)
  {
    cliPutch('l');
  }
  else
  {
    cliPutch('h');
  }
}

static void moveAddStr(uint8_t x, uint8_t y, const char *p_str)
{
  cliGui()->move(x, y);
  cliGui()->addStr(p_str);
}

static void showTopLine(const char * str)
{
  int col;
  uint32_t str_pos = 0;
  uint32_t str_len;

  str_len = strlen(str);
  if (str_len > 0)
  {
    str_pos = 0;

    if (getWidth() > str_len)
      str_pos = (getWidth() - str_len)/2;
  }

  cliGui()->move(0, 1);
  cliGui()->setAttr(A_BOLD | F_WHITE | B_BLUE);

  for (col = 0; col < str_pos; col++)
  {
    cliGui()->addChar(' ');
  }
  cliGui()->addStr(str);
  for (col = 0; col < getWidth() - str_pos - str_len; col++)
  {
    cliGui()->addChar(' ');
  }

  cliGui()->setAttr(A_NORMAL);
}

static void showBottomLine(const char * str)
{
  uint8_t col;

  cliGui()->move (0, cli_gui_h - 3);
  cliGui()->setAttr (A_BOLD | F_WHITE | B_BLUE);

  for (col = 0; col < cli_gui_w; col++)
  {
    cliGui()->addChar(' ');
  }

  cliGui()->moveAddStr(2, cli_gui_h - 3, str);
  cliGui()->setAttr(A_NORMAL);
}

static void addPrintf(const char *fmt, ...)
{
  va_list arg;
  va_start (arg, fmt);
  
  vsnprintf(line_buf, 1024, fmt, arg);
  va_end (arg);

  addStr(line_buf);
}

static void movePrintf(uint8_t x, uint8_t y, const char *fmt, ...)
{
  va_list arg;
  va_start (arg, fmt);
  
  vsnprintf(line_buf, 1024, fmt, arg);
  va_end (arg);

  moveAddStr(x, y, line_buf);
}

static void setScrollArea(uint8_t top, uint8_t bottom)
{
  cli_gui_scrl_start = top;
  cli_gui_scrl_end = bottom;
}

static void delChar(void)
{
  cliPrintf(SEQ_DELCH);
}

static void shiftLeft(uint8_t x, uint8_t y, uint8_t ch)
{
  uint8_t col;

  move(getWidth() - 2, y);
  addChar (ch);
  move(x, y);

  for (col = getWidth() - 2; col > x; col--)
  {
    delay(5);
    delChar();
  }
}

static void shiftLeftStr(uint8_t x, uint8_t y, char * str)
{
  char *  s;
  uint8_t xx = x;

  setAttr(F_RED);

  for (s = str; *s; s++)
  {
    if (*s != ' ')
    {
      shiftLeft(xx, y, *s);
    }
    xx++;
  }

  move(x, y);
  setAttr(A_REVERSE);

  for (s = str; *s; s++)
  {
    addChar(*s);
    delay(25);
  }

  move(x, y);
  setAttr(F_BLUE);

  for (s = str; *s; s++)
  {
    addChar(*s);
    delay(25);
  }
}

static void guiSetScrollArea(uint_fast8_t top, uint_fast8_t bottom)
{
  if (top == bottom)
  {
    cliPrintf(SEQ_RESET_SCRREG); // reset scrolling region
  }
  else
  {
    cliPrintf("%s%d;%dr", SEQ_CSI, top + 1, bottom + 1);
  }
}

static void scroll(void)
{
    guiSetScrollArea (cli_gui_scrl_start, cli_gui_scrl_end);              // set scrolling region
    guiMove(0, cli_gui_scrl_end);                                         // goto to last line of scrolling region
    cliPrintf(SEQ_NEXTLINE);                                              // next line
    guiSetScrollArea (0, 0);                                              // reset scrolling region
    guiMove(cli_gui_curx, cli_gui_cury);                                  // restore position
}

static void insertLine(void)
{
    guiSetScrollArea(cli_gui_cury, cli_gui_scrl_end);                     // set scrolling region
    guiMove(0, cli_gui_cury);                                             // goto to current line
    cliPrintf(SEQ_INSERTLINE);                                            // insert line
    guiSetScrollArea(0, 0);                                               // reset scrolling region
    guiMove(cli_gui_curx, cli_gui_cury);                                  // restore position
}

static void insChar(uint8_t ch)
{
  addCh_Or_InsCh(ch, true);
}

static void clearToEol(void)
{
  cliPrintf(SEQ_CLRTOEOL);
}

static void message(const char * msg)
{
  move(0, getHeight() - 2);
  addStr(msg);
  clearToEol();
}

cli_gui_api_t *cliGui(void)
{
  static cli_gui_api_t cli_gui_api = 
  {
    .initScreen = initScreen,
    .closeScreen = closeScreen,
    .getWidth = getWidth,
    .getHeight = getHeight,
    .setAttr = setAttr,
    .clear = clear,
    .move = move,
    .addChar = addChar,
    .addStr = addStr,
    .moveAddStr = moveAddStr,
    .showCursor = showCursor,
    .showTopLine = showTopLine,
    .showBottomLine = showBottomLine,
    .setScrollArea = setScrollArea,
    .addPrintf = addPrintf,
    .movePrintf = movePrintf,
    .shiftLeftStr = shiftLeftStr,
    .scroll = scroll,
    .insertLine = insertLine,
    .insChar = insChar,
    .delChar = delChar,
    .message = message,

    .drawBox = drawBox,
    .drawBoxLine = drawBoxLine,
    .eraseBox = eraseBox,
  };

  return &cli_gui_api;
}

#endif