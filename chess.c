// AUTHOR: Dulhan Jayalath

#include <stdlib.h>
#include <avr/io.h>
#include <string.h>
#include "unifiedLcd.h"
#include "rotary.h"

// Turn on debugging during execution
// (see all the ifdef DEBUG statements for usage)
#define DEBUG 1

/* Board size constraints */

#define BOARD_SIZE 8
#define SQ_SIZE 30
#define LEFT_OFFST 40

/* Board square colour constants */

#define LT_SQ_COL SANDY_BROWN
#define DK_SQ_COL SADDLE_BROWN
#define OPN_COL PALE_GREEN
#define LOCK_COL GREEN
#define HL_COL 0xC618

/* Piece type constants */

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

/* Initialisation functions */

void init_pieces();

/* Draw functions */

void draw_board();
void draw_credits();
void draw_pieces();
void draw_square(uint8_t x, uint8_t y, uint16_t colour);
void draw_piece(uint8_t x, uint8_t y);
void draw_open_moves();
void reset_open_moves();

void draw_checkmate();
void draw_stalemate();
void draw_indicator();

/* Helper functions */

uint8_t dp_to_rf(uint8_t x, uint8_t y);
void rf_to_dp(uint8_t rf, uint8_t* x, uint8_t* y);

/* Move square computations */

uint64_t compute_king_incomplete(uint64_t king_loc, uint64_t own_side);


uint64_t knight_attacked(uint64_t knight_loc);
uint64_t knight_moveable(uint64_t knight_loc, uint64_t own_side);

uint64_t white_pawn_attacked(uint64_t pawn_loc);
uint64_t white_pawn_moveable(uint64_t pawn_loc);

uint64_t black_pawn_attacked(uint64_t pawn_loc);
uint64_t black_pawn_moveable(uint64_t pawn_loc);

uint64_t rook_attacked(uint64_t rook_loc, uint64_t all_pieces);
uint64_t rook_moveable(uint64_t rook_loc, uint64_t own_side, uint64_t all_pieces);

uint64_t bishop_attacked(uint64_t bishop_loc, uint64_t all_pieces);
uint64_t bishop_moveable(uint64_t bishop_loc, uint64_t own_side, uint64_t all_pieces);

uint64_t queen_attacked(uint64_t queen_loc, uint64_t all_pieces);
uint64_t queen_moveable(uint64_t queen_loc, uint64_t own_side, uint64_t all_pieces);

/* "King danger" square computations */

uint64_t compute_white_attacked_minus_black_king();
uint64_t compute_black_attacked_minus_white_king();

/* In-check capture and push mask computation */

void is_white_checked(uint64_t king_loc, uint64_t* capture_mask, uint64_t* push_mask);
void is_black_checked(uint64_t king_loc, uint64_t* capture_mask, uint64_t* push_mask);
uint8_t is_double_checked(uint64_t capture_mask);

/* Castling */

uint64_t castle_set_white();
uint64_t castle_set_black();
void castle(uint64_t castle_square);

const uint64_t WHITE_KING_INITIAL = 0x10;

const uint64_t WHITE_KINGSIDE_ROOK =  0x80;
const uint64_t WHITE_KINGSIDE_ROOK_CASTLED = 0x20;
const uint64_t WHITE_KINGSIDE_KING_CASTLED = 0x40;

const uint64_t WHITE_QUEENSIDE_ROOK = 0x1;
const uint64_t WHITE_QUEENSIDE_ROOK_CASTLED = 0x8;
const uint64_t WHITE_QUEENSIDE_KING_CASTLED = 0x4;

const uint64_t BLACK_KING_INITIAL = 0x1000000000000000;

const uint64_t BLACK_KINGSIDE_ROOK =         0x8000000000000000;
const uint64_t BLACK_KINGSIDE_ROOK_CASTLED = 0x2000000000000000;
const uint64_t BLACK_KINGSIDE_KING_CASTLED = 0x4000000000000000;

const uint64_t BLACK_QUEENSIDE_ROOK =         0x0100000000000000;
const uint64_t BLACK_QUEENSIDE_ROOK_CASTLED = 0x0800000000000000;
const uint64_t BLACK_QUEENSIDE_KING_CASTLED = 0x0400000000000000;

/* Piece pinned to king mask computation */

uint64_t compute_pin_mask_white(uint64_t piece);
uint64_t compute_pin_mask_black(uint64_t piece);

uint64_t masks_white(uint64_t piece);
uint64_t masks_black(uint64_t piece);

/* Representational piece movement */

void move_piece(uint64_t p, uint64_t q, uint8_t px, uint8_t py, uint8_t qx, uint8_t qy, uint8_t own_side, uint8_t enemy_side);
uint64_t generate_moves(uint64_t piece_loc, uint8_t piece_type);

/* Polling for basic game functions */

void poll_selector();
void poll_redraw_selected();
void poll_move_gen();

#ifdef DEBUG
    /* Debug functions (TODO: Can be removed if memory constrained) */
    void debug_bitboard(uint64_t bb);
    const uint64_t ERROR_ERRONEOUS_CASTLE_CALL = 0xFF;
#endif

// Selector state enumeration
enum {
    SELECTOR_FREE,
    SELECTOR_LOCKED,
};

// Rank lookup table indexes
enum {
    RANK_1, RANK_2, RANK_3, RANK_4,
    RANK_5, RANK_6, RANK_7, RANK_8
};

// File lookup table indexes
enum {
    FILE_A, FILE_B, FILE_C, FILE_D,
    FILE_E, FILE_F, FILE_G, FILE_H
};

enum {
    CASTLE_WHITE_KINGSIDE,
    CASTLE_WHITE_QUEENSIDE,
    CASTLE_BLACK_KINGSIDE,
    CASTLE_BLACK_QUEENSIDE
};

enum {
    PLAYER_WHITE,
    PLAYER_BLACK
};

uint8_t current_player = PLAYER_WHITE;

// Capture castling flags in a byte variable using enums above for indexing
uint8_t castle_flags = 0x0F;

// Encapsulate state of selection modes
struct {
    uint8_t state;
    uint8_t sel_x, sel_y;
    uint8_t sel_x_last, sel_y_last;
    uint8_t lock_x, lock_y;
} selector;

// Piece type lookup table and visual representation
// Note: indexed as [X][Y] NOT [ROW][COL] where (0,0) is top left
// Right is +x, Down is +y
uint8_t board[BOARD_SIZE][BOARD_SIZE];

// Bitboards for efficient computation
// FIXME: Not actually using all allocated bitboard memory here. Can limit array size further if needed.
uint64_t bitboards[BOARD_SIZE * BOARD_SIZE];

// Bitboards with only rank-file index bit set
uint64_t piece[BOARD_SIZE * BOARD_SIZE];

// Moves open to player on board
uint64_t open_moves;

// Is the open move buffer valid? Or does it need re-computing?
uint8_t open_valid = 0;

// Is a redraw needed?
volatile uint8_t redraw_select = 0;

// Visual display characters for the piece types and colours
// TODO: Future extension - Replace visual characters with sprites?
const char* display_pieces = " PNBRQKpnbrqk";

/* Lookup tables */

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

