#pragma once

#include "AppContext.hpp"

class IrcClientPrivate;
class IrcClient
{
public:
  IrcClient();
  ~IrcClient();

  void run(const std::function<void(AppContextPtr)> &init);

private:
  std::unique_ptr<IrcClientPrivate> private_;

  friend class IrcClientPrivate;
};
