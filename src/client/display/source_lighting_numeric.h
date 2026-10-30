#ifndef MC_CLIENT_DISPLAY_SOURCE_LIGHTING_NUMERIC_H
#define MC_CLIENT_DISPLAY_SOURCE_LIGHTING_NUMERIC_H

#include "source_internal.h"
#include <emmintrin.h>
#include <float.h>
#include <math.h>
#include <string.h>

/* Included after Display's fail helper. Caller has already admitted strict
 * native FP and decoded the genuine source vertices. This is an admission
 * bound, not a second lighting implementation: no result, source field, GL
 * object, primitive or buffer is changed here. Driver arithmetic precision and
 * cancellation remain subjects of the device qualification. */
static inline bool source_lighting_numeric_finite(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000);
}

static inline bool source_lighting_numeric_span(const float *values, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (!source_lighting_numeric_finite(values[i]))
            return false;
    return true;
}

static inline double source_lighting_numeric_limit(void) {
    /* Leave room for rounded intermediate additions/multiplications rather
     * than admitting a double bound exactly at binary32's overflow edge. */
    return (double)FLT_MAX / (1.0 + 16.0 * FLT_EPSILON);
}

static inline double source_lighting_numeric_unit_bound(void) {
    /* A finite, normally scaled normalization has unit length up to rounding.
     * This allowance bounds admissions only; it does not alter shader values. */
    return 1.0 + 16.0 * FLT_EPSILON;
}

typedef struct source_lighting_numeric_interval {
    double center, radius;
} source_lighting_numeric_interval;

static inline double source_lighting_numeric_upper(source_lighting_numeric_interval value) {
    return fabs(value.center) + value.radius;
}

static inline source_lighting_numeric_interval source_lighting_numeric_round(double center,
                                                                             double radius) {
    /* Preserve provably exact ordinary binary32 operations, including zero
     * and identity matrix paths. Otherwise enclose rounding and permitted
     * subnormal flushing. Centers/radii are admissions, never shader inputs. */
    if (radius == 0 && fabs(center) <= (double)FLT_MAX &&
        (center == 0 || fabs(center) >= (double)FLT_MIN)) {
        const float narrowed = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(center)));
        if ((double)narrowed == center)
            return (source_lighting_numeric_interval){center, 0};
    }
    const double magnitude = fabs(center) + radius;
    return (source_lighting_numeric_interval){center,
                                              radius + magnitude * FLT_EPSILON + (double)FLT_MIN};
}

static inline source_lighting_numeric_interval
source_lighting_numeric_add(source_lighting_numeric_interval left,
                            source_lighting_numeric_interval right) {
    if (left.center == 0 && left.radius == 0)
        return right;
    if (right.center == 0 && right.radius == 0)
        return left;
    const double center = left.center + right.center;
    const double virtual_right = center - left.center;
    const double residual =
        (left.center - (center - virtual_right)) + (right.center - virtual_right);
    const double radii = left.radius + right.radius;
    return source_lighting_numeric_round(center, radii + fabs(residual) + radii * DBL_EPSILON);
}

static inline source_lighting_numeric_interval
source_lighting_numeric_negate(source_lighting_numeric_interval value) {
    value.center = -value.center;
    return value;
}

static inline source_lighting_numeric_interval
source_lighting_numeric_multiply(source_lighting_numeric_interval left,
                                 source_lighting_numeric_interval right) {
    if ((left.center == 0 && left.radius == 0) || (right.center == 0 && right.radius == 0))
        return (source_lighting_numeric_interval){0};
    if (right.radius == 0 && right.center == 1)
        return left;
    if (left.radius == 0 && left.center == 1)
        return right;
    if (right.radius == 0 && right.center == -1)
        return source_lighting_numeric_negate(left);
    if (left.radius == 0 && left.center == -1)
        return source_lighting_numeric_negate(right);
    const double radius = fabs(left.center) * right.radius + fabs(right.center) * left.radius +
                          left.radius * right.radius;
    const double center = left.center * right.center;
    return source_lighting_numeric_round(center, radius + fabs(center) * DBL_EPSILON);
}