/* Handle rotary encoder changes on timer interrupts */
ISR(TIMER1_COMPA_vect) {

    if (rotary) {

        // Store the previously selected square (needs to be redrawn)
        selector.sel_x_last = selector.sel_x;
        selector.sel_y_last = selector.sel_y;

        // Update with location of new selected square
        if (rotary > 0) {
            if (selector.sel_x > 0) {
                selector.sel_x--;
            } else {
                if (selector.sel_y > 0) {
                    selector.sel_y--;
                    selector.sel_x = 7;
                }
            }
        } else if (rotary < 0) {
            if (selector.sel_x < 7) {
                selector.sel_x++;
            } else {
                if (selector.sel_y < 7) {
                    selector.sel_y++;
                    selector.sel_x = 0;
                }
            }
        }

        // Need to redraw squares
        redraw_select = 1;

        // Reset rotary status buffer
        rotary = 0;

    }

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

    const char* board_rep =
        "rnbqkbnr"
        "pppppppp"
        "........"
        "........"
        "........"
        "........"
        "PPPPPPPP"
        "RNBQKBNR";
    
    // const char* board_rep = 
    //     "....k..."
    //     "........"
    //     "......n."
    //     "....R..."
    //     "........"
    //     "........"
    //     "........"
    //     "....K...";

    // const char* board_rep = 
    //     ".k......"
    //     "........"
    //     "........"
    //     "....q..."
    //     "........"
    //     "....R..."
    //     "........"
    //     "....K...";

    // const char* board_rep = 
    //     "k......R"
    //     "........"
    //     "........"
    //     "....q..."
    //     "........"
    //     "....R..."
    //     "........"
    //     "....K...";

    // const char* board_rep = 
    //     "........"
    //     "........"
    //     "........"
    //     "........"
    //     "........"
    //     "..b...q."
    //     "........"
    //     "...QK...";

    // const char* board_rep =
    //     "r...k..r"
    //     "pppppppp"
    //     "........"
    //     "........"
    //     "........"
    //     "....b..."
    //     "........"
    //     "R...K..R";

    // Draw basic components
    draw_board();
    draw_credits();

    // Initialise and draw the board
    init_pieces(board_rep);
    draw_pieces();

    // Indicate white to move
    draw_indicator();

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

        // Compute colour of the previously highlighted square
        uint16_t col = ((selector.sel_x_last + selector.sel_y_last) & 1) ? DK_SQ_COL : LT_SQ_COL;
        // If it was a open move square, ensure to use that colour
        uint8_t rf = dp_to_rf(selector.sel_x_last, selector.sel_y_last);
        if ((piece[rf] & open_moves) && open_valid) {
            col = OPN_COL;
        }
        if (selector.state == SELECTOR_LOCKED && selector.sel_x_last == selector.lock_x && selector.sel_y_last == selector.lock_y) {
            col = LOCK_COL;
        }

        // Restore the previously highlighted square
        draw_square(selector.sel_x_last, selector.sel_y_last, col);
        draw_piece(selector.sel_x_last, selector.sel_y_last);

        // Highlight the newly selected square
        draw_square(selector.sel_x, selector.sel_y, HL_COL);
        draw_piece(selector.sel_x, selector.sel_y);

        // Set flag to complete!
        redraw_select = 0;

        sei();
    }
}


/* Polls the center button flags for selection */
void poll_selector() {

    uint8_t loop = 0;
    uint8_t last = selector.state;

    // Detect select confirm
    while (~PINE & _BV(SWC)) {

        cli();

        // Cache selector state to debounce switch presses
        uint8_t selector_cached = loop ? last : selector.state;

        if (selector_cached == SELECTOR_FREE) {

            // Check the selected square is a non-enemy square
            uint8_t square_type = board[selector.sel_x][selector.sel_y];

            if ((current_player == PLAYER_WHITE && square_type >= EMPTY && square_type <= W_KING) ||
                (current_player == PLAYER_BLACK && (square_type >= B_PAWN || square_type == EMPTY))) {

                // The selector was free and has now been pressed, we need to lock in the selected square
            
                // Redraw square to show it is locked
                draw_square(selector.sel_x, selector.sel_y, LOCK_COL);
                draw_piece(selector.sel_x, selector.sel_y);

                // Update selector state with locked square
                selector.lock_x = selector.sel_x;
                selector.lock_y = selector.sel_y;
                selector.state = SELECTOR_LOCKED;

                // Invalidate open move buffer
                open_valid = 0;

                // Redraw open move squares
                reset_open_moves();

            }

        } else {

            uint8_t rf = dp_to_rf(selector.sel_x, selector.sel_y);

            if (piece[rf] & open_moves) {

                // An open move square has been selected, move the locked piece here

                // Move piece
                uint8_t rf_old = dp_to_rf(selector.lock_x, selector.lock_y);


                if ( ( ( bitboards[W_KING] & piece[rf_old] ) && ( bitboards[W_ROOK] & piece[rf] ) ) ||
                     ( ( bitboards[B_KING] & piece[rf_old] ) && ( bitboards[B_ROOK] & piece[rf] ) ) ) {
                    castle(piece[rf]);
                } else if ( ( ( bitboards[W_ROOK] & piece[rf_old] ) && ( bitboards[W_KING] & piece[rf] ) ) || 
                            ( ( bitboards[B_ROOK] & piece[rf_old] ) && ( bitboards[B_KING] & piece[rf] ) ) ) {
                    castle(piece[rf_old]);
                } else {

                    uint8_t ty = board[selector.lock_x][selector.lock_y];
                    uint8_t own_side = (ty < B_PAWN) ? W_ALL : B_ALL;
                    uint8_t enemy_side = (own_side == W_ALL) ? B_ALL : W_ALL;
                    move_piece(piece[rf_old], piece[rf], selector.lock_x, selector.lock_y, selector.sel_x, selector.sel_y, own_side, enemy_side);

                    // Redraw old position
                    uint16_t col = ((selector.lock_x + selector.lock_y) & 1) ? DK_SQ_COL : LT_SQ_COL;
                    draw_square(selector.lock_x, selector.lock_y, col);
                    draw_piece(selector.lock_x, selector.lock_y);

                    // Redraw new position
                    draw_square(selector.sel_x, selector.sel_y, HL_COL);
                    draw_piece(selector.sel_x, selector.sel_y);

                }

                uint64_t capture_mask_black = 0;
                uint64_t capture_mask_white = 0;
                uint64_t push_mask = 0;
                uint64_t move_set_black = 0;
                uint64_t move_set_white = 0;

                // Check if black just got mated
                is_black_checked(bitboards[B_KING], &capture_mask_black, &push_mask);
                push_mask = 0;
                is_white_checked(bitboards[W_KING], &capture_mask_white, &push_mask);

                // Compute move set for all of black's pieces

                // WARNING: Risk of stack crashing into heap here. Sanity check this.
                for (uint8_t i = B_PAWN; i <= B_KING; i++) {
                    move_set_black |= generate_moves(bitboards[i], i);
                }

                for (uint8_t i = W_PAWN; i <= W_KING; i++) {
                    move_set_white |= generate_moves(bitboards[i], i);
                }

                if (move_set_black == 0) {

                    if (capture_mask_black) {
                        // CHECKMATE
                        draw_checkmate();
                        for(;;) {}

                    } else {
                        // STALEMATE
                        draw_stalemate();
                        for (;;) {}
                    }
                    
                }

                if (move_set_white == 0) {

                    if (capture_mask_white) {
                        // CHECKMATE
                        draw_checkmate();
                        for(;;) {}

                    } else {
                        // STALEMATE
                        draw_stalemate();
                        for (;;) {}
                    }
                    
                }

                // // TODO: Analyse check-mate
                // switch (current_player) {

                //     uint64_t capture_mask = 0;
                //     uint64_t push_mask = 0;
                //     uint64_t move_set = 0;

                //     case PLAYER_WHITE:

                //         // Check if black just got mated
                //         is_black_checked(bitboards[B_KING], &capture_mask, &push_mask);

                //         // Compute move set for all of black's pieces

                //         // WARNING: Risk of stack crashing into heap here. Sanity check this.
                //         for (uint8_t i = B_PAWN; i <= B_KING; i++) {
                //             move_set |= generate_moves(bitboards[i], i);
                //         }

                //         if (move_set == 0) {


                //             if (capture_mask) {
                //                 // CHECKMATE
                //                 draw_checkmate();
                //                 for(;;) {}

                //             } else {
                //                 // STALEMATE
                //                 draw_stalemate();
                //                 for (;;) {}
                //             }
                            
                //         }


                //         break;

                //     case PLAYER_BLACK:

                //         // Check if white just got mated
                //         is_white_checked(bitboards[W_KING], &capture_mask, &push_mask);

                //         // Compute move set for all of black's pieces

                //         // WARNING: Risk of stack crashing into heap here. Sanity check this.
                //         for (uint8_t i = W_PAWN; i <= W_KING; i++) {
                //             move_set |= generate_moves(bitboards[i], i);
                //         }

                //         if (move_set == 0) {


                //             if (capture_mask) {
                //                 // CHECKMATE
                //                 draw_checkmate();
                //                 for(;;) {}

                //             } else {
                //                 // STALEMATE
                //                 draw_stalemate();
                //                 for (;;) {}
                //             }
                            
                //         }                       

                //         break;
                // }

                // Next player's turn
                current_player = (current_player + 1) % 2;

                draw_indicator();


            } else {

                // A non-open square has been selected, free the locked square

                // Overwrite locked square with usual colour
                uint16_t col = ((selector.lock_x + selector.lock_y) & 1) ? DK_SQ_COL : LT_SQ_COL;
                draw_square(selector.lock_x, selector.lock_y, col);
                draw_piece(selector.lock_x, selector.lock_y);
                
            }

            // Invalidate the open move buffer
            open_valid = 0;

            // Redraw squares as usual
            reset_open_moves();

            // Update selector state
            selector.state = SELECTOR_FREE;

        }

        loop = 1;
        sei();

    }
}

