#include <stdlib.h>
#include <avr/io.h>
#include <string.h>
#include "unifiedLcd.h"
#include "rotary.h"

#define LT_SQ_COL SANDY_BROWN
#define DK_SQ_COL SADDLE_BROWN

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

#define W_ALL 13
#define B_ALL 14
#define WB_ALL 15

void draw_board();
void draw_credits();
void init_pieces();
void draw_pieces();
void draw_square(uint8_t x, uint8_t y, uint16_t colour);
void draw_piece(uint8_t x, uint8_t y);

uint8_t xy_to_rankFile(uint8_t x, uint8_t y);
uint64_t compute_king(uint64_t king_loc, uint64_t own_side);
uint8_t dp_to_rf(uint8_t x, uint8_t y);
void rf_to_dp(uint8_t rf, uint8_t* x, uint8_t* y);



// State of the board as 2D array for easy drawing
uint8_t board[BOARD_SIZE][BOARD_SIZE];

// State of board as bitboards for efficient computation
uint64_t bitboards[BOARD_SIZE * BOARD_SIZE];

// Lookup tables
uint64_t clear_rank[BOARD_SIZE];
uint64_t mask_rank[BOARD_SIZE];
uint64_t clear_file[BOARD_SIZE];
uint64_t mask_file[BOARD_SIZE];
uint64_t piece[BOARD_SIZE * BOARD_SIZE];

enum {
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8
};

enum {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H
};

// Selected square
volatile uint8_t sel_x = 0;
volatile uint8_t sel_y = 0;
volatile uint8_t sel_x_last = 0;
volatile uint8_t sel_y_last = 0;

volatile uint8_t redraw_select = 0;
volatile uint8_t select_enable = 1;

const char* display_pieces = " PNBRQKpnbrqk";


ISR(TIMER1_COMPA_vect) {

    if (select_enable) {

        if (rotary > 0) {

            sel_x_last = sel_x;
            sel_y_last = sel_y;

            if (sel_x > 0) {
                sel_x--;
            } else {
                if (sel_y > 0) {
                    sel_y--;
                    sel_x = 7;
                }
            }
            redraw_select = 1;
        }
        if (rotary < 0) {

            sel_x_last = sel_x;
            sel_y_last = sel_y;

            if (sel_x < 7) {
                sel_x++;
            } else {
                if (sel_y < 7) {
                    sel_y++;
                    sel_x = 0;
                }
            }
            redraw_select = 1;
        }

    }

    rotary = 0;

}

int main() {

    // Turn off default prescaling of clock (AT90USB DS, p. 48)
    CLKPR = 1 << CLKPCE;
    CLKPR = 0;

    // Setup rotary encoder
    init_rotary();

    // Enable rotary interrupt
    EIMSK |= _BV(INT4) | _BV(INT5);

    // Enable game timer interrupt (Timer 1 CTC Mode 4)
	TCCR1A = 0;
	TCCR1B = _BV(WGM12);
	TCCR1B |= _BV(CS10);
	TIMSK1 |= _BV(OCIE1A);

    // Setup screen I/O
    // Clock already prescaled so set clock option to 0
    init_lcd(0);

    cli();

    draw_board();
    init_pieces();
    draw_pieces();
    draw_credits();

    sei();
    for (;;) {

        if (redraw_select) {

            // Disable interrupts (display routine must NOT be disturbed)
            cli();

            // Redraw last square
            uint16_t col = ((sel_x_last + sel_y_last) & 1) ? DK_SQ_COL : LT_SQ_COL;
            draw_square(sel_x_last, sel_y_last, col);
            draw_piece(sel_x_last, sel_y_last);

            // Overwrite new square
            draw_square(sel_x, sel_y, TURQUOISE);
            draw_piece(sel_x, sel_y);

            // Set flag to complete!
            redraw_select = 0;

            // Renable interrupts
            sei();
        }

        uint8_t loop = 0;
        uint8_t last = select_enable;

        // Detect select confirm
        while (~PINE & _BV(SWC)) {

            cli();

            uint8_t selector = loop ? last : select_enable;

            if (selector) {
            
                // Overwrite square
                draw_square(sel_x, sel_y, GREEN);
                draw_piece(sel_x, sel_y);

                select_enable = 0;

            } else {

                // Overwrite square
                draw_square(sel_x, sel_y, TURQUOISE);
                draw_piece(sel_x, sel_y);

                select_enable = 1;

            }

            sei();

            loop = 1;

        }

        if (select_enable == 0) {

            if (board[sel_x][sel_y] == W_KING) {

                uint64_t val = compute_king(piece[dp_to_rf(sel_x, sel_y)], bitboards[W_ALL]);

                for (uint8_t i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {

                    if (val & piece[i]) {

                        uint8_t y_pos;
                        uint8_t x_pos;

                        rf_to_dp(i, &x_pos, &y_pos);

                        draw_square(x_pos, y_pos, LIGHT_PINK);

                    }

                }

            }

        }


    }
    cli();


}

// Verified correct.
uint8_t dp_to_rf(uint8_t x, uint8_t y) {
    y = BOARD_SIZE - y - 1;
    return x + y * BOARD_SIZE;
}

// Verified correct.
void rf_to_dp(uint8_t rf, uint8_t* x, uint8_t* y) {
    *y = BOARD_SIZE - 1 - rf / BOARD_SIZE;
    *x = rf % BOARD_SIZE;
}

void draw_board() {

    uint16_t i, j;

    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {

            uint16_t col = ((i + j) & 1) ? DK_SQ_COL : LT_SQ_COL;
            draw_square(i, j, col);

        }
    }
}

