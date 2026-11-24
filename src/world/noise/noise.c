#include "mc/world/noise/noise.h"

#include <limits.h>

typedef struct noise_axis {
    size_t cell;
    double fraction;
    double fade;
} noise_axis;

static double noise_fade(double value) {
    return ((value * value) * value) * (value * (value * 6.0 - 15.0) + 10.0);
}

static double noise_lerp(double factor, double lower, double upper) {
    return lower + factor * (upper - lower);
}

static noise_axis noise_coordinate(double value) {
    int32_t integer = mc_java_double_to_i32(value);
    if (value < (double)integer) {
        integer = mc_java_i32_sub(integer, 1);
    }
    noise_axis axis;
    axis.cell = (size_t)((uint32_t)integer & UINT32_C(255));
    axis.fraction = value - (double)integer;
    axis.fade = noise_fade(axis.fraction);
    return axis;
}

static double noise_gradient(uint16_t hash, double x, double y, double z) {
    unsigned int direction = (unsigned int)hash & 15u;
    double first = direction < 8 ? x : y;
    double second = direction < 4 ? y : direction == 12 || direction == 14 ? x : z;
    return ((direction & 1u) == 0 ? first : -first) + ((direction & 2u) == 0 ? second : -second);
}

static double noise_gradient2(uint16_t hash, double x, double z) {
    unsigned int direction = (unsigned int)hash & 15u;
    /* This multiplication is present in the original specialized corner.
     * Selecting a component instead changes signed-zero/nonfinite arithmetic. */
    double first = (double)(1u - ((direction & 8u) >> 3u)) * x;
    double second = direction < 4 ? 0.0 : direction == 12 || direction == 14 ? x : z;
    return ((direction & 1u) == 0 ? first : -first) + ((direction & 2u) == 0 ? second : -second);
}

static void noise_construct(mc_noise_perlin *noise, mc_java_random *random) {
    noise->offset_x = mc_java_random_next_double(random) * 256.0;
    noise->offset_y = mc_java_random_next_double(random) * 256.0;
    noise->offset_z = mc_java_random_next_double(random) * 256.0;
    for (size_t i = 0; i < 256; ++i) {
        noise->permutation[i] = (uint16_t)i;
    }
    for (size_t i = 0; i < 256; ++i) {
        int32_t choice = 0;
        (void)mc_java_random_next_bounded(random, (int32_t)(256u - i), &choice);
        size_t selected = i + (size_t)choice;
        uint16_t previous = noise->permutation[i];
        noise->permutation[i] = noise->permutation[selected];
        noise->permutation[selected] = previous;
        noise->permutation[i + 256] = noise->permutation[i];
    }
}

bool mc_noise_perlin_init(mc_noise_perlin *noise, mc_java_random *random) {
    if (noise == NULL || random == NULL) {
        return false;
    }
    noise_construct(noise, random);
    return true;
}

double mc_noise_perlin_sample(const mc_noise_perlin *noise, double x, double y, double z) {
    double shifted_x = x + noise->offset_x;
    double shifted_y = y + noise->offset_y;
    double shifted_z = z + noise->offset_z;
    noise_axis ax = noise_coordinate(shifted_x);
    noise_axis ay = noise_coordinate(shifted_y);
    noise_axis az = noise_coordinate(shifted_z);
    const uint16_t *permutation = noise->permutation;
    size_t left = (size_t)permutation[ax.cell] + ay.cell;
    size_t left_lower = (size_t)permutation[left] + az.cell;
    size_t left_upper = (size_t)permutation[left + 1] + az.cell;
    size_t right = (size_t)permutation[ax.cell + 1] + ay.cell;
    size_t right_lower = (size_t)permutation[right] + az.cell;
    size_t right_upper = (size_t)permutation[right + 1] + az.cell;
    double fx = ax.fraction;
    double fy = ay.fraction;
    double fz = az.fraction;
    double lower_left = noise_lerp(ax.fade, noise_gradient(permutation[left_lower], fx, fy, fz),
                                   noise_gradient(permutation[right_lower], fx - 1.0, fy, fz));
    double lower_right =
        noise_lerp(ax.fade, noise_gradient(permutation[left_upper], fx, fy - 1.0, fz),
                   noise_gradient(permutation[right_upper], fx - 1.0, fy - 1.0, fz));
    double lower = noise_lerp(ay.fade, lower_left, lower_right);
    double upper_left =
        noise_lerp(ax.fade, noise_gradient(permutation[left_lower + 1], fx, fy, fz - 1.0),
                   noise_gradient(permutation[right_lower + 1], fx - 1.0, fy, fz - 1.0));
    double upper_right =
        noise_lerp(ax.fade, noise_gradient(permutation[left_upper + 1], fx, fy - 1.0, fz - 1.0),
                   noise_gradient(permutation[right_upper + 1], fx - 1.0, fy - 1.0, fz - 1.0));
    double upper = noise_lerp(ay.fade, upper_left, upper_right);
    return noise_lerp(az.fade, lower, upper);
}

