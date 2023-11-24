#include "irc/IrcClient.hpp"

#include "irc/IrcParser.hpp"

#ifdef __clang__
#define BOOST_ASIO_HAS_CO_AWAIT 1
#endif

#include <wx/log.h>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/wintls.hpp>

#include <print>

namespace boost::beast
{

namespace detail
{

template <class AsyncStream> struct wintls_shutdown_op : boost::asio::coroutine
{
  wintls_shutdown_op(boost::wintls::stream<AsyncStream> &s, role_type role)
      : s_(s),
        role_(role)
  {
  }

  template <class Self>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  void operator()(Self &self, error_code ec = {}, std::size_t /*unused*/ = 0)
  {
    BOOST_ASIO_CORO_REENTER(*this)
    {
      self.reset_cancellation_state(net::enable_total_cancellation());

      BOOST_ASIO_CORO_YIELD
      s_.async_shutdown(std::move(self));
      ec_ = ec;

      using boost::beast::websocket::async_teardown;
      BOOST_ASIO_CORO_YIELD
      async_teardown(role_, s_.next_layer(), std::move(self));
      if (!ec_)
      {
        ec_ = ec;
      }

      self.complete(ec_);
    }
  }

private:
  boost::wintls::stream<AsyncStream> &s_;
  role_type role_;
  error_code ec_;
};

} // namespace detail

template <class AsyncStream, class TeardownHandler>
void async_teardown(role_type role, boost::wintls::stream<AsyncStream> &stream,
                    TeardownHandler &&handler)
{
  return boost::asio::async_compose<TeardownHandler, void(error_code)>(
      detail::wintls_shutdown_op<AsyncStream>(stream, role), handler, stream);
}

} // namespace boost::beast

namespace
{

namespace wintls = boost::wintls;
namespace ip = boost::asio::ip;
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;

using asio::awaitable;
using asio::co_spawn;
using asio::io_context;
using asio::use_awaitable;
using asio::experimental::channel;
using boost::system::error_code;
using ip::tcp;
using WebSocketStream =
    websocket::stream<wintls::stream<beast::tcp_stream::rebind_executor<
        asio::use_awaitable_t<>::executor_with_default<asio::any_io_executor>>::
                                         other>>;

using namespace boost::asio::experimental::awaitable_operators;
using namespace std::string_literals;

void fail(beast::error_code ec, char const *what)
{
  wxLogMessage("%s: %s", what, ec.message());
}

void fail(const std::exception &ex, char const *what)
{
  wxLogMessage("%s: %s", what, ex.what());
}

asio::redirect_error_t<asio::use_awaitable_t<asio::any_io_executor>>
await_ec(auto &target)
{
  return asio::redirect_error(use_awaitable, target);
}

auto logOrDie(auto action)
{
  return [action](const std::exception_ptr &e) noexcept
  {
    if (e)
    {
      try
      {
        std::rethrow_exception(e);
      }
      catch (std::exception &ex)
      {
        std::println(stderr, "{}: {}", action, ex.what());
      }
    }
  };
}

struct Endpoint
{
  std::string host;
  std::string port;
  std::string path;
};

class WebSocketSession
{
public:
  WebSocketSession(AppContextPtr app, io_context &ctx,
                   wintls::context &sslContext);

  awaitable<void> run(const Endpoint &endpoint);
  awaitable<void> teardown();

private:
  awaitable<void> connect(const Endpoint &endpoint);
  awaitable<void> initConnection();
  awaitable<void> listenIrc();
  awaitable<error_code> parseMessages(beast::flat_buffer &buf);

  awaitable<void> feedMessages();

  awaitable<error_code> write(const std::string &msg);

  AppContextPtr app_;
  Rules rules_;
  std::string lastChannel_;

  WebSocketStream ws_;

