#pragma once

#include <stdio.h>

// Fast file I/O - automatically uses DMA-capable buffers for /sd and /int paths
// When CONFIG_FATFS_USE_FASTOPEN is enabled, files opened with paths starting
// with "/sd" or "/int" will use internal DMA-capable RAM for stdio buffers,
// improving throughput by avoiding PSRAM cache synchronization overhead.
FILE* fastopen(const char* path, const char* mode);
void  fastclose(FILE* f);