/* Computes move generation for the selected piece */
void poll_move_gen() {

    // Only compute possible moves when a piece is locked and the move buffer is invalidated
    if (selector.state == SELECTOR_LOCKED && open_valid == 0) {

        uint8_t rf = dp_to_rf(selector.lock_x, selector.lock_y);

        open_moves = generate_moves(piece[rf], board[selector.lock_x][selector.lock_y]);

        // Moves have been computed, so draw them
        draw_open_moves();

        // Validate open move buffer
        open_valid = 1;
    }
}

uint64_t generate_moves(uint64_t piece_loc, uint8_t piece_type) {
    switch(piece_type) {

        case EMPTY:

            // OK, Idiot.

            open_valid = 1;
            return 0;
        

        case B_KING:

            return compute_king_incomplete(piece_loc, bitboards[B_ALL]) &
                            ~compute_white_attacked_minus_black_king() | 
                            castle_set_black();
            break;


        case W_KING:

            return compute_king_incomplete(piece_loc, bitboards[W_ALL]) &
                            ~compute_black_attacked_minus_white_king() |
                            castle_set_white();
            break;


        case B_KNIGHT:

            return knight_moveable(piece_loc, bitboards[B_ALL]) & masks_black(piece_loc);

            break;


        case W_KNIGHT:

            return knight_moveable(piece_loc, bitboards[W_ALL]) & masks_white(piece_loc);

            break;


        case B_PAWN:

            return black_pawn_moveable(piece_loc) & masks_black(piece_loc);

            break;

        case W_PAWN:

            return white_pawn_moveable(piece_loc) & masks_white(piece_loc);

            break;

        case B_ROOK:

            return rook_moveable(piece_loc, bitboards[B_ALL], bitboards[WB_ALL]) & masks_black(piece_loc);

            break;

        case W_ROOK:

            return rook_moveable(piece_loc, bitboards[W_ALL], bitboards[WB_ALL]) & masks_white(piece_loc);

            break;

        case B_BISHOP:

            return bishop_moveable(piece_loc, bitboards[B_ALL], bitboards[WB_ALL]) & masks_black(piece_loc);

            break;

        case W_BISHOP:

            return bishop_moveable(piece_loc, bitboards[W_ALL], bitboards[WB_ALL]) & masks_white(piece_loc);

            break;

        case B_QUEEN:

            return queen_moveable(piece_loc, bitboards[B_ALL], bitboards[WB_ALL]) & masks_black(piece_loc);

            break;

        case W_QUEEN:

            return queen_moveable(piece_loc, bitboards[W_ALL], bitboards[WB_ALL]) & masks_white(piece_loc);

            break;

        default:
            break;

    }
}

uint64_t masks_white(uint64_t piece) {

    uint64_t capture_mask = 0;
    uint64_t push_mask = 0;
    uint64_t pin_mask = 0;

    uint64_t total_mask = 0xFFFFFFFFFFFFFFFF;

    is_white_checked(bitboards[W_KING], &capture_mask, &push_mask);
    // debug_bitboard(capture_mask);

    if (capture_mask) {
        if (is_double_checked(capture_mask)) {
            total_mask = 0;
            return total_mask;
        }
        total_mask &= capture_mask | push_mask;
    }

    pin_mask = compute_pin_mask_white(piece);
    total_mask &= pin_mask & ~bitboards[B_KING];

    return total_mask;

}

uint64_t masks_black(uint64_t piece) {

    uint64_t capture_mask = 0;
    uint64_t push_mask = 0;
    uint64_t pin_mask = 0;

    uint64_t total_mask = 0xFFFFFFFFFFFFFFFF;

    is_black_checked(bitboards[B_KING], &capture_mask, &push_mask);

    if (capture_mask) {
        if (is_double_checked(capture_mask)) {
            total_mask = 0;
            return total_mask;
        }
        total_mask &= capture_mask | push_mask;
    }
    
    pin_mask = compute_pin_mask_black(piece);
    total_mask &= pin_mask & ~bitboards[W_KING];

    return total_mask;

}

