#pragma once

#include "pax_types.h"

#ifndef DISABLE_SYNTHWAVE
void synthwave(pax_buf_t* fb);
void synthwave_step(pax_buf_t* fb);
#else
static inline void synthwave(pax_buf_t* fb) { (void)fb; }
static inline void synthwave_step(pax_buf_t* fb) { (void)fb; }
#endif
