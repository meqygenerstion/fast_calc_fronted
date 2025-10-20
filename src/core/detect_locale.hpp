// detect_locale.hpp
#pragma once
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>

std::string normalize_to_bcp47(std::string s);
std::string bcp47_to_posix_utf8(std::string tag);
std::string detect_bcp47_language();