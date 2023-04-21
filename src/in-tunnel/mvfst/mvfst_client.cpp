#include <quic/common/test/TestClientUtils.h>
#include <quic/common/test/TestUtils.h>
#include <quic/fizz/client/handshake/FizzClientQuicHandshakeContext.h>
#include <quic/samples/echo/LogQuicStats.h>

#include "mvfst_client.h"

MvfstClient::MvfstClient(std::string_view host, uint16_t port, std::string qlog_path) noexcept
  : _qlog_path(std::move(qlog_path)),
    _addr(host.data(), port),
    _network_thread("QuicTransportThread")
{
}

Capabilities MvfstClient::get_capabilities()
{
  Capabilities cap;
  cap.cc.emplace_back("newreno");
  cap.cc.emplace_back("cubic");
  cap.cc.emplace_back("copa");
  cap.cc.emplace_back("bbr");
  cap.cc.emplace_back("none");

  cap.datagrams = true;
  
  cap.impl = IMPL_NAME;

  return cap;
}

void MvfstClient::start()
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
    settings.maxRecvPacketSize = 4096;
    settings.canIgnorePathMTU = true;
    
    _quic_transport->setTransportSettings(settings);
    _quic_transport->setTransportStatsCallback(std::make_shared<quic::samples::LogQuicStats>("client"));
    _quic_transport->setQLogger(QLog<quic::VantagePoint::Client>::create(_qlog_path));

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

void MvfstClient::stop()
{
  auto evb = _network_thread.getEventBase();

  evb->runInEventBaseThreadAndWait([this, &evb]() {
				     std::unique_lock<std::mutex> lock(mutex);
				 _quic_transport->closeTransport();
				 _quic_transport = nullptr;
			       });
}

bool MvfstClient::set_cc(std::string_view cc) noexcept
{
   if(cc == "newreno")    _cc = quic::CongestionControlType::NewReno;
   else if(cc == "cubic") _cc = quic::CongestionControlType::Cubic;
   else if(cc == "copa")  _cc = quic::CongestionControlType::Copa;
   else if(cc == "bbr")   _cc = quic::CongestionControlType::BBR;
   else                   _cc = quic::CongestionControlType::None;

   return true;
}

void MvfstClient::send_message(quic::StreamId id, quic::BufQueue& data)
{
  auto message = data.move();
  auto res = _quic_transport->writeChain(id, message->clone(), true);
  if(res.hasError()) {
    LOG(ERROR) << "In quic tunnel write chaine error=" << uint32_t(res.error());
  }
}

void MvfstClient::send_message_stream(const char * buffer, size_t len)
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

void MvfstClient::send_message_datagram(const char * buffer, size_t len)
{
  std::string m{buffer, len};
  auto evb = _network_thread.getEventBase();

  evb->runInEventBaseThread([this, msg = std::move(m)]() {
    quic::Buf qbuf = folly::IOBuf::copyBuffer(std::move(msg));

    {
      std::unique_lock<std::mutex> lock(mutex);
      if(!_quic_transport) return;
      
      auto res = _quic_transport->writeDatagram(std::move(qbuf));
      if(res.hasError()) {
	LOG(ERROR) << "In quic tunnel write dgram error=" << uint32_t(res.error());
	std::exit(1);
      }
    }
  });
}


// ConnectionSetupCallback ////////////////////////////////////////////////////
void MvfstClient::onFirstPeerPacketProcessed() noexcept
{
  LOG(INFO) << "onFirstPeerPacketProcessed";
}

void MvfstClient::onConnectionSetupError(quic::QuicError error) noexcept
{
  onConnectionError(std::move(error));
}

void MvfstClient::onTransportReady() noexcept
{
  LOG(INFO) << "onTransportReady";
  _start_done.post();
}

void MvfstClient::onReplaySafe() noexcept
{
  LOG(INFO) << "onReplaysafe";
}

void MvfstClient::onFullHandshakeDone() noexcept
{
  LOG(INFO) << "onFullhandshakedone";
}

