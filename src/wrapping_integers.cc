#include "wrapping_integers.hh"

using namespace std;

/* Convert absolute seqno -> seqno */
Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;  // N convert from uint64_t to uint32_t implicitly.
}

/* Convert seqno -> absolute seqno */
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t add_num = 1 << 32;
  uint64_t zero_point_64 = zero_point.raw_value_;
  bool move = false;
  while ( zero_point_64 < checkpoint ) {
    zero_point_64 += add_num;
    move = true;
  }
  if (move && zero_point_64 - checkpoint > checkpoint - (zero_point_64 - add_num))
    zero_point_64 -= add_num;
  return zero_point_64;
}
