#pragma once

#include <string>

struct Rules
{
  std::string command;
  std::string channel;
  bool allowSubs;
  bool allowNonSubs;
  size_t threshold;
};
