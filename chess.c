#include <stdlib.h>
#include <avr/io.h>
#include "unifiedLcd.h"

#define LT_SQ_COL compile(240, 217, 183)
#define DK_SQ_COL compile(180, 135, 102)

#define BOARD_SIZE 8
#define SQ_SIZE 30
#define LEFT_OFFST 40

void draw_board();
void draw_credits();
void draw_pieces();

uint64_t bitboard;

int main() {

    // Turn off default prescaling of clock (AT90USB DS, p. 48)
    CLKPR = 1 << CLKPCE;
    CLKPR = 0;

    // Setup screen I/O
    // Clock already prescaled so set clock option to 0
    init_lcd(0);

    draw_board();
    //draw_credits();

}

void draw_board() {

    rectangle r;
    uint16_t colour = LT_SQ_COL;
    uint16_t i, j;


    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 1; j < BOARD_SIZE; j++) {
            
            r.left = LEFT_OFFST + i * SQ_SIZE;
            r.right = LEFT_OFFST + (i + 1) * SQ_SIZE;
            r.top = j * SQ_SIZE - SQ_SIZE;
            r.bottom = (j + 1) * SQ_SIZE - SQ_SIZE;

            fill_rectangle(r, colour);

            if (colour == LT_SQ_COL)
                colour = DK_SQ_COL;
            else
                colour = LT_SQ_COL;
        }
    }

    // Write last row
    uint8_t k;
    r.top = BOARD_SIZE * SQ_SIZE - SQ_SIZE;
    r.bottom = r.top + SQ_SIZE;
    colour = DK_SQ_COL;
    for (k = 0; k < BOARD_SIZE; k++) {
        r.left = LEFT_OFFST + k * SQ_SIZE;
        r.right = r.left + SQ_SIZE;
        fill_rectangle(r, colour);
        colour = (colour == LT_SQ_COL) ? DK_SQ_COL : LT_SQ_COL;
    }
}

void draw_credits() {

    display_curser_move(LEFT_OFFST, SQ_SIZE * BOARD_SIZE);
    display_string("Fortuna Chess by Dulhan Jayalath");

}

void draw_pieces() {

}
