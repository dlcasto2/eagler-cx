#ifndef SAVEIO_H
#define SAVEIO_H

#include <cstdint>
#include <zlib.h>

#include "gl.h"
#include "items.h"

// Typed, endian-independent writes into a gz save file. After the first
// failure ok() stays false and every later call returns false.
class SaveWriter
{
public:
    explicit SaveWriter(gzFile f) : f(f) {}

    bool u8(uint8_t v);
    bool u16(uint16_t v);
    bool u32(uint32_t v);
    bool i32(int32_t v);
    bool fix(GLFix v);
    bool stack(const ItemStack &s);   // u16 id, u8 count, u16 meta
    bool ok() const { return good; }

private:
    bool bytes(const uint8_t *data, unsigned n);
    gzFile f;
    bool good = true;
};

class SaveReader
{
public:
    explicit SaveReader(gzFile f) : f(f) {}

    bool u8(uint8_t &v);
    bool u16(uint16_t &v);
    bool u32(uint32_t &v);
    bool i32(int32_t &v);
    bool fix(GLFix &v);
    bool stack(ItemStack &s);
    bool ok() const { return good; }

private:
    bool bytes(uint8_t *data, unsigned n);
    gzFile f;
    bool good = true;
};

#endif // SAVEIO_H
