#include "saveio.h"

bool SaveWriter::bytes(const uint8_t *data, unsigned n)
{
    if(good && gzwrite(f, data, n) != static_cast<int>(n))
        good = false;
    return good;
}

bool SaveWriter::u8(uint8_t v) { return bytes(&v, 1); }

bool SaveWriter::u16(uint16_t v)
{
    const uint8_t b[2] = { static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8) };
    return bytes(b, 2);
}

bool SaveWriter::u32(uint32_t v)
{
    const uint8_t b[4] = { static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24) };
    return bytes(b, 4);
}

bool SaveWriter::i32(int32_t v) { return u32(static_cast<uint32_t>(v)); }

bool SaveWriter::fix(GLFix v) { return i32(v.value); }

bool SaveWriter::stack(const ItemStack &s) { return u16(s.id) && u8(s.count) && u16(s.meta); }

bool SaveReader::bytes(uint8_t *data, unsigned n)
{
    if(good && gzread(f, data, n) != static_cast<int>(n))
        good = false;
    return good;
}

bool SaveReader::u8(uint8_t &v) { return bytes(&v, 1); }

bool SaveReader::u16(uint16_t &v)
{
    uint8_t b[2];
    if(!bytes(b, 2))
        return false;
    v = static_cast<uint16_t>(b[0] | (b[1] << 8));
    return true;
}

bool SaveReader::u32(uint32_t &v)
{
    uint8_t b[4];
    if(!bytes(b, 4))
        return false;
    v = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
    return true;
}

bool SaveReader::i32(int32_t &v)
{
    uint32_t u;
    if(!u32(u))
        return false;
    v = static_cast<int32_t>(u);
    return true;
}

bool SaveReader::fix(GLFix &v)
{
    int32_t raw;
    if(!i32(raw))
        return false;
    v.value = raw;
    return true;
}

bool SaveReader::stack(ItemStack &s)
{
    return u16(s.id) && u8(s.count) && u16(s.meta);
}
