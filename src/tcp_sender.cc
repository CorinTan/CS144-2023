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
  , window_size_( 1 )
  , consecutive_retrans_cnt_( 0 )

  , retrans_timer_()
  , send_segments_()
  , track_segments_()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  if ( track_segments_.empty() )
    return 0;
  // 还没有收到ack的序号数
  const uint64_t last_abs_ackno = last_ackno_.unwrap( isn_, next_abs_seqno_ );
  return next_abs_seqno_ - last_abs_ackno;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retrans_cnt_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // 发送TCP段（可能的情况下）
  optional<TCPSenderMessage> seg_maybe_send;
  // 计时器过期，重传最早的TCP段（若存在) (计时器过期，一定非空？)
  if ( retrans_timer_.is_expired() && !track_segments_.empty() ) {
    seg_maybe_send = track_segments_.front(); // 收到ack才pop
    ++consecutive_retrans_cnt_;               // 记录连续重传次数, 没有连续重传的时候要置为0
  }

  // 发送缓存中的TCP段( 根据缓存和接收窗口的情况 )
  else if ( !send_segments_.empty() ) {
    consecutive_retrans_cnt_ = 0; // 正常发送，连续重传次数置为0
    seg_maybe_send = send_segments_.front();
    send_segments_.pop();
    next_abs_seqno_ += seg_maybe_send.value().sequence_length();
    track_segments_.push( seg_maybe_send.value() );
  }
  return seg_maybe_send;
}

void TCPSender::push( Reader& outbound_stream )
{
  // 从 outbound_stream 读取相应字节数，并放入发送队列
  const uint64_t need_bytes
    = TCPConfig::MAX_PAYLOAD_SIZE < cur_window_size_ ? TCPConfig::MAX_PAYLOAD_SIZE : cur_window_size_;
  string payload;

  while ( !outbound_stream.is_finished() && payload.size() != need_bytes ) {
    string_view next = outbound_stream.peek();
    uint64_t bytes_to_pop = next.size();
    if ( payload.size() + next.size() > need_bytes ) 
      payload += next.substr( 0, need_bytes - payload.size() );
  }
    outbound_stream.pop( payload.size() );

  TCPSenderMessage seg_to_send;
  seg_to_send.seqno = Wrap32::wrap( outbound_stream.bytes_popped() - payload.size(), isn_ );
  seg_to_send.SYN = false; // 什么时候设置SYN？
  seg_to_send.payload = payload;
  seg_to_send.FIN = false;            // // 什么时候设置FIN？
  send_segments_.push( seg_to_send ); // 性能ok：只复制了智能指针
  cur_window_size_ -= seg_to_send.sequence_length();
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // 不占据序列号，也不追踪和重发
  Wrap32 seqno = Wrap32::wrap( next_abs_seqno_, isn_ );
  return { seqno, false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  cur_window_size_ = window_size_;

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
  }
  // 3. 重启重传定时器
  retrans_timer_.restart( cur_RTO_ms_ );
}

/* Timer function definations */

inline void Timer::set_expired()
{
  is_expired_ = true;
}

inline void Timer::reset()
{
  round_time_ = 0;
  RTO_ms_ = 0;
  is_running_ = false;
  is_expired_ = false;
}

inline void Timer::start( const uint64_t cur_RTO_ms )
{
  is_running_ = true;
  RTO_ms_ = cur_RTO_ms;
}

inline void Timer::stop()
{
  is_running_ = false;
}

inline void Timer::restart( const uint64_t cur_RTO_ms )
{
  reset();
  start( cur_RTO_ms );
}

inline bool Timer::is_running() const
{
  return is_running_;
}

inline bool Timer::is_expired() const
{
  return is_expired_;
}

inline size_t Timer::get_round_time() const
{
  return round_time_;
}

inline void Timer::increase_round_time( const size_t ms )
{
  round_time_ += ms;
  if ( round_time_ >= RTO_ms_ )
    set_expired();
}