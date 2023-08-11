#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <deque>

class Timer
{
private:
  size_t round_time_ = 0;   // 未判定为超时的累积计时
  uint64_t RTO_ms_ = 0;
  bool is_running_ = false; // 运行状态
  bool is_expired_ = false; // 过期状态
public:
  Timer() = default;
  Timer( const Timer& other ) = delete;
  Timer& operator=( const Timer& other ) = delete;
  // ~Timer() {}

  inline void start(const uint64_t cur_RTO_ms) { is_running_ = true; RTO_ms_ = cur_RTO_ms; }
  inline void stop() { is_running_ = false; }
  inline bool is_running() const { return is_running_; }
  inline bool is_expired() const { return is_expired_; }
  inline size_t get_round_time() const { return round_time_; }
  inline void increase_round_time(const size_t ms) { round_time_ += ms; }
  inline void set_expired() { is_expired_ = true; }
  inline void reset();
};

class TCPSender
{
private:
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t cur_RTO_ms_;
  
  Timer retrans_timer_;
  std::deque<>

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
