#include <stdlib.h>
#include <avr/io.h>
#include <string.h>
#include "unifiedLcd.h"
#include "rotary.h"

#define LT_SQ_COL SANDY_BROWN
#define DK_SQ_COL SADDLE_BROWN
#define OPN_COL LIGHT_PINK

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

uint8_t dp_to_rf(uint8_t x, uint8_t y);
void rf_to_dp(uint8_t rf, uint8_t* x, uint8_t* y);

uint64_t compute_king_incomplete(uint64_t king_loc, uint64_t own_side);
uint64_t compute_knight(uint64_t knight_loc, uint64_t own_side);
uint64_t compute_white_pawn(uint64_t pawn_loc);
uint64_t compute_black_pawn(uint64_t pawn_loc);
uint64_t compute_rook(uint64_t rook_loc, uint64_t own_side, uint64_t enemy_side);
uint64_t compute_bishop(uint64_t bishop_loc, uint64_t own_side, uint64_t enemy_side);
uint64_t compute_queen(uint64_t queen_loc, uint64_t own_side, uint64_t enemy_side);

void poll_selector();
void poll_redraw_selected();
void poll_move_gen();
void draw_open_moves();
void reset_open_moves();

void debug_bitboard(uint64_t bb);

// State of the board as 2D array for easy drawing
uint8_t board[BOARD_SIZE][BOARD_SIZE];

// State of board as bitboards for efficient computation
uint64_t bitboards[BOARD_SIZE * BOARD_SIZE];

// Lookup tables
const uint64_t clear_rank[BOARD_SIZE] = {
    0xFFFFFFFFFFFFFF00,
    0xFFFFFFFFFFFF00FF,
    0xFFFFFFFFFF00FFFF,
    0xFFFFFFFF00FFFFFF,
    0xFFFFFF00FFFFFFFF,
    0xFFFF00FFFFFFFFFF,
    0xFF00FFFFFFFFFFFF,
    0x00FFFFFFFFFFFFFF
};


const uint64_t mask_rank[BOARD_SIZE] = {
    0x00000000000000FF,
    0x000000000000FF00,
    0x0000000000FF0000,
    0x00000000FF000000,
    0x000000FF00000000,
    0x0000FF0000000000,
    0x00FF000000000000,
    0xFF00000000000000
};

const uint64_t clear_file[BOARD_SIZE] = {
    0xFEFEFEFEFEFEFEFE,
    0xFDFDFDFDFDFDFDFD,
    0xFBFBFBFBFBFBFBFB,
    0xF7F7F7F7F7F7F7F7,

    0xEFEFEFEFEFEFEFEF,
    0xDFDFDFDFDFDFDFDF,
    0xBFBFBFBFBFBFBFBF,
    0x7F7F7F7F7F7F7F7F
};


const uint64_t mask_file[BOARD_SIZE] = {

    0x0101010101010101,
    0x0202020202020202,
    0x0404040404040404,
    0x0808080808080808,

    0x1010101010101010,
    0x2020202020202020,
    0x4040404040404040,
    0x8080808080808080

};


uint64_t piece[BOARD_SIZE * BOARD_SIZE];

// Moves open to player on board
uint64_t open_moves;
uint8_t open_valid = 0; // Whether the current open_moves buffer is invalidated or not

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

/* Handle rotary encoder changes on timer interrupts */
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
        poll_redraw_selected();
        poll_selector();
        poll_move_gen();
    }
    cli();


}

/* Redraws the selected box if a new one is picked */
void poll_redraw_selected() {
    if (redraw_select) {

        // Disable interrupts (display routine must NOT be disturbed)
        cli();

        // Redraw last square
        uint16_t col = ((sel_x_last + sel_y_last) & 1) ? DK_SQ_COL : LT_SQ_COL;
        
        // If it was a open move square, ensure to use that colour
        uint8_t rf = dp_to_rf(sel_x_last, sel_y_last);
        if ((piece[rf] & open_moves) && open_valid) {
            col = OPN_COL;
        }

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
}


/* Polls the center button flags for selection */
void poll_selector() {

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

            // Invalidate open move buffer
            open_valid = 0;
            reset_open_moves();

        } else {

            // Overwrite square
            draw_square(sel_x, sel_y, TURQUOISE);
            draw_piece(sel_x, sel_y);

            select_enable = 1;

        }

        loop = 1;
        sei();

    }
}

