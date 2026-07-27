#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef bool (*storage_backend_visitor_t)(const char *filename, void *ctx);

bool storage_backend_mount(void);
bool storage_backend_ready(void);
const char *storage_backend_status(void);
int storage_backend_list(const char *relative_dir,
                         storage_backend_visitor_t visitor,
                         void *ctx);
FILE *storage_backend_open(const char *relative_path, const char *mode);
