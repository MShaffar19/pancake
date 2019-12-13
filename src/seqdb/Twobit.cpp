// Authors: Ivan Sovic

#include <pacbio/seqdb/Lookups.h>
#include <pacbio/seqdb/Twobit.h>
#include <cmath>
#include <sstream>

#include <iostream>

namespace PacBio {
namespace Pancake {

int32_t CompressSequence(const std::string& bases, std::vector<uint8_t>& twobit,
                      std::vector<PacBio::Pancake::Range>& ranges)
{
    twobit.clear();
    ranges.clear();
    int32_t numBases = static_cast<int32_t>(bases.size());
    int32_t finalSize = std::ceil(static_cast<float>(bases.size()) / 4.0f);
    twobit.reserve(finalSize);
    uint8_t buff = 0x0;
    int32_t count = 0;
    int32_t addedBases = 0;
    PacBio::Pancake::Range range{0, 0};
    // Loops through all the input bases, converts them to a 2-bit representation
    // if they are ACTG. When a non-ACTG base is encountered, a new "range"
    // is opened. All bases are densely packed (the end of the previous range
    // and the beginning of the next range can be in the same byte).
    // A range is a contiguous non-ACTG sequence.
    for (int32_t i = 0; i < numBases; ++i) {
        int32_t base = static_cast<int32_t>(bases[i]);
        uint8_t twobitBase = BaseToTwobit[base];
        // A non-ACTG base was found. Store the range and start a new one.
        if (twobitBase > 3) {
            // Set the end location of the range in both coordinates,
            // and check if we actually accumulated any bases before adding
            // a range to the ranges vector.
            range.end = i;
            if (range.Span() > 0) {  // Non-N base span should be > 0.
                ranges.emplace_back(range);
            }
            // Refresh the start positions for the next range.
            range.start = i + 1;  // Skip the 'N' base.
            continue;
        }
        // Add to the buffer.
        buff = (buff << 2) | twobitBase;
        ++count;
        ++addedBases;
        // Push a new base if the buffer is filled.
        if (count == 4) {
            twobit.emplace_back(buff);
            buff = 0x0;
            count = 0;
        }
    }
    // If the last byte wasn't filled all the way, padd it with zeros.
    if (count > 0 && count < 4) {
        buff = buff << (4 - count) * 2;
        twobit.emplace_back(buff);
    }
    // Handle the final range.
    range.end = numBases;
    if (range.Span() > 0) {  // Non-N base span should be > 0.
        ranges.emplace_back(range);
    }
    return addedBases;
}

void DecompressSequence(const std::vector<uint8_t>& twobit, int32_t numBases,
                        const std::vector<PacBio::Pancake::Range>& ranges, std::string& bases)
{
    bases.clear();

    // Empty input == empty output.
    if (twobit.empty() && numBases == 0) {
        return;
    }

    // Sanity checks.
    if (ranges.empty()) {
        std::ostringstream errOss;
        errOss << "There are no ranges specified associated with the compressed sequence.";
        throw std::runtime_error{errOss.str()};
    }
    if (numBases < 0 || numBases < ranges.back().end) {
        std::ostringstream errOss;
        errOss << "Invalid numBases. numBases = " << numBases
               << ", last range end position = " << ranges.back().end << ".";
        throw std::runtime_error{errOss.str()};
    }

    std::ostringstream oss;

    // Leading unknown bases, that go before the first range.
    if (ranges.size() && ranges.front().start > 0) {
        oss << std::string(ranges.front().start, 'N');
    }

    // Unpacks the entire sequence by iterating through all ranges.
    // A range is a contiguous ACTG sequence, and space in between ranges are
    // non-ACTG symbols which will be replaced by N-bases.
    // The "start2" is the start position of a range in the cleaned-up sequence
    // (the version of the sequence where all non-ACTG bases are removed, and
    // all other bases simply concatenated into a contiguous array).
    int32_t start2 = 0;
    for (size_t i = 0; i < ranges.size(); start2 += ranges[i].Span(), ++i) {
        const auto& r = ranges[i];

        if (r.Span() <= 0) {
            std::ostringstream errOss;
            errOss << "Invalid span value: " << r.Span() << ".";
            throw std::runtime_error{errOss.str()};
        }

        // Find the first and last base of the range in the compressed sequence.
        // Bases are compressed in the 2-bit format, so identifying the byte
        // is done by division, and identifying the bit position of the base
        // is just the remainder.
        int32_t end2 = start2 + r.Span();
        int32_t rangeStartByte = start2 / 4.0f;
        int32_t rangeStartOffset = start2 % 4;
        int32_t rangeEndByte = end2 / 4.0f;
        int32_t rangeEndOffset = end2 % 4;

        if (rangeStartByte == rangeEndByte) {
            // Special case when the range is small and starts and ends in the same byte.
            std::string bases(ByteToBases[twobit[rangeStartByte]]);
            oss << bases.substr(rangeStartOffset, rangeEndOffset - rangeStartOffset);

        } else {
            // Take the bases from the range which are stored in the first byte.
            // The range can begin at any arbitrary pair position because we
            // packed it densely.
            std::string frontBases(ByteToBases[twobit[rangeStartByte]]);
            oss << frontBases.substr(rangeStartOffset);
            // Internal bytes - simply tile the sequence.
            for (int32_t j = rangeStartByte + 1; j < rangeEndByte; ++j) {
                oss << ByteToBases[twobit[j]];
            }
            // Last byte - again, the range can end within the byte. We need
            // to pick only the valid bases.
            std::string backBases(ByteToBases[twobit[rangeEndByte]]);
            oss << backBases.substr(0, rangeEndOffset);
        }

        // Add the unknown bases in between ranges.
        if ((i + 1) < ranges.size()) {
            int32_t betweenSpan = ranges[i + 1].start - ranges[i].end;
            oss << std::string(betweenSpan, 'N');
        }
    }

    // Trailing unknown bases, that go behind the last range.
    if (ranges.size() && ranges.back().end < numBases) {
        oss << std::string((numBases - ranges.back().end), 'N');
    }

    bases = oss.str();
}

}  // namespace Pancake
}  // namespace PacBio