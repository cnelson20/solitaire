typedef struct mouse {
	unsigned short x_pos;
	unsigned short y_pos;
	unsigned char buttons;
};

void mouse_get(struct mouse *mp);

void waitforjiffy();

unsigned char rand_byte();
unsigned short rand_word();

void clear_screen();
void set_bg_screen();

void enable_mouse();