/* Determines if a check is a double check from the capture mask */
uint8_t is_double_checked(uint64_t capture_mask) {

    // WARNING: Bit hack detects if number is a power of 2 but incorrectly
    // recognises 0, so use AFTER ensuring there is a check.
    return (capture_mask & (capture_mask - 1)) != 0;
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

void draw_checkmate() {
    cli();

    rectangle r;
    r.left = 90;
    r.right = 230;
    r.top = 110;
    r.bottom = 130;

    fill_rectangle(r, GOLD);

    r.left += 2;
    r.right -= 2;
    r.top += 2;
    r.bottom -= 2;

    fill_rectangle(r, BLACK);

    display_string_xy("CHECKMATE", 130, 117);

    sei();
}

void draw_stalemate() {
    cli();

    rectangle r;
    r.left = 90;
    r.right = 230;
    r.top = 110;
    r.bottom = 130;

    fill_rectangle(r, GOLD);

    r.left += 2;
    r.right -= 2;
    r.top += 2;
    r.bottom -= 2;

    fill_rectangle(r, BLACK);

    display_string_xy("STALEMATE", 130, 117);

    sei();
}

/* Draw player move indicator */
void draw_indicator() {
    cli();

    rectangle r_prev;
    r_prev.left = 298;
    r_prev.right = 302;
    r_prev.top = (current_player == PLAYER_WHITE) ? 23 : 223;
    r_prev.bottom = (current_player == PLAYER_WHITE) ? 27 : 227;

    fill_rectangle(r_prev, BLACK);

    rectangle r;
    r.left = 298;
    r.right = 302;
    r.top = (current_player == PLAYER_BLACK) ? 23 : 223;
    r.bottom = (current_player == PLAYER_BLACK) ? 27 : 227;

    fill_rectangle(r, WHITE);

    sei();
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

/* Initialises the game */
void init_pieces(const char* board_rep) {

    // Initialise selector
    selector.state = SELECTOR_FREE;
    selector.sel_x = 0;
    selector.sel_y = 0;
    selector.sel_x_last = 0;
    selector.sel_y_last = 0;

    uint64_t ONE_64 = 1;

    uint8_t i = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    while (board_rep[i]) {

        uint8_t j = dp_to_rf(x, y);

        switch (board_rep[i]) {

            case 'P':
                bitboards[W_PAWN] |= ONE_64 << j;
                board[x][y] = W_PAWN;
                break;
            case 'R':
                bitboards[W_ROOK] |= ONE_64 << j;
                board[x][y] = W_ROOK;
                break;
            case 'N':
                bitboards[W_KNIGHT] |= ONE_64 << j;
                board[x][y] = W_KNIGHT;
                break;
            case 'B':
                bitboards[W_BISHOP] |= ONE_64 << j;
                board[x][y] = W_BISHOP;
                break;
            case 'Q':
                bitboards[W_QUEEN] |= ONE_64 << j;
                board[x][y] = W_QUEEN;
                break;
            case 'K':
                bitboards[W_KING] |= ONE_64 << j;
                board[x][y] = W_KING;
                break;

            case 'p':
                bitboards[B_PAWN] |= ONE_64 << j;
                board[x][y] = B_PAWN;
                break;
            case 'r':
                bitboards[B_ROOK] |= ONE_64 << j;
                board[x][y] = B_ROOK;
                break;
            case 'n':
                bitboards[B_KNIGHT] |= ONE_64 << j;
                board[x][y] = B_KNIGHT;
                break;
            case 'b':
                bitboards[B_BISHOP] |= ONE_64 << j;
                board[x][y] = B_BISHOP;
                break;
            case 'q':
                bitboards[B_QUEEN] |= ONE_64 << j;
                board[x][y] = B_QUEEN;
                break;
            case 'k':
                bitboards[B_KING] |= ONE_64 << j;
                board[x][y] = B_KING;
                break;

            default:
                break;

        }

        if (x + 1 >= BOARD_SIZE) {
            x = 0;
            y++;
        } else {
            x++;
        }
        
        i++;
    }

    // All bitboards (remember to keep these updated!)
    bitboards[W_ALL] = bitboards[W_PAWN] | bitboards[W_ROOK] | bitboards[W_KNIGHT] | bitboards[W_BISHOP] | bitboards[W_QUEEN] | bitboards[W_KING];
    bitboards[B_ALL] = bitboards[B_PAWN] | bitboards[B_ROOK] | bitboards[B_KNIGHT] | bitboards[B_BISHOP] | bitboards[B_QUEEN] | bitboards[B_KING];
    bitboards[WB_ALL] = bitboards[W_ALL] | bitboards[B_ALL];

    for (uint64_t i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        uint64_t one = 1;
        piece[i] = one << i;
    }


    // /* Setup bitboards */

    // // RIGHT shift is towards LSB and is equivalent to moving a piece LEFT.
    // // White pieces are nearest LSB.
    // // See mapping: http://pages.cs.wisc.edu/~psilord/blog/data/chess-pages/rep.html

    // const uint64_t pawns = 0x0000;
    // // const uint64_t pawns = 0xFF00;
    // const uint64_t rooks = 0x81;
    // const uint64_t knights = 0x00;
    // // const uint64_t knights = 0x42;
    // const uint64_t bishops = 0x24;
    // const uint64_t queens = 0x8;
    // const uint64_t kings = 0x10;

    // // White bitboards
    // bitboards[W_PAWN] =   pawns;
    // bitboards[W_ROOK] =   rooks;
    // bitboards[W_KNIGHT] = knights;
    // bitboards[W_BISHOP] = bishops;
    // bitboards[W_QUEEN] =  queens;
    // bitboards[W_KING] =   kings;

    // // Black bitboards
    // bitboards[B_PAWN] =   pawns << 40;
    // bitboards[B_ROOK] =   rooks << 56;
    // bitboards[B_KNIGHT] = knights << 56;
    // bitboards[B_BISHOP] = bishops << 56;
    // bitboards[B_QUEEN] =  queens << 56;
    // bitboards[B_KING] =   kings << 56;

    // // All bitboards (remember to keep these updated!)
    // bitboards[W_ALL] = bitboards[W_PAWN] | bitboards[W_ROOK] | bitboards[W_KNIGHT] | bitboards[W_BISHOP] | bitboards[W_QUEEN] | bitboards[W_KING];
    // bitboards[B_ALL] = bitboards[B_PAWN] | bitboards[B_ROOK] | bitboards[B_KNIGHT] | bitboards[B_BISHOP] | bitboards[B_QUEEN] | bitboards[B_KING];
    // bitboards[WB_ALL] = bitboards[W_ALL] | bitboards[B_ALL];

    // for (uint64_t i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
    //     uint64_t one = 1;
    //     piece[i] = one << i;
    // }

    // // debug_bitboard(piece[28]);

    // /* Setup display board */

    // // Clear board
    // memset(board, EMPTY, sizeof(board));

    // // Pawns
    // uint8_t i;
    // for (i = 0; i < BOARD_SIZE; i++) {
    //     board[i][1] = B_PAWN;
    //     board[i][6] = W_PAWN;
    // }

    // // Black pieces
    // board[0][0] = B_ROOK;
    // board[1][0] = B_KNIGHT;
    // board[2][0] = B_BISHOP;
    // board[3][0] = B_QUEEN;
    // board[4][0] = B_KING;
    // board[5][0] = B_BISHOP;
    // board[6][0] = B_KNIGHT;
    // board[7][0] = B_ROOK;

    // // White pieces
    // board[0][7] = W_ROOK;
    // board[1][7] = W_KNIGHT;
    // board[2][7] = W_BISHOP;
    // board[3][7] = W_QUEEN;
    // board[4][7] = W_KING;
    // board[5][7] = W_BISHOP;
    // board[6][7] = W_KNIGHT;
    // board[7][7] = W_ROOK;

}

/* Compute the bitboard of valid moves for a king */
uint64_t compute_king_incomplete(uint64_t king_loc, uint64_t own_side) {

    // Account for file overflow/underflow
    uint64_t king_clip_h = king_loc & clear_file[FILE_H];
    uint64_t king_clip_a = king_loc & clear_file[FILE_A];

    // If bits (NOT necessarily the piece) are moving right by more than one,
    // we should clip.
    uint64_t pos_1 = king_clip_a << 7; // NW
    // uint64_t pos_1 = king_clip_h << 7; // NW
    uint64_t pos_2 = king_loc << 8; // N
    uint64_t pos_3 = king_clip_h << 9; // NE
    uint64_t pos_4 = king_clip_h << 1;

    uint64_t pos_5 = king_clip_h >> 7;
    // uint64_t pos_5 = king_clip_a >> 7;
    uint64_t pos_6 = king_loc >> 8;
    uint64_t pos_7 = king_clip_a >> 9;
    uint64_t pos_8 = king_clip_a >> 1;

    uint64_t king_moves = pos_1 | pos_2 | pos_3 | pos_4 | pos_5 | pos_6 | pos_7 | pos_8;

    return king_moves & ~own_side;
}

/* Set of squares attacked by a knight */
uint64_t knight_attacked(uint64_t knight_loc) {

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

    uint64_t knight_attacked = pos_1 | pos_2 | pos_3 | pos_4 | pos_5 | pos_6 | pos_7 | pos_8;

    return knight_attacked;
}

/* Set of squares a knight can move to */
uint64_t knight_moveable(uint64_t knight_loc, uint64_t own_side) {
    return knight_attacked(knight_loc) & ~own_side;
}

/* Set of squares attacked by a white pawn */
uint64_t white_pawn_attacked(uint64_t pawn_loc) {

    // Left and right attacks
    uint64_t left_att = (pawn_loc & clear_file[FILE_A]) << 7;
    uint64_t right_att = (pawn_loc & clear_file[FILE_H]) << 9;

    // TODO: En passant

    return left_att | right_att;
}

/* Set of squares a white pawn can move to */
uint64_t white_pawn_moveable(uint64_t pawn_loc) {

    // Calculate pawn moves

    // Single space in front of pawn
    uint64_t one_step = (pawn_loc << 8) & ~bitboards[WB_ALL];

    // Check second step if one step is possible from rank 2
    uint64_t two_step = ((one_step & mask_rank[RANK_3]) << 8) & ~bitboards[WB_ALL];

    uint64_t valid_moves = one_step | two_step;
    uint64_t valid_att = white_pawn_attacked(pawn_loc) & bitboards[B_ALL];

    return valid_moves | valid_att;
}

/* Set of squares attacked by a black pawn */
uint64_t black_pawn_attacked(uint64_t pawn_loc) {

    uint64_t left_att = (pawn_loc & clear_file[FILE_A]) >> 9;
    uint64_t right_att = (pawn_loc & clear_file[FILE_H]) >> 7;

    // TODO: En passant

    return left_att | right_att;
}

/* Set of squares a black pawn can move to */
uint64_t black_pawn_moveable(uint64_t pawn_loc) {

    // Calculate pawn moves

    // Single space in front of pawn
    uint64_t one_step = (pawn_loc >> 8) & ~bitboards[WB_ALL];

    // Check second step if one step is possible from rank 7
    uint64_t two_step = ((one_step & mask_rank[RANK_6]) >> 8) & ~bitboards[WB_ALL];

    uint64_t valid_moves = one_step | two_step;
    uint64_t valid_att = black_pawn_attacked(pawn_loc) & bitboards[W_ALL];

    return valid_moves | valid_att;
}

/* Set of squares attacked by a rook */
uint64_t rook_attacked(uint64_t rook_loc, uint64_t all_pieces) {

    // Rays are horizontal and vertical
    // => We can use masks!

    // Memory constraints => we can't use lookup tables here.
    // Would require 8 * 256 * 8 * 2 = 33kB > 8kB RAM for all combinations.

    // We need to stop the ray as soon as it hits the first enemy piece

    uint64_t valid = 0;

    for (uint8_t rf = 0; rf < BOARD_SIZE * BOARD_SIZE; rf++) {

        if (rook_loc & piece[rf]) {

            // Build upward ray
            int8_t p = rf;
            while (p + 8 < BOARD_SIZE * BOARD_SIZE) {
                p += 8;
                valid |= piece[p];
                if (piece[p] & all_pieces) break;
            }

            // Build downward ray
            p = rf;
            while (p - 8 >= 0) {
                p -= 8;
                valid |= piece[p];
                if (piece[p] & all_pieces) break;
            }

            uint8_t left_edge = (rf / BOARD_SIZE) * BOARD_SIZE;
            uint8_t right_edge = left_edge + BOARD_SIZE - 1;

            // Build right ray
            p = rf;
            while ((p + 1) <= right_edge) {
                p++;
                valid |= piece[p];
                if (piece[p] & all_pieces) break;
            }

            // Build left ray
            p = rf;
            while ((p - 1) >= left_edge) {
                p--;
                valid |= piece[p];
                if (piece[p] & all_pieces) break;
            }

        }

    }    

    return valid;
}

/* Set of squares a rook can move to */
uint64_t rook_moveable(uint64_t rook_loc, uint64_t own_side, uint64_t all_pieces) {
    return rook_attacked(rook_loc, all_pieces) & ~own_side;
}

/* Set of squares attacked by a bishop */
uint64_t bishop_attacked(uint64_t bishop_loc, uint64_t all_pieces) {

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
                valid |= piece[p];
                if (piece[p] & all_pieces) break;
            }

            x_tmp = x;
            y_tmp = y;

            // TL
            while(x_tmp - 1 >= 0 && y_tmp - 1 >= 0) {
                x_tmp--;
                y_tmp--;
                p = dp_to_rf(x_tmp, y_tmp);
                valid |= piece[p];
                if (piece[p] & all_pieces) break;
            }

            x_tmp = x;
            y_tmp = y;

            // BL
            while(x_tmp - 1 >= 0 && y_tmp + 1 < BOARD_SIZE) {
                x_tmp--;
                y_tmp++;
                p = dp_to_rf(x_tmp, y_tmp);
                valid |= piece[p];
                if (piece[p] & all_pieces) break;
            }

            x_tmp = x;
            y_tmp = y;

            // BR
            while(x_tmp + 1 < BOARD_SIZE && y_tmp + 1 < BOARD_SIZE) {
                x_tmp++;
                y_tmp++;
                p = dp_to_rf(x_tmp, y_tmp);
                valid |= piece[p];
                if (piece[p] & all_pieces) break;
            }


        }

    }

    return valid;

}