static bool noise_grid_valid(const mc_noise_grid *grid, const double *output, size_t length,
                             size_t *elements) {
    if (grid == NULL || (output == NULL && length != 0) || length > SIZE_MAX / sizeof(double) ||
        grid->size_x > INT32_MAX || grid->size_y > INT32_MAX || grid->size_z > INT32_MAX) {
        return false;
    }
    size_t count = 0;
    if (grid->size_x != 0 && grid->size_y != 0 && grid->size_z != 0) {
        if (grid->size_x > (size_t)INT32_MAX / grid->size_y) {
            return false;
        }
        count = grid->size_x * grid->size_y;
        if (count > (size_t)INT32_MAX / grid->size_z) {
            return false;
        }
        count *= grid->size_z;
    }
    if (count > length) {
        return false;
    }
    *elements = count;
    return true;
}

static void noise_add_height_one(const mc_noise_perlin *noise, const mc_noise_grid *grid,
                                 double reciprocal, double *output) {
    const uint16_t *permutation = noise->permutation;
    size_t index = 0;
    for (size_t x = 0; x < grid->size_x; ++x) {
        noise_axis ax =
            noise_coordinate((grid->origin_x + (double)x) * grid->scale_x + noise->offset_x);
        for (size_t z = 0; z < grid->size_z; ++z) {
            noise_axis az =
                noise_coordinate((grid->origin_z + (double)z) * grid->scale_z + noise->offset_z);
            size_t left = permutation[ax.cell];
            size_t left_corner = (size_t)permutation[left] + az.cell;
            size_t right = permutation[ax.cell + 1];
            size_t right_corner = (size_t)permutation[right] + az.cell;
            double fx = ax.fraction;
            double fz = az.fraction;
            double lower = noise_lerp(ax.fade, noise_gradient2(permutation[left_corner], fx, fz),
                                      noise_gradient(permutation[right_corner], fx - 1.0, 0.0, fz));
            double upper =
                noise_lerp(ax.fade, noise_gradient(permutation[left_corner + 1], fx, 0.0, fz - 1.0),
                           noise_gradient(permutation[right_corner + 1], fx - 1.0, 0.0, fz - 1.0));
            double sample = noise_lerp(az.fade, lower, upper);
            output[index] = output[index] + sample * reciprocal;
            ++index;
        }
    }
}

