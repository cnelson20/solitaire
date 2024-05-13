#include <ascii_charmap.h>
#include <cbm.h>
#include <peekpoke.h>

#include "helper.h"
#include "game.h"

#define MAX_DECK_SIZE 52
#define NUM_PILES 7

#define CLICKED_EMPTY ((unsigned char *)0xFFFF)

extern unsigned char deck[MAX_DECK_SIZE];

unsigned char deck_size;

extern unsigned char facedown_piles[NUM_PILES][8];
extern unsigned char faceup_piles[NUM_PILES][16];

unsigned char scratch_pile[MAX_DECK_SIZE];
unsigned char completed_piles[4][16];

struct mouse mouse_input;
unsigned char *selected_card;

unsigned char cards_to_deal;

unsigned char game_over;
unsigned char victory;

void main() {
	setup_video();
	
	prompt_difficulty();
	
	while (1) {
		start_game();
		check_faceup_piles();
		
		display_piles();
		
		while (!game_over) {
			// Do mouse movement
			parse_keys();
			parse_mouse_mvmt();
			check_faceup_piles();
			
			waitforjiffy();
		};
		if (victory != 0) {
			display_win();
			clear_screen();
		}
		
	}
}

void start_game() {
	unsigned char pile_num, pile_ind;
	unsigned char deck_ind;
	
	game_over = 0;
	victory = 0;
	selected_card = NULL;
	
	shuffle_deck();
	
	// Deal out into piles
	deck_ind = deck_size - 1;
	for (pile_num = 0; pile_num < NUM_PILES; ++pile_num) {
		for (pile_ind = 0; pile_ind <= pile_num; ++pile_ind) {
			facedown_piles[pile_num][pile_ind] = deck[deck_ind];
			
			deck[deck_ind] = CARD_NP;
			--deck_ind;
		}
		facedown_piles[pile_num][pile_ind] = CARD_NP;
		
		faceup_piles[pile_num][0] = CARD_NP;
	}
	
	for (pile_num = 0; pile_num < 4; ++pile_num) {
		completed_piles[pile_num][0] = CARD_NP;
	}
	scratch_pile[0] = CARD_NP;
}

unsigned char get_pile_size(unsigned char *pile) {
	static unsigned char i;
	i = 0;
	
	while (pile[i] != CARD_NP) {
		++i;
	}
	return i;
}

void move_cards(unsigned char *dest_pile, unsigned char dest_offset,
	unsigned char *src_pile, unsigned char src_offset) {
	
	unsigned char i;
	
	dest_pile += dest_offset;
	src_pile += src_offset;
	
	for (i = 0; src_pile[i] != CARD_NP; ++i) {
		dest_pile[i] = src_pile[i];
		src_pile[i] = CARD_NP;
	}
	dest_pile[i] = CARD_NP;
}

void check_faceup_piles() {
	static unsigned char pile_num, i;
	
	for (pile_num = 0; pile_num < NUM_PILES; ++pile_num) {
		
		if (get_pile_size(faceup_piles[pile_num]) == 0) {
			i = get_pile_size(facedown_piles[pile_num]);
			if (i != 0) {
				move_cards(faceup_piles[pile_num], 0, facedown_piles[pile_num], i - 1);
			}
		}
	}
	
	for (pile_num = 0; pile_num < 4; ++pile_num) {
		if (get_pile_size(completed_piles[pile_num]) < 13) {
			return;
		}
	}
	game_over = 1;
	victory = 1;
	
}

void shuffle_deck() {
	static unsigned char ind, i;
	i = 1;
	// Populate deck
	for (ind = 0; ind < MAX_DECK_SIZE; ++ind) {
		deck[ind] = i;
		if ((i & 0xF) == CARD_KING) {
			i = (i & 0xF0) + 17;
		} else {
			++i;
		}
	}
	// Now shuffle it
	for (ind = 0; ind < MAX_DECK_SIZE - 1; ++ind) {
		i = (rand_word() % (MAX_DECK_SIZE - ind)) + ind;
		// Swap cards at indices ind and i
		__asm__ ("ldy %v", i);
		__asm__ ("ldx %v", ind);
		__asm__ ("lda %v, Y", deck);
		__asm__ ("pha");
		__asm__ ("lda %v, X", deck);
		__asm__ ("sta %v, Y", deck);
		__asm__ ("pla");
		__asm__ ("sta %v, X", deck);
		
	}
	
	deck_size = 52;
}