/* Set of squares a bishop can move to */
uint64_t bishop_moveable(uint64_t bishop_loc, uint64_t own_side, uint64_t all_pieces) {
    return bishop_attacked(bishop_loc, all_pieces) & ~own_side;
}

/* Set of squares attacked by a queen */
uint64_t queen_attacked(uint64_t queen_loc, uint64_t all_pieces) {
    return rook_attacked(queen_loc, all_pieces) | bishop_attacked(queen_loc, all_pieces);
}

/* Set of squares a queen can move to */
uint64_t queen_moveable(uint64_t queen_loc, uint64_t own_side, uint64_t all_pieces) {
    return queen_attacked(queen_loc, all_pieces) & ~own_side;
}

uint64_t compute_white_attacked_minus_black_king() {

    // Non-sliders can be computed as usual
    uint64_t pawns = white_pawn_attacked(bitboards[W_PAWN]);
    uint64_t king = compute_king_incomplete(bitboards[W_KING], bitboards[W_ALL]);
    uint64_t knights = knight_attacked(bitboards[W_KNIGHT]);

    // Sliders must ignore the black king to invalidate moves away from slider attacks by black king
    uint64_t black_minus_king = bitboards[B_ALL] & ~bitboards[B_KING];
    uint64_t rooks = rook_attacked(bitboards[W_ROOK], bitboards[WB_ALL] & ~bitboards[B_KING]);
    uint64_t bishops = bishop_attacked(bitboards[W_BISHOP], bitboards[WB_ALL] & ~bitboards[B_KING]);
    uint64_t queens = queen_attacked(bitboards[W_QUEEN], bitboards[WB_ALL] & ~bitboards[B_KING]);

    return pawns | king | knights | rooks | bishops | queens;

}

