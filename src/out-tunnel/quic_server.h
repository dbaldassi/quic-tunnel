#ifndef QUIC_SERVER_H
#define QUIC_SERVER_H

#include <string>
#include <memory>
#include <optional>

#include "capabilities.h"
#include "udp_socket.h"

/**
 * @brief Interface for a quic server implementation
 */
class QuicServer
{
public:
  /**
   * @brief Default file name for the qlog file
   */
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-out-logs";
  
  explicit QuicServer() = default;
  virtual ~QuicServer() = default;

  /**
   * @brief Start the quic server in a forever loop
   */
  virtual void start() = 0;

  /**
   * @brief Graceful shutdown of the quic server
   */
  virtual void stop() = 0;

  /**
   * @brief Set a custom qlog file name
   */
  virtual void set_qlog_filename(std::string file_name) = 0;

  /**
   * @brief Get the path to the qlog file
   * @return The path of the qlog file
   */
  virtual std::string_view get_qlog_path() const noexcept = 0;

  /**
   * @brief Get the file name of the qlog file
   * @return The file name of the qlog file
   */
  virtual std::string_view get_qlog_filename() const noexcept = 0;

  /**
   * @brief Whether to use quic datagrams of quic streams
   * @param enable True to use quic datagrams. False for quic streams
   * @return false if not supported or an error occured, true otherwise.
   */
  virtual bool set_datagrams(bool enable) = 0;

  /**
   * @brief Set the congestion control algorithm to use
   * @param cc The congestion control name.
   * @return false if not supported or an error occured, true otherwise.
   */
  virtual bool set_cc(std::string_view cc) noexcept = 0;
};

/**
 * @brief Helper class to build a quic server
 */

class QuicServerBuilder
{
public:
  enum class QuicImplementation { MVFST, QUICGO, TCP, UDP };

  static void get_capabilities(std::vector<Capabilities>& capabilities);
  
  std::string host; // ip address the server will bind to
  uint16_t port; // port the server will listen to
  std::string dst_host;
  uint16_t dst_port;
  QuicImplementation impl; // implementation of quic to use
  out::UdpSocket * udp_socket; // pointer to the out udp socket

  std::optional<std::string> qlog_path;

  QuicServerBuilder() noexcept;
  ~QuicServerBuilder() = default;

  /**
   * @brief Create a quic server
   * @return A Quic Server instance
   */
  std::unique_ptr<QuicServer> create() const;
};

#endif /* QUIC_SERVER_H */
