#ifndef CLI_GUI_H_
#define CLI_GUI_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_CLI_GUI


#define CLI_GUI_WIDTH     HW_CLI_GUI_WIDTH
#define CLI_GUI_HEIGHT    HW_CLI_GUI_HEIGHT



#ifndef PSTR
#define PSTR
#endif

#define SEQ_CSI                                 PSTR("\033[")                   // code introducer
#define SEQ_CLEAR                               PSTR("\033[2J")                 // clear screen
#define SEQ_CLRTOBOT                            PSTR("\033[J")                  // clear to bottom
#define SEQ_CLRTOEOL                            PSTR("\033[K")                  // clear to end of line
#define SEQ_DELCH                               PSTR("\033[P")                  // delete character
#define SEQ_NEXTLINE                            PSTR("\033E")                   // goto next line (scroll up at end of scrolling region)
#define SEQ_INSERTLINE                          PSTR("\033[L")                  // insert line
#define SEQ_DELETELINE                          PSTR("\033[M")                  // delete line
#define SEQ_ATTRSET                             PSTR("\033[0")                  // set attributes, e.g. "\033[0;7;1m"
#define SEQ_ATTRSET_REVERSE                     PSTR(";7")                      // reverse
#define SEQ_ATTRSET_UNDERLINE                   PSTR(";4")                      // underline
#define SEQ_ATTRSET_BLINK                       PSTR(";5")                      // blink
#define SEQ_ATTRSET_BOLD                        PSTR(";1")                      // bold
#define SEQ_ATTRSET_DIM                         PSTR(";2")                      // dim
#define SEQ_ATTRSET_FCOLOR                      PSTR(";3")                      // forground color
#define SEQ_ATTRSET_BCOLOR                      PSTR(";4")                      // background color
#define SEQ_INSERT_MODE                         PSTR("\033[4h")                 // set insert mode
#define SEQ_REPLACE_MODE                        PSTR("\033[4l")                 // set replace mode
#define SEQ_RESET_SCRREG                        PSTR("\033[r")                  // reset scrolling region
#define SEQ_LOAD_G1                             PSTR("\033)0")                  // load G1 character set
#define SEQ_CURSOR_VIS                          PSTR("\033[?25")                // set cursor visible/not visible


/*---------------------------------------------------------------------------------------------------------------------------------------------------
 * attributes, may be ORed
 *---------------------------------------------------------------------------------------------------------------------------------------------------
 */
#define A_NORMAL                0x0000                                              // normal
#define A_UNDERLINE             0x0001                                              // underline
#define A_REVERSE               0x0002                                              // reverse
#define A_BLINK                 0x0004                                              // blink
#define A_BOLD                  0x0008                                              // bold
#define A_DIM                   0x0010                                              // dim
#define A_STANDOUT              A_BOLD                                              // standout (same as bold)

#define F_BLACK                 0x0100                                              // foreground black
#define F_RED                   0x0200                                              // foreground red
#define F_GREEN                 0x0300                                              // foreground green
#define F_BROWN                 0x0400                                              // foreground brown
#define F_BLUE                  0x0500                                              // foreground blue
#define F_MAGENTA               0x0600                                              // foreground magenta
#define F_CYAN                  0x0700                                              // foreground cyan
#define F_WHITE                 0x0800                                              // foreground white
#define F_YELLOW                F_BROWN                                             // some terminals show brown as yellow (with A_BOLD)
#define F_COLOR                 0x0F00                                              // foreground mask

#define B_BLACK                 0x1000                                              // background black
#define B_RED                   0x2000                                              // background red
#define B_GREEN                 0x3000                                              // background green
#define B_BROWN                 0x4000                                              // background brown
#define B_BLUE                  0x5000                                              // background blue
#define B_MAGENTA               0x6000                                              // background magenta
#define B_CYAN                  0x7000                                              // background cyan
#define B_WHITE                 0x8000                                              // background white
#define B_YELLOW                B_BROWN                                             // some terminals show brown as yellow (with A_BOLD)
#define B_COLOR                 0xF000                                              // background mask


