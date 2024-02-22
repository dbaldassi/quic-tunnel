#include "msquic_client.h"

#define QUIC_API_ENABLE_PREVIEW_FEATURES 1
#include <msquic.h>

const QUIC_BUFFER Alpn = { sizeof("echo") - 1, (uint8_t*)"echo" };
const QUIC_REGISTRATION_CONFIG RegConfig = { "quicserver", QUIC_EXECUTION_PROFILE_LOW_LATENCY };


QUIC_STATUS ClientConnectionCallback(HQUIC Connection, void* Context, QUIC_CONNECTION_EVENT* Event)
{
  auto client = reinterpret_cast<MsquicClient*>(Context);
  return client->connection_callback(Connection, Event);
}

QUIC_STATUS ClientStreamCallback(HQUIC Stream, void* Context, QUIC_STREAM_EVENT* Event)
{
  auto client = reinterpret_cast<MsquicClient*>(Context);
  return client->stream_callback(Stream, Event);
}

MsquicClient::MsquicClient(std::string host, int port) noexcept : _host(std::move(host)), _port(port)
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
}

MsquicClient::~MsquicClient()
{
  if (_msquic) {
    if (_configuration) _msquic->ConfigurationClose(_configuration);
    if (_registration)  _msquic->RegistrationClose(_registration);
    
    MsQuicClose(_msquic);
  }
}

void MsquicClient::set_qlog_filename(std::string filename) noexcept
{
 
}

unsigned int MsquicClient::connection_callback(QUIC_HANDLE* connection, QUIC_CONNECTION_EVENT* event)
{
  switch (event->Type) {
  case QUIC_CONNECTION_EVENT_CONNECTED:
    //
    // The handshake has completed for the connection.
    //
    printf("[conn][%p] Connected\n", connection);
    // ClientSend(connection);
    break;
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
    // printf("[conn][%p] Shut down by peer, 0x%llu\n", connection, (unsigned long long)event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode);
    break;
  case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
    //
    // The connection has completed the shutdown process and is ready to be
    // safely cleaned up.
    //
    // printf("[conn][%p] All done\n", connection);
    if (!event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
      _msquic->ConnectionClose(connection);
    }
    break;
  case QUIC_CONNECTION_EVENT_RESUMPTION_TICKET_RECEIVED:
    //
    // A resumption ticket (also called New Session Ticket or NST) was
    // received from the server.
    //
    printf("[conn][%p] Resumption ticket received (%u bytes):\n", connection, event->RESUMPTION_TICKET_RECEIVED.ResumptionTicketLength);
    for (uint32_t i = 0; i < event->RESUMPTION_TICKET_RECEIVED.ResumptionTicketLength; i++) {
      printf("%.2X", (uint8_t)event->RESUMPTION_TICKET_RECEIVED.ResumptionTicket[i]);
    }
    printf("\n");
    break;
  case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
    //
    // The peer has started/created a new stream. The app MUST set the
    // callback handler before returning.
    //
    // printf("[strm][%p] Peer started\n", event->PEER_STREAM_STARTED.Stream);
    _msquic->SetCallbackHandler(event->PEER_STREAM_STARTED.Stream, (void*)ClientStreamCallback, (void*)this);
    break;
  case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED:
    // printf("[conn] Event Datagram received\n");
    if(_on_received_callback)
      _on_received_callback((const char*)event->DATAGRAM_RECEIVED.Buffer->Buffer, event->DATAGRAM_RECEIVED.Buffer->Length);
    break;
  case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
    // printf("[conn] Event Datagram Send State Changed\n");
    on_datagram_send_state_changed(event->DATAGRAM_SEND_STATE_CHANGED.State, event->DATAGRAM_SEND_STATE_CHANGED.ClientContext);
    break;
  case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
    // printf("[conn] Event Datagram state changed\n");
    break;
  default:
    break;
  }
  
  return QUIC_STATUS_SUCCESS;
}

unsigned int MsquicClient::stream_callback(QUIC_HANDLE* stream, QUIC_STREAM_EVENT* event)
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
  case QUIC_STREAM_EVENT_RECEIVE:
    //
    // Data was received from the peer on the stream.
    //
    // printf("[strm][%p] Data received\n", stream);
    for(int i = 0; i < event->RECEIVE.BufferCount; ++i) {
      const QUIC_BUFFER Buffer = event->RECEIVE.Buffers[i];
      if(_on_received_callback) _on_received_callback((const char*)Buffer.Buffer, Buffer.Length);
    }
    break;
  case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
    //
    // The peer gracefully shut down its send direction of the stream.
    //
    printf("[strm][%p] Peer aborted\n", stream);
    break;
  case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
    //
    // The peer aborted its send direction of the stream.
    //
    // printf("[strm][%p] Peer shut down\n", stream);
    break;
  case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
    //
    // Both directions of the stream have been shut down and MsQuic is done
    // with the stream. It can now be safely cleaned up.
    //
    // printf("[strm][%p] All done\n", stream);
    if (!event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
      _msquic->StreamClose(stream);
    }
    break;
  default:
    break;
  }
  return QUIC_STATUS_SUCCESS;
}

