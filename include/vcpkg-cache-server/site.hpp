#pragma once

#include <string_view>

namespace vcache {

class Store;

namespace site {

std::string match();
std::string match(std::string_view abi, std::string_view package, const Store& store);
std::string list(const Store& store);
std::string find(std::string_view package, const Store& store);
std::string sha(std::string_view package, const Store& store);
std::string favicon();

}  // namespace site

}  // namespace vcache
