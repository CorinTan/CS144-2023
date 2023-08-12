#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <optional>
#include <random>
using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , cur_RTO_ms_( initial_RTO_ms )
  , next_abs_seqno_( 0 )
  , last_ackno_( isn_ )
  , windows_size_( 1 )
  , consecutive_retrans_cnt_( 0 )

  , retrans_timer_()
  , send_segments_()
  , track_segments_()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  const uint64_t last_abs_ackno = last_ackno_.unwrap( isn_, next_abs_seqno_ );
  return next_abs_seqno_ - last_abs_ackno;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retrans_cnt_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  optional<TCPSenderMessage> seg_maybe_send;
  // 计时器过期，重传最早的TCP段（若存在)
  if ( retrans_timer_.is_expired() && !track_segments_.empty() ) {
    seg_maybe_send = track_segments_.front();
    track_segments_.pop();
    ++consecutive_retrans_cnt_; // 记录连续重传次数, 没有连续重传的时候要置为0
  }
  // 发送缓存中的TCP段
  else if ( !send_segments_.empty() ) {
    seg_maybe_send = send_segments_.front();
    send_segments_.pop();
  }
  return seg_maybe_send;
}

void TCPSender::push( Reader& outbound_stream )
{
  // 从 outbound_stream 读取相应字节数，并封装成 TCP segment放入发送队列
  const uint64_t payload_size = min( TCPConfig::MAX_PAYLOAD_SIZE, outbound_stream.bytes_buffered() );
  string payload;
  while ( payload.size() != payload_size ) {
    string_view next = outbound_stream.peek();
    uint64_t bytes_to_pop = next.size();
    if ( payload.size() + bytes_to_pop > payload_size )
      bytes_to_pop = payload_size - ( payload.size() + bytes_to_pop );
    payload += next.substr( 0, bytes_to_pop );
    outbound_stream.pop( bytes_to_pop );
  }
  TCPSenderMessage seg_to_send;
  // 此处的payload_size = seq数
  seg_to_send.seqno = Wrap32::wrap( outbound_stream.bytes_popped() - payload_size, isn_ );
  seg_to_send.SYN = false;
  seg_to_send.payload = payload;  
  seg_to_send.FIN = false;
  send_segments_.push(std::move(seg_to_send));
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // 不占据序列号，也不追踪和重发
  Wrap32 seqno = Wrap32::wrap( next_abs_seqno_, isn_ );
  return { seqno, false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  windows_size_ = msg.window_size;

  if ( msg.ackno ) {
    cur_RTO_ms_ = initial_RTO_ms_;       // 重置RTO
    consecutive_retrans_cnt_ = 0;        // 重置连续重传计数器
    while ( !track_segments_.empty() ) { // 移除buffer中已经被确认的segments
      const auto& front = track_segments_.front();
      const uint64_t front_abs_seqno = front.seqno.unwrap( isn_, next_abs_seqno_ );
      const uint64_t lower_bound = msg.ackno.value().unwrap( isn_, next_abs_seqno_ );
      if ( front_abs_seqno < lower_bound )
        track_segments_.pop();
      else
        break;
    }
    if ( track_segments_.empty() )
      retrans_timer_.stop(); // 所有segment都被接收， 停止timer
    else
      retrans_timer_.restart( cur_RTO_ms_ ); // 重启定时器
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if ( retrans_timer_.is_running() && !retrans_timer_.is_expired() ) {
    retrans_timer_.increase_round_time( ms_since_last_tick );
    if ( retrans_timer_.get_round_time() >= cur_RTO_ms_ )
      retrans_timer_.set_expired();
  }

  // 3. 重启重传定时器
  retrans_timer_.restart( cur_RTO_ms_ );
}

/* Timer function definations */
inline void Timer::reset()
{
  round_time_ = 0;
  uint64_t RTO_ms_ = 0;
  is_running_ = false;
  is_expired_ = false;
}