#pragma once

#include "byte_stream.hh"

#include <list>
#include <string>
#include <unordered_map>
#include <utility>

class Reassembler
{
public:
  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring, Writer& output );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

private:
  std::unordered_map<uint64_t, std::pair<std::string, bool> > buffer_data = {};
  std::list<std::pair<uint64_t, uint64_t>> buffer_domains = {};

  uint64_t total_bytes_pending = 0;
  uint64_t lower_bound = 0;  // next_need_index
  uint64_t upper_bound = 0;  // [low_bound, upper_bound)

  bool outOfBound( const uint64_t first_index, const string &data);
  bool sendNow(  const uint64_t first_index, string &data);
  void popValidDomains( Writer& output ); // 检查buffer中是否存在可发送的数据，存在则都发送
  void insertBuffer( uint64_t first_index, string &data, bool is_last_substring);
  bool mergerBuffer(std::list<std::pair<uint64_t, uint64_t>>::iterator &pos, string &data, const bool is_last);
  inline void updateBounds(Writer &output);
  void printBufferDomains(); // debug
  inline void pushToWriter(const string &data, Writer &output, const bool last);
};