static inline source_lighting_numeric_interval source_lighting_numeric_divide(float numerator,
                                                                              float denominator) {
    if (denominator == 1)
        return (source_lighting_numeric_interval){numerator, 0};
    if (denominator == -1)
        return (source_lighting_numeric_interval){-(double)numerator, 0};
    const double center = (double)numerator / denominator;
    return source_lighting_numeric_round(center, fabs(center) * DBL_EPSILON);
}

static inline source_lighting_numeric_interval
source_lighting_numeric_dot3(const source_lighting_numeric_interval left[3],
                             const source_lighting_numeric_interval right[3],
                             source_lighting_numeric_interval seed) {
    source_lighting_numeric_interval value = seed;
    double magnitude = source_lighting_numeric_upper(seed);
    size_t terms = magnitude != 0 ? 1u : 0u;
    for (size_t i = 0; i < 3; ++i) {
        const source_lighting_numeric_interval term =
            source_lighting_numeric_multiply(left[i], right[i]);
        const double upper = source_lighting_numeric_upper(term);
        terms += upper != 0 ? 1u : 0u;
        magnitude += upper;
        value = source_lighting_numeric_add(value, term);
    }
    /* Matrix/dot implementations may associate or contract three/four terms
     * differently. Two nonzero terms already have a single covered sum. */
    if (terms > 2)
        value.radius += magnitude * (16.0 * FLT_EPSILON);
    return value;
}

static inline bool
source_lighting_numeric_normalization(const source_lighting_numeric_interval input[3],
                                      source_lighting_numeric_interval output[3]) {
    double lower_square = 0, upper_square = 0;
    bool exact_zero = true;
    bool exact_unit_axis = true;
    size_t nonzero_components = 0;
    for (size_t component = 0; component < 3; ++component) {
        const double upper = source_lighting_numeric_upper(input[component]);
        const double lower = fmax(0, fabs(input[component].center) - input[component].radius);
        lower_square += lower * lower;
        upper_square += upper * upper;
        exact_zero = exact_zero && upper == 0;
        if (upper != 0) {
            ++nonzero_components;
            exact_unit_axis = exact_unit_axis && input[component].radius == 0 &&
                              fabs(input[component].center) == 1;
        }
    }
    if (exact_zero) {
        if (output != NULL)
            memset(output, 0, 3 * sizeof(*output));
        return true;
    }
    if (exact_unit_axis && nonzero_components == 1) {
        if (output != NULL)
            memcpy(output, input, 3 * sizeof(*output));
        return true;
    }
    /* The lower bound must prove a normally scaled squared length. An upper
     * magnitude bound cannot establish that cancellation left a safe vector.
     * Include dot-product rounding and possible flushing of its three terms. */
    const double allowance = 16.0 * FLT_EPSILON;
    lower_square = lower_square * (1.0 - allowance) - 3.0 * FLT_MIN;
    upper_square = upper_square * (1.0 + allowance) + 3.0 * FLT_MIN;
    if (lower_square < (double)FLT_MIN || upper_square > source_lighting_numeric_limit())
        return false;
    if (output == NULL)
        return true;
    const double minimum_length = sqrt(lower_square), maximum_length = sqrt(upper_square);
    for (size_t component = 0; component < 3; ++component) {
        if (input[component].center == 0 && input[component].radius == 0) {
            output[component] = (source_lighting_numeric_interval){0};
            continue;
        }
        const double low = input[component].center - input[component].radius;
        const double high = input[component].center + input[component].radius;
        const double normalized_low = low / (low < 0 ? minimum_length : maximum_length);
        const double normalized_high = high / (high > 0 ? minimum_length : maximum_length);
        const double center = (normalized_low + normalized_high) * 0.5;
        const double radius = (normalized_high - normalized_low) * 0.5;
        output[component] = (source_lighting_numeric_interval){
            center, radius + (fabs(center) + radius) * allowance + (double)FLT_MIN};
    }
    return true;
}

