#pragma once

#include <boost/beast/core/flat_buffer.hpp>

#include <optional>
#include <string_view>
#include <tuple>

struct IrcMessage
{
  bool isSub = false;
  bool isPing = false;
  std::string_view user;
  std::string_view content;
};

std::pair<std::optional<IrcMessage>, std::size_t>
parseIrcMessage(std::string_view buffer);
