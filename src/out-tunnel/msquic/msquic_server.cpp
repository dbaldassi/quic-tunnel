#include "msquic_server.h"

#include <filesystem>
#include <sstream>
#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace fs = std::filesystem;

#define QUIC_API_ENABLE_PREVIEW_FEATURES 1
#include <msquic.h>

const QUIC_BUFFER Alpn = { sizeof("echo") - 1, (uint8_t*)"echo" };
const QUIC_REGISTRATION_CONFIG RegConfig = { "quicserver", QUIC_EXECUTION_PROFILE_LOW_LATENCY };

struct QUIC_CREDENTIAL_CONFIG_HELPER {
    QUIC_CREDENTIAL_CONFIG CredConfig;
    union {
        QUIC_CERTIFICATE_HASH CertHash;
        QUIC_CERTIFICATE_HASH_STORE CertHashStore;
        QUIC_CERTIFICATE_FILE CertFile;
        QUIC_CERTIFICATE_FILE_PROTECTED CertFileProtected;
    };
};

QUIC_STATUS ServerListenerCallback(HQUIC Listener, void* Context, QUIC_LISTENER_EVENT* Event)
{
  auto server = reinterpret_cast<MsquicServer*>(Context);
  return server->server_listener_callback(Listener, Event);
}

QUIC_STATUS ServerConnectionCallback(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event)
{
  auto server = reinterpret_cast<MsquicServer*>(Context);
  return server->server_connection_callback(Connection, Event);
}

QUIC_STATUS ServerStreamCallback(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event)
{
  auto server = reinterpret_cast<MsquicServer*>(Context);
  return server->server_stream_callback(Stream, Event);
}

MsquicServer::MsquicServer(const std::string& host, uint16_t port, out::UdpSocket * udp_socket)
  : _host(host), _port(port), _udp_socket(udp_socket)
{
  QUIC_STATUS status = QUIC_STATUS_SUCCESS;
  if (QUIC_FAILED(status = MsQuicOpen2(&_msquic))) {
    printf("MsQuicOpen2 failed, 0x%x!\n", status);
    return;
  }

  if (QUIC_FAILED(status = _msquic->RegistrationOpen(&RegConfig, &_registration))) {
    printf("RegistrationOpen failed, 0x%x!\n", status);
    return;
  }

  _udp_socket->set_callback(this);
}

MsquicServer::~MsquicServer()
{  
  if (_msquic) {
    if (_configuration) _msquic->ConfigurationClose(_configuration);
    if (_registration)  _msquic->RegistrationClose(_registration);

    MsQuicClose(_msquic);
  }
}

Capabilities MsquicServer::get_capabilities()
{
  Capabilities caps;
  caps.impl = "msquic";
  caps.datagrams = true;
  caps.streams = true;
  caps.cc.push_back("bbr");
  caps.cc.push_back("cubic");
  
  return caps;
}

void MsquicServer::set_qlog_filename(std::string file_name)
{
  _qlog_file = std::move(file_name);
}
  
