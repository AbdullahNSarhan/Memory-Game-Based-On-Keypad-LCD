#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>

// === LCD Definitions ===
#define LCD_C_P PORTB
#define LCD_D_P PORTA
#define LCD_C_DDR DDRB
#define LCD_D_DDR DDRA
#define EN 0
#define RW 1
#define RS 2

// === Keypad Definitions ===
#define KEYPAD_PORT PORTD
#define KEYPAD_PIN  PIND
#define KEYPAD_DDR  DDRD

// === Global Variables ===
unsigned char sequence[10];
unsigned char user_input[10];
unsigned char level, lives, score;

unsigned char KEYS[4][4] = {
	{'1','4','7','A'},
	{'2','5','8','0'},
	{'3','6','9','='},
	{'/','*','-','+'}
};

// === LCD Enable Pulse ===
void enable() {
	_delay_us(1);
	LCD_C_P |= (1 << EN);
	_delay_us(1);
	LCD_C_P &= ~(1 << EN);
	_delay_ms(2);
}

// === Send Command or Data to LCD ===
void SEND_C_D(unsigned char a, unsigned char b) {
	if (b == 0)
		LCD_C_P &= ~(1 << RS); // Command mode
	else if (b == 1)
		LCD_C_P |= (1 << RS); // Data mode

	LCD_C_P &= ~(1 << RW); // Write mode
	LCD_D_P &= 0xF0; // Clear upper nibble
	LCD_D_P |= (a >> 4); // Send upper nibble
	enable();

	LCD_D_P &= 0xF0; // Clear upper nibble again
	LCD_D_P |= (a & 0x0F); // Send lower nibble
	enable();
}

// === Set Cursor Position ===
void LCD_X_Y(unsigned char x, unsigned char y) {
	if (y == 0)
		SEND_C_D((0x80 + x), 0);
	if (y == 1)
		SEND_C_D((0xC0 + x), 0);
}

// === Send String to LCD ===
void SEND_D_ST(const char* s) {
	while (*s != '\0') {
		SEND_C_D(*s, 1);
		_delay_ms(5);
		s++;
	}
}

// === Initialize LCD ===
void LCD_INIT() {
	unsigned char com[] = {0x33, 0x32, 0x01, 0x28, 0x0C, 0x06, 0x80};
	for (unsigned char i = 0; i <= 6; i++) {
		SEND_C_D(com[i], 0);
		_delay_ms(20);
	}
}

// === Get Pressed Key from Keypad ===
unsigned char GET_KEY() {
	unsigned char key = 0;
	unsigned char row_data;
	
	// Set columns as outputs and rows as inputs with pull-ups
	KEYPAD_DDR = 0x0F;  // PD0-PD3 as outputs, PD4-PD7 as inputs
	KEYPAD_PORT = 0xFF; // Enable pull-ups on all pins
	
	// Scan each column
	for (unsigned char col = 0; col < 4; col++) {
		// Set current column LOW
		KEYPAD_PORT &= ~(1 << col);
		_delay_us(1);  // Keep short settling time
		
		// Read row data
		row_data = KEYPAD_PIN >> 4;  // Shift right to get row bits
		
		// Check each row
		for (unsigned char row = 0; row < 4; row++) {
			if (!(row_data & (1 << row))) {  // If key is pressed
				key = KEYS[row][col];
				_delay_ms(5);  // Reduced from 15ms to 5ms
				
				// Check if key is still pressed
				if (!(KEYPAD_PIN & (1 << (row + 4)))) {
					// Wait for key release
					while (!(KEYPAD_PIN & (1 << (row + 4))));
					_delay_ms(5);  // Reduced from 15ms to 5ms
					return key;
				}
			}
		}
		
		// Set column back to HIGH
		KEYPAD_PORT |= (1 << col);
	}
	
	return 0;
}

