#pragma once

#include <ext/machnet.h>

#undef PAGE_SIZE // both define PAGE_SIZE
#include <block_cache.h>
#include <operations.h>

#include <gflags/gflags.h>

#include <csignal>
