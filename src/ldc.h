#pragma once

#include <ext/machnet.h>

#undef PAGE_SIZE // both define PAGE_SIZE
#include <block_cache.h>

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

#include "packet.capnp.h"

#include <csignal>
#include <gflags/gflags.h>

#include "unordered_dense.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>