// === Display Generated Sequence on LCD ===
void DISPLAY_SEQUENCE(unsigned char* sequence, unsigned char len) {
	for (unsigned char i = 0; i < len; i++) {
		SEND_C_D(0x01, 0);
		_delay_ms(2);
		LCD_X_Y(0, 0);
		SEND_C_D(sequence[i], 1);
		_delay_ms(300);
	}
	SEND_C_D(0x01, 0);
	_delay_ms(2);
}

// === Get User Input from Keypad ===
void GET_USER_INPUT(unsigned char* input, unsigned char len) {
	unsigned char i = 0;
	unsigned char key;
	
	LCD_X_Y(0, 0);
	SEND_D_ST("Repeat:");
	LCD_X_Y(0, 1);

	while (i < len) {
		key = GET_KEY();
		if (key) {
			if (key == 'A' && i > 0) {
				i--;
				LCD_X_Y(i, 1);
				SEND_C_D(' ', 1);
				LCD_X_Y(i, 1);
			} else if (key != 'A') {
				input[i] = key;
				SEND_C_D(key, 1);
				i++;
			}
		}
	}
}

// === Compare User Input with Sequence ===
unsigned char CHECK_MATCH(unsigned char* seq1, unsigned char* seq2, unsigned char len) {
	for (unsigned char i = 0; i < len; i++) {
		if (seq1[i] != seq2[i]) return 0;
	}
	return 1;
}

// === Generate Random Sequence Using TCNT0 ===
void GENERATE_SEQUENCE(unsigned char* seq, unsigned char len) {
	for (unsigned char i = 0; i < len; i++) {
		seq[i] = '0' + (TCNT0 % 10);
		TCNT0 += 17;
	}
}

// === Game Over Display ===
void GAME_OVER() {
	SEND_C_D(0x01, 0);
	_delay_ms(2);
	LCD_X_Y(0, 0);
	SEND_D_ST("Game Over!");
	LCD_X_Y(0, 1);
	SEND_D_ST("Score:");
	SEND_C_D(score + '0', 1);
	_delay_ms(1500);
}

// === Game Win Display ===
void GAME_WIN() {
	SEND_C_D(0x01, 0);
	_delay_ms(2);
	LCD_X_Y(0, 0);
	SEND_D_ST("You Win!");
	LCD_X_Y(0, 1);
	SEND_D_ST("Score:");
	SEND_C_D(score + '0', 1);
	_delay_ms(1500);
}

// === Reset Game Variables ===
void RESET_GAME() {
	level = 1;
	lives = 3;
	score = 0;
}

// === Wait for Start Signal ===
void WAIT_FOR_START() {
	SEND_C_D(0x01, 0);
	_delay_ms(2);
	LCD_X_Y(0, 0);
	SEND_D_ST("Press Any Key");
	LCD_X_Y(0, 1);
	SEND_D_ST("To Start Game");

	while (!GET_KEY());

	SEND_C_D(0x01, 0);
	_delay_ms(2);
}

// === Main Function ===
int main(void) {
	LCD_D_DDR |= 0x0F;
	LCD_C_DDR |= 0x07;
	KEYPAD_DDR = 0x0F;
	KEYPAD_PORT = 0xFF;

	LCD_INIT();
	TCNT0 = 47;

	while (1) {
		RESET_GAME();
		WAIT_FOR_START();

		while (lives > 0 && level <= 9) {
			SEND_C_D(0x01, 0);
			_delay_ms(2);
			LCD_X_Y(0, 0);
			SEND_D_ST("Level: ");
			SEND_C_D(level + '0', 1);
			_delay_ms(300);

			GENERATE_SEQUENCE(sequence, level);
			DISPLAY_SEQUENCE(sequence, level);
			GET_USER_INPUT(user_input, level);

			if (CHECK_MATCH(sequence, user_input, level)) {
				score++;
				level++;
			} else {
				lives--;
				if (lives > 0) {
					LCD_X_Y(0, 0);
					SEND_D_ST("Try Again");
					_delay_ms(800);
				}
			}
		}

		if (lives == 0)
			GAME_OVER();
		else
			GAME_WIN();
	}
}
