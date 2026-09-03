#include "uint64_decimal.h"

bool uint64_decimal_format(uint64_t value,
                           size_t minimum_width,
                           char *destination,
                           size_t capacity,
                           size_t *length)
{
    char reversed[UINT64_DECIMAL_MAX_DIGITS];
    size_t digit_count = 0U;
    size_t output_length;
    size_t index;

    if (length != NULL) {
        *length = 0U;
    }
    if ((destination == NULL) || (length == NULL) ||
        (minimum_width > UINT64_DECIMAL_MAX_DIGITS)) {
        return false;
    }

    do {
        reversed[digit_count] = (char)('0' + (value % UINT64_C(10)));
        digit_count++;
        value /= UINT64_C(10);
    } while (value > 0U);

    output_length = digit_count > minimum_width ? digit_count : minimum_width;
    if (capacity <= output_length) {
        return false;
    }

    for (index = 0U; index < output_length - digit_count; index++) {
        destination[index] = '0';
    }
    for (index = 0U; index < digit_count; index++) {
        destination[output_length - 1U - index] = reversed[index];
    }
    destination[output_length] = '\0';
    *length = output_length;
    return true;
}
