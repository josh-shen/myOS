#include <stdint.h>
#include <io.h>
#include <stdio.h>

#include <keyboard.h>
#include <interrupts.h>

static char get_key_val(char *val);

#define KEYBOARD_IRQ 1
#define KEYBOARD_DATA 0x60
#define KEYBOARD_RW 0x64
int keyboard_shift = 0;

static char get_key_val(char *val) {
    if (keyboard_shift && val[0] != '\0')
        return val[1];
    else 
        return val[0];
}

void keyboard_callback() {
    uint8_t scancode = inb(KEYBOARD_DATA);
    char key_val = '\0';
    int keydown = 0;
    switch(scancode) {
        #define KEY(code, val) case code: {key_val = get_key_val(val); keydown = 1;} break;
        #include <scancodes.h>
        #undef KEY
        #define KEY(code, val) case (code + 0x80): {key_val = get_key_val(val); keydown = 0;} break;
        #include <scancodes.h>
        #undef KEY
    }
    if ((scancode == 0x2A) | (scancode == 0x36) | (scancode == 0xAA) | (scancode == 0xB6))
        keyboard_shift = keydown;
    if (keydown)
        printf("%c", key_val);
    irq_eoi(KEYBOARD_IRQ);
}   

void keyboard_init() {
    irq_set_handler(KEYBOARD_IRQ, &keyboard_callback);
}