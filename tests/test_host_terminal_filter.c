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
    struct host_terminal_filter filter = { HOST_TERMINAL_TEXT, 0 };
    int first;
    int second;
    int tail;

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

static void test_utf8_output(void)
{
    static const unsigned char input[] =
        "UTF-8 quotes: \xE2\x80\x9C" "left\xE2\x80\x9D" " right";

    require_filtered_at_every_split(input, sizeof(input) - 1, input, sizeof(input) - 1,
                                    "UTF-8 continuation bytes should be unchanged");
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
    test_utf8_output();
    test_utf8_osc_payload();
    test_incomplete_sequences();
    test_unknown_escape();
    return 0;
}
