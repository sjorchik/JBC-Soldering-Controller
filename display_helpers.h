#ifndef DISPLAY_HELPERS_H
#define DISPLAY_HELPERS_H
#include <Adafruit_GFX.h>
#include <string.h>

// Спільні спрайти. labelCell ширина 56 - покриває найширшу зону (вольтаж 52)
static GFXcanvas16 labelCell(56, 26);
static GFXcanvas16 digitCell(38, 50);

static inline void pushCell(const GFXcanvas16& cell, int16_t zoneX, int16_t zoneY,
                            int16_t zoneW, int16_t zoneH)
{
  display.startWrite();
  display.setAddrWindow(zoneX, zoneY, zoneW, zoneH);
  uint16_t* buf = cell.getBuffer();
  for (int16_t r = 0; r < zoneH; r++)
    display.writePixels(buf + r * cell.width(), zoneW);
  display.endWrite();
}

// "Спокійний напис": не чіпає дисплей, якщо текст той самий;
// інакше композить у RAM і шле одним блоком (без блимання).
static inline void drawLabelCalm(const char* text, uint16_t color, const GFXfont* font,
                                 int16_t zoneX, int16_t zoneY, int16_t zoneW, int16_t zoneH,
                                 int16_t textX, int16_t textY, char* lastBuf)
{
  if (strcmp(text, lastBuf) == 0) return;
  labelCell.fillScreen(0x0000);
  labelCell.setFont(font);
  labelCell.setTextColor(color);
  labelCell.setCursor(textX - zoneX, textY - zoneY);
  labelCell.print(text);
  pushCell(labelCell, zoneX, zoneY, zoneW, zoneH);
  strcpy(lastBuf, text);
}

// "Спокійний символ": тьмяний фон ('8') + новий символ одним блоком.
// Для зон вищих за 26 px автоматом бере digitCell.
static inline void drawCharCalm(char backCh, uint16_t backColor,
                                char ch, uint16_t color, const GFXfont* font,
                                int16_t zoneX, int16_t zoneY, int16_t zoneW, int16_t zoneH,
                                int16_t curX, int16_t curY)
{
  GFXcanvas16& cell = (zoneH > 26) ? digitCell : labelCell;
  cell.fillScreen(0x0000);
  cell.setFont(font);
  cell.setTextColor(backColor);
  cell.setCursor(curX - zoneX, curY - zoneY);
  cell.print(backCh);
  cell.setTextColor(color);
  cell.setCursor(curX - zoneX, curY - zoneY);
  cell.print(ch);
  pushCell(cell, zoneX, zoneY, zoneW, zoneH);
}
#endif
