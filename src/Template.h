#pragma once

#include <string>

class LoadOrder;
void ResolveTemplate(const std::string& input, const LoadOrder& loadOrder, std::string& output);
