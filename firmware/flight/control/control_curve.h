#ifndef OPENFLIGHTCOMPUTER_CONTROL_CURVE_H
#define OPENFLIGHTCOMPUTER_CONTROL_CURVE_H

#include <stdbool.h>
#include <stdint.h>

#define CONTROL_CURVE_MAXIMUM_POINTS 8U
#define CONTROL_CURVE_MAXIMUM_SEGMENTS (CONTROL_CURVE_MAXIMUM_POINTS - 1U)

typedef enum {
    CONTROL_CURVE_TYPE_CONTROL_POINTS = 0,
    CONTROL_CURVE_TYPE_COUNT,
} control_curve_type_t;

typedef enum {
    CONTROL_CURVE_INTERPOLATION_LINEAR = 0,
    CONTROL_CURVE_INTERPOLATION_COUNT,
} control_curve_interpolation_t;

typedef struct {
    float input;
    float output;
} control_curve_point_t;

typedef struct {
    control_curve_type_t type;
    control_curve_interpolation_t interpolation;
    uint8_t point_count;
    control_curve_point_t points[CONTROL_CURVE_MAXIMUM_POINTS];
} control_curve_config_t;

typedef struct {
    float upper_input;
    /* Horner coefficients: ((c3*x + c2)*x + c1)*x + c0. */
    float coefficient[4];
} control_curve_segment_t;

typedef struct {
    control_curve_type_t type;
    control_curve_interpolation_t interpolation;
    uint8_t segment_count;
    control_curve_segment_t segments[CONTROL_CURVE_MAXIMUM_SEGMENTS];
    bool initialized;
} prepared_control_curve_t;

bool control_curve_config_is_valid(const control_curve_config_t *config);
bool control_curve_prepare(const control_curve_config_t *config,
                           prepared_control_curve_t *prepared);
bool control_curve_apply(const prepared_control_curve_t *prepared,
                         float input,
                         float *output);
const char *control_curve_type_name(control_curve_type_t type);
const char *control_curve_interpolation_name(
    control_curve_interpolation_t interpolation);

#endif