uint64_t compute_black_attacked_minus_white_king() {

    // Non-sliders can be computed as usual
    uint64_t pawns = black_pawn_attacked(bitboards[B_PAWN]);
    uint64_t king = compute_king_incomplete(bitboards[B_KING], bitboards[B_ALL]);
    uint64_t knights = knight_attacked(bitboards[B_KNIGHT]);

    // Sliders must ignore the black king to invalidate moves away from slider attacks by black king
    uint64_t white_minus_king = bitboards[W_ALL] & ~bitboards[W_KING];
    uint64_t rooks = rook_attacked(bitboards[B_ROOK], bitboards[WB_ALL] & ~bitboards[W_KING]);
    uint64_t bishops = bishop_attacked(bitboards[B_BISHOP], bitboards[WB_ALL] & ~bitboards[W_KING]);
    uint64_t queens = queen_attacked(bitboards[B_QUEEN], bitboards[WB_ALL] & ~bitboards[W_KING]);

    return pawns | king | knights | rooks | bishops | queens;

}

void is_white_checked(uint64_t king_loc, uint64_t* capture_mask, uint64_t* push_mask) {

    *capture_mask = 0;
    *push_mask = 0;

    // Strategy: place enemy piece types on king position and see if they attack a real enemy piece
    
    // Pawns are a unique case as pawn attack direction is tightly coupled
    // Check if king were a WHITE pawn, would it attack a BLACK pawn?
    uint64_t pawn_move = white_pawn_attacked(king_loc);
    *capture_mask |= pawn_move & bitboards[B_PAWN];

    // Knights
    uint64_t knight_move = knight_attacked(king_loc);
    *capture_mask |= knight_move & bitboards[B_KNIGHT];

    // For sliding pieces, we must also calculate a push mask to block checks

    // Bishops
    uint64_t bishop_move = bishop_attacked(king_loc, bitboards[WB_ALL]);
    *capture_mask |= bishop_move & bitboards[B_BISHOP];
    // FIXME: Verify if this is correct?
    *push_mask |= bishop_move & bishop_attacked(bitboards[B_BISHOP], bitboards[WB_ALL]) & ~bitboards[W_KING];

    // Rooks
    uint64_t rook_move = rook_attacked(king_loc, bitboards[WB_ALL]);
    *capture_mask |= rook_move & bitboards[B_ROOK];
    *push_mask |= rook_move & rook_attacked(bitboards[B_ROOK], bitboards[WB_ALL]) & ~bitboards[W_KING];

    // Queens
    uint64_t queen_move = queen_attacked(king_loc, bitboards[WB_ALL]);
    *capture_mask |= queen_move & bitboards[B_QUEEN];
    *push_mask |= queen_move & queen_attacked(bitboards[B_QUEEN], bitboards[WB_ALL]) & ~bitboards[W_KING];

    // No need to check for kings as that's impossible.
}

void is_black_checked(uint64_t king_loc, uint64_t* capture_mask, uint64_t* push_mask) {

    *capture_mask = 0;
    *push_mask = 0;

    // Strategy: place enemy piece types on king position and see if they attack a real enemy piece
    
    // Pawns are a unique case as pawn attack direction is tightly coupled
    // Check if king were a BLACK pawn, would it attack a WHITE pawn?
    uint64_t pawn_move = black_pawn_attacked(king_loc);
    *capture_mask |= pawn_move & bitboards[W_PAWN];

    // Knights
    uint64_t knight_move = knight_attacked(king_loc);
    *capture_mask |= knight_move & bitboards[W_KNIGHT];

    // For sliding pieces, we must also calculate a push mask to block checks

    // Bishops
    uint64_t bishop_move = bishop_attacked(king_loc, bitboards[WB_ALL]);
    *capture_mask |= bishop_move & bitboards[W_BISHOP];
    *push_mask |= bishop_move & bishop_attacked(bitboards[W_BISHOP], bitboards[WB_ALL]) & ~bitboards[B_KING];

    // Rooks
    uint64_t rook_move = rook_attacked(king_loc, bitboards[WB_ALL]);
    *capture_mask |= rook_move & bitboards[W_ROOK];
    // Set bits BETWEEN the rook and king. Take conjunction of rook moves from both positions!
    *push_mask |= rook_move & rook_attacked(bitboards[W_ROOK], bitboards[WB_ALL]) & ~bitboards[B_KING];

    // Queens
    uint64_t queen_move = queen_attacked(king_loc, bitboards[WB_ALL]);
    *capture_mask |= queen_move & bitboards[W_QUEEN];
    *push_mask |= queen_move & queen_attacked(bitboards[W_QUEEN], bitboards[WB_ALL]) & ~bitboards[B_KING];

}

void move_piece(uint64_t p, uint64_t q, uint8_t px, uint8_t py, uint8_t qx, uint8_t qy, uint8_t own_side, uint8_t enemy_side) {

    // Moving piece type
    uint8_t t = board[px][py];

    // Destination piece type
    uint8_t u = board[qx][qy];

    // Update castling rights
    if (t == W_KING) {
        castle_flags &= ~(1 << CASTLE_WHITE_KINGSIDE) | ~(1 << CASTLE_WHITE_QUEENSIDE);
    } else if (t == B_KING) {
        castle_flags &= ~(1 << CASTLE_BLACK_KINGSIDE) | ~(1 << CASTLE_BLACK_QUEENSIDE);
    } else if ( (t == W_ROOK && p == WHITE_KINGSIDE_ROOK) || (u == W_ROOK && q == WHITE_KINGSIDE_ROOK) ) {
        castle_flags &= ~(1 << CASTLE_WHITE_KINGSIDE);
    } else if ( (t == W_ROOK && p == WHITE_QUEENSIDE_ROOK) || (u == W_ROOK && q == WHITE_QUEENSIDE_ROOK) ) {
        castle_flags &= ~(1 << CASTLE_WHITE_QUEENSIDE);
    } else if ( (t == B_ROOK && p == BLACK_KINGSIDE_ROOK) || (u == B_ROOK && q == BLACK_KINGSIDE_ROOK) ) {
        castle_flags &= ~(1 << CASTLE_BLACK_KINGSIDE);
    } else if ( (t == B_ROOK && p == BLACK_QUEENSIDE_ROOK) || (u == B_ROOK && q == BLACK_QUEENSIDE_ROOK) ) {
        castle_flags &= ~(1 << CASTLE_BLACK_QUEENSIDE);
    }

    // Unset current position of moving piece
    bitboards[t] &= ~p;
    // Set new position of moving piece
    bitboards[t] |= q;

    // Remove taken piece
    bitboards[u] &= ~q;
    bitboards[enemy_side] &= ~q;

    // Update own side bitboard
    bitboards[own_side] &= ~p;
    bitboards[own_side] |= q;

    // Update all piece bitboard
    bitboards[WB_ALL] = bitboards[own_side] | bitboards[enemy_side];

    // Update lookup table
    board[px][py] = EMPTY;
    board[qx][qy] = t;

}

