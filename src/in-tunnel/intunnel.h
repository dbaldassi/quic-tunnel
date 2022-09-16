#ifndef INTUNNEL_H
#define INTUNNEL_H

#include <string_view>
#include <unordered_map>
#include <memory>

#include "udp_socket.h"
#include "random_generator.h"

class QuicClient;

/**
 * @brief Configure the in quic session, 
 * listens to the incoming RTP data and forward it in the quic connection to the out tunnel
 */
class InTunnel
{
  int _id; // Session id
  int _in_port; // udp socket listening port
  std::unique_ptr<QuicClient> _quic_client;
  in::UdpSocket _udp_socket; // Udp socket listening to incoming RTP data
  bool _datagrams; // Whether to use datagrams or streams

  bool _external_file_transfer; // Starts an external file transfer using scp
  bool _multiplexed_file_transfer; // Starts a file transfer using quic streams

  std::string _scp_file_url; // URL of the file to download with SCP syntax user@host:path/to/file

  static RandomGenerator _random_generator; // Random generator to generate the session id
public:
  static constexpr auto MAX_NUMBER_SESSION = 5; // Max InTunnel sessions

  // Keep a pointer of tunnel session for graceful shutdown
  static std::unordered_map<int, std::shared_ptr<InTunnel>> sessions;

  /**
   * @brief InTunnel ctr. Called by the static member create
   * @param id Id of the session
   * @param server_addr Address of the server (ipv4 or ipv6)
   * @param server_port Port of the quic server
   */
  InTunnel(int id, std::string_view server_addr, uint16_t server_port);

  ~InTunnel() noexcept;

  /**
   * @brief Set the congestion controller to use for the quic client
   * @param cc The congestion controller to use. "newreno", "cubic", "copa", "bbr"
   * @return false if could not set the cc (not supported by the implementation). true if success
   */
  bool set_cc(std::string_view cc);

  /**
   * @brief Whether to use quic datagrams or quic streams
   * @param enable true to use datagrams, false to use streams
   * @return false if implementation does not support datagrams
   */
  void set_datagram(bool enable);

  /** 
   * @brief Find and returns a port to bind the UDP socket listening to RTP
   * @return The port which the UDP socket can be bound to
   */
  int allocate_in_port();

  /**
   * @brief Get the current session id
   * @return The current session id
   */
  int id() const { return _id; }

  /**
   * @brief Get the qlog file path
   * @return The qlog file path
   */
  std::string get_qlog_file();

  /**
   * @brief Start the quic client and bind the UDP socket to listen to incoming RTP data
   */
  void run();

  /**
   * @brief Stop the quic connection and the UDP socket
   */
  void stop();

  /**
   * @brief Enable a scp file transfer
   * If enabled, it will be started when calling run
   */
  void enable_external_file_transfer();

  /**
   * @brief Enable a multiplexed file transfer
   * If enabled, it will be started when calling run
   */
  void enable_multiplexed_file_transfer();

  /**
   * @brief Create a InTunnel instance
   * @return The InTunnel instance as a shared ptr
   */
  static std::shared_ptr<InTunnel> create(std::string_view server_addr, uint16_t server_port);
};

#endif /* INTUNNEL_H */