void MsquicServer::start()
{
  _udp_socket->start();

  QUIC_STATUS status = QUIC_STATUS_SUCCESS;
  
  QUIC_ADDR Address = {0};
  QuicAddrSetFamily(&Address, QUIC_ADDRESS_FAMILY_UNSPEC);
  QuicAddrSetPort(&Address, _port);

  QUIC_SETTINGS settings = { 0 };
  settings.DatagramReceiveEnabled = TRUE;
  settings.IsSet.DatagramReceiveEnabled = TRUE;

  settings.PeerBidiStreamCount = 1;
  settings.IsSet.PeerBidiStreamCount = TRUE;

  settings.PeerUnidiStreamCount = 1;
  settings.IsSet.PeerUnidiStreamCount = TRUE;

  settings.CongestionControlAlgorithm = _cc;
  settings.IsSet.CongestionControlAlgorithm = TRUE;

  settings.NetStatsEventEnabled = TRUE;
  settings.IsSet.NetStatsEventEnabled = TRUE;

  QUIC_CREDENTIAL_CONFIG_HELPER Config;
  memset(&Config, 0, sizeof(Config));
  Config.CredConfig.Flags = QUIC_CREDENTIAL_FLAG_NONE;

  const char Cert[] = "./cert.pem";
  const char KeyFile[] = "./key.pem";

  Config.CertFileProtected.CertificateFile = (char*)Cert;
  Config.CertFileProtected.PrivateKeyFile = (char*)KeyFile;
  Config.CredConfig.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
  Config.CredConfig.CertificateFile = &Config.CertFile;

  if (QUIC_FAILED(status = _msquic->ConfigurationOpen(_registration, &Alpn, 1, &settings, sizeof(settings), NULL, &_configuration))) {
    printf("ConfigurationOpen failed, 0x%x!\n", status);
    return;
  }

  if (QUIC_FAILED(status = _msquic->ConfigurationLoadCredential(_configuration, &Config.CredConfig))) {
    printf("ConfigurationLoadCredential failed, 0x%x!\n", status);
    return;
  }

  if (QUIC_FAILED(status = _msquic->ListenerOpen(_registration, ServerListenerCallback, (void*)this, &_listener))) {
    printf("ListenerOpen failed, 0x%x!\n", status);
    return;
  }

  //
  // Starts listening for incoming connections.
  //
  if (QUIC_FAILED(status = _msquic->ListenerStart(_listener, &Alpn, 1, &Address))) {
    printf("ListenerStart failed, 0x%x!\n", status);
    return;
  }

  printf("Youhou welcome to msquic\n");

  std::unique_lock<std::mutex> lock(_mutex);
  _cv.wait(lock);
}


void MsquicServer::stop()
{
  printf("Stopping MsQuic server\n");
  _udp_socket->close();

  if(_qlog_ofs.is_open()) _qlog_ofs.close();
  
  if(_msquic) {
    if(_listener) {
      printf("Closing listener\n");
      _msquic->ListenerStop(_listener);
      _msquic->ListenerClose(_listener);
      _listener = nullptr;
    }

    if(_connection) {
      printf("Closing connection\n");
      _msquic->ConnectionClose(_connection);
      _connection = nullptr;
    }

    if(_configuration) {
      printf("Closing configuration\n");
      _msquic->ConfigurationClose(_configuration);
      _configuration = nullptr;
    }
  }
  
  _cv.notify_all();
}

QUIC_STATUS MsquicServer::server_listener_callback(QUIC_HANDLE* listener, QUIC_LISTENER_EVENT* event)
{
  QUIC_STATUS status = QUIC_STATUS_NOT_SUPPORTED;
  
  switch (event->Type) {
  case QUIC_LISTENER_EVENT_NEW_CONNECTION:
    //
    // A new connection is being attempted by a client. For the handshake to
    // proceed, the server must provide a configuration for QUIC to use. The
    // app MUST set the callback handler before returning.
    //
    _connection = event->NEW_CONNECTION.Connection;
    _msquic->SetCallbackHandler(_connection, (void*)ServerConnectionCallback, (void*)this);
    status = _msquic->ConnectionSetConfiguration(event->NEW_CONNECTION.Connection, _configuration);
    break;
  default:
    break;
  }
  
  return status;
}

