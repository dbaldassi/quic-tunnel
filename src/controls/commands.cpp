#include <thread>
#include <fstream>
#include <filesystem>

#include <wait.h>

#include "commands.h"
#include "response.h"
#include "link.h"

#include "in-tunnel/intunnel.h"
#include "out-tunnel/outtunnel.h"

#include "out-tunnel/quic_server.h"

#include "fmt/core.h"

namespace cmd
{

// StartClient ////////////////////////////////////////////////////////////////

std::unique_ptr<response::Response> StartClient::run()
{
  // Create quic client
  auto client = ::InTunnel::create(impl, quic_host, quic_port);

  // Ensure it is created
  if(client == nullptr) {
    auto resp = std::make_unique<response::Error>();
    resp->trans_id = _trans_id;
    resp->message = "Internal error, could not create quic session";

    return resp;
  }

  // Set the cc
  client->set_cc(cc);
  client->set_datagram(datagrams);

  if(external_file_transfer)
    client->enable_external_file_transfer();
  
  // Create websocket response
  auto resp = std::make_unique<response::StartClient>();
  
  // Fill websocket response
  resp->trans_id = _trans_id;
  resp->port = client->allocate_in_port();
  resp->id = client->id();

  // Run the client in a detached thread
  std::thread([client]() -> void { client->run(); }).detach();

  // return response
  return resp;
}

// StopClient /////////////////////////////////////////////////////////////////

ResponsePtr StopClient::run()
{
  // Check if the specified id exists
  if(InTunnel::sessions.contains(id)) {
    auto client = InTunnel::sessions[id];
    InTunnel::sessions.erase(id);

    client->stop();

    auto resp = std::make_unique<response::StopClient>();
    resp->trans_id = _trans_id;

    if(const char * qvis = std::getenv("QVIS_URL")) {
      resp->url = qvis;
    }
    else {
      resp->url = "https://qvis.dabaldassi.fr";
    }
    
    resp->url += "?file=" + client->get_qlog_file();

    fmt::print("-- > {}\n", resp->url);
    
    client = nullptr;

    return resp;
  }

  // Does not exist, return an error
  auto resp = std::make_unique<response::Error>();
  resp->trans_id = _trans_id;
  resp->message = "Specified quic session id does not exist";
  
  return std::move(resp);
}

// StartServer ////////////////////////////////////////////////////////////////

ResponsePtr StartServer::run()
{
  // Create quic client
  auto server = ::OutTunnel::create(impl, addr_out, quic_port, port_out);
  server->set_cc(cc);
  server->set_datagrams(datagrams);
  server->set_external_file_transfer(external_file_transfer);
  
  // Ensure it is created
  if(server == nullptr) {
    auto resp = std::make_unique<response::Error>();
    resp->trans_id = _trans_id;
    resp->message = "Internal error, could not create quic session";

    return resp;
  }

  // Create websocket response
  auto resp = std::make_unique<response::StartServer>();
  
  // Fill websocket response
  resp->trans_id = _trans_id;
  resp->id = server->id();

  // Run the client in a detached thread
  std::thread([server]() -> void { server->run(); }).detach();

  // return response
  return resp;
}

// StopServer /////////////////////////////////////////////////////////////////

ResponsePtr StopServer::run()
{
  // Check if the specified id exists
  if(OutTunnel::sessions.contains(id)) {
    auto server = OutTunnel::sessions[id];
    OutTunnel::sessions.erase(id);

    server->stop();

    auto resp = std::make_unique<response::StopServer>();
    resp->trans_id = _trans_id;
    
    server = nullptr;

    return resp;
  }

  // Does not exist, return an error
  auto resp = std::make_unique<response::Error>();
  resp->trans_id = _trans_id;
  resp->message = "Specified quic session id does not exist";
  
  return std::move(resp);
}

// Link ///////////////////////////////////////////////////////////////////////
ResponsePtr Link::run()
{
  bool success = true;

  if(reset) {
    tc::Link::reset_limit();
  }
  else {
    success = tc::Link::set_limit(std::chrono::milliseconds(delay),
				  bit::KiloBits(bitrate),
				  loss.value_or(0),
				  duplicates.value_or(0));
  }

  if(!success) {
    auto resp = std::make_unique<response::Error>();
    resp->trans_id = _trans_id;
    resp->message = "Could not set link limit";
    return resp;
  }
  
  auto resp = std::make_unique<response::Link>();
  resp->trans_id = _trans_id;
  
  return resp;
}

// Impl ///////////////////////////////////////////////////////////////////////
ResponsePtr Capabilities::run()
{    
  auto resp = std::make_unique<response::Capabilities>();
  resp->trans_id = _trans_id;
  
  if(in_requested)  InTunnel::get_capabilities(resp->in_cap);
  if(out_requested) OutTunnel::get_capabilities(resp->out_cap);
  
  return resp;
}

// Stats //////////////////////////////////////////////////////////////////////

ResponsePtr UploadRTCStats::run()
{
  auto resp = std::make_unique<response::UploadRTCStats>();
  resp->trans_id = _trans_id;

  // Write stats in a csv file, bitrate.csv
  std::ofstream ofs("bitrate.csv");

  if(!ofs.is_open()) {
    auto resp = std::make_unique<response::Error>();
    resp->trans_id = _trans_id;
    resp->message = "Could not create rtc stats file";

    return resp;
  }
  
  for(auto& pt : stats) {
    ofs << pt.x << ","
	<< pt.bitrate << ","
	<< pt.link << ","
	<< pt.fps << ","
	<< pt.frame_decoded << ","
	<< pt.frame_key_decoded << ","
	<< pt.frame_dropped << ","
	<< pt.frame_rendered 
	<< std::endl;
  }
  
  return resp;
}

namespace fs = std::filesystem;

template<typename Dest, typename SrcFile>
void move_files(Dest&& dst, SrcFile&& src)
{
  try {
    fs::copy(src, dst);
    fs::remove(src);
  } catch (fs::filesystem_error& e) {
    if constexpr (std::is_same_v<std::remove_cvref_t<SrcFile>, fs::path>)
		   fmt::print("Could not copy file {} : {}", src.c_str(), e.what());
    else
      fmt::print("Could not copy file {} : {}", src, e.what());
  }
}

template<typename Dest, typename SrcFile, typename... Files>
void move_files(Dest&& dst, SrcFile&& src, Files&& ... others)
{
  try {
    fs::copy(src, dst);
    fs::remove(src);
  } catch (fs::filesystem_error& e) {
    if constexpr (std::is_same_v<std::remove_cvref_t<SrcFile>, fs::path>)
		   fmt::print("Could not copy file {} : {}", src.c_str(), e.what());
    else
      fmt::print("Could not copy file {} : {}", src, e.what());
  } 
  move_files(dst, std::forward<Files>(others)...);
}

ResponsePtr GetStats::run()
{
  // create results directory
  fs::path results_path = fs::path{"result"}/exp_name;
  fs::create_directory(results_path);
  fs::permissions(results_path, fs::perms::owner_all | fs::perms::group_all | fs::perms::others_all);

  // copy stats files into results directory
  move_files(results_path, "bitrate.csv", "quic.csv", "file.csv");
  
  // Generate csv curve from stats file
  pid_t pid = fork();

  if(pid == 0) {
    [[maybe_unused]] auto _ = chdir(results_path.c_str());
    execlp("/usr/local/bin/show_csv.py", "/usr/local/bin/show_csv.py", exp_name.c_str(), "save", NULL);
  }
  else waitpid(pid, NULL, 0);
  
  // create response
  auto resp = std::make_unique<response::GetStats>();
  resp->trans_id = _trans_id;

  const char * res_url = std::getenv("RESULT_URL");
  
  if(!res_url) {
    fmt::print("No RESULT_URL specified");
    return resp;
  }

  resp->url = (res_url / results_path / (exp_name + ".png")).c_str();

  fs::path dump_file{"medooze_dumps"};
  fs::path mp4_file{"medooze_dumps"};
  dump_file /= fs::path{medooze_dump_url}.filename();
  mp4_file /= fs::path{medooze_dump_url}.filename().replace_extension(".mp4");

  move_files(results_path, dump_file, mp4_file);

  resp->medooze_url = "https://medooze.github.io/bwe-stats-viewer/?url=";
  *resp->medooze_url += (res_url / results_path / dump_file.filename()).c_str();
    
  fs::remove(dump_file.replace_extension(".pcap"));
  
  if(transport == "tcp") {
    move_files(results_path, "sender-ss.csv", "result_tcp.png");
    resp->tcp_url = (res_url / results_path / "result_tcp.png").c_str();
  }
  else if(transport == "quic") {
    fs::path               qlog_path{QuicServer::DEFAULT_QLOG_PATH};
    fs::directory_iterator qlog_it{qlog_path};
    
    move_files(results_path, qlog_it->path());

    if(const char * qvis = std::getenv("QVIS_URL")) resp->qvis_url = qvis;
    else resp->qvis_url = "https://qvis.dabaldassi.fr";
    
    *resp->qvis_url += std::string("?file=") + (results_path / qlog_it->path().filename()).c_str();
  }

  return resp;
}

}
