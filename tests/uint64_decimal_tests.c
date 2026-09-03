#include "uint64_decimal.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void expect_format(uint64_t value,
                          size_t minimum_width,
                          const char *expected)
{
    char output[UINT64_DECIMAL_BUFFER_CAPACITY];
    size_t length = 0U;

    assert(uint64_decimal_format(value,
                                 minimum_width,
                                 output,
                                 sizeof(output),
                                 &length));
    assert(length == strlen(expected));
    assert(strcmp(output, expected) == 0);
}

static void formats_boundaries(void)
{
    expect_format(0U, 0U, "0");
    expect_format(9U, 0U, "9");
    expect_format(10U, 0U, "10");
    expect_format(999999999U, 0U, "999999999");
    expect_format(UINT64_C(1000000000), 0U, "1000000000");
    expect_format(UINT64_MAX, 0U, "18446744073709551615");
}

static void adds_bounded_zero_padding(void)
{
    expect_format(0U, 10U, "0000000000");
    expect_format(42U, 10U, "0000000042");
    expect_format(UINT64_MAX, 10U, "18446744073709551615");
    expect_format(1U, UINT64_DECIMAL_MAX_DIGITS,
                  "00000000000000000001");
}

static void rejects_invalid_destinations_and_capacity(void)
{
    char output[UINT64_DECIMAL_BUFFER_CAPACITY] = "unchanged";
    size_t length = 7U;

    assert(!uint64_decimal_format(1U, 0U, NULL, sizeof(output), &length));
    assert(!uint64_decimal_format(1U, 0U, output, sizeof(output), NULL));
    assert(!uint64_decimal_format(1U,
                                  UINT64_DECIMAL_MAX_DIGITS + 1U,
                                  output,
                                  sizeof(output),
                                  &length));
    assert(!uint64_decimal_format(UINT64_MAX,
                                  0U,
                                  output,
                                  UINT64_DECIMAL_MAX_DIGITS,
                                  &length));
    assert(length == 0U);
}

int main(void)
{
    formats_boundaries();
    adds_bounded_zero_padding();
    rejects_invalid_destinations_and_capacity();
    return 0;
}