uint64_t castle_set_white() {

    uint64_t castle_set = 0;

    uint64_t attacked = compute_black_attacked_minus_white_king();

    uint64_t kingside_attacked = (piece[4] | piece[5] | piece[6]) & attacked;
    uint64_t queenside_attacked = (piece[4] | piece[3] | piece[2]) & attacked;

    if (castle_flags & (1 << CASTLE_WHITE_KINGSIDE) && kingside_attacked == 0) {

        // Check ray from king to rook
        uint64_t hray = rook_attacked(WHITE_KING_INITIAL, bitboards[WB_ALL]);

        if (hray & WHITE_KINGSIDE_ROOK)
            castle_set |= WHITE_KINGSIDE_ROOK;
    }

    if (castle_flags & (1 << CASTLE_WHITE_QUEENSIDE) && queenside_attacked == 0) {

        // Check ray from king to rook
        uint64_t hray = rook_attacked(WHITE_KING_INITIAL, bitboards[WB_ALL]);

        if (hray & WHITE_QUEENSIDE_ROOK)
            castle_set |= WHITE_QUEENSIDE_ROOK;
    }

    return castle_set;

}


uint64_t castle_set_black() {

    uint64_t castle_set = 0;
    uint64_t one = 1;

    uint64_t attacked = compute_white_attacked_minus_black_king();

    uint64_t kingside_attacked = (piece[60] | piece[61] | piece[62]) & attacked;
    uint64_t queenside_attacked = (piece[60] | piece[59] | piece[58]) & attacked;

    if (castle_flags & (1 << CASTLE_BLACK_KINGSIDE) && kingside_attacked == 0) {

        // Check ray from king to rook
        uint64_t hray = rook_attacked(BLACK_KING_INITIAL, bitboards[WB_ALL]);

        if (hray & BLACK_KINGSIDE_ROOK)
            castle_set |= BLACK_KINGSIDE_ROOK;
    }

    if (castle_flags & (1 << CASTLE_BLACK_QUEENSIDE) && queenside_attacked == 0) {

        uint64_t hray = rook_attacked(BLACK_KING_INITIAL, bitboards[WB_ALL]);
        
        if (hray & BLACK_QUEENSIDE_ROOK)
            castle_set |= BLACK_QUEENSIDE_ROOK;
    }

    return castle_set;

}

void castle(uint64_t castle_square) {

    uint64_t king_initial;
    uint64_t king_castled;
    uint64_t rook_initial;
    uint64_t rook_castled;

    uint8_t side;
    uint8_t king;
    uint8_t rook;

    uint8_t x_start, x_end;
    uint8_t y;

    if (castle_square & WHITE_KINGSIDE_ROOK) {

        // Set initialisation variables
        king_initial = WHITE_KING_INITIAL;
        king_castled = WHITE_KINGSIDE_KING_CASTLED;
        rook_initial = WHITE_KINGSIDE_ROOK;
        rook_castled = WHITE_KINGSIDE_ROOK_CASTLED;
        side = W_ALL;
        king = W_KING;
        rook = W_ROOK;

        // Update flags
        castle_flags &= ~(1 << CASTLE_WHITE_KINGSIDE);
        castle_flags &= ~(1 << CASTLE_WHITE_QUEENSIDE);

        // Update display representation
        board[4][7] = EMPTY;
        board[7][7] = EMPTY;
        board[6][7] = W_KING;
        board[5][7] = W_ROOK;

        x_start = 4;
        x_end = 7;
        y = 7;

    } else if (castle_square & WHITE_QUEENSIDE_ROOK) {

        king_initial = WHITE_KING_INITIAL;
        king_castled = WHITE_QUEENSIDE_KING_CASTLED;
        rook_initial = WHITE_QUEENSIDE_ROOK;
        rook_castled = WHITE_QUEENSIDE_ROOK_CASTLED;
        side = W_ALL;
        king = W_KING;
        rook = W_ROOK;

        castle_flags &= ~(1 << CASTLE_WHITE_KINGSIDE);
        castle_flags &= ~(1 << CASTLE_WHITE_QUEENSIDE);

        board[4][7] = EMPTY;
        board[0][7] = EMPTY;
        board[2][7] = W_KING;
        board[3][7] = W_ROOK;

        x_start = 0;
        x_end = 4;
        y = 7;

    } else if (castle_square & BLACK_KINGSIDE_ROOK) {

        king_initial = BLACK_KING_INITIAL;
        king_castled = BLACK_KINGSIDE_KING_CASTLED;
        rook_initial = BLACK_KINGSIDE_ROOK;
        rook_castled = BLACK_KINGSIDE_ROOK_CASTLED;
        side = B_ALL;
        king = B_KING;
        rook = B_ROOK;

        castle_flags &= ~(1 << CASTLE_BLACK_KINGSIDE);
        castle_flags &= ~(1 << CASTLE_BLACK_QUEENSIDE);

        board[4][0] = EMPTY;
        board[7][0] = EMPTY;
        board[6][0] = B_KING;
        board[5][0] = B_ROOK;

        x_start = 4;
        x_end = 7;
        y = 0;

    } else if (castle_square & BLACK_QUEENSIDE_ROOK) {

        king_initial = BLACK_KING_INITIAL;
        king_castled = BLACK_QUEENSIDE_KING_CASTLED;
        rook_initial = BLACK_QUEENSIDE_ROOK;
        rook_castled = BLACK_QUEENSIDE_ROOK_CASTLED;
        side = B_ALL;
        king = B_KING;
        rook = B_ROOK;

        castle_flags &= ~(1 << CASTLE_BLACK_KINGSIDE);
        castle_flags &= ~(1 << CASTLE_BLACK_QUEENSIDE);

        board[4][0] = EMPTY;
        board[0][0] = EMPTY;
        board[2][0] = B_KING;
        board[3][0] = B_ROOK;

        x_start = 0;
        x_end = 4;
        y = 0;

    }
#ifdef DEBUG
    else {
        // SHOULDN'T HAPPEN
        debug_bitboard(ERROR_ERRONEOUS_CASTLE_CALL);
        return;
    }
#endif

    // Move king
    bitboards[king] = king_castled;
    
    // Move rook
    bitboards[rook] &= ~rook_initial;
    bitboards[rook] |= rook_castled;

    // Update side board
    bitboards[side] &= ~king_initial & ~rook_initial;
    bitboards[side] |= king_castled | rook_castled;

    // Update global board
    bitboards[WB_ALL] = bitboards[W_ALL] | bitboards[B_ALL];

    // Redraw squares
    for (uint8_t k = x_start; k <= x_end; k++) {
        uint16_t col = ((k + y) & 1) ? DK_SQ_COL : LT_SQ_COL;
        draw_square(k, y, col);
        draw_piece(k, y);
    }

}

