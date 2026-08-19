#include "panpath/sequence.hpp"

#include <cctype>
#include <stdexcept>

namespace panpath {

bool is_iupac(unsigned char symbol) {
    switch (std::toupper(symbol)) {
        case 'A': case 'C': case 'G': case 'T': case 'R': case 'Y': case 'S':
        case 'W': case 'K': case 'M': case 'B': case 'D': case 'H': case 'V': case 'N':
            return true;
        default:
            return false;
    }
}

bool is_ambiguous(unsigned char symbol) {
    const auto upper = std::toupper(symbol);
    return upper != 'A' && upper != 'C' && upper != 'G' && upper != 'T';
}

unsigned char complement(unsigned char symbol) {
    const bool lower = std::islower(symbol);
    unsigned char result;
    switch (std::toupper(symbol)) {
        case 'A': result = 'T'; break; case 'C': result = 'G'; break;
        case 'G': result = 'C'; break; case 'T': result = 'A'; break;
        case 'R': result = 'Y'; break; case 'Y': result = 'R'; break;
        case 'S': result = 'S'; break; case 'W': result = 'W'; break;
        case 'K': result = 'M'; break; case 'M': result = 'K'; break;
        case 'B': result = 'V'; break; case 'D': result = 'H'; break;
        case 'H': result = 'D'; break; case 'V': result = 'B'; break;
        case 'N': result = 'N'; break;
        default: throw std::runtime_error("invalid nucleotide");
    }
    return lower ? static_cast<unsigned char>(std::tolower(result)) : result;
}

std::size_t segment_position(Orientation orientation, std::size_t index, std::size_t length) {
    return orientation == Orientation::forward ? index + 1 : length - index;
}

std::vector<std::pair<std::string_view, Orientation>> walk_steps(std::string_view walk) {
    std::vector<std::pair<std::string_view, Orientation>> result;
    std::size_t start = 0;
    while (start < walk.size()) {
        const auto marker = walk[start];
        if (marker != '>' && marker != '<') throw std::runtime_error("malformed walk");
        const auto end = walk.find_first_of("><", start + 1);
        const auto name = walk.substr(start + 1, end - start - 1);
        if (name.empty()) throw std::runtime_error("malformed walk");
        result.emplace_back(name, marker == '>' ? Orientation::forward : Orientation::reverse);
        start = end == std::string_view::npos ? walk.size() : end;
    }
    return result;
}

}
