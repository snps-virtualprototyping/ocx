/******************************************************************************
 * Copyright (C) 2019 Synopsys, Inc.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 ******************************************************************************/

#ifndef COMMON_H
#define COMMON_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define INFO(...)                                                             \
    do {                                                                      \
        fprintf(stderr, "%s:%d ", __FILE__, __LINE__);                        \
        fprintf(stderr, __VA_ARGS__);                                         \
        fprintf(stderr, "\n");                                                \
    } while (0)


#define ERROR(...)                                                            \
    do {                                                                      \
        INFO(__VA_ARGS__);                                                    \
        abort();                                                              \
    } while (0)

#define ERROR_ON(cond, ...)                                                   \
    do {                                                                      \
        if (cond) {                                                           \
            ERROR(__VA_ARGS__);                                               \
        }                                                                     \
    } while (0)

#endif
