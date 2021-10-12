/**
 * Prints a 24-bit colour depending on the current branch name.
 *
 * Uses `git rev-parse HEAD` to obtain the current commit SHA.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

// "Control Sequence Introducer"
// https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_(Control_Sequence_Introducer)_sequences
#define CSI "\033["
// "Select graphics rendition"
#define SGR "m"
#define SET_WHITE_FOREGROUND "37"

enum {
    SHA1_HEX_LENGTH = 40,
    EXPECTED_OUTPUT_LENGTH = SHA1_HEX_LENGTH + sizeof("\n"),
};

enum perceived_brightness_t {
    LIGHT, DARK
};


static uint8_t parse_hex_octet(const char *);
static enum perceived_brightness_t light_or_dark(int r, int g, int b);


char buffer[EXPECTED_OUTPUT_LENGTH];

int main(void) {
    int status_code = EXIT_FAILURE;
    FILE* command = NULL;
    uint8_t r, g, b;

    command = popen("git rev-parse HEAD 2>/dev/null", "r");
    if (command == NULL)
        goto cleanup;

    char *output = fgets(buffer, EXPECTED_OUTPUT_LENGTH, command);
    if (output == NULL)
        goto cleanup;

    if (strnlen(buffer, EXPECTED_OUTPUT_LENGTH) < SHA1_HEX_LENGTH)
        goto cleanup;

    r = parse_hex_octet(buffer);
    g = parse_hex_octet(buffer + 2);
    b = parse_hex_octet(buffer + 4);

    if (errno)
        goto cleanup;

    // See: https://en.wikipedia.org/wiki/ANSI_escape_code#24-bit
    enum {
        SET_24BIT_FOREGROUND = 38,
        SET_24BIT_BACKGROUND = 48,
    };

    int mode = (light_or_dark(r, g, b) == LIGHT)
        ? SET_24BIT_FOREGROUND
        : SET_24BIT_BACKGROUND;

    // See: https://en.wikipedia.org/wiki/ANSI_escape_code#24-bit
    printf(CSI "%d;2;%d;%d;%d" SGR, mode, r, g, b);

    // set the foreground to white first!
    if (mode == SET_24BIT_BACKGROUND) {
        printf(CSI SET_WHITE_FOREGROUND SGR);
    }

    status_code = EXIT_SUCCESS;

cleanup:
    if (command)
        pclose(command);

    return status_code;
}

static uint8_t parse_hex_ascii_byte(char c) {
    if (c >= '0' && c <= '9')
        return c & 0x0F;

    if (c >= 'a' && c <= 'f')
        return 9 + (c & 0x0F);

    errno = EINVAL;
    return 0xFF;
}

/**
 * Parses a hexadecimal octet from an ASCII string.
 */
static uint8_t parse_hex_octet(const char *start) {
    uint8_t upper = parse_hex_ascii_byte(start[0]);
    uint8_t lower = parse_hex_ascii_byte(start[1]);
    return (upper << 4) | lower;
}

static enum perceived_brightness_t light_or_dark(int r, int g, int b) {
    // Using Luminance formula option 2: https://stackoverflow.com/a/596243
    // which in turn is dervied from: https://www.w3.org/TR/AERT/#color-contrast
    int luminance = (299 * r + 587 * g + 114 * b) / 1000;
    assert(luminance >= 0 && luminance <= 255);

    if (luminance > (0xFF / 2)) {
        return LIGHT;
    } else {
        return DARK;
    }
}