  asio::deadline_timer lifetime_;
  channel<void()> writeLock_;
};

WebSocketSession::WebSocketSession(AppContextPtr app, io_context &ctx,
                                   wintls::context &sslContext)
    : app_(std::move(app)),
      rules_(this->app_->readRules()),
      ws_(ctx, sslContext),
      lifetime_(ctx, boost::posix_time::pos_infin),
      writeLock_(ctx, 1)
{
}

awaitable<void> WebSocketSession::run(const Endpoint &endpoint)
{
  co_await this->connect(endpoint);
  co_spawn(co_await asio::this_coro::executor, this->feedMessages(),
           logOrDie("feed"));
  co_await this->listenIrc();
  co_await this->teardown();
}

awaitable<void> WebSocketSession::teardown()
{

  try
  {
    this->lifetime_.cancel();

    if (!this->ws_.is_open())
    { // already closed
      co_return;
    }

    // Close the WebSocket connection
    co_await this->ws_.async_close(websocket::close_code::normal);
    wxLogMessage("Closed connection gracefully");
  }
  catch (const std::exception &ex)
  {
    wxLogMessage("Failed to close connection: %s", ex.what());
  }
}

awaitable<void> WebSocketSession::connect(const Endpoint &endpoint)
{
  wxLogMessage("Resolving %s:%s", endpoint.host, endpoint.port);

  auto tcpResolver = tcp::resolver(co_await asio::this_coro::executor);
  error_code resolveError;
  auto target = co_await tcpResolver.async_resolve(endpoint.host, endpoint.port,
                                                   await_ec(resolveError));
  if (resolveError)
  {
    fail(resolveError, "resolve");
    co_return;
  }
  wxLogMessage("Connecting to %s:%s/%s", endpoint.host, endpoint.port,
               endpoint.path);

  // Make the connection on the IP address we get from a lookup
  auto tcpEndpoint =
      co_await beast::get_lowest_layer(this->ws_).async_connect(target);

  // Set SNI Hostname (many hosts need this to handshake successfully)
  this->ws_.next_layer().set_server_hostname(endpoint.host);
  this->ws_.next_layer().set_certificate_revocation_check(true);

  // Set a timeout on the operation
  beast::get_lowest_layer(this->ws_).expires_after(std::chrono::seconds(30));

  // Set a decorator to change the User-Agent of the handshake
  this->ws_.set_option(websocket::stream_base::decorator(
      [](websocket::request_type &req)
      {
        req.set(http::field::user_agent,
                std::format("{} SkipMySong", BOOST_BEAST_VERSION_STRING));
      }));

  // Perform the SSL handshake
  boost::system::error_code sslHandshakeError;
  co_await this->ws_.next_layer().async_handshake(
      wintls::handshake_type::client, await_ec(sslHandshakeError));
  if (sslHandshakeError)
  {
    fail(sslHandshakeError, "TLS handshake");
    co_return;
  }
  wxLogMessage("Completed TLS handshake");

  // Turn off the timeout on the tcp_stream, because
  // the websocket stream has its own timeout system.
  beast::get_lowest_layer(this->ws_).expires_never();

  // Set suggested timeout settings for the websocket
  this->ws_.set_option(
      websocket::stream_base::timeout::suggested(beast::role_type::client));

  // Perform the websocket handshake
  boost::system::error_code wsHandshakeError;
  co_await this->ws_.async_handshake(
      std::format("{}:{}", endpoint.host, tcpEndpoint.port()), endpoint.path);
  if (wsHandshakeError)
  {
    fail(wsHandshakeError, "handshake");
    co_return;
  }
  this->ws_.text(true);

  wxLogMessage("Completed WS handshake");

  co_await this->initConnection();
}

awaitable<void> WebSocketSession::initConnection()
{
  co_await this->write("CAP REQ :twitch.tv/tags\r\n"s);
  co_await this->write("PASS oauth:\r\n"s);
  co_await this->write("NICK justinfan12345\r\n"s);
  if (!this->rules_.channel.empty())
  {
    this->lastChannel_ = this->rules_.channel;
    wxLogMessage("JOIN #%s", this->rules_.channel);
    co_await this->write(std::format("JOIN #{}\n", this->rules_.channel));
  }
}

awaitable<void> WebSocketSession::listenIrc()
{
  try
  {
    beast::flat_buffer buf;
    for (;;)
    {
      error_code ec;
      co_await this->ws_.async_read(buf, await_ec(ec));
      if (ec.value() == asio::error::eof)
      {
        co_return;
      }
      if (ec)
      {
        wxLogMessage("Failed to read -> [%d] %s", ec.value(), ec.message());
        co_return;
      }

      co_await this->parseMessages(buf);
    }
  }
  catch (const std::exception &ex)
  {
    fail(ex, "listen[std::exception]");
    co_return;
  }
}

awaitable<error_code> WebSocketSession::parseMessages(beast::flat_buffer &buf)
{
  auto read = buf.cdata();

  std::pair<std::optional<IrcMessage>, std::size_t> parsed;
  while ((parsed = parseIrcMessage(
              {reinterpret_cast<const char *>(read.data()), read.size()}))
             .second != 0)
  {
    if (parsed.first)
    {
      const auto &msg = parsed.first;
      if (msg->isPing)
      {
        auto ec = co_await this->write(std::format("PONG :{}\n", msg->content));
        if (ec)
        {
          co_return ec;
        }
      }
      else
      {
        bool pass = (this->rules_.allowSubs && msg->isSub) ||
                    (this->rules_.allowNonSubs && !msg->isSub);
        pass = pass && msg->content.starts_with(this->rules_.command);

        if (pass)
        {
          this->app_->publishVote(std::string(msg->user));
        }
      }
    }
    buf.consume(parsed.second);
    read = buf.cdata();
  }

  co_return error_code{};
}

awaitable<error_code> WebSocketSession::write(const std::string &msg)
{
  error_code ec;
  co_await this->writeLock_.async_send(await_ec(ec)); // lock
  if (ec)
  {
    co_return ec;
  }
  co_await this->ws_.async_write(asio::buffer(msg), await_ec(ec));
  this->writeLock_.try_receive([](auto...) {}); // unlock
  if (ec)
  {
    wxLogMessage("Failed to send -> [%d] %s", ec.value(), ec.message());
  }
  co_return ec;
}

awaitable<void> WebSocketSession::feedMessages()
{
  try
  {
    for (;;)
    {
      error_code ecRules;
      error_code ecTimer;
      auto event = co_await (
          this->app_->rulesChanged().async_receive(await_ec(ecRules)) ||
          this->lifetime_.async_wait(await_ec(ecTimer)));
      if (event.index() == 1)
      {
        co_return; // we got woken up
      }

      // we got a ping, let's update the rules
      this->rules_ = this->app_->readRules();
      if (this->lastChannel_ != this->rules_.channel)
      {
        auto last = std::move(this->lastChannel_);
        this->lastChannel_ = this->rules_.channel;
        if (!last.empty())
        {
          wxLogMessage("PART #%s", last);
          co_await this->write(std::format("PART #{}\r\n", last));
        }
        wxLogMessage("JOIN #%s", this->rules_.channel);
        co_await this->write(std::format("JOIN #{}\r\n", this->rules_.channel));
      }
    }
  }
  catch (const boost::system::system_error &e)
  {
    wxLogMessage("Failed to feed messages: [%d] %s", e.code().value(),
                 e.code().message());
  }
  catch (const std::exception &ex)
  {
    wxLogMessage("Failed to feed messages: %s", ex.what());
  }
}

} // namespace

