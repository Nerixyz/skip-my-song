#include "irc/IrcParser.hpp"

namespace
{

using namespace std::string_view_literals;

} // namespace

std::pair<std::optional<IrcMessage>, std::size_t>
parseIrcMessage(std::string_view buffer)
{
  constexpr std::pair<std::optional<IrcMessage>, std::size_t> needMoreData = {
      std::nullopt, 0};
  constexpr std::size_t npos = std::string_view::npos;

  if (buffer.empty())
  {
    return needMoreData;
  }

  std::size_t consumed = 0;
  IrcMessage msg;

  if (buffer[0] == '@')
  {
    auto space = buffer.find(' ');
    if (space == npos)
    {
      return needMoreData;
    }
    std::string_view tags{buffer.data() + 1, space - 1};
    auto badgeStart = tags.find("badges="sv);
    if (badgeStart != std::string_view::npos)
    {
      auto fromBadge = tags.substr(badgeStart + 7);
      auto badgeEnd = fromBadge.find(';');
      msg.isSub = fromBadge.substr(0, badgeEnd).contains("subscriber"sv);
    }
    consumed += space + 1;
    buffer = buffer.substr(space + 1);
  }

  if (buffer.empty())
  {
    return needMoreData;
  }

  if (buffer[0] == ':')
  {
    auto excl = buffer.find('!');
    if (excl == npos)
    {
      return needMoreData;
    }
    if (excl > 1)
    {
      msg.user = buffer.substr(1, excl - 1);
    }

    auto space = buffer.find(' ');
    if (space == npos)
    {
      return needMoreData;
    }
    consumed += space + 1;
    buffer = buffer.substr(space + 1);
  }

  if (buffer.empty())
  {
    return needMoreData;
  }

  if (!buffer.starts_with("PRIVMSG"sv))
  {
    auto clrf = buffer.find("\r\n"sv);
    if (clrf != npos)
    {
      consumed += clrf + 2;
      if (buffer.starts_with("PING :"sv))
      {
        msg.isPing = true;
        msg.content = buffer.substr(6, clrf - 6);
        return {msg, consumed};
      }
      return {std::nullopt, consumed};
    }
    return needMoreData;
  }
  auto col = buffer.find(':');
  if (col == npos)
  {
    return needMoreData;
  }

  auto clrf = buffer.find("\r\n"sv);
  if (clrf == npos)
  {
    return needMoreData;
  }
  msg.content = std::string_view{buffer.data() + col + 1,
                                 clrf < col ? 0 : clrf - col - 1};
  return {msg, consumed + clrf + 2};
}
