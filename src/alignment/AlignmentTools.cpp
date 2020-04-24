// Authors: Ivan Sovic

#include <pacbio/alignment/AlignmentTools.h>

#include <array>
#include <sstream>
#include <stdexcept>
#include <string>

#include <pbcopper/third-party/edlib.h>

namespace PacBio {
namespace Pancake {

PacBio::BAM::Cigar EdlibAlignmentToCigar(const unsigned char* aln, int32_t alnLen)
{
    if (alnLen <= 0) {
        return {};
    }

    // Edlib move codes: 0: '=', 1: 'I', 2: 'D', 3: 'X'
    std::array<PacBio::BAM::CigarOperationType, 4> opToCigar = {
        PacBio::BAM::CigarOperationType::SEQUENCE_MATCH, PacBio::BAM::CigarOperationType::INSERTION,
        PacBio::BAM::CigarOperationType::DELETION,
        PacBio::BAM::CigarOperationType::SEQUENCE_MISMATCH};

    PacBio::BAM::CigarOperationType prevOp = PacBio::BAM::CigarOperationType::UNKNOWN_OP;
    int32_t count = 0;
    PacBio::BAM::Cigar ret;
    for (int32_t i = 0; i <= alnLen; i++) {
        if (i == alnLen || (opToCigar[aln[i]] != prevOp &&
                            prevOp != PacBio::BAM::CigarOperationType::UNKNOWN_OP)) {
            ret.emplace_back(PacBio::BAM::CigarOperation(prevOp, count));
            count = 0;
        }
        if (i < alnLen) {
            prevOp = opToCigar[aln[i]];
            count += 1;
        }
    }
    return ret;
}

void EdlibAlignmentDiffCounts(const unsigned char* aln, int32_t alnLen, int32_t& numEq,
                              int32_t& numX, int32_t& numI, int32_t& numD)
{
    numEq = numX = numI = numD = 0;

    if (alnLen <= 0) {
        return;
    }

    for (int32_t i = 0; i < alnLen; i++) {
        switch (aln[i]) {
            case EDLIB_EDOP_MATCH:
                ++numEq;
                break;
            case EDLIB_EDOP_MISMATCH:
                ++numX;
                break;
            case EDLIB_EDOP_INSERT:
                ++numI;
                break;
            case EDLIB_EDOP_DELETE:
                ++numD;
                break;
            default:
                throw std::runtime_error("Unknown Edlib operation: " +
                                         std::to_string(static_cast<int32_t>(aln[i])));
                break;
        }
    }
}

void CigarDiffCounts(const PacBio::BAM::Cigar& cigar, int32_t& numEq, int32_t& numX, int32_t& numI,
                     int32_t& numD)
{
    numEq = numX = numI = numD = 0;

    if (cigar.empty()) {
        return;
    }

    for (const auto& op : cigar) {
        if (op.Type() == PacBio::BAM::CigarOperationType::SEQUENCE_MATCH) {
            numEq += op.Length();
        } else if (op.Type() == PacBio::BAM::CigarOperationType::SEQUENCE_MISMATCH) {
            numX += op.Length();
        } else if (op.Type() == PacBio::BAM::CigarOperationType::INSERTION) {
            numI += op.Length();
        } else if (op.Type() == PacBio::BAM::CigarOperationType::DELETION) {
            numD += op.Length();
        }
    }
}

void AppendToCigar(PacBio::BAM::Cigar& cigar, PacBio::BAM::CigarOperationType newOp, int32_t newLen)
{
    if (newLen == 0) {
        return;
    }
    if (cigar.empty() || newOp != cigar.back().Type()) {
        cigar.emplace_back(PacBio::BAM::CigarOperation(newOp, newLen));
    } else {
        cigar.back().Length(cigar.back().Length() + newLen);
    }
}

PacBio::BAM::Cigar ExpandMismatches(const char* query, int64_t queryLen, const char* target,
                                    int64_t targetLen, const PacBio::BAM::Cigar& cigar)
{
    PacBio::BAM::Cigar ret;
    if (cigar.size() <= 1) {
        return cigar;
    }
    int64_t queryPos = 0;
    int64_t targetPos = 0;
    int32_t lastAddedOp = -1;
    int32_t numCigarOps = cigar.size();

    for (int32_t i = 1; i < numCigarOps; ++i) {
        const auto& prevOp = cigar[i - 1];
        const auto& currOp = cigar[i];

        if (queryPos >= queryLen || targetPos >= targetLen) {
            std::ostringstream oss;
            oss << "Invalid CIGAR string: queryPos = " << queryPos << ", targetPos = " << targetPos
                << ", queryLen = " << queryLen << ", targetLen = " << targetLen;
            throw std::runtime_error(oss.str());
        }

        // Check if we have an INS+DEL or DEL+INS pair. If so, we'll convert them
        // into a single diagonal set of MATCH/MISMATCH operations, plus the left
        // hang and right hang indel operations.
        if ((prevOp.Type() == PacBio::BAM::CigarOperationType::INSERTION &&
             currOp.Type() == PacBio::BAM::CigarOperationType::DELETION) ||
            (prevOp.Type() == PacBio::BAM::CigarOperationType::DELETION &&
             currOp.Type() == PacBio::BAM::CigarOperationType::INSERTION)) {

            int32_t minLen = std::min(prevOp.Length(), currOp.Length());
            int32_t leftHang = static_cast<int32_t>(prevOp.Length()) - minLen;
            int32_t rightHang = static_cast<int32_t>(currOp.Length()) - minLen;

            AppendToCigar(ret, prevOp.Type(), leftHang);
            if (prevOp.Type() == PacBio::BAM::CigarOperationType::DELETION) {
                targetPos += leftHang;
            } else {
                queryPos += leftHang;
            }

            for (int32_t pos = 0; pos < minLen; ++pos) {
                if (query[queryPos] == target[targetPos]) {
                    AppendToCigar(ret, PacBio::BAM::CigarOperationType::SEQUENCE_MATCH, 1);
                } else {
                    AppendToCigar(ret, PacBio::BAM::CigarOperationType::SEQUENCE_MISMATCH, 1);
                }
                ++queryPos;
                ++targetPos;
            }

            AppendToCigar(ret, currOp.Type(), rightHang);
            if (currOp.Type() == PacBio::BAM::CigarOperationType::DELETION) {
                targetPos += rightHang;
            } else {
                queryPos += rightHang;
            }
            lastAddedOp = i;
            ++i;
            continue;
        } else {
            AppendToCigar(ret, prevOp.Type(), prevOp.Length());
            lastAddedOp = i - 1;
            if (prevOp.Type() != PacBio::BAM::CigarOperationType::DELETION) {
                queryPos += prevOp.Length();
            }
            if (prevOp.Type() != PacBio::BAM::CigarOperationType::INSERTION) {
                targetPos += prevOp.Length();
            }
        }
    }
    // Any remaining ops just get passed in.
    for (int32_t i = lastAddedOp + 1; i < numCigarOps; ++i) {
        AppendToCigar(ret, cigar[i].Type(), cigar[i].Length());
    }
    return ret;
}

}  // namespace Pancake
}  // namespace PacBio