class IrcClientPrivate
{
public:
  IrcClientPrivate(AppContextPtr app, io_context &ctx,
                   wintls::context &sslContext)
      : ctx_(ctx),
        sslContext_(sslContext),
        app_(std::move(app)){};

  awaitable<void> runLoop(const Endpoint &endpoint)
  {

    for (;;)
    {
      // TODO: add backoff

      WebSocketSession sess{this->app_, this->ctx_, this->sslContext_};
      co_await sess.run(endpoint);
    }
  }

private:
  io_context &ctx_;
  wintls::context &sslContext_;

  AppContextPtr app_;

  friend class IrcClient;
};

IrcClient::IrcClient() = default;
IrcClient::~IrcClient() = default;
void IrcClient::run(const std::function<void(AppContextPtr)> &init)
{
  io_context ctx;

  AppContextPtr app = std::make_shared<AppContext>(ctx.get_executor(), nullptr);
  init(app);

  wintls::context sslCtx{wintls::method::system_default};
  sslCtx.use_default_certificates(true);
  sslCtx.verify_server_certificate(true);

  this->private_ = std::make_unique<IrcClientPrivate>(app, ctx, sslCtx);
  auto *d = this->private_.get();

  Endpoint twitch = {
      .host = "irc-ws.chat.twitch.tv"s,
      .port = "443"s,
      .path = "/"s,
  };

  try
  {
    co_spawn(ctx, d->runLoop(twitch), logOrDie("main loop"));

    ctx.run();
  }
  catch (const std::exception &ex)
  {
    std::println(stderr, "Exception: {}", ex.what());
  }
}
