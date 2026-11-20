/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_terminal_filter.h"

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static int process_chunks(const unsigned char *input, int input_len, int split,
                          unsigned char *output, int output_size)
{
    struct host_terminal_filter filter;
    int first;
    int second;
    int tail;

    host_terminal_filter_init(&filter);
    first = host_terminal_filter_process(&filter, input, split, output, output_size);
    require(first >= 0, "first terminal-filter chunk should fit");

    second = host_terminal_filter_process(&filter, input + split, input_len - split,
                                          output + first, output_size - first);
    require(second >= 0, "second terminal-filter chunk should fit");

    tail = host_terminal_filter_finish(&filter, output + first + second,
                                       output_size - first - second);
    require(tail >= 0, "terminal-filter tail should fit");
    return first + second + tail;
}

static void require_filtered_at_every_split(const unsigned char *input, int input_len,
                                            const unsigned char *expected, int expected_len,
                                            const char *message)
{
    unsigned char output[512];

    for (int split = 0; split <= input_len; split++) {
        int output_len;

        memset(output, 0, sizeof(output));
        output_len = process_chunks(input, input_len, split, output, sizeof(output));
        if (output_len != expected_len || memcmp(output, expected, expected_len) != 0) {
            fprintf(stderr, "%s at split %d\n", message, split);
            exit(1);
        }
    }
}

static void test_plain_text(void)
{
    static const unsigned char input[] = "plain text\r\n";

    require_filtered_at_every_split(input, sizeof(input) - 1, input, sizeof(input) - 1,
                                    "plain text should be unchanged");
}

static void test_csi_conversion(void)
{
    static const unsigned char input[] = "\x1B[31mred\x1B[0m";
    static const unsigned char expected[] = "\233" "31mred" "\233" "0m";

    require_filtered_at_every_split(input, sizeof(input) - 1,
                                    expected, sizeof(expected) - 1,
                                    "ANSI CSI should become Amiga CSI");
}

static void test_osc_st_filtering(void)
{
    static const unsigned char input[] =
        "before\x1B]3008;start=01234567;machineid=abcdef;type=shell;cwd=/home/user\x1B\\after";
    static const unsigned char expected[] = "beforeafter";

    require_filtered_at_every_split(input, sizeof(input) - 1,
                                    expected, sizeof(expected) - 1,
                                    "OSC terminated by ST should be discarded");
}

static void test_osc_bel_filtering(void)
{
    static const unsigned char input[] = "left\x1B]0;window title\x07right";
    static const unsigned char expected[] = "leftright";

    require_filtered_at_every_split(input, sizeof(input) - 1,
                                    expected, sizeof(expected) - 1,
                                    "OSC terminated by BEL should be discarded");
}

static void test_osc_c1_st_filtering(void)
{
    static const unsigned char input[] = "left\x1B]0;window title\x9C" "right";
    static const unsigned char expected[] = "leftright";

    require_filtered_at_every_split(input, sizeof(input) - 1,
                                    expected, sizeof(expected) - 1,
                                    "OSC terminated by C1 ST should be discarded");
}

static void test_c1_osc_filtering(void)
{
    static const unsigned char st_input[] =
        "left\x9D" "0;window title\x9C" "right";
    static const unsigned char bel_input[] =
        "left\x9D" "0;window title\x07right";
    static const unsigned char expected[] = "leftright";

    require_filtered_at_every_split(st_input, sizeof(st_input) - 1,
                                    expected, sizeof(expected) - 1,
                                    "C1 OSC terminated by C1 ST should be discarded");
    require_filtered_at_every_split(bel_input, sizeof(bel_input) - 1,
                                    expected, sizeof(expected) - 1,
                                    "C1 OSC terminated by BEL should be discarded");
}

static void test_utf8_output(void)
{
    /* e acute, a with diaeresis, pound, no-break space: all in ISO-8859-1 */
    static const unsigned char input[] =
        "caf\xC3\xA9 gr\xC3\xBC\xC3\x9F \xC2\xA3" "5\xC2\xA0" "x";
    static const unsigned char expected[] = "caf\xE9 gr\xFC\xDF \xA3" "5\xA0" "x";

    require_filtered_at_every_split(input, sizeof(input) - 1, expected, sizeof(expected) - 1,
                                    "UTF-8 should become ISO-8859-1");
}

static void test_utf8_without_latin1_form(void)
{
    /* curly quotes, en dash, ellipsis, bullet, Greek alpha, a 4-byte emoji */
    static const unsigned char input[] =
        "\xE2\x80\x9C" "q\xE2\x80\x9D \xE2\x80\x98" "s\xE2\x80\x99 a\xE2\x80\x93" "b\xE2\x80\xA6"
        " \xE2\x80\xA2 \xCE\xB1 \xF0\x9F\x98\x80";
    static const unsigned char expected[] = "\"q\" 's' a-b... * ? ?";

    require_filtered_at_every_split(input, sizeof(input) - 1, expected, sizeof(expected) - 1,
                                    "characters without an ISO-8859-1 form should become ASCII or ?");
}