#define ACS_LRCORNER            0x8a                                                // DEC graphic 0x6a: lower right corner
#define ACS_URCORNER            0x8b                                                // DEC graphic 0x6b: upper right corner
#define ACS_ULCORNER            0x8c                                                // DEC graphic 0x6c: upper left corner
#define ACS_LLCORNER            0x8d                                                // DEC graphic 0x6d: lower left corner
#define ACS_PLUS                0x8e                                                // DEC graphic 0x6e: crossing lines
#define ACS_HLINE               0x91                                                // DEC graphic 0x71: horizontal line
#define ACS_LTEE                0x94                                                // DEC graphic 0x74: left tee
#define ACS_RTEE                0x95                                                // DEC graphic 0x75: right tee
#define ACS_BTEE                0x96                                                // DEC graphic 0x76: bottom tee
#define ACS_TTEE                0x97                                                // DEC graphic 0x77: top tee
#define ACS_VLINE               0x98                                                // DEC graphic 0x78: vertical line


#define ACS_DIAMOND             0x80                                                // DEC graphic 0x60: diamond
#define ACS_CKBOARD             0x81                                                // DEC graphic 0x61: checker board
#define ACS_DEGREE              0x86                                                // DEC graphic 0x66: degree symbol
#define ACS_PLMINUS             0x87                                                // DEC graphic 0x66: plus/minus

#define ACS_S1                  0x8f                                                // DEC graphic 0x6f: scan line 1
#define ACS_S3                  0x90                                                // DEC graphic 0x70: scan line 3
#define ACS_S5                  0x91                                                // DEC graphic 0x71: scan line 5
#define ACS_S7                  0x92                                                // DEC graphic 0x72: scan line 7
#define ACS_S9                  0x93                                                // DEC graphic 0x73: scan line 9
#define ACS_LEQUAL              0x99                                                // DEC graphic 0x79: less/equal
#define ACS_GEQUAL              0x9a                                                // DEC graphic 0x7a: greater/equal
#define ACS_PI                  0x9b                                                // DEC graphic 0x7b: Pi
#define ACS_NEQUAL              0x9c                                                // DEC graphic 0x7c: not equal
#define ACS_STERLING            0x9d                                                // DEC graphic 0x7d: uk pound sign
#define ACS_BULLET              0x9e                                                // DEC graphic 0x7e: bullet




typedef struct
{
  void      (*initScreen)(int16_t w, int16_t h);
  void      (*closeScreen)(void);
  uint32_t  (*getWidth)(void);
  uint32_t  (*getHeight)(void);
  void      (*setAttr)(uint16_t attr);
  void      (*clear)(void);
  void      (*move)(uint8_t x, uint8_t y);
  void      (*addChar)(uint8_t ch);
  void      (*addStr)(const char * str);
  void      (*moveAddStr)(uint8_t x, uint8_t y, const char *p_str);
  void      (*showCursor)(bool visibility);
  void      (*showTopLine)(const char * str);
  void      (*showBottomLine)(const char * str);
  void      (*setScrollArea)(uint8_t top, uint8_t bottom);
  void      (*addPrintf)(const char *fmt, ...);
  void      (*movePrintf)(uint8_t x, uint8_t y, const char *fmt, ...);
  void      (*shiftLeftStr)(uint8_t x, uint8_t y, char * str);
  void      (*scroll)(void);
  void      (*insertLine)(void);
  void      (*insChar)(uint8_t ch);
  void      (*delChar)(void);
  void      (*message)(const char * msg);


  void      (*drawBoxLine)(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *title);
  void      (*drawBox)(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const char *title);
  void      (*eraseBox)(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
} cli_gui_api_t;


cli_gui_api_t *cliGui(void);


#endif

#ifdef __cplusplus
 }
#endif

#endif