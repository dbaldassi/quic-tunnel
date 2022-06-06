#include <quic/common/test/TestClientUtils.h>
#include <quic/common/test/TestUtils.h>
#include <quic/fizz/client/handshake/FizzClientQuicHandshakeContext.h>
#include <quic/samples/echo/LogQuicStats.h>

#include "quic_client.h"

QuicClient::QuicClient(std::string_view host, uint16_t port, std::string qlog_path) noexcept
  : _qlog_path(std::move(qlog_path)),
    _addr(host.data(), port),
    _network_thread("QuicTransportThread")
{
}

void QuicClient::start()
{
  auto evb = _network_thread.getEventBase();

  evb->runInEventBaseThreadAndWait([this, &evb]() {
    auto sock = std::make_unique<folly::AsyncUDPSocket>(evb);
    auto fizz_client_context = quic::FizzClientQuicHandshakeContext::Builder()
      .setCertificateVerifier(quic::test::createTestCertificateVerifier())
      .build();

    _quic_transport = std::make_shared<quic::QuicClientTransport>(evb,
								  std::move(sock),
								  std::move(fizz_client_context));

    _quic_transport->setHostname("echo.com");
    _quic_transport->addNewPeerAddress(_addr);
    _quic_transport->setDatagramCallback(this);
    _quic_transport->setCongestionControllerFactory(std::make_shared<quic::DefaultCongestionControllerFactory>());

    quic::TransportSettings settings;
    // enable datagram
    settings.datagramConfig.enabled      = true;
    settings.datagramConfig.readBufSize  = 2048;
    settings.datagramConfig.writeBufSize = 2048;
    settings.defaultCongestionController = _cc;
    
    _quic_transport->setTransportSettings(settings);
    _quic_transport->setTransportStatsCallback(std::make_shared<quic::samples::LogQuicStats>("client"));
    _quic_transport->setQLogger(QLog::create(_qlog_path));

    LOG(INFO) << "In quic tunnel connection to :" << _addr.describe();
    _quic_transport->start(this, this);
  });
  
  _start_done.wait();

  auto dcid = _quic_transport->getClientChosenDestConnectionId();
  if(dcid.hasValue()) _qlog_file_name = dcid.value().hex() + quic::FileQLogger::kQlogExtension;
  
  evb->runInEventBaseThread([this]() {
    _stream_id = _quic_transport->createBidirectionalStream().value();
    _quic_transport->setReadCallback(_stream_id, this);
  });
}

void QuicClient::stop()
{
  auto evb = _network_thread.getEventBase();

  evb->runInEventBaseThreadAndWait([this, &evb]() {
				 _quic_transport->closeTransport();
				 _quic_transport = nullptr;
			       });
}

void QuicClient::send_message(quic::StreamId id, quic::BufQueue& data)
{
  auto message = data.move();
  auto res = _quic_transport->writeChain(id, message->clone(), true);
  if(res.hasError()) {
    LOG(ERROR) << "In quic tunnel write chaine error=" << uint32_t(res.error());
  }
}

void QuicClient::send_message_stream(const char * buffer, size_t len)
{
  std::string m{buffer, len};
  auto evb = _network_thread.getEventBase();

  evb->runInEventBaseThread([this, msg=std::move(m)]() {
    quic::Buf qbuf = folly::IOBuf::copyBuffer(std::move(msg));
    
    auto res = _quic_transport->writeChain(_stream_id, std::move(qbuf), false);
    if(res.hasError()) {
      LOG(ERROR) << "In quic tunnel write chain error=" << uint32_t(res.error());
    }
  });
}

void QuicClient::send_message_datagram(const char * buffer, size_t len)
{
  std::string m{buffer, len};
  auto evb = _network_thread.getEventBase();

  evb->runInEventBaseThread([this, msg = std::move(m)]() {
			      // LOG(INFO) << "Sending : ";
    quic::Buf qbuf = folly::IOBuf::copyBuffer(std::move(msg));
    auto l = qbuf->length();
    
    auto state = _quic_transport->getState();
    auto maxWriteFrameSize = state->datagramState.maxWriteFrameSize;

    auto res = _quic_transport->writeDatagram(std::move(qbuf));
    if(res.hasError()) {
      LOG(ERROR) << "In quic tunnel write dgram error=" << uint32_t(res.error())
		 << " " << _quic_transport->getDatagramSizeLimit()
		 << " " << l
		 << " " << maxWriteFrameSize;
      
      std::exit(1);
    }
  });
}


// ConnectionSetupCallback ////////////////////////////////////////////////////
void QuicClient::onFirstPeerPacketProcessed() noexcept
{
  LOG(INFO) << "onFirstPeerPacketProcessed";
}

void QuicClient::onConnectionSetupError(quic::QuicError error) noexcept
{
  onConnectionError(std::move(error));
}

