#include "wrapping_integers.hh"

using namespace std;

/* Convert absolute seqno -> seqno */
Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n; // N convert from uint64_t to uint32_t implicitly.
}

/* Convert seqno -> absolute seqno */
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t dist = raw_value_ - zero_point.raw_value_;
  uint64_t add_num = static_cast<uint64_t>( 1 ) << 32;
  bool move = false;
  while ( dist < checkpoint ) {
    dist += add_num;
    move = true;
  }
  if ( move && dist - checkpoint > checkpoint - ( dist - add_num ) )
    dist -= add_num;
  return dist;
}