void parse_keys() {
	static unsigned char key_pressed;
	key_pressed = cbm_k_getin();
	
	if (key_pressed == 0x1B) {
		game_over = 1;
		victory = 0;
	}
}

void parse_mouse_mvmt() {
	static unsigned char *selection;
	
	mouse_get(&mouse_input);
	selection = find_selection();
	
	// Flip cards from deck
	if (selection == deck) {
		static unsigned char deck_pile_size, scratch_pile_size;
		deck_pile_size = get_pile_size(deck);
		scratch_pile_size = get_pile_size(scratch_pile);
		
		if (get_pile_size(deck) == 0) {
			// Refill deck
			while (scratch_pile_size != 0) {
				move_cards(deck, deck_pile_size, scratch_pile, scratch_pile_size - 1);
				
				++deck_pile_size;
				--scratch_pile_size;
			}
		} else {
			unsigned char i;
			// Move cards from deck to scratch
			for (i = cards_to_deal; i != 0 && deck_pile_size != 0; --i) {
				move_cards(scratch_pile, scratch_pile_size, deck, deck_pile_size - 1);
				++scratch_pile_size;
				--deck_pile_size;
			}
		}
		if (selected_card >= scratch_pile && selected_card < (scratch_pile + sizeof(scratch_pile))) {
			// deselect cards
			selected_card = NULL;
			draw_selected_card();
		}
		draw_deck_scratch_pile();
		return;
	}
	
	if (selected_card == NULL) {
		if (selection != CLICKED_EMPTY) {
			selected_card = selection;
			draw_selected_card();
		} 
	} else {
		if (selection == CLICKED_EMPTY) {
			selected_card = NULL;
			draw_selected_card();
		} else if (selection != NULL && selection != selected_card) {
			// Need to check if move is valid
			if (check_valid_move(selection)) {
				selected_card = NULL;
				check_faceup_piles();
				display_piles();
			} else {
				cbm_k_chrout(0x7); // BELL
			}
		}
		
	}
}

unsigned char *find_selection() {
	static unsigned char mouse_click_last = 0;
	static unsigned char click_eor;
	
	static unsigned char tile_x, tile_y, tile_val;
	static unsigned char pile_num, pile_size;
	static unsigned char facedown_pile_size;
	
	tile_x = mouse_input.x_pos >> 3;
	tile_y = mouse_input.y_pos >> 3;
	
	click_eor = mouse_input.buttons & ~(mouse_click_last);
	mouse_click_last = mouse_input.buttons;
	
	if (click_eor & 0x2) {
		selected_card = NULL;
	} else if (click_eor & 0x1) {
		// If mouse clicked, see what card was clicked
		POKE(0x9F20, tile_x << 1);
		POKE(0x9F21, tile_y);
		POKE(0x9F22, 0);
		// if we clicked on a empty tile, return
		tile_val = PEEK(0x9F23);
		
		// We clicked on something
		if (tile_y >= COMPLETED_Y_OFFSET && tile_y < COMPLETED_Y_END) {
			// Maybe the completed piles
			for (pile_num = 0; pile_num < 4; ++pile_num) {
				if (tile_x >= get_x_offset(pile_num) && tile_x < get_x_offset(pile_num + 1)) {
					// Clicked on the ith completed stack
					pile_size = get_pile_size(completed_piles[pile_num]); 
					if (pile_size == 0 && selected_card != NULL) {
						return completed_piles[pile_num];
					}
					if (pile_size != 0 && tile_val != 0x20) {
						return &( completed_piles[pile_num][pile_size - 1] );
					}
					break;
				}
			}
		} else if (tile_y >= PILES_Y_OFFSET && tile_y < PILES_Y_END) {
			// Maybe the working piles
			for (pile_num = 0; pile_num < NUM_PILES; ++pile_num) {
				if (tile_x >= get_x_offset(pile_num) && tile_x < get_x_offset(pile_num + 1)) {
					// Clicked on the ith working pile   
					pile_size = get_pile_size(faceup_piles[pile_num]);
					if (pile_size == 0) {
						if (tile_y < PILES_Y_OFFSET + CARD_GRAPHICS_HEIGHT && selected_card != NULL) {
							return faceup_piles[pile_num];
						}
					} else {
						facedown_pile_size = get_pile_size(facedown_piles[pile_num]);
						if (tile_val != 0x20 && (tile_y >= PILES_Y_OFFSET + (facedown_pile_size << 1))) {
							unsigned char j = (tile_y - PILES_Y_OFFSET - (facedown_pile_size << 1)) >> 1;
							if (j >= pile_size) { j = pile_size - 1; }
							return & ( faceup_piles[pile_num][j] );
						}
					} 
					break;
				}
			}
		} else if (tile_y >= DECK_Y_OFFSET && tile_y < DECK_Y_END) {
			if (tile_x >= get_x_offset(0) && tile_x < get_x_offset(1)) {
				return deck; // Return pointer to deck
			} else if (tile_x < get_x_offset(2)) {
				pile_size = get_pile_size(scratch_pile);
				if (pile_size != 0) {
					return &( scratch_pile[pile_size - 1] );
				}
			}
		}
		if (tile_val == 0x20) {
			return CLICKED_EMPTY;
		}
		
	}
	return NULL;
}