static void test_malformed_utf8(void)
{
    /* lead byte cut short, then text; overlong NUL; surrogate; C1 control dropped */
    static const unsigned char cut_short[] = "a\xC3" "b";
    static const unsigned char cut_short_expected[] = "a?b";
    static const unsigned char overlong[] = "a\xE0\x80\x80" "b";
    static const unsigned char overlong_expected[] = "a?b";
    static const unsigned char surrogate[] = "a\xED\xA0\x80" "b";
    static const unsigned char surrogate_expected[] = "a?b";
    static const unsigned char c1[] = "a\xC2\x9B" "b";
    static const unsigned char c1_expected[] = "ab";
    static const unsigned char stray[] = "a\xA3" "b";
    static const unsigned char dangling[] = "a\xE2\x80";
    static const unsigned char dangling_expected[] = "a?";

    require_filtered_at_every_split(cut_short, sizeof(cut_short) - 1, cut_short_expected,
                                    sizeof(cut_short_expected) - 1,
                                    "a character cut short should become ?");
    require_filtered_at_every_split(overlong, sizeof(overlong) - 1, overlong_expected,
                                    sizeof(overlong_expected) - 1,
                                    "an overlong sequence should become ?");
    require_filtered_at_every_split(surrogate, sizeof(surrogate) - 1, surrogate_expected,
                                    sizeof(surrogate_expected) - 1,
                                    "an encoded surrogate should become ?");
    require_filtered_at_every_split(c1, sizeof(c1) - 1, c1_expected, sizeof(c1_expected) - 1,
                                    "an encoded C1 control must not reach the Amiga console");
    require_filtered_at_every_split(stray, sizeof(stray) - 1, stray, sizeof(stray) - 1,
                                    "a stray continuation byte should pass through");
    require_filtered_at_every_split(dangling, sizeof(dangling) - 1, dangling_expected,
                                    sizeof(dangling_expected) - 1,
                                    "a character cut off by the end of output should become ?");
}

static void test_utf8_after_escape(void)
{
    static const unsigned char input[] = "\x1B" "\xC3\xA9" "x";
    static const unsigned char expected[] = "\x1B\xE9" "x";

    require_filtered_at_every_split(input, sizeof(input) - 1, expected, sizeof(expected) - 1,
                                    "a UTF-8 character after a literal ESC should be converted");
}

static void test_csi_with_utf8_text(void)
{
    static const unsigned char input[] = "\x1B[1m\xC3\xA9\x1B[0m\xC3\xA9";
    static const unsigned char expected[] = "\233" "1m\xE9\233" "0m\xE9";

    require_filtered_at_every_split(input, sizeof(input) - 1, expected, sizeof(expected) - 1,
                                    "CSI conversion and UTF-8 decoding should work together");
}

static void test_utf8_osc_payload(void)
{
    static const unsigned char input[] =
        "before\x1B]0;UTF-8 \xE2\x80\x9C" "title\xE2\x80\x9D\x1B\\after";
    static const unsigned char expected[] = "beforeafter";

    require_filtered_at_every_split(input, sizeof(input) - 1,
                                    expected, sizeof(expected) - 1,
                                    "UTF-8 OSC payload should remain filtered");
}

static void test_utf8_osc_continuation_bytes(void)
{
    /* U+025C is C9 9C: the 0x9C continuation byte must not end the OSC */
    static const unsigned char input[] = "a\x1B]0;x\xC9\x9Cy\x07" "b";
    static const unsigned char expected[] = "ab";

    require_filtered_at_every_split(input, sizeof(input) - 1, expected, sizeof(expected) - 1,
                                    "a continuation byte equal to C1 ST should stay in the OSC");
}

static void test_encode_input(void)
{
    static const unsigned char input[] = "a\xE9\xFC\x9B" "A\x08z\xA0\xFF";
    static const unsigned char expected[] =
        "a\xC3\xA9\xC3\xBC\x1B[A\x7Fz\xC2\xA0\xC3\xBF";
    unsigned char output[64];
    int len;

    len = host_terminal_encode_input(input, sizeof(input) - 1, output, sizeof(output));
    require(len == (int)sizeof(expected) - 1 && memcmp(output, expected, len) == 0,
            "typed ISO-8859-1 should become UTF-8, CSI ESC [ and Backspace DEL");
    require(host_terminal_encode_input(input, sizeof(input) - 1, output, 5) == -1,
            "input that does not fit the output buffer should fail");

    /* the worst case doubles the length, as host-shell's buffers assume */
    {
        unsigned char high[1024];
        unsigned char big[2048];

        memset(high, 0xE9, sizeof(high));
        require(host_terminal_encode_input(high, sizeof(high), big, sizeof(big)) == 2048,
                "1024 high bytes should encode to 2048 bytes");
    }
}

static void test_incomplete_sequences(void)
{
    static const unsigned char dangling_escape[] = "text\x1B";
    static const unsigned char dangling_escape_expected[] = "text\x1B";
    static const unsigned char incomplete_osc[] = "text\x1B]3008;start=unfinished";
    static const unsigned char incomplete_osc_expected[] = "text";

    require_filtered_at_every_split(dangling_escape, sizeof(dangling_escape) - 1,
                                    dangling_escape_expected,
                                    sizeof(dangling_escape_expected) - 1,
                                    "a dangling non-OSC escape should be preserved");
    require_filtered_at_every_split(incomplete_osc, sizeof(incomplete_osc) - 1,
                                    incomplete_osc_expected,
                                    sizeof(incomplete_osc_expected) - 1,
                                    "an incomplete OSC should be discarded");
}

static void test_unknown_escape(void)
{
    static const unsigned char input[] = "a\x1BPb";

    require_filtered_at_every_split(input, sizeof(input) - 1, input, sizeof(input) - 1,
                                    "an unknown escape should be preserved");
}

int main(void)
{
    test_plain_text();
    test_csi_conversion();
    test_osc_st_filtering();
    test_osc_bel_filtering();
    test_osc_c1_st_filtering();
    test_c1_osc_filtering();
    test_utf8_output();
    test_utf8_without_latin1_form();
    test_malformed_utf8();
    test_utf8_after_escape();
    test_csi_with_utf8_text();
    test_utf8_osc_payload();
    test_utf8_osc_continuation_bytes();
    test_encode_input();
    test_incomplete_sequences();
    test_unknown_escape();
    return 0;
}
