#include "keyboard.h"
#include "pic.h"

namespace {
    constexpr uint16_t PS2_DATA_PORT = 0x60;
    constexpr uint16_t PS2_STATUS_PORT = 0x64;
    constexpr uint16_t PS2_COMMAND_PORT = 0x64;

    constexpr uint8_t PS2_STATUS_OUTPUT_FULL = 1 << 0;
    constexpr uint8_t PS2_STATUS_INPUT_FULL = 1 << 1;
    constexpr uint8_t PS2_STATUS_SECOND_PORT = 1 << 5;

    constexpr uint8_t KEYBOARD_ENABLE_SCANNING = 0xF4;
    constexpr uint8_t PS2_ENABLE_FIRST_PORT = 0xAE;
    constexpr uint8_t KEYBOARD_BUFFER_SIZE = 64;

    volatile uint8_t buffer_head = 0;
    volatile uint8_t buffer_tail = 0;
    char keyboard_buffer[KEYBOARD_BUFFER_SIZE];

    bool left_shift_pressed = false;
    bool right_shift_pressed = false;
    bool caps_lock_active = false;
    bool extended_scancode = false;

    inline void outb(uint16_t port, uint8_t value) {
        asm volatile("outb %0, %1" :: "a"(value), "Nd"(port));
    }

    inline uint8_t inb(uint16_t port) {
        uint8_t value;
        asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
        return value;
    }

    void wait_for_input_buffer_empty() {
        for (uint32_t timeout = 0; timeout < 100000; timeout++) {
            if ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0) {
                return;
            }
        }
    }

    void push_char(char character) {
        uint8_t next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
        if (next_head == buffer_tail) {
            return; // Full buffer: preserve the oldest character.
        }

        keyboard_buffer[buffer_head] = character;
        asm volatile("" ::: "memory");
        buffer_head = next_head;
    }

    char translate_scancode(uint8_t scancode) {
        static const char normal_map[128] = {
            0,   0,   '1', '2', '3', '4', '5', '6', '7', '8',
            '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r',
            't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
            'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
            '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n',
            'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,
        };
        static const char shifted_map[128] = {
            0,   0,   '!', '@', '#', '$', '%', '^', '&', '*',
            '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R',
            'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
            'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
            '"', '~', 0,  '|',  'Z', 'X', 'C', 'V', 'B', 'N',
            'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,
        };

        bool shift_active = left_shift_pressed || right_shift_pressed;
        char character = shift_active ? shifted_map[scancode] : normal_map[scancode];

        if (character >= 'a' && character <= 'z' && caps_lock_active) {
            character = character - 'a' + 'A';
        } else if (character >= 'A' && character <= 'Z' && caps_lock_active) {
            character = character - 'A' + 'a';
        }

        return character;
    }
}

extern "C" void keyboard_init() {
    // Discard data left behind by the bootloader.
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
        (void)inb(PS2_DATA_PORT);
    }

    wait_for_input_buffer_empty();
    outb(PS2_COMMAND_PORT, PS2_ENABLE_FIRST_PORT);
    wait_for_input_buffer_empty();
    outb(PS2_DATA_PORT, KEYBOARD_ENABLE_SCANNING);

    pic_clear_mask(1);
}

extern "C" void keyboard_handle_irq() {
    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & PS2_STATUS_OUTPUT_FULL) == 0) {
        return;
    }

    uint8_t scancode = inb(PS2_DATA_PORT);
    if (status & PS2_STATUS_SECOND_PORT) {
        return;
    }

    if (scancode == 0xE0) {
        extended_scancode = true;
        return;
    }

    bool released = (scancode & 0x80) != 0;
    scancode &= 0x7F;

    if (scancode == 0x2A) {
        left_shift_pressed = !released;
    } else if (scancode == 0x36) {
        right_shift_pressed = !released;
    } else if (scancode == 0x3A && !released) {
        caps_lock_active = !caps_lock_active;
    } else if (!released && !extended_scancode) {
        char character = translate_scancode(scancode);
        if (character != 0) {
            push_char(character);
        }
    }

    extended_scancode = false;
}

extern "C" bool keyboard_char_available() {
    return buffer_head != buffer_tail;
}

extern "C" char keyboard_read_char() {
    if (!keyboard_char_available()) {
        return 0;
    }

    char character = keyboard_buffer[buffer_tail];
    asm volatile("" ::: "memory");
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return character;
}
