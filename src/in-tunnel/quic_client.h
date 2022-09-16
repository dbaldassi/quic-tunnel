#ifndef QUIC_CLIENT_H
#define QUIC_CLIENT_H

#include <string_view>
#include <functional>
#include <optional>
#include <memory>

class QuicClient
{
public:
  using QuicDataCallback = std::function<void(const char*, size_t)>;
protected:
  // Called when we receive data from the quic server
  QuicDataCallback _on_received_callback;

public:
  static constexpr const char * DEFAULT_QLOG_PATH = "tunnel-in-logs";
  
  QuicClient() noexcept = default;
  ~QuicClient() = default;

  /**
   * @brief Get the path to the qlog file
   * @return The path to the qlog file
   */
  virtual std::string_view get_qlog_path()   const noexcept = 0;

  /**
   * @brief Get the qlog filename
   * @return The qlog file name
   */
  virtual std::string_view get_qlog_filename() const noexcept = 0;

  /**
   * @brief Set the congestion controller
   * @param cc The congestion control to use
   * @return False if not supported or an error occured. True otherwise.
   */
  virtual bool set_cc(std::string_view cc) noexcept = 0;

  /**
   * @brief Start the quic connection
   */
  virtual void start() = 0;

  /**
   * @brief Stop the quic connection
   */
  virtual void stop() = 0;

  /**
   * @brief Send data in a quic stream
   */
  virtual void send_message_stream(const char * buffer, size_t len) = 0;

  /** 
   * @brief Send data in a quic datagram
   */
  virtual void send_message_datagram(const char * buffer, size_t len) = 0;

  /**
   * @brief Set the callback to get the data from the quic server
   * @param callback The callback called when data are received fron the quic connection
   */
  void set_on_received_callback(QuicDataCallback callback) noexcept {
    _on_received_callback = callback;
  }
};

class QuicClientBuilder
{
public:
  enum class QuicImplementation { MVFST };

  std::string        host; // Quic Server address
  uint16_t           port; // Quic server port
  QuicImplementation impl; // Quic implementation

  std::optional<std::string> qlog_path; // Quic qlog file

  QuicClientBuilder() noexcept;
  ~QuicClientBuilder() = default;
  
  std::unique_ptr<QuicClient> create() const;
};

#endif /* QUIC_CLIENT_H */
