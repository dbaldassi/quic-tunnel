#include "link.h"

#include <libnl3/netlink/route/tc.h>
#include <libnl3/netlink/route/qdisc.h>
#include <libnl3/netlink/route/qdisc/netem.h>
#include <libnl3/netlink/route/qdisc/tbf.h>

#include <fmt/core.h>
#include <fmt/color.h>

namespace tc
{

struct nl_sock    * Link::nl_sock = NULL;
struct rtnl_qdisc * Link::qtbf    = NULL;
struct rtnl_qdisc * Link::qnetem  = NULL;
struct nl_cache   * Link::cache   = NULL;
struct rtnl_link  * Link::link    = NULL;

bool Link::_init = false;

int Link::if_index = 0;

bool display_tc_error(const char * msg)
{
  fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "[tc] error : ");
  fmt::print("{}\n", msg);
  return false;
}

bool Link::init(const char * if_name)
{
  // Alloc netlink socket
  nl_sock = nl_socket_alloc();
  if(!nl_sock) return display_tc_error("Could not alloc socket");
	
  int ret = nl_connect(nl_sock, NETLINK_ROUTE);
  if(ret != 0) return display_tc_error("Could not connect socket");
	
  ret = rtnl_link_alloc_cache(nl_sock, AF_UNSPEC, &cache);
  if(ret != 0) return display_tc_error("Could not alloc cache");

  // Retrieve the network interface
  fmt::print("Getting link interface: {}\n", if_name);
  link = rtnl_link_get_by_name(cache, if_name);
  if(!link) return display_tc_error("Could not get link by name");

  // Get the index of the network interface
  if_index = rtnl_link_get_ifindex(link);
  if(if_index == 0) return display_tc_error("Could not get if_index");

  // Alloc netem qdisc
  qnetem = rtnl_qdisc_alloc();
  rtnl_tc_set_ifindex(TC_CAST(qnetem), if_index); // dev DEV
  rtnl_tc_set_parent(TC_CAST(qnetem), TC_H_ROOT); // root
  rtnl_tc_set_handle(TC_CAST(qnetem), TC_HANDLE(1, 0)); // handle 1:
  rtnl_tc_set_kind(TC_CAST(qnetem), "netem"); // netem

  // Alloc tbf qdisc, with netem as the parent
  qtbf = rtnl_qdisc_alloc();
  rtnl_tc_set_ifindex(TC_CAST(qtbf), if_index); // dev DEV
  rtnl_tc_set_parent(TC_CAST(qtbf), TC_HANDLE(1,0)); // parent 1:
  rtnl_tc_set_handle(TC_CAST(qtbf), TC_HANDLE(2,0)); // handle 2:
  // rtnl_tc_set_parent(TC_CAST(qtbf), TC_H_ROOT); // root
  // rtnl_tc_set_handle(TC_CAST(qtbf), TC_HANDLE(1, 0)); // handle 1:
  rtnl_tc_set_kind(TC_CAST(qtbf), "tbf"); // tbf

  _init = true;
  
  return true;
}

void Link::exit()
{
  if(!_init) return;
  // First remove link constraints
  reset_limit();

  // free qdiscs
  rtnl_qdisc_put(qnetem);
  rtnl_qdisc_put(qtbf);

  // free cache, socket
  nl_socket_free(nl_sock);
  rtnl_link_put(link);
  nl_cache_put(cache);

  _init = false;
}

void Link::reset_limit()
{
  if(!_init) return;
  
  rtnl_qdisc_delete(nl_sock, qtbf);
  rtnl_qdisc_delete(nl_sock, qnetem);
}

bool Link::set_limit(std::chrono::milliseconds delay, bit::KiloBits rate,
		     std::optional<int> loss, std::optional<int> duplicates)
{
  if(!_init) return false;
  
  reset_limit();

  // Set delay for netem qdisc
  auto delay_us = std::chrono::duration_cast<std::chrono::microseconds>(delay);
  rtnl_netem_set_delay(qnetem, delay_us.count());
  if(loss.has_value()) {
    fmt::print("Set loss to {} \n", *loss);
    rtnl_netem_set_loss(qnetem, *loss);
  }
  if(duplicates.has_value()) {
    fmt::print("Set duplicates to {} \n", *duplicates);
    rtnl_netem_set_duplicate(qnetem, *duplicates);
  }

  // Set rate and limit for tbf qdisc
  auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(LATENCY);
  rtnl_qdisc_tbf_set_rate(qtbf, rate.bytes(), BURST.bytes(), 0);
  rtnl_qdisc_tbf_set_limit_by_latency(qtbf, latency_us.count());

  // Add netem qdisc (qdisc add)
  auto ret = rtnl_qdisc_add(nl_sock, qnetem, NLM_F_CREATE);
  if(ret != 0) return display_tc_error("Could not add netem qdisc");

  // Add tbf qdisc
  ret = rtnl_qdisc_add(nl_sock, qtbf, NLM_F_CREATE);
  if(ret != 0) return display_tc_error("Could not add tbf qdisc");
  
  return true;
}

}