/* Computes move generation for the selected piece */
void poll_move_gen() {
    if (select_enable == 0 && open_valid == 0) {
        uint8_t rf = dp_to_rf(sel_x, sel_y);
        switch(board[sel_x][sel_y]) {

            // OK, Idiot.
            case EMPTY:
                open_valid = 1;
                return;
            
            case B_KING:
                open_moves = compute_king_incomplete(piece[rf], bitboards[B_ALL]);
                break;
            case W_KING:
                open_moves = compute_king_incomplete(piece[rf], bitboards[W_ALL]);
                break;
            case B_KNIGHT:
                open_moves = compute_knight(piece[rf], bitboards[B_ALL]);
                break;
            case W_KNIGHT:
                open_moves = compute_knight(piece[rf], bitboards[W_ALL]);
                break;
            case W_PAWN:
                open_moves = compute_white_pawn(piece[rf]);
                break;
            case B_PAWN:
                open_moves = compute_black_pawn(piece[rf]);
                break;
            case B_ROOK:
                open_moves = compute_rook(piece[rf], bitboards[B_ALL], bitboards[W_ALL]);
                break;
            case W_ROOK:
                open_moves = compute_rook(piece[rf], bitboards[W_ALL], bitboards[B_ALL]);
                break;
            case B_BISHOP:
                open_moves = compute_bishop(piece[rf], bitboards[B_ALL], bitboards[W_ALL]);
                break;
            case W_BISHOP:
                open_moves = compute_bishop(piece[rf], bitboards[W_ALL], bitboards[B_ALL]);
                break;
            case B_QUEEN:
                open_moves = compute_queen(piece[rf], bitboards[B_ALL], bitboards[W_ALL]);
                break;
            case W_QUEEN:
                open_moves = compute_queen(piece[rf], bitboards[W_ALL], bitboards[B_ALL]);
                break;
            default:
                break;

        }

        draw_open_moves();

        // Validate open move buffer
        open_valid = 1;
    }
}

/* Resets the colours of the current open move squares */
void reset_open_moves() {
    for (uint8_t i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (open_moves & piece[i]) {
            uint8_t y_pos;
            uint8_t x_pos;
            rf_to_dp(i, &x_pos, &y_pos);
            uint16_t col = ((x_pos + y_pos) & 1) ? DK_SQ_COL : LT_SQ_COL;
            draw_square(x_pos, y_pos, col);
            draw_piece(x_pos, y_pos);
        }
    }
    open_moves = 0;
}

/* Draw squares set in the open move buffer */
void draw_open_moves() {
    for (uint8_t i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (open_moves & piece[i]) {
            uint8_t y_pos;
            uint8_t x_pos;
            rf_to_dp(i, &x_pos, &y_pos);
            draw_square(x_pos, y_pos, OPN_COL);
            draw_piece(x_pos, y_pos);
        }
    }
}

/* Convert a square-relative display coordinate into a rank-file index */
uint8_t dp_to_rf(uint8_t x, uint8_t y) {
    y = BOARD_SIZE - y - 1;
    return x + y * BOARD_SIZE;
}

/* Convert a rank-file index into a square-relative display coordinate */
void rf_to_dp(uint8_t rf, uint8_t* x, uint8_t* y) {
    *y = BOARD_SIZE - 1 - rf / BOARD_SIZE;
    *x = rf % BOARD_SIZE;
}

/* Draw all squares on the board */
void draw_board() {
    uint16_t i, j;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            uint16_t col = ((i + j) & 1) ? DK_SQ_COL : LT_SQ_COL;
            draw_square(i, j, col);
        }
    }
}

/* Draw a single square on the board */
void draw_square(uint8_t x, uint8_t y, uint16_t colour) {
    rectangle r;
    r.left = LEFT_OFFST + SQ_SIZE * x;
    r.right = r.left + SQ_SIZE;
    r.top = SQ_SIZE * y;
    r.bottom = r.top + SQ_SIZE; 
    fill_rectangle(r, colour);
}