static void noise_add_general(const mc_noise_perlin *noise, const mc_noise_grid *grid,
                              double reciprocal, double *output) {
    const uint16_t *permutation = noise->permutation;
    size_t index = 0;
    for (size_t x = 0; x < grid->size_x; ++x) {
        noise_axis ax =
            noise_coordinate((grid->origin_x + (double)x) * grid->scale_x + noise->offset_x);
        for (size_t z = 0; z < grid->size_z; ++z) {
            noise_axis az =
                noise_coordinate((grid->origin_z + (double)z) * grid->scale_z + noise->offset_z);
            size_t previous_y = 0;
            double lower_left = 0.0;
            double lower_right = 0.0;
            double upper_left = 0.0;
            double upper_right = 0.0;
            for (size_t y = 0; y < grid->size_y; ++y) {
                noise_axis ay = noise_coordinate((grid->origin_y + (double)y) * grid->scale_y +
                                                 noise->offset_y);
                if (y == 0 || ay.cell != previous_y) {
                    previous_y = ay.cell;
                    size_t left = (size_t)permutation[ax.cell] + ay.cell;
                    size_t left_lower = (size_t)permutation[left] + az.cell;
                    size_t left_upper = (size_t)permutation[left + 1] + az.cell;
                    size_t right = (size_t)permutation[ax.cell + 1] + ay.cell;
                    size_t right_lower = (size_t)permutation[right] + az.cell;
                    size_t right_upper = (size_t)permutation[right + 1] + az.cell;
                    double fx = ax.fraction;
                    double fy = ay.fraction;
                    double fz = az.fraction;
                    lower_left =
                        noise_lerp(ax.fade, noise_gradient(permutation[left_lower], fx, fy, fz),
                                   noise_gradient(permutation[right_lower], fx - 1.0, fy, fz));
                    lower_right = noise_lerp(
                        ax.fade, noise_gradient(permutation[left_upper], fx, fy - 1.0, fz),
                        noise_gradient(permutation[right_upper], fx - 1.0, fy - 1.0, fz));
                    upper_left = noise_lerp(
                        ax.fade, noise_gradient(permutation[left_lower + 1], fx, fy, fz - 1.0),
                        noise_gradient(permutation[right_lower + 1], fx - 1.0, fy, fz - 1.0));
                    upper_right = noise_lerp(
                        ax.fade,
                        noise_gradient(permutation[left_upper + 1], fx, fy - 1.0, fz - 1.0),
                        noise_gradient(permutation[right_upper + 1], fx - 1.0, fy - 1.0, fz - 1.0));
                }
                /* The cached gradients retain the earlier Y fraction while
                 * its masked cell is unchanged. Only this fade is current. */
                double lower = noise_lerp(ay.fade, lower_left, lower_right);
                double upper = noise_lerp(ay.fade, upper_left, upper_right);
                double sample = noise_lerp(az.fade, lower, upper);
                output[index] = output[index] + sample * reciprocal;
                ++index;
            }
        }
    }
}

static void noise_add(const mc_noise_perlin *noise, const mc_noise_grid *grid, double divisor,
                      double *output) {
    double reciprocal = 1.0 / divisor;
    if (grid->size_y == 1) {
        noise_add_height_one(noise, grid, reciprocal, output);
    } else {
        noise_add_general(noise, grid, reciprocal, output);
    }
}

bool mc_noise_perlin_add(const mc_noise_perlin *noise, const mc_noise_grid *grid, double divisor,
                         double *output, size_t output_length) {
    size_t elements;
    if (noise == NULL || !noise_grid_valid(grid, output, output_length, &elements)) {
        return false;
    }
    if (elements != 0) {
        noise_add(noise, grid, divisor, output);
    }
    return true;
}

bool mc_noise_octaves_init(mc_noise_octaves *noise, mc_noise_perlin *layers, size_t count,
                           mc_java_random *random) {
    if (noise == NULL || count > INT32_MAX || count > SIZE_MAX / sizeof(*layers) ||
        (count != 0 && (layers == NULL || random == NULL))) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        noise_construct(&layers[i], random);
    }
    noise->layers = layers;
    noise->count = count;
    return true;
}

double mc_noise_octaves_sample2(const mc_noise_octaves *noise, double x, double y) {
    double sum = 0.0;
    double frequency = 1.0;
    for (size_t i = 0; i < noise->count; ++i) {
        sum += mc_noise_perlin_sample(&noise->layers[i], x * frequency, y * frequency, 0.0) /
               frequency;
        frequency /= 2.0;
    }
    return sum;
}

bool mc_noise_octaves_fill(const mc_noise_octaves *noise, const mc_noise_grid *grid, double *output,
                           size_t output_length) {
    size_t elements;
    if (noise == NULL || noise->count > INT32_MAX ||
        noise->count > SIZE_MAX / sizeof(*noise->layers) ||
        (noise->count != 0 && noise->layers == NULL) ||
        !noise_grid_valid(grid, output, output_length, &elements)) {
        return false;
    }
    for (size_t i = 0; i < output_length; ++i) {
        output[i] = 0.0;
    }
    if (elements == 0) {
        return true;
    }
    double frequency = 1.0;
    for (size_t i = 0; i < noise->count; ++i) {
        mc_noise_grid scaled = *grid;
        scaled.scale_x = grid->scale_x * frequency;
        scaled.scale_y = grid->scale_y * frequency;
        scaled.scale_z = grid->scale_z * frequency;
        noise_add(&noise->layers[i], &scaled, frequency, output);
        frequency /= 2.0;
    }
    return true;
}
