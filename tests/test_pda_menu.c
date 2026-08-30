#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "aq_serial.h"
#include "pda_menu.h"
#include "utils.h"

int getLogLevel(logmask_t from)
{
  (void)from;
  return LOG_ERR;
}

void LOG(const logmask_t from, const int msg_level, const char *format, ...)
{
  (void)from;
  (void)msg_level;
  (void)format;
}

static bool process(unsigned char command, unsigned char line,
                    unsigned char start, unsigned char end,
                    unsigned char mode)
{
  unsigned char packet[11] = {0};
  packet[PKT_CMD] = command;
  packet[PKT_DATA] = line;
  packet[PKT_DATA + 1] = start;
  packet[PKT_DATA + 2] = end;
  packet[PKT_DATA + 3] = mode;
  return process_pda_menu_packet(packet, sizeof(packet), false);
}

static void set_line(unsigned char line, const char *text)
{
  unsigned char packet[PKT_DATA + AQ_MSGLEN + 2] = {0};
  packet[PKT_CMD] = CMD_MSG_LONG;
  packet[PKT_DATA] = line;
  strncpy((char *)&packet[PKT_DATA + 1], text, AQ_MSGLEN);
  assert(process_pda_menu_packet(packet, sizeof(packet), false));
}

static void assert_highlight_chars(const char *expected, int expected_length)
{
  int length = -1;
  char *value = pda_m_hlightchars(&length);
  assert(value != NULL);
  assert(length == expected_length);
  assert(strncmp(value, expected, expected_length) == 0);
}

static void assert_no_highlight_chars(void)
{
  int length = -1;
  assert(pda_m_hlightchars(&length) == NULL);
  assert(length == 0);
}

int main(void)
{
  pda_m_invalidate();
  set_line(1, "FILTER PUMP  ***");
  set_line(2, "SPA          OFF");
  set_line(3, "  SET TO 36`F");

  assert(process(CMD_PDA_HIGHLIGHT, 2, 0, 0, 0));
  assert(pda_m_hlightindex() == 2);
  assert(strcmp(pda_m_hlight(), "SPA          OFF") == 0);

  assert(process(CMD_PDA_HIGHLIGHTCHARS, 1, 13, 16, 2));
  assert(pda_m_hlightindex() == 2);
  assert_highlight_chars("***", 3);

  assert(process(CMD_PDA_HIGHLIGHTCHARS, 1, 13, 16, 0));
  assert(pda_m_hlightindex() == 2);
  assert_no_highlight_chars();

  assert(process(CMD_PDA_HIGHLIGHTCHARS, 3, 9, 11, 1));
  assert(pda_m_hlightindex() == 2);
  assert_highlight_chars("36", 2);

  assert(process(CMD_PDA_HIGHLIGHTCHARS, 3, 8, 12, 4));
  assert(pda_m_hlightindex() == 2);
  assert_highlight_chars(" 36`", 4);

  assert(!process(CMD_PDA_HIGHLIGHTCHARS, PDA_LINES, 0, 1, 1));
  assert(pda_m_hlightindex() == 2);
  assert_no_highlight_chars();

  assert(!process(CMD_PDA_HIGHLIGHTCHARS, 3, 12, 8, 1));
  assert(pda_m_hlightindex() == 2);

  assert(process(CMD_PDA_HIGHLIGHT, 3, 0, 0, 0));
  assert(pda_m_hlightindex() == 3);
  assert_no_highlight_chars();

  assert(process(CMD_PDA_CLEAR, 0, 0, 0, 0));
  assert(pda_m_hlightindex() == -1);
  assert_no_highlight_chars();
  assert(strcmp(pda_m_line(1), "") == 0);

  puts("PDA menu parser tests passed");
  return 0;
}
