#ifndef COTTONTAIL_SRC_MURMUR_HASH_H_
#define COTTONTAIL_SRC_MURMUR_HASH_H_

#include <cstdint>

namespace cottontail {

uint64_t murmur_hash_64a(const void *key, int length, unsigned int seed);

} // namespace cottontail

#endif // COTTONTAIL_SRC_MURMUR_HASH_H_
