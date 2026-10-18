/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
struct cube_lut { uint32_t size; float domain_min[3], domain_max[3]; float *rgba; };
/* On failure, out is empty; error points to a static, user-facing message. */
bool cube_load(const char *path, struct cube_lut *out, const char **error);
void cube_free(struct cube_lut *lut);
