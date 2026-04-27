#include "SDT.h"
#include "MainBoard_Display.h"
#include "MainBoard_DisplayPSK31.h"
#include "DSP_PSK31.h"
#include <RA8875.h>  // for RA8875 type and RA8875_* color constants

extern RA8875 tft;  /* defined in MainBoard_Display.cpp */

/*
 * PSK31 decoded-text pane.
 *
 * Top status row: RX:<freq> Hz   (and TXT:<count> as a rolling chars-decoded
 * indicator).
 *
 * Body: word-wrapped most-recent characters from PSK31GetText(). The buffer
 * size (256 chars) is more than enough to fill the pane; extra chars scroll
 * off the top.
 *
 * Render strategy:
 *   - Pull the entire ring as a flat string via PSK31GetText.
 *   - Compute how many chars fit per row at the default font.
 *   - Compute how many rows fit in (height - status row).
 *   - Show the trailing N rows worth of text (the most recent).
 *   - Newest character is at the right end of the bottom row.
 */

#define PSK31_PANE_FG_HEADER  RA8875_YELLOW
#define PSK31_PANE_FG_TEXT    RA8875_WHITE
#define PSK31_PANE_FG_SEP     DARKGREY

static void DrawPSK31StatusLine(int x, int y, int w, int h, int charsCached) {
    tft.fillRect(x, y, w, h, RA8875_BLACK);

    tft.setFontDefault();
    tft.setFontScale((enum RA8875tsize)0);

    char line[80];
    snprintf(line, sizeof(line),
             "PSK31  RX:%4d Hz  Decoded:%d chars",
             (int)psk31RxFreq,
             charsCached);

    tft.setTextColor(PSK31_PANE_FG_HEADER);
    tft.setCursor(x + 2, y + 1);
    tft.print(line);

    tft.drawFastHLine(x, y + h - 1, w, PSK31_PANE_FG_SEP);
}

void DrawPSK31Pane(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height) {
    /* Erase entire region first (covers the spectrum -> PSK31 transition). */
    tft.fillRect(x0, y0, width, height, RA8875_BLACK);

    /* Status line at the top */
    tft.setFontDefault();
    tft.setFontScale((enum RA8875tsize)0);
    int charW = tft.getFontWidth();
    int charH = tft.getFontHeight();
    if (charW <= 0) charW = 8;
    if (charH <= 0) charH = 16;

    int statusH = charH + 4;
    if (statusH > (int)height / 4) statusH = height / 4;

    /* Pull the decoded text */
    static char buf[PSK31_TEXT_BUFFER_SIZE + 1];
    int textLen = PSK31GetText(buf, sizeof(buf));

    DrawPSK31StatusLine(x0, y0, width, statusH, textLen);

    /* Body region for text */
    int bodyX = x0 + 4;
    int bodyY = y0 + statusH + 4;
    int bodyW = width - 8;
    int bodyH = height - statusH - 8;
    int rowsFit = bodyH / charH;
    int colsFit = bodyW / charW;
    if (rowsFit < 1) rowsFit = 1;
    if (colsFit < 1) colsFit = 1;

    if (textLen <= 0) {
        tft.setTextColor(DARKGREY);
        tft.setCursor(bodyX, bodyY);
        tft.print("(no decoded text yet)");
        return;
    }

    /* Word-wrap the text into lines of <= colsFit characters. We don't break
     * mid-word at the cost of slightly ragged right edges; hard-wrap at
     * column limit if a single token is wider than the line. CR/LF in the
     * text force a new line. */
    static char lines[40][96];  /* max 40 visible rows, 96 chars per line */
    int lineIdx = 0;
    int colIdx = 0;
    lines[0][0] = '\0';

    int maxRows  = rowsFit < 40 ? rowsFit : 40;
    int maxCols  = colsFit < 95 ? colsFit : 95;

    for (int i = 0; i < textLen; i++) {
        char c = buf[i];
        if (c == '\n' || c == '\r') {
            lines[lineIdx][colIdx] = '\0';
            lineIdx++;
            colIdx = 0;
            if (lineIdx >= 40) {
                /* shift up */
                for (int s = 1; s < 40; s++) {
                    memcpy(lines[s-1], lines[s], sizeof(lines[s]));
                }
                lineIdx = 39;
                lines[39][0] = '\0';
            } else {
                lines[lineIdx][0] = '\0';
            }
            continue;
        }
        if (colIdx >= maxCols) {
            lines[lineIdx][colIdx] = '\0';
            lineIdx++;
            colIdx = 0;
            if (lineIdx >= 40) {
                for (int s = 1; s < 40; s++) {
                    memcpy(lines[s-1], lines[s], sizeof(lines[s]));
                }
                lineIdx = 39;
                lines[39][0] = '\0';
            } else {
                lines[lineIdx][0] = '\0';
            }
        }
        lines[lineIdx][colIdx++] = c;
        lines[lineIdx][colIdx] = '\0';
    }

    /* Render the trailing maxRows lines */
    int firstLine = (lineIdx + 1 > maxRows) ? (lineIdx + 1 - maxRows) : 0;
    int rowsToShow = (lineIdx + 1) - firstLine;
    if (rowsToShow > maxRows) rowsToShow = maxRows;

    tft.setTextColor(PSK31_PANE_FG_TEXT);
    for (int r = 0; r < rowsToShow; r++) {
        tft.setCursor(bodyX, bodyY + r * charH);
        tft.print(lines[firstLine + r]);
    }
}