static inline const float *source_lighting_numeric_material(const mc_client_source_fog_state *fog,
                                                            size_t side, int32_t property,
                                                            const float color[4]) {
    if (source_material_property_tracked(fog, side, property))
        return color;
    const mc_client_source_material_state *material = &fog->materials[side];
    if (property == 4608)
        return material->ambient;
    if (property == 4609)
        return material->diffuse;
    if (property == 4610)
        return material->specular;
    return material->emission;
}

static inline bool source_lighting_numeric_admit(const mc_client_source_state *state,
                                                 const mc_client_source_gpu_vertex *vertices,
                                                 size_t count, const float normal_matrix[16],
                                                 float normal_scale,
                                                 mc_client_display_error *error) {
    if (!state->lighting_enabled || count == 0)
        return true;
    const char *operation = "source lighting numerical admission";
    const char *detail = "nonfinite used lighting input";
    const mc_client_source_fog_state *fog = &state->fog;
    const double limit = source_lighting_numeric_limit();
    const double unit = source_lighting_numeric_unit_bound();
    const bool enabled_light = fog->lights[0] || fog->lights[1];
    if (!source_lighting_numeric_span(state->modelview.column_major, 16) ||
        !source_lighting_numeric_span(fog->light_model_ambient, 3))
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation, detail, 0);
    for (size_t column = 0; column < 3; ++column)
        if (!source_lighting_numeric_span(normal_matrix + 4 * column, 3))
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation, detail, 0);
    if (fog->rescale_normal_enabled && !source_lighting_numeric_finite(normal_scale))
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation, detail, 0);

    source_lighting_numeric_interval light_cartesian[2][3] = {{{0}}};
    source_lighting_numeric_interval directional_unit[2][3] = {{{0}}};
    bool positional[2] = {false, false};
    for (size_t light = 0; light < 2; ++light) {
        if (!fog->lights[light])
            continue;
        const mc_client_source_light_state *parameters = &fog->light_parameters[light];
        if (!source_lighting_numeric_span(parameters->position_eye, 4) ||
            !source_lighting_numeric_span(parameters->ambient, 3) ||
            !source_lighting_numeric_span(parameters->diffuse, 3) ||
            !source_lighting_numeric_span(parameters->specular, 3))
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation, detail, 0);
        positional[light] = parameters->position_eye[3] != 0;
        for (size_t component = 0; component < 3; ++component) {
            light_cartesian[light][component] =
                positional[light]
                    ? source_lighting_numeric_divide(parameters->position_eye[component],
                                                     parameters->position_eye[3])
                    : (source_lighting_numeric_interval){parameters->position_eye[component], 0};
            if (source_lighting_numeric_upper(light_cartesian[light][component]) > limit)
                return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                            "positional light division can overflow binary32", 0);
        }
        if (!positional[light] &&
            !source_lighting_numeric_normalization(light_cartesian[light], directional_unit[light]))
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                        "directional light normalization exceeds the normal binary32 profile", 0);
    }

    for (size_t vertex = 0; vertex < count; ++vertex) {
        const mc_client_source_gpu_vertex *input = &vertices[vertex];
        if (!source_lighting_numeric_span(input->position, 3) ||
            !source_lighting_numeric_span(input->normal, 3))
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation, detail, 0);
        double normal[3] = {0};
        source_lighting_numeric_interval eye_interval[3] = {{0}};
        source_lighting_numeric_interval normal_interval[3] = {{0}};
        for (size_t row = 0; row < 3; ++row) {
            double eye_magnitude = fabs((double)state->modelview.column_major[12 + row]);
            double normal_magnitude = 0;
            source_lighting_numeric_interval position[3], current_normal[3], model[3], inverse[3];
            for (size_t column = 0; column < 3; ++column) {
                position[column] = (source_lighting_numeric_interval){input->position[column], 0};
                current_normal[column] =
                    (source_lighting_numeric_interval){input->normal[column], 0};
                model[column] = (source_lighting_numeric_interval){
                    state->modelview.column_major[column * 4 + row], 0};
                inverse[column] =
                    (source_lighting_numeric_interval){normal_matrix[column * 4 + row], 0};
                eye_magnitude += fabs(model[column].center) * fabs(position[column].center);
                normal_magnitude +=
                    fabs(inverse[column].center) * fabs(current_normal[column].center);
            }
            eye_interval[row] = source_lighting_numeric_dot3(
                model, position,
                (source_lighting_numeric_interval){state->modelview.column_major[12 + row], 0});
            normal_interval[row] = source_lighting_numeric_dot3(
                inverse, current_normal, (source_lighting_numeric_interval){0});
            /* Retain absolute product/sum bounds for overflow: cancellation
             * must not hide an overflowing alternative dot association. */
            if (eye_magnitude > limit || normal_magnitude > limit ||
                source_lighting_numeric_upper(eye_interval[row]) > limit ||
                source_lighting_numeric_upper(normal_interval[row]) > limit)
                return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                            "eye or normal transform can overflow binary32", 0);
            if (fog->rescale_normal_enabled)
                normal_interval[row] = source_lighting_numeric_multiply(
                    normal_interval[row], (source_lighting_numeric_interval){normal_scale, 0});
            if (fog->rescale_normal_enabled)
                normal_magnitude *= fabs((double)normal_scale);
            normal[row] =
                fmax(normal_magnitude, source_lighting_numeric_upper(normal_interval[row]));
            if (normal[row] > limit)
                return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                            "normal rescaling can overflow binary32", 0);
        }
        if (fog->normalize_enabled) {
            source_lighting_numeric_interval normalized[3];
            if (!source_lighting_numeric_normalization(normal_interval, normalized))
                return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                            "normal normalization exceeds the normal binary32 profile", 0);
            memcpy(normal_interval, normalized, sizeof(normalized));
        }
        const double normal_length =
            fog->normalize_enabled
                ? unit
                : sqrt((normal[0] * normal[0] + normal[1] * normal[1]) + normal[2] * normal[2]);
        const double dot_bound = enabled_light ? normal_length * unit : 0;
        if (dot_bound > limit)
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                        "normal-light dot product can overflow binary32", 0);

        source_lighting_numeric_interval light_unit[2][3];
        memcpy(light_unit, directional_unit, sizeof(light_unit));
        for (size_t light = 0; light < 2; ++light) {
            if (!fog->lights[light])
                continue;
            if (positional[light]) {
                source_lighting_numeric_interval delta[3];
                for (size_t component = 0; component < 3; ++component)
                    delta[component] = source_lighting_numeric_add(
                        light_cartesian[light][component],
                        source_lighting_numeric_negate(eye_interval[component]));
                if (!source_lighting_numeric_normalization(delta, light_unit[light]))
                    return fail(
                        error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                        "positional light normalization exceeds the normal binary32 profile", 0);
            }
        }

        source_lighting_numeric_interval viewer[3] = {{0, 0}, {0, 0}, {1, 0}};
        bool viewer_ready = !fog->light_model_local_viewer;
        for (size_t side = 0; side < (fog->light_model_two_side ? 2u : 1u); ++side) {
            const mc_client_source_material_state *material = &fog->materials[side];
            const float *ambient = source_lighting_numeric_material(fog, side, 4608, input->color);
            const float *diffuse = source_lighting_numeric_material(fog, side, 4609, input->color);
            const float *specular = source_lighting_numeric_material(fog, side, 4610, input->color);
            const float *emission = source_lighting_numeric_material(fog, side, 5632, input->color);
            if (!source_lighting_numeric_span(ambient, 3) ||
                !source_lighting_numeric_finite(diffuse[3]) ||
                (enabled_light && !source_lighting_numeric_span(diffuse, 3)) ||
                (dot_bound > 0 && (!source_lighting_numeric_span(specular, 3) ||
                                   !source_lighting_numeric_finite(material->shininess))) ||
                !source_lighting_numeric_span(emission, 3))
                return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation, detail, 0);
            double half_bound = dot_bound;
            for (size_t light = 0; light < 2; ++light) {
                if (!fog->lights[light] || dot_bound == 0 || material->shininess == 0)
                    continue;
                bool specular_used = false;
                for (size_t component = 0; component < 3; ++component)
                    specular_used =
                        specular_used || (specular[component] != 0 &&
                                          fog->light_parameters[light].specular[component] != 0);
                if (!specular_used)
                    continue;
                source_lighting_numeric_interval side_normal[3];
                for (size_t component = 0; component < 3; ++component)
                    side_normal[component] =
                        side == 0 ? normal_interval[component]
                                  : source_lighting_numeric_negate(normal_interval[component]);
                const source_lighting_numeric_interval diffuse_dot = source_lighting_numeric_dot3(
                    side_normal, light_unit[light], (source_lighting_numeric_interval){0});
                if (diffuse_dot.center + diffuse_dot.radius <= 0)
                    continue; /* The actual source specular branch cannot be entered. */
                if (!viewer_ready) {
                    source_lighting_numeric_interval toward_viewer[3];
                    for (size_t component = 0; component < 3; ++component)
                        toward_viewer[component] =
                            source_lighting_numeric_negate(eye_interval[component]);
                    if (!source_lighting_numeric_normalization(toward_viewer, viewer))
                        return fail(
                            error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                            "local viewer normalization exceeds the normal binary32 profile", 0);
                    viewer_ready = true;
                }
                source_lighting_numeric_interval half[3], normalized_half[3];
                for (size_t component = 0; component < 3; ++component)
                    half[component] = source_lighting_numeric_add(light_unit[light][component],
                                                                  viewer[component]);
                if (!source_lighting_numeric_normalization(half, normalized_half))
                    return fail(
                        error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                        "specular half-vector normalization exceeds the normal binary32 profile",
                        0);
                const source_lighting_numeric_interval specular_dot = source_lighting_numeric_dot3(
                    side_normal, normalized_half, (source_lighting_numeric_interval){0});
                half_bound = fmax(half_bound, specular_dot.center + specular_dot.radius);
            }
            double power = 1;
            if (enabled_light && half_bound > 1 && material->shininess != 0) {
                const double exponent = (double)material->shininess * log(half_bound);
                if (exponent > log(limit))
                    return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                "specular power can overflow binary32", 0);
                power = exp(exponent);
            }
            for (size_t component = 0; component < 3; ++component) {
                const double a = fabs((double)ambient[component]);
                const double d = enabled_light ? fabs((double)diffuse[component]) : 0;
                const double s = dot_bound > 0 ? fabs((double)specular[component]) : 0;
                double base = fabs((double)emission[component]) +
                              fabs((double)fog->light_model_ambient[component]) * a;
                double sum_specular = 0;
                if (base > limit)
                    return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                "emission/global ambient can overflow binary32", 0);
                for (size_t light = 0; light < 2; ++light) {
                    if (!fog->lights[light])
                        continue;
                    const mc_client_source_light_state *parameters = &fog->light_parameters[light];
                    const double ambient_term = fabs((double)parameters->ambient[component]) * a;
                    const double diffuse_product = fabs((double)parameters->diffuse[component]) * d;
                    const double specular_product =
                        fabs((double)parameters->specular[component]) * s;
                    if (diffuse_product > limit || specular_product > limit)
                        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                    "light/material product can overflow binary32", 0);
                    base += ambient_term + diffuse_product * dot_bound;
                    sum_specular += specular_product * power;
                    if (base > limit || sum_specular > limit)
                        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                    "light accumulation can overflow binary32", 0);
                }
                if (fog->light_model_color_control != 33274 && base + sum_specular > limit)
                    return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                "single-color lighting sum can overflow binary32", 0);
            }
        }
    }
    return true;
}

#endif
