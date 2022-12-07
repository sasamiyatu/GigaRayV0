#pragma once

#include "tiny_obj_loader.h"

struct Mesh;

Mesh load_obj_from_file(const char* filepath);