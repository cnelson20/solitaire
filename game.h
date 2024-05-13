void shuffle_deck();
void prompt_difficulty();
void start_game();
void check_faceup_piles();

void parse_keys();
void parse_mouse_mvmt();
unsigned char *find_selection();
unsigned char check_valid_move(unsigned char *target);

void move_cards(unsigned char *dest_addr, unsigned char dest_offset,
	unsigned char *src_pile, unsigned char src_offset);

unsigned char get_card_suite(unsigned char card);
unsigned char get_card_color(unsigned char card);
unsigned char get_card_rank(unsigned char card);
unsigned char get_card_char(unsigned char card);

void setup_video();
void clear_rect(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2);
unsigned char get_x_offset(unsigned char pile_num);
void draw_blank_card();
void draw_empty_card();
void draw_card(unsigned char card);
void draw_card_front(unsigned char card);
void draw_card_back();
void draw_deck_scratch_pile();
void draw_selected_card();
void display_piles();

void display_win();

#define CARD_JACK 11
#define CARD_QUEEN 12
#define CARD_KING 13
#define CARD_NP 0

#define DEFAULT_COLOR 0x61

#define CARD_GRAPHICS_WIDTH 8
#define CARD_GRAPHICS_HEIGHT 10

#define COMPLETED_Y_OFFSET 1
#define COMPLETED_Y_END (COMPLETED_Y_OFFSET + CARD_GRAPHICS_HEIGHT)

#define PILES_Y_OFFSET (COMPLETED_Y_END + 1)
#define PILES_Y_END (PILES_Y_OFFSET + 34)

#define DECK_Y_OFFSET (PILES_Y_END + 1)
#define DECK_Y_END (DECK_Y_OFFSET + CARD_GRAPHICS_HEIGHT)