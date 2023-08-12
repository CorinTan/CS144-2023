#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <optional>
#include <queue>

class Timer
{
private:
  uint64_t round_time_ = 0;   // 未判定为超时的累积计时
  uint64_t RTO_ms_ = 0;     // 当前RTO_ms
  bool is_running_ = false; // 运行状态
  bool is_expired_ = false; // 过期状态

  inline void set_expired();
  inline void reset();

public:
  inline void start( const uint64_t cur_RTO_ms );
  inline void stop();
  inline void restart( const uint64_t cur_RTO_ms );

  inline bool is_running() const;
  inline bool is_expired() const;
  inline uint64_t get_round_time() const;

  inline void increase_round_time( const size_t ms );
};

class TCPSender
{
private:
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t cur_RTO_ms_;
  
  uint64_t next_abs_seqno_; // 发送的/下一个序列号数
  Wrap32 last_ackno_;       // 最后接收到的ack序号
  uint16_t window_size_;   // 窗口大小  
  uint16_t cur_window_size_; // 当前窗口大小
  uint64_t consecutive_retrans_cnt_;  // 连续重传次数
  
  Timer retrans_timer_;
  std::queue<TCPSenderMessage> send_segments_;  // 要发送的TCP段队列
  std::queue<TCPSenderMessage> track_segments_; // 追踪已经发出但未被确认的tcp段

  
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