void draw_square(uint8_t x, uint8_t y, uint16_t colour) {

    rectangle r;
    r.left = LEFT_OFFST + SQ_SIZE * x;
    r.right = r.left + SQ_SIZE;
    r.top = SQ_SIZE * y;
    r.bottom = r.top + SQ_SIZE; 

    fill_rectangle(r, colour);

}

void draw_credits() {

    const char* title = "Fortuna Chess";
    const char* p = title;
    uint8_t i = 0;

    while (*p) {
        display_curser_move(15, 22 + 15 * i);
        display_char(*p);
        i++;
        p++;
    }

}

// [X][Y] NOT [ROW][COL]
void init_pieces() {

    /* Setup bitboards */

    // RIGHT shift is towards LSB and is equivalent to moving a piece LEFT.
    // White pieces are nearest LSB.
    // See mapping: http://pages.cs.wisc.edu/~psilord/blog/data/chess-pages/rep.html

    const uint64_t pawns = 0xEF00;
    // const uint64_t pawns = 0xFF00;
    const uint64_t rooks = 0x81;
    const uint64_t knights = 0x42;
    const uint64_t bishops = 0x24;
    const uint64_t queens = 0x8;
    const uint64_t kings = 0x10;

    // White bitboards
    bitboards[W_PAWN] =   pawns;
    bitboards[W_ROOK] =   rooks;
    bitboards[W_KNIGHT] = knights;
    bitboards[W_BISHOP] = bishops;
    bitboards[W_QUEEN] =  queens;
    bitboards[W_KING] =   kings;

    // Black bitboards
    bitboards[B_PAWN] =   pawns << 56;
    bitboards[B_ROOK] =   rooks << 56;
    bitboards[B_KNIGHT] = knights << 56;
    bitboards[B_BISHOP] = bishops << 56;
    bitboards[B_QUEEN] =  queens << 56;
    bitboards[B_KING] =   kings << 56;

    // All bitboards (remember to keep these updated!)
    bitboards[W_ALL] = bitboards[W_PAWN] | bitboards[W_ROOK] | bitboards[W_KNIGHT] | bitboards[W_BISHOP] | bitboards[W_QUEEN] | bitboards[W_KING];
    bitboards[B_ALL] = bitboards[B_PAWN] | bitboards[B_ROOK] | bitboards[B_KNIGHT] | bitboards[B_BISHOP] | bitboards[B_QUEEN] | bitboards[B_KING];
    bitboards[WB_ALL] = bitboards[W_ALL] | bitboards[B_ALL];

    // Setup lookup tables
    clear_rank[RANK_1] = ~0xFF;
    clear_rank[RANK_2] = ~0xFF00;
    clear_rank[RANK_3] = ~0xFF0000;
    clear_rank[RANK_4] = ~0xFF000000;
    clear_rank[RANK_5] = ~0xFF00000000;
    clear_rank[RANK_6] = ~0xFF0000000000;
    clear_rank[RANK_7] = ~0xFF000000000000;
    clear_rank[RANK_8] = ~0xFF00000000000000;

    for (uint8_t i = 0; i < BOARD_SIZE; i++) mask_rank[i] = ~clear_rank[i];

    for (uint8_t i = 0; i < BOARD_SIZE; i++) {
        for (uint8_t j = 0; j < BOARD_SIZE; j++) {
            mask_file[i] |= (1 << i) << (j * 8);
        }
    }

    for (uint8_t i = 0; i < BOARD_SIZE; i++) clear_file[i] = ~mask_file[i];

    for (uint8_t i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        piece[i] = 1 << i;
    }


    /* Setup display board */

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

    // White pieces
    board[0][7] = W_ROOK;
    board[1][7] = W_KNIGHT;
    board[2][7] = W_BISHOP;
    board[3][7] = W_QUEEN;
    board[4][7] = W_KING;
    board[5][7] = W_BISHOP;
    board[6][7] = W_KNIGHT;
    board[7][7] = W_ROOK;

}

