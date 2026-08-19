#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace panpath {

enum class Orientation { forward, reverse };

bool is_iupac(unsigned char symbol);
bool is_ambiguous(unsigned char symbol);
unsigned char complement(unsigned char symbol);
std::size_t segment_position(Orientation orientation, std::size_t index, std::size_t length);
std::vector<std::pair<std::string_view, Orientation>> walk_steps(std::string_view walk);

}
