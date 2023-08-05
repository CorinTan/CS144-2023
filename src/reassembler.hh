#pragma once

#include "byte_stream.hh"

#include <list>
#include <string>
#include <unordered_map>
#include <utility>

struct Strobj
{
  uint64_t first_index = -1;
  std::string data = "";
  bool is_last_substring = false;
  // constructor
  Strobj() = default;
  // move constructor
  Strobj( uint64_t index, std::string&& content, bool is_last ) noexcept;
  Strobj( Strobj&& strobj ) noexcept;
};

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
  std::unordered_map<uint64_t, Strobj> buffer_strobj = {};
  std::list<std::pair<uint64_t, uint64_t>> buffer_domains = {};

  uint64_t total_bytes_pending = 0;
  uint64_t next_need_index = -1;

  bool outOfBound( const Strobj& segment, uint64_t available_capacity );
  bool sendNow( Strobj& segment, uint64_t avaiable_capacity );
  void storeInternally( Strobj&& segment, Writer& output );
  void mergeBufferDomain();
  void popFirstDomain( Writer& output );
};
