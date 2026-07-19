#pragma once

#include <stdbool.h>

bool app_favorite_get(const char* slug);
void app_favorite_set(const char* slug, bool favorite);