void MsquicClient::on_datagram_send_state_changed(unsigned int state, void *ctx)
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


std::string_view MsquicClient::get_qlog_path()   const noexcept
{
  return {};
}

std::string_view MsquicClient::get_qlog_filename() const noexcept
{
  return {};
}

bool MsquicClient::set_cc(std::string_view cc) noexcept
{
  if(cc == "bbr")        _cc = QUIC_CONGESTION_CONTROL_ALGORITHM_BBR;
  else if(cc == "cubic") _cc = QUIC_CONGESTION_CONTROL_ALGORITHM_CUBIC;
  
  return true;
}

void MsquicClient::start()
{
   QUIC_SETTINGS settings = { 0 };

  settings.DatagramReceiveEnabled = TRUE;
  settings.IsSet.DatagramReceiveEnabled = TRUE;
  
  settings.CongestionControlAlgorithm = _cc;
  settings.IsSet.CongestionControlAlgorithm = TRUE;

  settings.PeerBidiStreamCount = 1;
  settings.IsSet.PeerBidiStreamCount = TRUE;

  settings.PeerUnidiStreamCount = 1;
  settings.IsSet.PeerUnidiStreamCount = TRUE;

  QUIC_CREDENTIAL_CONFIG cred_config;
  memset(&cred_config, 0, sizeof(cred_config));
  cred_config.Type = QUIC_CREDENTIAL_TYPE_NONE;
  cred_config.Flags = QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;

  QUIC_STATUS status = QUIC_STATUS_SUCCESS;
  if (QUIC_FAILED(status = _msquic->ConfigurationOpen(_registration, &Alpn, 1, &settings, sizeof(settings), NULL, &_configuration))) {
    printf("ConfigurationOpen failed, 0x%x!\n", status);
    return;
  }

  if (QUIC_FAILED(status = _msquic->ConfigurationLoadCredential(_configuration, &cred_config))) {
    printf("ConfigurationLoadCredential failed, 0x%x!\n", status);
    return;
  }

  if (QUIC_FAILED(status = _msquic->ConnectionOpen(_registration, ClientConnectionCallback, (void*)this, &_connection))) {
    printf("ConnectionOpen failed, 0x%x!\n", status);
    return;
  }

  printf("[conn][%p] Connecting...\n", _connection);
  if (QUIC_FAILED(status = _msquic->ConnectionStart(_connection, _configuration, QUIC_ADDRESS_FAMILY_UNSPEC, _host.c_str(), _port))) {
    printf("ConnectionStart failed, 0x%x!\n", status);
    return;
  }
}

void MsquicClient::stop()
{
  printf("Stopping MsQuic client\n");
    
  printf("Closing connection\n");
  _msquic->ConnectionClose(_connection);
  _connection = nullptr;

  printf("Closing configuration\n");
  _msquic->ConfigurationClose(_configuration);
  _configuration = nullptr;
}

void MsquicClient::send_message_stream(const char * buf, size_t len)
{
  QUIC_STATUS status;
  HQUIC stream;

  QUIC_BUFFER * buffer = (QUIC_BUFFER*)malloc(sizeof(QUIC_BUFFER) + len);
  buffer->Length = len;
  buffer->Buffer = (uint8_t*)buffer + sizeof(QUIC_BUFFER);
  memcpy(buffer->Buffer, buf, len);

  if(QUIC_FAILED(status = _msquic->StreamOpen(_connection, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL, ClientStreamCallback, this, &stream))) {
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

void MsquicClient::send_message_datagram(const char * buf, size_t len)
{
  QUIC_STATUS status;

  QUIC_BUFFER * buffer = (QUIC_BUFFER*)malloc(sizeof(QUIC_BUFFER) + len);
  buffer->Length = len;
  buffer->Buffer = (uint8_t*)buffer + sizeof(QUIC_BUFFER);
  memcpy(buffer->Buffer, buf, len);
  
  if(QUIC_FAILED(status = _msquic->DatagramSend(_connection, buffer, 1, QUIC_SEND_FLAG_NONE, buffer))) {
    printf("Datagram Send failed, 0x%x!\n", status);
    free(buffer);
  }
}
  
Capabilities MsquicClient::get_capabilities()
{
  Capabilities caps;
  caps.impl = IMPL_NAME;
  caps.datagrams = true;
  caps.streams = true;
  caps.cc.push_back("bbr");
  caps.cc.push_back("cubic");

  return caps;
}