QUIC_STATUS MsquicServer::server_connection_callback(QUIC_HANDLE* connection, QUIC_CONNECTION_EVENT* event)
{
  switch (event->Type) {
  case QUIC_CONNECTION_EVENT_CONNECTED: {
    //
    // The handshake has completed for the connection.
    //
    printf("[conn][%p] Connected\n", connection);
    _msquic->ConnectionSendResumptionTicket(connection, QUIC_SEND_RESUMPTION_FLAG_NONE, 0, NULL);

    uint32_t SizeOfBuffer = 8;
    uint8_t Buffer[8] = {0};
    _msquic->GetParam(connection, QUIC_PARAM_CONN_ORIG_DEST_CID, &SizeOfBuffer, Buffer);

    std::ostringstream oss;

    for(int i = 0; i < SizeOfBuffer; ++i) oss << std::to_string(Buffer[i]);
    set_qlog_filename(oss.str());

    printf("dcid : %s\n", oss.str().c_str());
    
    break;
  }
  case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
    //
    // The connection has been shut down by the transport. Generally, this
    // is the expected way for the connection to shut down with this
    // protocol, since we let idle timeout kill the connection.
    //
    if (event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status == QUIC_STATUS_CONNECTION_IDLE) {
      printf("[conn][%p] Successfully shut down on idle.\n", connection);
    } else {
      printf("[conn][%p] Shut down by transport, 0x%x\n", connection, event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status);
    }
    break;
  case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
    //
    // The connection was explicitly shut down by the peer.
    //
    printf("[conn][%p] Shut down by peer, 0x%llu\n", connection, (unsigned long long)event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
    break;
  case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
    //
    // The connection has completed the shutdown process and is ready to be
    // safely cleaned up.
    //
    // printf("[conn][%p] All done\n", connection);
    _msquic->ConnectionClose(connection);
    break;
  case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
    //
    // The peer has started/created a new stream. The app MUST set the
    // callback handler before returning.
    //
    // printf("[strm][%p] Peer started\n", event->PEER_STREAM_STARTED.Stream);
    _msquic->SetCallbackHandler(event->PEER_STREAM_STARTED.Stream, (void*)ServerStreamCallback, (void*)this);
    break;
  case QUIC_CONNECTION_EVENT_RESUMED:
    //
    // The connection succeeded in doing a TLS resumption of a previous
    // connection's session.
    //
    printf("[conn][%p] Connection resumed!\n", connection);
    break;
  case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED:
    // printf("[conn] Event Datagram received\n");
    _udp_socket->send((const char*)event->DATAGRAM_RECEIVED.Buffer->Buffer, event->DATAGRAM_RECEIVED.Buffer->Length);
    break;
  case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
    // printf("[conn] Event Datagram Send State Changed\n");
    server_on_datagram_send_state_changed(event->DATAGRAM_SEND_STATE_CHANGED.State, event->DATAGRAM_SEND_STATE_CHANGED.ClientContext);
    break;
  case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
    // printf("[conn] Event Datagram state changed\n");
    break;
  case QUIC_CONNECTION_EVENT_NETWORK_STATISTICS:
    write_stats(event);
  default:
    break;
  }
  return QUIC_STATUS_SUCCESS;
}

QUIC_STATUS MsquicServer::server_stream_callback(QUIC_HANDLE* stream, QUIC_STREAM_EVENT* event)
{
  switch (event->Type) {
  case QUIC_STREAM_EVENT_SEND_COMPLETE:
    //
    // A previous StreamSend call has completed, and the context is being
    // returned back to the app.
    //
    free(event->SEND_COMPLETE.ClientContext);
    // printf("[strm][%p] Data sent\n", stream);
    break;
  case QUIC_STREAM_EVENT_RECEIVE:{
    //
    // Data was received from the peer on the stream.
    //
    // printf("[strm][%p] Data received\n", stream);

    for(int i = 0; i < event->RECEIVE.BufferCount; ++i) {
      const QUIC_BUFFER Buffer = event->RECEIVE.Buffers[i];
      _udp_socket->send((const char*)Buffer.Buffer, Buffer.Length);
    }
    
    break;
  }
  case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
    //
    // The peer gracefully shut down its send direction of the stream.
    //
    // printf("[strm][%p] Peer shut down\n", stream);
    // server_send(stream);
    break;
  case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
    //
    // The peer aborted its send direction of the stream.
    //
    printf("[strm][%p] Peer aborted\n", stream);
    _msquic->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
    break;
  case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
    //
    // Both directions of the stream have been shut down and MsQuic is done
    // with the stream. It can now be safely cleaned up.
    //
    // printf("[strm][%p] All done\n", stream);
    _msquic->StreamClose(stream);
    break;
  default:
    break;
  }
  return QUIC_STATUS_SUCCESS;
}

void MsquicServer::server_on_datagram_send_state_changed(unsigned int state, void *ctx)
{
  switch(state) {
  case QUIC_DATAGRAM_SEND_UNKNOWN:
    printf("QUIC_DATAGRAM_SEND_UNKNOWN\n");
    break;
  case QUIC_DATAGRAM_SEND_SENT:
    // printf("QUIC_DATAGRAM_SEND_SENT\n");
    free(ctx);
    break;
  case QUIC_DATAGRAM_SEND_LOST_SUSPECT:
    printf("QUIC_DATAGRAM_SEND_LOST_SUSPECT\n");
    break;
  case QUIC_DATAGRAM_SEND_LOST_DISCARDED:
    printf("QUIC_DATAGRAM_SEND_LOST_DISCARDED\n");
    break;
  case QUIC_DATAGRAM_SEND_ACKNOWLEDGED:
    // printf("QUIC_DATAGRAM_SEND_ACKNOWLEDGED\n");
    break;
  case QUIC_DATAGRAM_SEND_ACKNOWLEDGED_SPURIOUS:
    printf("QUIC_DATAGRAM_SEND_ACKNOWLEDGED_SPURIOUS\n");
    break;
  case QUIC_DATAGRAM_SEND_CANCELED:
    printf("QUIC_DATAGRAM_SEND_CANCELED\n");
    break;
  default:
    printf("Unknown datagram send state\n");
  }
}

void MsquicServer::write_stats(const QUIC_CONNECTION_EVENT* event)
{
  if(_qlog_file.empty()) return;
  
  auto stats = event->NETWORK_STATISTICS;

  if(!_qlog_ofs.is_open()) {
    fs::path path = get_qlog_path();
    path /= get_qlog_filename();
    path.replace_extension("qlog");
    
    _qlog_ofs.open(path);
    
    _time_0 = std::chrono::system_clock::now();
  }
  
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(now - _time_0));
  
  json data = {
    { "bytes_in_flight", stats.BytesInFlight },
    { "congestion_window", stats.CongestionWindow },
    { "smoothed_rtt", stats.SmoothedRTT },
    { "posted_bytes", stats.PostedBytes },
    { "estimated_bandwidth", stats.Bandwidth }
  };
  
  json obj = {
    { "time", time.count() },
    { "name", "recovery:metrics_updated" },
    { "data", data }
  };

  _qlog_ofs << obj.dump() << std::endl;
}