unsigned char check_valid_move(unsigned char *target) {
	static unsigned char sc, tc;
	
	sc = *selected_card;
	tc = *target;
	if (target >= (unsigned char *)completed_piles 
		&& target < (unsigned char *)completed_piles + sizeof(completed_piles)) {
		// Check if target is one of the completed piles
		// Can only move one card to completed pile at a time
		if (*(selected_card + 1) != CARD_NP) return 0;
		
		if (tc == CARD_NP) {
			if (get_card_rank(sc) == 1) {
				move_cards(target, 0, selected_card, 0);
				return 1;
			}
		} else {
			if (get_card_rank(sc) == get_card_rank(tc) + 1 && get_card_suite(sc) == get_card_suite(tc)) {
				move_cards(target, 1, selected_card, 0);
				return 1;
			}
		}
	} else if (target >= (unsigned char *)faceup_piles 
		&& target < (unsigned char *)faceup_piles + sizeof(faceup_piles)) {
		// Moving between piles that are non-empty
		if (tc == CARD_NP) {
			if (get_card_rank(sc) == CARD_KING) {
				unsigned short diff = target - (unsigned char *)faceup_piles;
				if ((diff & 0xF) == 0) {
					move_cards(target, 0, selected_card, 0);
					return 1;
				}
			}
			return 0;
		}
		if (get_card_color(sc) == get_card_color(tc)) {
			return 0;
		}
		if (get_card_rank(sc) + 1 == get_card_rank(tc)) {
			move_cards(target, 1, selected_card, 0);
			return 1;
		}
		return 0;
	}
	return 0;
}

// Spade, Club, Heart, Diamond
unsigned char suite_petscii_table[] = {0x41, 0x58, 0x53, 0x5A};
unsigned char suite_colors_table[] = {0x60, 0x60, 0x62, 0x62};

unsigned char get_card_suite(unsigned char card) {
	return suite_petscii_table[(card & 0xF0) >> 4];
}

unsigned char get_card_color(unsigned char card) {
	return suite_colors_table[(card & 0xF0) >> 4];
}

unsigned char get_card_rank(unsigned char card) {
	return card & 0xF;
}

// Ten, Jack, Queen, King
unsigned char rank_petscii_table[] = {0x14, 0x0A, 0x11, 0x0B};

unsigned char get_card_char(unsigned char card) {
	unsigned char card_num = card & 0xF;
	
	if (card_num == 1) {
		return 0x01; // screen code for 'A'
	} else if (card_num >= 10) {
		return rank_petscii_table[card_num - 10];
	} else {
		return card_num | 0x30;
	}
}

