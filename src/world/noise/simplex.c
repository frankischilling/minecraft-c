#include "mc/world/noise/simplex.h"

#include <limits.h>

/* Raw initialized cc/bi fields f/g observed on the pinned original JARs with
 * Temurin 17.0.17+10: 3fd76cf5d0b09954 and 3fcb0cb174df99c8 respectively. */
const double mc_noise_simplex_skew = 0x1.76cf5d0b09954p-2;
const double mc_noise_simplex_unskew = 0x1.b0cb174df99c8p-3;

/* Preserve the repeated projected rows of the original three-component table;
 * this sampler's dot product uses only its first two components. */
static const int8_t simplex_gradients[12][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1},
                                                {1, 0}, {-1, 0}, {1, 0},  {-1, 0},
                                                {0, 1}, {0, -1}, {0, 1},  {0, -1}};

static void simplex_construct(mc_noise_simplex *noise, mc_java_random *random) {
    noise->offset_x = mc_java_random_next_double(random) * 256.0;
    noise->offset_z = mc_java_random_next_double(random) * 256.0;
    noise->offset_unused = mc_java_random_next_double(random) * 256.0;
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

bool mc_noise_simplex_init(mc_noise_simplex *noise, mc_java_random *random) {
    if (noise == NULL || random == NULL) {
        return false;
    }
    simplex_construct(noise, random);
    return true;
}

static int32_t simplex_floor(double value) {
    int32_t integer = mc_java_double_to_i32(value);
    return value > 0.0 ? integer : mc_java_i32_sub(integer, 1);
}

static double simplex_corner(size_t gradient, double x, double z) {
    double attenuation = (0.5 - x * x) - z * z;
    if (attenuation < 0.0) {
        return 0.0;
    }
    attenuation *= attenuation;
    double weight = attenuation * attenuation;
    /* Multiplication by zero coefficients is intentional; selecting a signed
     * component instead changes signed-zero and nonfinite arithmetic. */
    double dot =
        (double)simplex_gradients[gradient][0] * x + (double)simplex_gradients[gradient][1] * z;
    return weight * dot;
}

static double simplex_sample(const mc_noise_simplex *noise, double x, double z) {
    double skew = (x + z) * mc_noise_simplex_skew;
    int32_t cell_x = simplex_floor(x + skew);
    int32_t cell_z = simplex_floor(z + skew);
    double unskew = (double)mc_java_i32_add(cell_x, cell_z) * mc_noise_simplex_unskew;
    double origin_x = (double)cell_x - unskew;
    double origin_z = (double)cell_z - unskew;
    double first_x = x - origin_x;
    double first_z = z - origin_z;
    size_t step_x;
    size_t step_z;
    if (first_x > first_z) {
        step_x = 1;
        step_z = 0;
    } else {
        step_x = 0;
        step_z = 1;
    }
    double middle_x = first_x - (double)step_x + mc_noise_simplex_unskew;
    double middle_z = first_z - (double)step_z + mc_noise_simplex_unskew;
    double last_x = first_x - 1.0 + 2.0 * mc_noise_simplex_unskew;
    double last_z = first_z - 1.0 + 2.0 * mc_noise_simplex_unskew;
    size_t masked_x = (size_t)((uint32_t)cell_x & UINT32_C(255));
    size_t masked_z = (size_t)((uint32_t)cell_z & UINT32_C(255));
    const uint16_t *permutation = noise->permutation;
    size_t first_gradient = permutation[masked_x + permutation[masked_z]] % 12u;
    size_t middle_gradient = permutation[masked_x + step_x + permutation[masked_z + step_z]] % 12u;
    size_t last_gradient = permutation[masked_x + 1 + permutation[masked_z + 1]] % 12u;
    double first = simplex_corner(first_gradient, first_x, first_z);
    double middle = simplex_corner(middle_gradient, middle_x, middle_z);
    double last = simplex_corner(last_gradient, last_x, last_z);
    return 70.0 * ((first + middle) + last);
}

static bool simplex_grid_valid(const mc_noise_simplex_grid *grid, const double *output,
                               size_t length, size_t *elements) {
    if (grid == NULL || (output == NULL && length != 0) || length > SIZE_MAX / sizeof(double) ||
        grid->size_x > INT32_MAX || grid->size_z > INT32_MAX) {
        return false;
    }
    size_t count = 0;
    if (grid->size_x != 0 && grid->size_z != 0) {
        if (grid->size_x > (size_t)INT32_MAX / grid->size_z) {
            return false;
        }
        count = grid->size_x * grid->size_z;
    }
    if (count > length) {
        return false;
    }
    *elements = count;
    return true;
}

static void simplex_add(const mc_noise_simplex *noise, const mc_noise_simplex_grid *grid,
                        double amplitude, double *output) {
    size_t index = 0;
    for (size_t x = 0; x < grid->size_x; ++x) {
        double sampled_x = (grid->origin_x + (double)x) * grid->scale_x + noise->offset_x;
        for (size_t z = 0; z < grid->size_z; ++z) {
            double sampled_z = (grid->origin_z + (double)z) * grid->scale_z + noise->offset_z;
            double sample = simplex_sample(noise, sampled_x, sampled_z);
            output[index] = output[index] + sample * amplitude;
            ++index;
        }
    }
}

bool mc_noise_simplex_add(const mc_noise_simplex *noise, const mc_noise_simplex_grid *grid,
                          double amplitude, double *output, size_t output_length) {
    size_t elements;
    if (noise == NULL || !simplex_grid_valid(grid, output, output_length, &elements)) {
        return false;
    }
    if (elements != 0) {
        simplex_add(noise, grid, amplitude, output);
    }
    return true;
}

bool mc_noise_simplex_octaves_init(mc_noise_simplex_octaves *noise, mc_noise_simplex *layers,
                                   size_t count, mc_java_random *random) {
    if (noise == NULL || count > INT32_MAX || count > SIZE_MAX / sizeof(*layers) ||
        (count != 0 && (layers == NULL || random == NULL))) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        simplex_construct(&layers[i], random);
    }
    noise->layers = layers;
    noise->count = count;
    return true;
}

bool mc_noise_simplex_octaves_fill(const mc_noise_simplex_octaves *noise,
                                   const mc_noise_simplex_grid *grid, double frequency_multiplier,
                                   double amplitude_divisor_multiplier, double *output,
                                   size_t output_length) {
    size_t elements;
    if (noise == NULL || noise->count > INT32_MAX ||
        noise->count > SIZE_MAX / sizeof(*noise->layers) ||
        (noise->count != 0 && noise->layers == NULL) ||
        !simplex_grid_valid(grid, output, output_length, &elements)) {
        return false;
    }
    double scale_x = grid->scale_x / 1.5;
    double scale_z = grid->scale_z / 1.5;
    for (size_t i = 0; i < output_length; ++i) {
        output[i] = 0.0;
    }
    if (elements == 0) {
        return true;
    }
    double frequency = 1.0;
    double divisor = 1.0;
    for (size_t i = 0; i < noise->count; ++i) {
        mc_noise_simplex_grid scaled = *grid;
        scaled.scale_x = scale_x * frequency;
        scaled.scale_z = scale_z * frequency;
        simplex_add(&noise->layers[i], &scaled, 0.55 / divisor, output);
        frequency *= frequency_multiplier;
        divisor *= amplitude_divisor_multiplier;
    }
    return true;
}