void draw_piece(uint8_t x, uint8_t y) {
    if (board[x][y]) {
        display_curser_move(LEFT_OFFST + x * SQ_SIZE + 7, y * SQ_SIZE + 7); 
        display_char(display_pieces[board[x][y]]); 
    }
}

void draw_pieces() {

    uint8_t i, j;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            draw_piece(i, j);                   
        }
    }

}

uint64_t compute_king(uint64_t king_loc, uint64_t own_side) {

    // Account for file overflow/underflow
    uint64_t king_clip_h = king_loc & clear_file[FILE_H];
    uint64_t king_clip_a = king_loc & clear_file[FILE_A];

    // If bits (NOT necessarily the piece) are moving right by more than one,
    // we should clip.
    uint64_t pos_1 = king_clip_h << 7; // NW
    uint64_t pos_2 = king_loc << 8; // N
    uint64_t pos_3 = king_clip_h << 9; // NE
    uint64_t pos_4 = king_clip_h << 1;

    uint64_t pos_5 = king_clip_a >> 7;
    uint64_t pos_6 = king_loc >> 8;
    uint64_t pos_7 = king_clip_a >> 9;
    uint64_t pos_8 = king_clip_a >> 1;

    uint64_t king_moves = pos_1 | pos_2 | pos_3 | pos_4 | pos_5 | pos_6 | pos_7 | pos_8;

    uint64_t king_valid = king_moves & ~own_side;

    return king_valid;
}

// uint64_t get_white_bitboard() {
//     return bitboards[W_PAWN] | bitboards[W_ROOK] | bitboards[W_KNIGHT] | bitboards[W_BISHOP] | bitboards[W_QUEEN] | bitboards[W_KING];
// }

// uint64_t get_black_bitboard() {
//     return bitboards[B_PAWN] | bitboards[B_ROOK] | bitboards[B_KNIGHT] | bitboards[B_BISHOP] | bitboards[B_QUEEN] | bitboards[B_KING];
// }

// uint64_t get_all_bitboard() {
//     return get_white_bitboard() | get_black_bitboard();
// }

// // Legal move generator function (note: this is CORRECT and NOT PSEUDO-LEGAL)
// // Expects moves to be an empty two dimensional array representing valid positions on the board
// void gen_moves(uint8_t x, uint8_t y, uint8_t moves[BOARD_SIZE][BOARD_SIZE]) {

//     switch (board[x][y]) {

//         // Ok then, idiot.
//         case EMPTY:
//             return;
        
//         case W_KING:

//             // Find black's attacked squares (and look THROUGH the king itself)
            
//             // Loop board for black pieces
//             for (uint8_t x = 0; x < BOARD_SIZE; x++) {
//                 for (uint8_t y = 0; y < BOARD_SIZE; y++) {

//                     // Skip non-black pieces
//                     if (board[x][y] < B_PAWN) continue;

//                     set_attacked(x, y, t, moves);




//                 }
//             }

//             // Find moves the king can make that are NOT attacked and NOT a white piece

//             // Mark white pieces
//             for (uint8_t dx = 0; dx <= 1; dx++) {
//                 for (uint8_t dy = 0; dy <= 1; dy++) {
//                     if (x + dx >= 0 && x + dx < BOARD_SIZE && y + dy >= 0 && y + dy < BOARD_SIZE) {
//                         // It's a white piece
//                         if (board[x + dx][y + dy] >= 0 && board[x + dx][y + dy] < B_PAWN) {
//                             moves[x + dx][y + dy] = 1;
//                         } else if (moves[x + dx][y + dy] == 0) {
//                             // Legal positions marked with a 2
//                             moves[x + dx][y + dy] = 2;
//                         }
//                     }
//                 }
//             }

//             // Loop through board and unmark positions king can't reach


//             return;


//     }
// }

// void set_attacked(uint8_t x, uint8_t y, uint8_t t, uint8_t moves[BOARD_SIZE][BOARD_SIZE]) {

//     switch (t) {
//         case B_PAWN:
//             if (x + 1 < BOARD_SIZE && y + 1 < BOARD_SIZE) moves[x + 1][y + 1] = 1;
//             if (x - 1 >= 0 && y - 1 >= 0 ) moves[x - 1][y - 1] = 1;
//             break;
//     }


// }