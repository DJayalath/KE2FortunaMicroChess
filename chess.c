#include <stdlib.h>
#include <avr/io.h>
#include <string.h>
#include "unifiedLcd.h"

#define LT_SQ_COL compile(240, 217, 183)
#define DK_SQ_COL compile(180, 135, 102)

#define BOARD_SIZE 8
#define SQ_SIZE 30
#define LEFT_OFFST 40

// Numbered constants for piece types
#define EMPTY 0

#define W_PAWN 1
#define W_KNIGHT 2
#define W_BISHOP 3
#define W_ROOK 4
#define W_QUEEN 5
#define W_KING 6

#define B_PAWN 7
#define B_KNIGHT 8
#define B_BISHOP 9
#define B_ROOK 10
#define B_QUEEN 11
#define B_KING 12

void draw_board();
void draw_credits();
void init_pieces();
void draw_pieces();

// State of the board
uint8_t board[BOARD_SIZE][BOARD_SIZE];

int main() {

    // Turn off default prescaling of clock (AT90USB DS, p. 48)
    CLKPR = 1 << CLKPCE;
    CLKPR = 0;

    // Setup screen I/O
    // Clock already prescaled so set clock option to 0
    init_lcd(0);

    draw_board();
    init_pieces();
    draw_pieces();
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

// [X][Y] NOT [ROW][COL]
void init_pieces() {

    // Clear board
    memset(board, EMPTY, sizeof(board));

    // Pawns
    uint8_t i;
    for (i = 0; i < BOARD_SIZE; i++) {
        board[i][1] = B_PAWN;
        board[i][6] = W_PAWN;
    }

    // Black pieces
    board[0][0] = B_ROOK;
    board[1][0] = B_KNIGHT;
    board[2][0] = B_BISHOP;
    board[3][0] = B_QUEEN;
    board[4][0] = B_KING;
    board[5][0] = B_BISHOP;
    board[6][0] = B_KNIGHT;
    board[7][0] = B_ROOK;

    board[0][7] = W_ROOK;
    board[1][7] = W_KNIGHT;
    board[2][7] = W_BISHOP;
    board[3][7] = W_QUEEN;
    board[4][7] = W_KING;
    board[5][7] = W_BISHOP;
    board[6][7] = W_KNIGHT;
    board[7][7] = W_ROOK;

}

void draw_pieces() {

    const char* display_pieces = " PNBRQKpnbrqk";

    uint8_t i, j;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) { 
            if (board[i][j]) {
                display_curser_move(LEFT_OFFST + i * SQ_SIZE + 7, j * SQ_SIZE + 7); 
                display_char(display_pieces[board[i][j]]); 
            }
                   
        }
    }


}
