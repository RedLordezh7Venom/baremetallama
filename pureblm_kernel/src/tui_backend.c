#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void draw_char_utf8(int x, int y, uint32_t codepoint, uint32_t fg, uint32_t bg);
void clear_screen(uint32_t color);

static int cursor_x = 0;
static int cursor_y = 0;

// High-Fidelity Colors
#define COLOR_PURPLE 0x00FF00FF
#define COLOR_CYAN 0x0000FFFF
#define COLOR_GREEN 0x0000FF00
#define COLOR_GOLD 0x00FFD700
#define COLOR_WHITE 0x00FFFFFF
#define COLOR_DIM 0x00808080

static uint32_t text_fg = COLOR_WHITE;
static uint32_t text_bg = 0x00000000;

// Stateful ANSI Controller
static int ansi_stage = 0;
static int ansi_params[3] = {0, 0, 0};
static int ansi_p_ptr = 0;

void tui_clear() {
  clear_screen(text_bg);
  cursor_x = 0;
  cursor_y = 0;
  ansi_stage = 0;
}

void tui_putc(uint32_t c) {
  if (ansi_stage == 0) {
    if (c == 0x1B) {
      ansi_stage = 1;
      return;
    }
  } else if (ansi_stage == 1) {
    if (c == '[') {
      ansi_stage = 2;
      ansi_params[0] = ansi_params[1] = ansi_params[2] = 0;
      ansi_p_ptr = 0;
      return;
    }
    ansi_stage = 0;
  } else if (ansi_stage == 2) {
    if (c >= '0' && c <= '9') {
      ansi_params[ansi_p_ptr] = ansi_params[ansi_p_ptr] * 10 + (c - '0');
      return;
    } else if (c == ';') {
      if (ansi_p_ptr < 2)
        ansi_p_ptr++;
      return;
    } else if (c == 'm') {
      for (int i = 0; i <= ansi_p_ptr; i++) {
        int n = ansi_params[i];
        if (n == 0)
          text_fg = COLOR_WHITE;
        else if (n == 35)
          text_fg = COLOR_PURPLE;
        else if (n == 36)
          text_fg = COLOR_CYAN;
        else if (n == 32)
          text_fg = COLOR_GREEN;
        else if (n == 33)
          text_fg = COLOR_GOLD;
        else if (n == 2)
          text_fg = COLOR_DIM;
      }
      ansi_stage = 0;
      return;
    }
    ansi_stage = 0;
    return;
  }

  if (c == '\n') {
    cursor_x = 0;
    cursor_y += 16;
    return;
  }
  if (c == '\r') {
    cursor_x = 0;
    return;
  }
  if (c == '\b') {
    if (cursor_x >= 8)
      cursor_x -= 8;
    draw_char_utf8(cursor_x, cursor_y, ' ', text_fg, text_bg);
    return;
  }

  draw_char_utf8(cursor_x, cursor_y, c, text_fg, text_bg);
  cursor_x += 8;
  if (cursor_x >= 800) {
    cursor_x = 0;
    cursor_y += 16;
  }
}

void tui_print(const char *s) {
  while (*s) {
    uint32_t cp;
    unsigned char b = (unsigned char)*s;
    if (b < 0x80) {
      cp = b;
      s++;
    } else if ((b & 0xE0) == 0xC0) {
      cp = ((b & 0x1F) << 6) | ((unsigned char)s[1] & 0x3F);
      s += 2;
    } else if ((b & 0xF0) == 0xE0) {
      cp = ((b & 0x0F) << 12) | (((unsigned char)s[1] & 0x3F) << 6) |
           ((unsigned char)s[2] & 0x3F);
      s += 3;
    } else {
      cp = '?';
      s++;
    }
    tui_putc(cp);
  }
}

void tui_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      int width = 0, left = 0;
      if (*fmt == '-') {
        left = 1;
        fmt++;
      }
      while (*fmt >= '0' && *fmt <= '9') {
        width = width * 10 + (*fmt - '0');
        fmt++;
      }
      if (*fmt == 's') {
        char *s = va_arg(args, char *);
        int len = 0;
        char *p = s;
        while (*p++)
          len++;
        len--; // Correct len
        if (!left)
          while (width-- > len)
            tui_putc(' ');
        tui_print(s);
        if (left)
          while (width-- > len)
            tui_putc(' ');
        fmt++;
      } else if (*fmt == 'c') {
        tui_putc((uint32_t)va_arg(args, int));
        fmt++;
      } else if (*fmt == 'd') {
        int n = va_arg(args, int);
        if (n == 0)
          tui_putc('0');
        else {
          char b[12];
          int i = 0;
          if (n < 0) {
           
           
            tui_putc('-');
            n = -n;
          }
          while (n) {
            b[i++] = (n % 10) + '0';
            n /= 10;
          }
          while (i--)
            tui_putc(b[i]);
        }
        fmt++;
      } else {
        tui_putc('%');
      }
    } else {
      tui_putc(*fmt++);
    }
  }
  va_end(args);
}