// ConnectionCallback /////////////////////////////////////////////////////////
void MvfstClient::onFlowControlUpdate(quic::StreamId id) noexcept
{
  
}

void MvfstClient::onNewBidirectionalStream(quic::StreamId id) noexcept
{
  LOG(INFO) << "In Quic tunnel: new bidirectional stream=" << id;
  _quic_transport->setReadCallback(id, this);
}

void MvfstClient::onNewUnidirectionalStream(quic::StreamId id) noexcept
{
  // LOG(INFO) << "In Quic tunnel: new unidirectional stream=" << id;
  _quic_transport->setReadCallback(id, this);
}

void MvfstClient::onStopSending(quic::StreamId id, quic::ApplicationErrorCode) noexcept
{
  LOG(INFO) << "In Quic tunnel got StopSending stream id=" << id;  
}

void MvfstClient::onConnectionEnd() noexcept
{
  LOG(INFO) << "Connection end";
}

void MvfstClient::onConnectionError(quic::QuicError error) noexcept
{
  LOG(ERROR) << "EchoClient error: " << toString(error.code)
	     << "; errStr=" << error.message;
  _start_done.post();
}

void MvfstClient::onConnectionEnd(quic::QuicError error) noexcept
{
  LOG(ERROR) << "OnConnectionEndError: " << toString(error.code)
               << "; errStr=" << error.message;
}

void MvfstClient::onBidirectionalStreamsAvailable(uint64_t)  noexcept
{
  LOG(INFO) << "onBidirectionalStreamsAvailable";
}

void MvfstClient::onUnidirectionalStreamsAvailable(uint64_t) noexcept
{
  LOG(INFO) << "onUnidirectionalStreamsAvailable";
}

// void MvfstClient::onAppRateLimited() noexcept
// {
//   LOG(INFO) << "onAppRateLimited";
// }

void MvfstClient::onKnob(uint64_t, uint64_t, quic::Buf)
{
  LOG(INFO) << "onKnob";
}

// Datagrams //////////////////////////////////////////////////////////////////

void MvfstClient::onDatagramsAvailable() noexcept
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
void MvfstClient::readAvailable(quic::StreamId id) noexcept
{
  constexpr auto MAX_LEN = 0;
  auto data = _quic_transport->read(id, MAX_LEN);
  
  if(data.hasError()) {
    LOG(ERROR) << "In Quic tunnel failed read from stream=" << id
	       << ", error=" << (uint32_t)data.error();

    return;
  }

  auto copy = data->first->clone();
  auto eof  = data->second;
  // LOG(INFO) << "received data(" << copy->length() << ") from: " << id;
  
  if(auto it = _recv_msg.find(id); it == _recv_msg.end()) {
    _recv_msg[id] = copy->moveToFbString().toStdString();
  }
  else {
    it->second += copy->moveToFbString().toStdString();
  }

  // End of the stream
  // TODO: check why onStopSending is never called
  if(eof) {
    if(_on_received_callback) _on_received_callback(_recv_msg[id].c_str(), _recv_msg[id].size());
    _recv_msg.erase(id);
  }
}

void MvfstClient::readError(quic::StreamId id, quic::QuicError error) noexcept
{
  LOG(ERROR) << "In Quic tunnel failed read from stream=" << id
	     << ", error=" << toString(error);
  // A read error only terminates the ingress portion of the stream state.
  // Your application should probably terminate the egress portion via
  // resetStream
}
  
// Writecallback //////////////////////////////////////////////////////////////
void MvfstClient::onStreamWriteReady(quic::StreamId id, uint64_t max_to_send) noexcept
{
  LOG(INFO) << "In Quic Tunnel socket is write ready with maxToSend="
	    << max_to_send;
  send_message(id, _pending_output[id]);
}

void MvfstClient::onConnectionWriteReady(uint64_t) noexcept
{
  
}

void MvfstClient::onStreamWriteError(quic::StreamId id, quic::QuicError error) noexcept
{
  LOG(ERROR) << "In quic tunnel write error with stream=" << id
	     << " error=" << toString(error);
}

void MvfstClient::onConnectionWriteError(quic::QuicError error) noexcept
{

}