// Comptue pin mask assuming enemy is black
uint64_t compute_pin_mask_white(uint64_t piece) {

    // Compute all sliding enemy moves + pawns and determine if rays
    // ever intersect with same move from king position.
    // Then, if the overlap contains the piece in quesiton, it is pinned.
    // The overlapping ray is then the pin mask.

    // ASSUMPTION: Pieces can't be double-pinned to the king right?
    // I can't think of a way this is possible. If I'm wrong, then this is broken.

    // TODO: Each pin should include position of attacking piece as well
    // so it can be taken

    uint64_t pin_mask = 0;
    uint64_t capture_mask = 0;

    uint64_t white_excluded = bitboards[W_ALL] & ~piece;

    // P
    // TODO: Check if this needs to use excluded white like the others.
    uint64_t p = black_pawn_moveable(bitboards[B_PAWN]);
    uint64_t p_from_k = white_pawn_moveable(bitboards[W_KING]);
    if (piece & p & p_from_k) {
        pin_mask |= ~piece & p & p_from_k;
        capture_mask |= p_from_k & bitboards[B_PAWN];
        return pin_mask | capture_mask;
    }

    // B
    uint64_t b = bishop_attacked(bitboards[B_BISHOP], bitboards[WB_ALL] & ~piece);
    uint64_t b_from_k = bishop_attacked(bitboards[W_KING], bitboards[WB_ALL] & ~piece);
    if (piece & b & b_from_k) {
        pin_mask |= ~piece & b & b_from_k;
        capture_mask |= b_from_k & bitboards[B_BISHOP];
        return pin_mask | capture_mask;
    }

    // R
    uint64_t r = rook_attacked(bitboards[B_ROOK], bitboards[WB_ALL] & ~piece);
    uint64_t r_from_k = rook_attacked(bitboards[W_KING], bitboards[WB_ALL] & ~piece);
    if (piece & r & r_from_k) {
        pin_mask |= ~piece & r & r_from_k;
        capture_mask |= r_from_k & bitboards[B_ROOK];
        return pin_mask | capture_mask;
    }

    // Queen diagonals
    uint64_t qd = bishop_attacked(bitboards[B_QUEEN], bitboards[WB_ALL] & ~piece);
    uint64_t qd_from_k = bishop_attacked(bitboards[W_KING], bitboards[WB_ALL] & ~piece);
    if (piece & qd & qd_from_k) {
        pin_mask |= qd & qd_from_k & ~piece;
        capture_mask |= qd_from_k & bitboards[B_QUEEN];
        return pin_mask | capture_mask;
    }

    // Queen non-diagonals
    uint64_t qn = rook_attacked(bitboards[B_QUEEN], bitboards[WB_ALL] & ~piece);
    uint64_t qn_from_k = rook_attacked(bitboards[W_KING], bitboards[WB_ALL] & ~piece);
    if (piece & qn & qn_from_k) {
        pin_mask |= ~piece & qn & qn_from_k;
        capture_mask |= qn_from_k & bitboards[B_QUEEN];
        return pin_mask | capture_mask;
    }

    // ASSUMPTION 2: There is no need to compute for a queen again here, this case is covered
    // by rook and bishops computations above.

    // // Q
    // uint64_t q = compute_queen(bitboards[B_QUEEN], bitboards[B_ALL], white_excluded);
    // uint64_t q_from_k = compute_queen(bitboards[W_KING], white_excluded, bitboards[B_ALL]);
    // if (piece & q & q_from_k) {
    //     pin_mask |= q & q_from_k;
    //     return pin_mask;
    // }
    

    return ~pin_mask;

}

// Comptue pin mask assuming enemy is white
uint64_t compute_pin_mask_black(uint64_t piece) {

    // Compute all sliding enemy moves + pawns and determine if rays
    // ever intersect with same move from king position.
    // Then, if the overlap contains the piece in quesiton, it is pinned.
    // The overlapping ray is then the pin mask.

    uint64_t pin_mask = 0;
    uint64_t capture_mask = 0;

    uint64_t black_excluded = bitboards[B_ALL] & ~piece;

    // P
    // TODO: Check if this needs to use excluded white like the others.
    uint64_t p = white_pawn_moveable(bitboards[W_PAWN]);
    uint64_t p_from_k = black_pawn_moveable(bitboards[B_KING]);
    if (piece & p & p_from_k) {
        pin_mask |= ~piece & p & p_from_k;
        capture_mask |= p_from_k & bitboards[W_PAWN];
        return pin_mask | capture_mask;
    }


    // B
    uint64_t b = bishop_attacked(bitboards[W_BISHOP], bitboards[WB_ALL] & ~piece);
    uint64_t b_from_k = bishop_attacked(bitboards[B_KING], bitboards[WB_ALL] & ~piece);
    if (piece & b & b_from_k) {
        pin_mask |= ~piece & b & b_from_k;
        capture_mask |= b_from_k & bitboards[W_BISHOP];
        return pin_mask | capture_mask;
    }
    

    // R
    uint64_t r = rook_attacked(bitboards[W_ROOK], bitboards[WB_ALL] & ~piece);
    uint64_t r_from_k = rook_attacked(bitboards[B_KING], bitboards[WB_ALL] & ~piece);
    if (piece & r & r_from_k) {
        pin_mask |= ~piece & r & r_from_k;
        capture_mask |= r_from_k & bitboards[W_ROOK];
        return pin_mask | capture_mask;
    }

    // Queen diagonals
    uint64_t qd = bishop_attacked(bitboards[W_QUEEN], bitboards[WB_ALL] & ~piece);
    uint64_t qd_from_k = bishop_attacked(bitboards[B_KING], bitboards[WB_ALL] & ~piece);
    if (piece & qd & qd_from_k) {
        pin_mask |= qd & qd_from_k & ~piece;
        capture_mask |= qd_from_k & bitboards[W_QUEEN];
        return pin_mask | capture_mask;
    }

    // Queen non-diagonals
    uint64_t qn = rook_attacked(bitboards[W_QUEEN], bitboards[WB_ALL] & ~piece);
    uint64_t qn_from_k = rook_attacked(bitboards[B_KING], bitboards[WB_ALL] & ~piece);
    if (piece & qn & qn_from_k) {
        pin_mask |= ~piece & qn & qn_from_k;
        capture_mask |= qn_from_k & bitboards[W_QUEEN];
        return pin_mask | capture_mask;
    }
    

    // // Q
    // uint64_t q = compute_queen(bitboards[W_QUEEN], bitboards[W_ALL], black_excluded);
    // uint64_t q_from_k = compute_queen(bitboards[B_KING], black_excluded, bitboards[W_ALL]);
    // if (piece & q & q_from_k) {
    //     pin_mask |= q & q_from_k;
    //     return pin_mask;
    // }
    

    return ~pin_mask;

}