void MsquicServer::server_send_stream(QUIC_BUFFER* buffer)
{
  QUIC_STATUS status;
  HQUIC stream;

  if(QUIC_FAILED(status = _msquic->StreamOpen(_connection, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL, ServerStreamCallback, this, &stream))) {
    printf("StreamOpen failed, 0x%x!\n", status);
    free(buffer);
    return;
  }

  if (QUIC_FAILED(status = _msquic->StreamSend(stream, buffer, 1, QUIC_SEND_FLAG_START | QUIC_SEND_FLAG_FIN, buffer))) {
    printf("StreamSend failed, 0x%x!\n", status);
    free(buffer);
    _msquic->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
  } 
}

void MsquicServer::server_send_datagram(QUIC_BUFFER* buffer)
{
  QUIC_STATUS status;

  if(QUIC_FAILED(status = _msquic->DatagramSend(_connection, buffer, 1, QUIC_SEND_FLAG_NONE, buffer))) {
    printf("Datagram Send failed, 0x%x!\n", status);
    free(buffer);
  }
}

void MsquicServer::onUdpMessage(const char *buffer, size_t len) noexcept
{  
  QUIC_BUFFER * qbuffer = (QUIC_BUFFER*)malloc(sizeof(QUIC_BUFFER) + len);
  qbuffer->Length = len;
  qbuffer->Buffer = (uint8_t*)qbuffer + sizeof(QUIC_BUFFER);
  memcpy(qbuffer->Buffer, buffer, len);
  
  if(_datagrams) server_send_datagram(qbuffer);
  else server_send_stream(qbuffer);
}

std::string_view MsquicServer::get_qlog_path() const noexcept
{
  return DEFAULT_QLOG_PATH;
}

std::string_view MsquicServer::get_qlog_filename() const noexcept
{
  return _qlog_file;
}

bool MsquicServer::set_datagrams(bool enable)
{
  _datagrams = enable;
  return true;
}
  
bool MsquicServer::set_cc(std::string_view cc) noexcept
{
  if(cc == "bbr")        _cc = QUIC_CONGESTION_CONTROL_ALGORITHM_BBR;
  else if(cc == "cubic") _cc = QUIC_CONGESTION_CONTROL_ALGORITHM_CUBIC;

  return true;
}