void QuicClient::onTransportReady() noexcept
{
  LOG(INFO) << "onTransportReady";
  _start_done.post();
}

void QuicClient::onReplaySafe() noexcept
{
  LOG(INFO) << "onReplaysafe";
}

void QuicClient::onFullHandshakeDone() noexcept
{
  LOG(INFO) << "onFullhandshakedone";
}

// ConnectionCallback /////////////////////////////////////////////////////////
void QuicClient::onFlowControlUpdate(quic::StreamId id) noexcept
{
  
}

void QuicClient::onNewBidirectionalStream(quic::StreamId id) noexcept
{
  LOG(INFO) << "In Quic tunnel: new bidirectional stream=" << id;
  _quic_transport->setReadCallback(id, this);
}

void QuicClient::onNewUnidirectionalStream(quic::StreamId id) noexcept
{
  LOG(INFO) << "In Quic tunnel: new unidirectional stream=" << id;
  _quic_transport->setReadCallback(id, this);
}

void QuicClient::onStopSending(quic::StreamId id, quic::ApplicationErrorCode) noexcept
{
  VLOG(10) << "In Quic tunnel got StopSending stream id=" << id;
}

void QuicClient::onConnectionEnd() noexcept
{
  LOG(INFO) << "Connection end";
}

void QuicClient::onConnectionError(quic::QuicError error) noexcept
{
  LOG(ERROR) << "EchoClient error: " << toString(error.code)
	     << "; errStr=" << error.message;
  _start_done.post();
}

void QuicClient::onConnectionEnd(quic::QuicError error) noexcept
{
  LOG(ERROR) << "OnConnectionEndError: " << toString(error.code)
               << "; errStr=" << error.message;
}

void QuicClient::onBidirectionalStreamsAvailable(uint64_t)  noexcept
{
  LOG(INFO) << "onBidirectionalStreamsAvailable";
}

void QuicClient::onUnidirectionalStreamsAvailable(uint64_t) noexcept
{
  LOG(INFO) << "onUnidirectionalStreamsAvailable";
}

// void QuicClient::onAppRateLimited() noexcept
// {
//   LOG(INFO) << "onAppRateLimited";
// }

void QuicClient::onKnob(uint64_t, uint64_t, quic::Buf)
{
  LOG(INFO) << "onKnob";
}

// Datagrams //////////////////////////////////////////////////////////////////

void QuicClient::onDatagramsAvailable() noexcept
{
  // LOG(INFO) << "On Datagram available";
  auto res = _quic_transport->readDatagrams();
  if(res.hasError()) {
    LOG(ERROR) << "readDatagrams() error: " << res.error();
    return;
  }

  for(const auto& dg : res.value()) {
    auto copy = dg.bufQueue().front()->cloneCoalesced();
    
    if(_on_received_callback) _on_received_callback((const char *)copy->data(), copy->length());
  }
}

// ReadCallback ///////////////////////////////////////////////////////////////
void QuicClient::readAvailable(quic::StreamId id) noexcept
{
  constexpr auto MAX_LEN = 0;
  auto data = _quic_transport->read(id, MAX_LEN);
  if(data.hasError()) {
    LOG(ERROR) << "In Quic tunnel failed read from stream=" << id
	       << ", error=" << (uint32_t)data.error();
  }

  auto copy = data->first->clone();
  if(auto it = _recv_offsets.find(id); it == _recv_offsets.end()) {
    _recv_offsets[id] = copy->length();
  }
  else {
    it->second += copy->length();
  }

  // LOG(INFO) << "Message received : " << copy->moveToFbString().toStdString();
  if(_on_received_callback) _on_received_callback((const char *)copy->data(), copy->length());
}

void QuicClient::readError(quic::StreamId id, quic::QuicError error) noexcept
{
  LOG(ERROR) << "In Quic tunnel failed read from stream=" << id
	     << ", error=" << toString(error);
  // A read error only terminates the ingress portion of the stream state.
  // Your application should probably terminate the egress portion via
  // resetStream
}
  
// Writecallback //////////////////////////////////////////////////////////////
void QuicClient::onStreamWriteReady(quic::StreamId id, uint64_t max_to_send) noexcept
{
  LOG(INFO) << "In Quic Tunnel socket is write ready with maxToSend="
	    << max_to_send;
  send_message(id, _pending_output[id]);
}

void QuicClient::onConnectionWriteReady(uint64_t) noexcept
{
  
}

void QuicClient::onStreamWriteError(quic::StreamId id, quic::QuicError error) noexcept
{
  LOG(ERROR) << "In quic tunnel write error with stream=" << id
	     << " error=" << toString(error);
}

void QuicClient::onConnectionWriteError(quic::QuicError error) noexcept
{

}
