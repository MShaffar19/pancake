// Author: Ivan Sovic

#include <seeddb/SequenceSeedsCached.h>
#include <cassert>
#include <cstdio>
#include <exception>
#include <type_traits>

namespace PacBio {
namespace Pancake {

SequenceSeedsCached::SequenceSeedsCached(std::string name, const __int128* seeds, int64_t seedsSize,
                                         int64_t id)
    : name_{std::move(name)}, seeds_{seeds}, size_{seedsSize}, id_{id}
{
}

const std::string& SequenceSeedsCached::Name() const { return name_; }

const __int128* SequenceSeedsCached::Seeds() const { return seeds_; }

int64_t SequenceSeedsCached::Size() const { return size_; }

int64_t SequenceSeedsCached::Id() const { return id_; }

SequenceSeedsCached& SequenceSeedsCached::Name(std::string name)
{
    name_ = std::move(name);
    return *this;
}

SequenceSeedsCached& SequenceSeedsCached::Seeds(const __int128* seeds)
{
    seeds_ = seeds;
    return *this;
}

SequenceSeedsCached& SequenceSeedsCached::Size(int64_t size)
{
    size_ = size;
    return *this;
}

SequenceSeedsCached& SequenceSeedsCached::Id(int64_t id)
{
    id_ = id;
    return *this;
}

}  // namespace BAM
}  // namespace PacBio