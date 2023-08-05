#include "reassembler.hh"
#include <iterator>
#include <iostream>

using namespace std;

/* Class Reassembler functions */
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  cout << "call insert " << endl;
  
  next_need_index = output.bytes_pushed();
  Segment segment( first_index, std::move( data ), is_last_substring );

  // discard:
  if ( outOfBound( segment, output.available_capacity() ) )
    return;

  // push to writer immediately
  
  // cout << "original data: " << segment.data << endl;
  if ( sendNow( segment, output.available_capacity() ) ) {
    // cout << "push imediately: " << segment.data << endl;
    output.push( segment.data );
    next_need_index += segment.data.length();
    cleanBuffer();
    if ( segment.is_last_substring )
      output.close();
    else
      popValidDomains( output );
  }

  // store internally
  else {
    insertBuffer( std::move( segment ));
    mergeBuffer();
    popValidDomains( output );
  }
}

uint64_t Reassembler::bytes_pending() const
{
  cout << "call bytes_pending " << endl;
  return total_bytes_pending;
}

bool Reassembler::outOfBound( const Segment& segment, const uint64_t available_capacity )
{
  cout << "call outOfBound " << endl;
  // The pipe is full or index out of bound.
  return ( available_capacity == 0 ) || ( segment.first_index > next_need_index + available_capacity - 1 )
         || ( segment.first_index + segment.data.length() - 1 < next_need_index );
}

bool Reassembler::sendNow( Segment& segment, const uint64_t avaiable_capacity )
{
  cout << "call sendNow " << endl;
  // cout << "ava_ca: " << avaiable_capacity << endl;
  // cout << "next_index: " << next_need_index << endl;
  // cout << "first_index str: " << segment.first_index << endl;
  if ( segment.first_index > next_need_index )
    return false;
  if ( segment.first_index == next_need_index && segment.data.length() <= avaiable_capacity )
    return true; // 不更改segment, 直接发送
  else {         // 更改segment, 再发送
    uint64_t str_lenth = std::min( avaiable_capacity, segment.data.length() );
    if (str_lenth)
      segment.data = segment.data.substr( next_need_index - segment.first_index, str_lenth );
    // cout << "data : " << segment.data << endl;
    return true;
  } 
}

void Reassembler::cleanBuffer()
{
  cout << "call cleanBuffer " << endl;
  while (!buffer_domains.empty() && buffer_domains.front().second < next_need_index ) {
    // 整个删除
    buffer_segment.erase( buffer_domains.front().first );
    total_bytes_pending -= buffer_domains.front().second - buffer_domains.front().first + 1;
    buffer_domains.pop_front();
  }
}

void Reassembler::popValidDomains( Writer& output )
{
  cout << "call popValidDomains " << endl;
  while ( !buffer_domains.empty() && buffer_domains.front().first <= next_need_index ) {
    auto& first_domain = buffer_domains.front(); // [ ]
    string to_send = std::move( buffer_segment[first_domain.first].data );
    uint64_t send_length = first_domain.second - next_need_index + 1;
    // 处理要传输的字符串，存储的都是有效区间
    if ( next_need_index > first_domain.first ) // 裁小
      to_send = to_send.substr( next_need_index - first_domain.first, send_length );
    output.push( to_send );
    total_bytes_pending -= first_domain.second - first_domain.first + 1;
    next_need_index += send_length;
    if ( buffer_segment[first_domain.first].is_last_substring )
      output.close();

    buffer_segment.erase( first_domain.first );
    buffer_domains.pop_front();
  }
}

void Reassembler::insertBuffer( Segment&& segment)
{
  // total_bytes_pending 由函数 mergeBuffer 写入
  cout << "call insertBuffer " << endl;
  // [start, end]
  uint64_t start = segment.first_index;
  uint64_t end = segment.first_index + segment.data.length() - 1;

  // 空直接插入
  if ( buffer_domains.empty() ) {
    buffer_domains.insert( buffer_domains.end(), { start, end } );
    buffer_segment.insert( { start, std::move( segment ) } );
    return;
  }

  // 非空插入: 查找插入位置
  auto it = buffer_domains.begin();
  while ( it != buffer_domains.end() ) {
    if ( start <= it->first ) {
      buffer_domains.insert( it, { start, end } );
      buffer_segment.insert( { start, std::move( segment ) } );
      break;
    }
  }
  if ( it == buffer_domains.end() ) {
    buffer_domains.insert( it, { start, end } );
    buffer_segment.insert( { start, std::move( segment ) } );
  }
}

void Reassembler::mergeBuffer()
{
  cout << "call mergeBuffer " << endl;
  auto it1 = buffer_domains.begin();
  auto it2 = it1;
  ++it2;
  uint64_t step = 0;
  
  while ( it2 != buffer_domains.end() ) {
    if ( it1->second >= it2->first ) // 有重叠区间
    {
      if ( it2->second <= it1->second ) // it2被包含，直接删除
      {
        buffer_segment.erase( it2->first );
        buffer_domains.erase( it2 );
      } else // if (it2->second > it1->second), 合并区间
      {
        // 合成新的segment
        string new_data = std::move( buffer_segment[it1->first].data );
        string join_data =  buffer_segment[it2->first].data.substr( it1->second - it2->first + 1, it2->second - it1->second);
        new_data += join_data;
        bool last = buffer_segment[it1->first].is_last_substring | buffer_segment[it2->first].is_last_substring;
        Segment new_segment( it1->first, std::move( new_data ), last );

        // 修改原有数据结构，并插入新的segment
        buffer_segment[it1->first] = std::move(new_segment);
        buffer_segment.erase( it2->first );
        it1->second = it2->second;
        buffer_domains.erase( it2 );
      }
    }
    it1 = buffer_domains.begin();
    advance(it1, ++step);
    // 移动迭代器
    it2 = it1;
    ++it2;
  }

  // 统计缓存字节数
  total_bytes_pending = 0;
  for (const auto &start_end : buffer_domains) 
    total_bytes_pending += start_end.second - start_end.first + 1;
}

/* Class Segment functions */
Segment::Segment( uint64_t index, string&& content, bool is_last ) noexcept
  : first_index( index ), data( std::move( content ) ), is_last_substring( is_last )
{}

Segment::Segment( Segment&& segment ) noexcept
{
  first_index = segment.first_index;
  data = std::move( segment.data );
  is_last_substring = segment.is_last_substring;
}

Segment& Segment::operator=( Segment&& segment ) noexcept
{
  if ( this == &segment ) // 自赋值
    return *this;
  this->data = std::move( segment.data );
  this->first_index = segment.first_index;
  this->is_last_substring = segment.is_last_substring;
  return *this;
}