unsigned char last4_char_updates[] = {
	0x00, 0x00, 0x00, 0x07, 0x0F, 0x1F, 0x1F, 0x1F, 
	0x00, 0x00, 0x00, 0xE0, 0xF0, 0xF8, 0xF8, 0xF8, 
	0x1F, 0x1F, 0x1F, 0x0F, 0x07, 0x00, 0x00, 0x00, 
	0xF8, 0xF8, 0xF8, 0xF0, 0xE0, 0x00, 0x00, 0x00,
};

/*
	More graphics code
*/

void setup_video() {
	unsigned char i;
	
	cbm_k_chrout(0x8E);
	
	POKE(0x9F25, 0);
	// 80x60
	POKE(0x9F2A, 128);
	POKE(0x9F2B, 128);
	// set mapbase to 00000
	POKE(0x9F35, 0);
	
	POKEW(0x9F20, 0xF7E0);
	POKE(0x9F22, 0x11);
	for (i = 0; i < sizeof(last4_char_updates); ++i) {
		POKE(0x9F23, last4_char_updates[i]);
	}
	
	enable_mouse();
	
	clear_screen();
}

unsigned char card_graphics_array_top[] = {0x55, 0x40, 0x40, 0x40, 0x40, 0x40, 0x49, 0x20};
unsigned char card_graphics_array_middle[] = {0x5D, 0x66, 0x66, 0x66, 0x66, 0x66, 0x5D, 0x20};
unsigned char card_graphics_array_bottom[] = {0x4A, 0x40, 0x40, 0x40, 0x40, 0x40, 0x4B, 0x20};