/* Draw a single piece on the board */
void draw_piece(uint8_t x, uint8_t y) {
    if (board[x][y]) {
        display_curser_move(LEFT_OFFST + x * SQ_SIZE + 7, y * SQ_SIZE + 7); 
        display_char(display_pieces[board[x][y]]); 
    }
}

/* Draw all pieces on the board */
void draw_pieces() {
    uint8_t i, j;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            draw_piece(i, j);                   
        }
    }
}

/* Draw the title credits */
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

/* Display a bitboard on screen for debugging. */
// WHITE = 1, GREY = 0
void debug_bitboard(uint64_t bb) {

    cli();
    for (int i = 0; i < 64; i++) {
        uint8_t x, y;
        rf_to_dp(i, &x, &y);
        if (bb >> i & 1) {
            draw_square(x, y, WHITE);
        } else {
            draw_square(x, y, GREY);
        }
    }

    // Enter loop
    for (;;) {}

}

// [X][Y] NOT [ROW][COL]
void init_pieces() {

    /* Setup bitboards */

    // RIGHT shift is towards LSB and is equivalent to moving a piece LEFT.
    // White pieces are nearest LSB.
    // See mapping: http://pages.cs.wisc.edu/~psilord/blog/data/chess-pages/rep.html

    const uint64_t pawns = 0x0000;
    // const uint64_t pawns = 0xFF00;
    const uint64_t rooks = 0x81;
    const uint64_t knights = 0x00;
    // const uint64_t knights = 0x42;
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
    bitboards[B_PAWN] =   pawns << 40;
    bitboards[B_ROOK] =   rooks << 56;
    bitboards[B_KNIGHT] = knights << 56;
    bitboards[B_BISHOP] = bishops << 56;
    bitboards[B_QUEEN] =  queens << 56;
    bitboards[B_KING] =   kings << 56;

    // All bitboards (remember to keep these updated!)
    bitboards[W_ALL] = bitboards[W_PAWN] | bitboards[W_ROOK] | bitboards[W_KNIGHT] | bitboards[W_BISHOP] | bitboards[W_QUEEN] | bitboards[W_KING];
    bitboards[B_ALL] = bitboards[B_PAWN] | bitboards[B_ROOK] | bitboards[B_KNIGHT] | bitboards[B_BISHOP] | bitboards[B_QUEEN] | bitboards[B_KING];
    bitboards[WB_ALL] = bitboards[W_ALL] | bitboards[B_ALL];

    for (uint64_t i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        uint64_t one = 1;
        piece[i] = one << i;
    }

    // debug_bitboard(piece[28]);

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

/* Compute the bitboard of valid moves for a king */
// Incomplete because we are not checking for moves putting us in check, etc.
// https://peterellisjones.com/posts/generating-legal-chess-moves-efficiently/
uint64_t compute_king_incomplete(uint64_t king_loc, uint64_t own_side) {

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

    return king_moves & ~own_side;
}

/* Compute the bitboard of valid moves for a knight */
uint64_t compute_knight(uint64_t knight_loc, uint64_t own_side) {

    // Account for file overflow/underflow
    uint64_t clip_1 = clear_file[FILE_A] & clear_file[FILE_B];
    uint64_t clip_2 = clear_file[FILE_A];
    uint64_t clip_3 = clear_file[FILE_H];
    uint64_t clip_4 = clear_file[FILE_H] & clear_file[FILE_G];
    uint64_t clip_5 = clear_file[FILE_H] & clear_file[FILE_G];
    uint64_t clip_6 = clear_file[FILE_H];
    uint64_t clip_7 = clear_file[FILE_A];
    uint64_t clip_8 = clear_file[FILE_A] & clear_file[FILE_B];

    uint64_t pos_1 = (knight_loc & clip_1) << 6;
    uint64_t pos_2 = (knight_loc & clip_2) << 15;
    uint64_t pos_3 = (knight_loc & clip_3) << 17;
    uint64_t pos_4 = (knight_loc & clip_4) << 10;

    uint64_t pos_5 = (knight_loc & clip_5) >> 6;
    uint64_t pos_6 = (knight_loc & clip_6) >> 15;
    uint64_t pos_7 = (knight_loc & clip_7) >> 17;
    uint64_t pos_8 = (knight_loc & clip_8) >> 10;

    uint64_t knight_moves = pos_1 | pos_2 | pos_3 | pos_4 | pos_5 | pos_6 | pos_7 | pos_8;
    uint64_t knight_valid = knight_moves & ~own_side;

    return knight_valid;
}

/* Compute the bitboard of valid moves for a white pawn */
uint64_t compute_white_pawn(uint64_t pawn_loc) {

    // -- Calculate pawn moves --

    // Single space in front of pawn
    uint64_t one_step = (pawn_loc << 8) & ~bitboards[WB_ALL];

    // Check second step if one step is possible from rank 2
    uint64_t two_step = ((one_step & mask_rank[RANK_3]) << 8) & ~bitboards[WB_ALL];

    uint64_t valid_moves = one_step | two_step;

    // -- Calculate pawn attacks --

    // Left attacks (considering underflow on file A)
    uint64_t left_att = (pawn_loc & clear_file[FILE_A]) << 7;

    // Right attacks (considering overflow on file H)
    uint64_t right_att = (pawn_loc & clear_file[FILE_H]) << 9;

    uint64_t valid_att = (left_att | right_att) & bitboards[B_ALL];

    // -- Combine --

    uint64_t valid = valid_moves | valid_att;

    return valid;
}

/* Compute the bitboard of valid moves for a black pawn */
uint64_t compute_black_pawn(uint64_t pawn_loc) {

    // -- Calculate pawn moves --

    // Single space in front of pawn
    uint64_t one_step = (pawn_loc >> 8) & ~bitboards[WB_ALL];

    // Check second step if one step is possible from rank 7
    uint64_t two_step = ((one_step & mask_rank[RANK_6]) >> 8) & ~bitboards[WB_ALL];

    uint64_t valid_moves = one_step | two_step;

    // -- Calculate pawn attacks --

    // Left attacks (considering underflow on file A)
    uint64_t left_att = (pawn_loc & clear_file[FILE_A]) >> 9;

    // Right attacks (considering overflow on file H)
    uint64_t right_att = (pawn_loc & clear_file[FILE_H]) >> 7;

    uint64_t valid_att = (left_att | right_att) & bitboards[B_ALL];

    // -- Combine --

    uint64_t valid = valid_moves | valid_att;

    return valid;
}

/* Compute the bitboard of valid moves for a rook */
uint64_t compute_rook(uint64_t rook_loc, uint64_t own_side, uint64_t enemy_side) {

    // Rays are horizontal and vertical
    // => We can use masks!

    // Memory constraints => we can't use lookup tables here.
    // Would require 8 * 256 * 8 * 2 = 33kB > 8kB for all combinations.

    // We need to stop the ray as soon as it hits the first enemy piece

    uint64_t valid = 0;

    for (uint8_t rf = 0; rf < BOARD_SIZE * BOARD_SIZE; rf++) {

        if (rook_loc & piece[rf]) {

            // Build upward ray
            int8_t p = rf;
            while (p + 8 < BOARD_SIZE * BOARD_SIZE) {
                p += 8;
                if (piece[p] & bitboards[WB_ALL]) {
                    if (piece[p] & enemy_side) {
                        valid |= piece[p];
                    }
                    break;
                } else {
                    valid |= piece[p];
                }
            }

            // Build downward ray
            p = rf;
            while (p - 8 >= 0) {
                p -= 8;
                if (piece[p] & bitboards[WB_ALL]) {
                    if (piece[p] & enemy_side) {
                        valid |= piece[p];
                    }
                    break;
                } else {
                    valid |= piece[p];
                }
            }

            uint8_t left_edge = (rf / BOARD_SIZE) * BOARD_SIZE;
            uint8_t right_edge = left_edge + BOARD_SIZE - 1;

            // Build right ray
            p = rf;
            while ((p + 1) <= right_edge) {
                p++;
                if (piece[p] & bitboards[WB_ALL]) {
                    if (piece[p] & enemy_side) {
                        valid |= piece[p];
                    }
                    break;
                } else {
                    valid |= piece[p];
                }
            }

            // Build left ray (BROKEN)
            p = rf;
            while ((p - 1) >= left_edge) {
                p--;
                if (piece[p] & bitboards[WB_ALL]) {
                    if (piece[p] & enemy_side) {
                        valid |= piece[p];
                    }
                    break;
                } else {
                    valid |= piece[p];
                }
            }

        }

    }    

    return valid;
}

uint64_t compute_bishop(uint64_t bishop_loc, uint64_t own_side, uint64_t enemy_side) {

    uint64_t valid = 0;

    for (uint8_t rf = 0; rf < BOARD_SIZE * BOARD_SIZE; rf++) {

        if (bishop_loc & piece[rf]) {

            uint8_t x, y;
            rf_to_dp(rf, &x, &y);

            uint8_t x_tmp = x;
            uint8_t y_tmp = y;
            uint8_t p;

            // TR
            while(x_tmp + 1 < BOARD_SIZE && y_tmp - 1 >= 0) {
                x_tmp++;
                y_tmp--;
                p = dp_to_rf(x_tmp, y_tmp);
                if (piece[p] & bitboards[WB_ALL]) {
                    if (piece[p] & enemy_side) {
                        valid |= piece[p];
                    }
                    break;
                } else {
                    valid |= piece[p];
                }
            }

            x_tmp = x;
            y_tmp = y;

            // TL
            while(x_tmp - 1 >= 0 && y_tmp - 1 >= 0) {
                x_tmp--;
                y_tmp--;
                p = dp_to_rf(x_tmp, y_tmp);
                if (piece[p] & bitboards[WB_ALL]) {
                    if (piece[p] & enemy_side) {
                        valid |= piece[p];
                    }
                    break;
                } else {
                    valid |= piece[p];
                }
            }

            x_tmp = x;
            y_tmp = y;

            // BL
            while(x_tmp - 1 >= 0 && y_tmp + 1 < BOARD_SIZE) {
                x_tmp--;
                y_tmp++;
                p = dp_to_rf(x_tmp, y_tmp);
                if (piece[p] & bitboards[WB_ALL]) {
                    if (piece[p] & enemy_side) {
                        valid |= piece[p];
                    }
                    break;
                } else {
                    valid |= piece[p];
                }
            }

            x_tmp = x;
            y_tmp = y;

            // BR
            while(x_tmp + 1 < BOARD_SIZE && y_tmp + 1 < BOARD_SIZE) {
                x_tmp++;
                y_tmp++;
                p = dp_to_rf(x_tmp, y_tmp);
                if (piece[p] & bitboards[WB_ALL]) {
                    if (piece[p] & enemy_side) {
                        valid |= piece[p];
                    }
                    break;
                } else {
                    valid |= piece[p];
                }
            }


        }

    }

    return valid;

}

uint64_t compute_queen(uint64_t queen_loc, uint64_t own_side, uint64_t enemy_side) {

    // Compute rook subset of moves
    uint64_t rook_moves = compute_rook(queen_loc, own_side, enemy_side);

    // Compute bishop subset of moves
    uint64_t bishop_moves = compute_bishop(queen_loc, own_side, enemy_side);

    return bishop_moves | rook_moves;
}

// uint64_t compute_attacked_by_white() {

//     uint64_t pawns = compute_white_pawn(bitboards[W_PAWN]);
//     uint64_t king = compute_king_incomplete(bitboards[W_KING], bitboards[W_ALL]);
//     uint64_t knights = compute_knight(bitboards[W_KNIGHT], bitboards[W_ALL]);

//     // INVALID. Needs to be able to compute for all at once!
//     uint64_t rooks = compute_rook(bitboards[W_ROOK], bitboards[W_ALL], bitboards[B_ALL]);
//     uint64_t bishops = compute_bishop(bitboards[W_BISHOP], bitboards[W_ALL], bitboards[B_ALL]);
//     uint64_t queens = compute_queen(bitboards[])

//     // bitboards[W_PAWN]

// }