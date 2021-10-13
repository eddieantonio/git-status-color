/**
 * git-status-color - prints a 24-bit colour depending on the current branch name.
 *
 * Uses `git rev-parse HEAD` to obtain the current commit SHA.
 *
 * In ZSH, you must turn on prompt substitution:
 *
 *      set -o PROMPT_SUBST
 *
 * Then you can set your PS1/PROMPT or RPROMPT as desired. For example,
 *
 *      RPROMPT="%{$(git-color-ref)%}$(git rev-parse --abbrev-ref HEAD)%k"
 *
 * If this sounds icky to you, that's because it is!
 *
 * Note about psvar and the %v prompt substitution:
 *
 * %v uses stradd() [1] to place whatever is in psvar[i] into the prompt.
 *
 * However, ZSH internally uses txtchangeset(), txtunset, and either
 * tsetcap()/set_colour_attribute() [2] to set the prompt colour.
 *
 * Meanwhile, stadd(), prints "nice" characters in the output buffer [3]
 * which encodes control characters [4]. So %v will not print your escape
 * sequence! 😭
 *
 * Sources:
 *
 * [1]: https://github.com/zsh-users/zsh/blob/00d20ed15e18f5af682f0daec140d6b8383c479a/Src/prompt.c#L736-L743
 * [2]: https://github.com/zsh-users/zsh/blob/00d20ed15e18f5af682f0daec140d6b8383c479a/Src/prompt.c#L511-L572
 * [3]: https://github.com/zsh-users/zsh/blob/00d20ed15e18f5af682f0daec140d6b8383c479a/Src/prompt.c#L932-L998
 * [4]: https://github.com/zsh-users/zsh/blob/00d20ed15e18f5af682f0daec140d6b8383c479a/Src/utils.c#L559-L564
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

/* CSI: Control Sequence Introducer */
/* https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_(Control_Sequence_Introducer)_sequences */
#define CSI "\033["
/* "Select graphics rendition" */
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