unsigned char card_graphics_blank[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
unsigned char card_graphics_empty[] = {0x5D, 0x60, 0x60, 0x60, 0x60, 0x60, 0x5D, 0x20};

unsigned char card_front_array_top[] = {0xFC, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xFD, 0x20};
unsigned char card_front_array_bottom[] = {0xFE, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xFF, 0x20};

unsigned char card_graphics_tables[][CARD_GRAPHICS_HEIGHT - 2] = {
	{0, 0, 0, 0, 0, 0, 0, 0}, // N/A
	{3, 0, 0, 1, 0, 0, 4, 0}, // Ace
	{5, 0, 0, 0, 0, 0, 6, 0}, // 2
	{5, 0, 0, 1, 0, 0, 6, 0}, // 3
	{7, 0, 0, 0, 0, 0, 8, 0}, // 4
	{7, 0, 0, 1, 0, 0, 8, 0}, // 5
	{7, 0, 0, 2, 0, 0, 8, 0}, // 6
	{7, 1, 0, 2, 0, 0, 8, 0}, // 7
	{7, 1, 0, 2, 0, 1, 8, 0}, // 8
	{7, 0, 2, 1, 2, 0, 8, 0}, // 9
	{7, 1, 2, 0, 2, 1, 8, 0}, // 10
	{9, 10, 11, 12, 13, 14, 15, 0}, // Jack
	{23, 24, 25, 26, 27, 28, 29, 0}, // Queen
	{16, 17, 18, 19, 20, 21, 22, 0}, // King
};

unsigned char card_front_rows[][CARD_GRAPHICS_WIDTH - 2] = {
	{0x60, 0x60, 0x60, 0x60, 0x60}, // 0
	{0x60, 0x60, 0x01, 0x60, 0x60}, // 1
	{0x60, 0x01, 0x60, 0x01, 0x60}, // 2
	
	{0x02, 0x60, 0x60, 0x60, 0x60}, // 3
	{0x60, 0x60, 0x60, 0x60, 0x02}, // 4
	
	{0x02, 0x60, 0x01, 0x60, 0x60}, // 5
	{0x60, 0x60, 0x01, 0x60, 0x02}, // 6
	
	{0x02, 0x01, 0x60, 0x01, 0x60}, // 7
	{0x60, 0x01, 0x60, 0x01, 0x02}, // 8
	
	{0x02, 0x60, 0x5F, 0xA0, 0x69}, // 9
	{0x01, 0x60, 0x60, 0x51, 0x60}, // 10
	{0x6A, 0x60, 0xE9, 0xA0, 0x60}, // 11
	{0x6A, 0xE9, 0xA0, 0x69, 0x74}, // 12
	{0x60, 0xA0, 0x69, 0x60, 0x74}, // 13
	{0x60, 0x51, 0x60, 0x60, 0x01}, // 14
	{0xE9, 0xA0, 0xDF, 0x60, 0x02}, // 15
	
	{0x02, 0x5F, 0xA0, 0x69, 0x60}, // 16
	{0x01, 0x60, 0x51, 0x60, 0x60}, // 17
	{0x60, 0x60, 0xA0, 0xDF, 0x74}, // 18
	{0x60, 0xE9, 0xA0, 0x69, 0x60}, // 19
	{0x6A, 0x5F, 0xA0, 0x60, 0x60}, // 20
	{0x60, 0x60, 0x51, 0x60, 0x01}, // 21
	{0x60, 0xE9, 0xA0, 0xDF, 0x02}, // 22
	
	{0x02, 0x60, 0x51, 0x5F, 0x60}, // 23
	{0x01, 0x60, 0xE9, 0xDF, 0x60}, // 24
	{0x60, 0x60, 0xA0, 0x69, 0x60}, // 25
	{0x60, 0x60, 0xA0, 0x60, 0x60}, // 26
	{0x60, 0xE9, 0xA0, 0x60, 0x60}, // 27
	{0x60, 0x5F, 0x69, 0x60, 0x01}, // 28
	{0x60, 0xDF, 0x51, 0x60, 0x02}, // 29
	
}; 

void clear_rect(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2) {
	static unsigned char xoff; 
	static unsigned char i, times;
	
	xoff = x1 << 1;
	times = x2 - x1;
	
	POKE(0x9F22, 0x10);
	POKE(0x9F21, y1);
	while (++y1 < y2) {
		POKE(0x9F20, xoff);
		for (i = 0; i < times; ++i) {
			POKE(0x9F23, 0x20);
			POKE(0x9F23, DEFAULT_COLOR);
		}
		__asm__ ("inc $9F21");
	}
}

void draw_line(unsigned char *graphics) {
	static unsigned char i;
	
	for (i = 0; i < CARD_GRAPHICS_WIDTH; ++i) {
		POKE(0x9F23, graphics[i]);
		POKE(0x9F23, DEFAULT_COLOR);
	}
	__asm__ ("inc $9F21");
}

void draw_repeat_card(unsigned char *graphics_line) {
	static unsigned char i;
	static unsigned char xoff, yoff;
	
	xoff = PEEK(0x9F20);
	yoff = PEEK(0x9F21);	
	
	for (i = 0; i < CARD_GRAPHICS_HEIGHT; ++i) {
		POKE(0x9F20, xoff);
		draw_line(graphics_line);
	}
	
	POKE(0x9F21, yoff + 2);
	POKE(0x9F20, xoff);
}

void draw_blank_card() {
	draw_repeat_card(card_graphics_blank);
}

void draw_empty_card() {
	static unsigned char xoff, i;
	xoff = PEEK(0x9F20);
	
	draw_line(card_graphics_array_top);
	for (i = 0; i < CARD_GRAPHICS_HEIGHT - 2; ++i) {
		POKE(0x9F20, xoff);
		draw_line(card_graphics_empty);
	}
	POKE(0x9F20, xoff);
	draw_line(card_graphics_array_bottom);
}

void draw_card(unsigned char card) {
	if (card != 0) {
		draw_card_front(card);
	} else {
		draw_card_back();
	}
}

void draw_front_line(unsigned char card, unsigned char *line) {
	unsigned char i;
	unsigned char schar, scolor, suite;
	
	suite = get_card_suite(card);
	schar = get_card_char(card);
	scolor = get_card_color(card);
	
	POKE(0x9F23, 0xF5);
	POKE(0x9F23, DEFAULT_COLOR);
	
	scolor = (scolor & 0xf) | 0x10;
	
	for (i = 0; i < CARD_GRAPHICS_WIDTH - 2 - 1; ++i) {
		if (line[i] == 0x02) {
			POKE(0x9F23, schar);
			POKE(0x9F23, scolor);
		} else if (line[i] == 0x01) {
			POKE(0x9F23, suite);
			POKE(0x9F23, scolor);
		} else {
			POKE(0x9F23, line[i]);
			POKE(0x9F23, scolor);
		}
	}
	
	POKE(0x9F23, 0xF6);
	POKE(0x9F23, DEFAULT_COLOR);
	POKE(0x9F23, 0x20);
	POKE(0x9F23, DEFAULT_COLOR);
	__asm__ ("inc $9F21");
}

void draw_card_front(unsigned char card) {
	static unsigned char x_offset, y_offset;
	static unsigned char j;
	static unsigned char suite_char, suite_color, card_rank;
	
	suite_char = get_card_suite(card);
	suite_color = get_card_color(card);
	card_rank = get_card_rank(card);
	
	x_offset = PEEK(0x9F20);
	y_offset = PEEK(0x9F21);
	
	// Draw top row of card
	draw_line(card_front_array_top);
	
	// Draw middle of card
	for (j = 0; j < (CARD_GRAPHICS_HEIGHT - 2); ++j) {
		POKE(0x9F20, x_offset);
		draw_front_line(card, card_front_rows[ card_graphics_tables[card_rank][j] ]);
	}
	// Draw last row of card
	POKE(0x9F20, x_offset);
	draw_line(card_front_array_bottom);
	
	POKE(0x9F20, x_offset);
	POKE(0x9F21, y_offset + 2);
}

void draw_card_back() {
	static unsigned char x_offset, y_offset;
	static unsigned char j;
	
	x_offset = PEEK(0x9F20);
	y_offset = PEEK(0x9F21);
	
	// Draw top row of card
	draw_line(card_graphics_array_top);
	
	// Draw middle of card
	for (j = 0; j < (CARD_GRAPHICS_HEIGHT - 2); ++j) {
		POKE(0x9F20, x_offset);
		draw_line(card_graphics_array_middle);
	}
	// Draw last row of card
	POKE(0x9F20, x_offset);
	draw_line(card_graphics_array_bottom);
	
	POKE(0x9F20, x_offset);
	POKE(0x9F21, y_offset + 2);
}

unsigned char get_x_offset(unsigned char pile_num) {
	return (2 + (CARD_GRAPHICS_WIDTH + 1) * pile_num);
}

void draw_selected_card() {
	POKE(0x9F20, get_x_offset(7) << 1);
	POKE(0x9F21, DECK_Y_OFFSET);
	POKE(0x9F22, 0x10);
	if (selected_card != NULL) {		
		draw_card(*selected_card);
	} else {
		draw_blank_card();
	}
}

void draw_deck_scratch_pile() {
	unsigned char i, j, pile_size;
	
	POKE(0x9F21, DECK_Y_OFFSET);
	POKE(0x9F20, get_x_offset(0) << 1);
	POKE(0x9F22, 0x10);
	
	if (get_pile_size(deck) != 0) {
		draw_card(0);
	} else {
		draw_empty_card();
	}
	
	POKE(0x9F21, DECK_Y_OFFSET);
	POKE(0x9F20, get_x_offset(1) << 1);
	POKE(0x9F22, 0x10);
	if (get_pile_size(scratch_pile) == 0) {
		for (i = 0; i < 3; ++i) {
			draw_blank_card();
		}
	} else {
		pile_size = get_pile_size(scratch_pile);
		i = (pile_size >= 3) ? (pile_size - 3) : 0;
		j = 0;
		for (j = 0; j < 3; ++j) {
			if (i < pile_size) {
				draw_card(scratch_pile[i]);
			} else {
				POKE(0x9F21, CARD_GRAPHICS_HEIGHT - 2 + PEEK(0x9F21));
				draw_blank_card();
			}
			++i;
		}
	}
}

void display_piles() {
	static unsigned char vera_x_offset;
	static unsigned char pile_num, pile_ind;
	
	waitforjiffy();
	
	// Display completed piles
	for (pile_num = 0; pile_num < 4; ++pile_num) {
		POKE(0x9F20, get_x_offset(pile_num) << 1);
		POKE(0x9F21, COMPLETED_Y_OFFSET);
		POKE(0x9F22, 0x10);
		
		if (completed_piles[pile_num][0] != CARD_NP) {
			unsigned char i;
			
			i = get_pile_size(completed_piles[pile_num]) - 1;
			draw_card(completed_piles[pile_num][i]);
		} else {
			draw_empty_card();
		}	
	}
	
	// Display layout piles
	for (pile_num = 0; pile_num < NUM_PILES; ++pile_num) {		
		vera_x_offset = get_x_offset(pile_num) << 1;
		
		POKE(0x9F20, vera_x_offset);
		POKE(0x9F21, PILES_Y_OFFSET);
		POKE(0x9F22, 0x10);
		
		pile_ind = 0;
		while (facedown_piles[pile_num][pile_ind] != CARD_NP) {
			draw_card(CARD_NP);	
			++pile_ind;
		}
		pile_ind = 0;
		while (faceup_piles[pile_num][pile_ind] != CARD_NP) {
			draw_card(faceup_piles[pile_num][pile_ind]);	
			++pile_ind;
		}
		vera_x_offset = vera_x_offset >> 1;
		if (PEEK(0x9F21) != PILES_Y_OFFSET) {
			pile_ind = PEEK(0x9F21) + (CARD_GRAPHICS_HEIGHT - 2);
		} else {
			pile_ind = PEEK(0x9F21);
		}
		clear_rect(vera_x_offset, pile_ind, vera_x_offset + CARD_GRAPHICS_WIDTH, PILES_Y_END + (pile_num << 1));
		
	}
	// Draw deck, scratch pile, selected card
	draw_deck_scratch_pile();
	
	draw_selected_card();
}

void poke_str(char *str) {
	static unsigned char c;
	while (*str) {
		c = *str;
		if (c >= 'A') { c -= 0x40; }
		POKE(0x9F23, c);
		POKE(0x9F23, DEFAULT_COLOR);
		
		++str;
	}
}

unsigned char center_str_offset(unsigned char strlen) {
	return (40 - (strlen >> 1)) << 1;
}

char easy_string[] = "PRESS 1 TO DEAL 1 CARD AT A TIME";
#define EASY_STRLEN (sizeof(easy_string) - 1)

char harder_string[] = "PRESS 3 TO DEAL 3 AT A TIME";
#define HARDER_STRLEN (sizeof(harder_string) - 1)

void prompt_difficulty() {
	POKE(0x9F22, 0x10);
	POKE(0x9F21, 29);
	POKE(0x9F20, center_str_offset(EASY_STRLEN));
	poke_str(easy_string);
	
	POKE(0x9F21, 31);
	POKE(0x9F20, center_str_offset(EASY_STRLEN));
	poke_str(harder_string);
	
	do {
		unsigned char key_pressed = cbm_k_getin();
		
		if (key_pressed == '1') {
			cards_to_deal = 1;
			break;
		} else if (key_pressed == '3') {
			cards_to_deal = 3;
			break;
		}	
	} while (1);
	
	clear_screen();
}

char victory_string[] = "CONGRATS! YOU WIN ";
#define VICTORY_STRLEN (sizeof(victory_string) - 1)

char press_string[] = "PRESS SPACE TO CONTINUE ";
#define PRESS_STRLEN (sizeof(press_string) - 1)


void display_win() {
	POKE(0x9F22, 0x10);
	POKE(0x9F21, 29);
	POKE(0x9F20, center_str_offset(VICTORY_STRLEN));
	poke_str(victory_string);
	
	POKE(0x9F21, 31);
	POKE(0x9F20, center_str_offset(PRESS_STRLEN));
	poke_str(press_string);
	
	while (cbm_k_getin() != 0x20);
}