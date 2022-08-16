#ifndef LINK_H
#define LINK_H

#include <chrono>
#include <concepts>
#include <optional>

namespace std
{

using kibi = std::ratio<1024, 1>;
using mebi = std::ratio<1024 * 1024, 1>;
using gibi = std::ratio<1024 * 1024 * 1024, 1>;

}

namespace bit
{

template<typename T>
concept Ratio = requires(T t) {
			       t.num;
			       t.den;
			       t.den != 0;
};

template<std::integral T, Ratio Ratio>
class DigitalInformation
{
  T _value;
  
public:
  constexpr explicit DigitalInformation(T val) noexcept
    : _value(val * Ratio::num / Ratio::den) {}

  constexpr T bytes() const noexcept { return _value; }
};

using Byte     = DigitalInformation<unsigned long long, std::ratio<1,1>>;
using KiloByte = DigitalInformation<unsigned long long, std::kibi>;
using MegaByte = DigitalInformation<unsigned long long, std::mebi>;
using GigaByte = DigitalInformation<unsigned long long, std::gibi>;

using Bits     = DigitalInformation<unsigned long long, std::ratio<1,8>>;
using KiloBits = DigitalInformation<unsigned long long, std::ratio<1000,8>>;
using MegaBits = DigitalInformation<unsigned long long, std::ratio<1000000,8>>;
using GigaBits = DigitalInformation<unsigned long long, std::ratio<1000000000,8>>;

}

namespace bit_literals
{

constexpr bit::Byte operator ""_B(unsigned long long bytes)
{
  return bit::Byte{bytes};
}

constexpr bit::KiloByte operator ""_KB(unsigned long long bytes)
{
  return bit::KiloByte{bytes};
}

constexpr bit::MegaByte operator ""_MB(unsigned long long bytes)
{
  return bit::MegaByte{bytes};
}

constexpr bit::GigaByte operator ""_GB(unsigned long long bytes)
{
  return bit::GigaByte{bytes};
}

constexpr bit::Bits operator ""_bits(unsigned long long bits)
{
  return bit::Bits{bits};
}

constexpr bit::KiloBits operator ""_Kbits(unsigned long long bits)
{
  return bit::KiloBits{bits};
}

constexpr bit::MegaBits operator ""_Mbits(unsigned long long bits)
{
  return bit::MegaBits{bits};
}

constexpr bit::GigaBits operator ""_Gbits(unsigned long long bits)
{
  return bit::GigaBits{bits};
}

}

struct nl_sock;
struct rtnl_qdisc;
struct nl_cache;
struct rtnl_link;

namespace tc
{

using namespace std::chrono_literals;
using namespace bit_literals;

class Link
{
  static struct nl_sock    * nl_sock;
  static struct rtnl_qdisc * qtbf;
  static struct rtnl_qdisc * qnetem;
  static struct nl_cache   * cache;
  static struct rtnl_link  * link;

  static int if_index;
  
public:
  
  static constexpr auto LATENCY = 400ms;
  static constexpr auto BURST = 20_KB;
  
  static bool init(const char * if_name);
  static void exit();

  static bool set_limit(std::chrono::milliseconds delay,
			bit::KiloBits rate,
			std::optional<int> loss,
			std::optional<int> duplicates);
  
  static void reset_limit();
};

}

#endif /* LINK_H */
