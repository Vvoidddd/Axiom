#ifndef AXIOM_SECURITY_H
#define AXIOM_SECURITY_H

#include <stdint.h>

enum security_capability {
    CAP_MOUNT   = 1ull << 0,
    CAP_TIME    = 1ull << 1,
    CAP_USERS   = 1ull << 2,
    CAP_NETWORK = 1ull << 3,
    CAP_POWER   = 1ull << 4,
    CAP_AUDIT   = 1ull << 5,
    CAP_DEVICE  = 1ull << 6,
    CAP_CHOWN   = 1ull << 7,
    CAP_DAC     = 1ull << 8,
    CAP_SERVICE = 1ull << 9,
    CAP_PACKAGE = 1ull << 10,
    CAP_ALL     = (1ull << 11) - 1
};

#endif
