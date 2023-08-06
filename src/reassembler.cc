#include "reassembler.hh"
#include <iostream>

using namespace std;

/* Class Reassembler functions */
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  updateBounds( output );
  // discard:
  if ( outOfBound( first_index, data ) )
    return;

  // push to writer immediately
  if ( sendNow( first_index, data ) )
    pushToWriter( data, output, is_last_substring );

  // store internally
  else
    insertBuffer( first_index, data, is_last_substring );
  popValidDomains( output );
}

inline void Reassembler::pushToWriter( const string& data, Writer& output, const bool last )
{
  output.push( data );
  updateBounds( output );
  if ( last )
    output.close();
}

inline void Reassembler::updateBounds( Writer& output )
{
  // [ )
  lower_bound = output.bytes_pushed();
  upper_bound = lower_bound + output.available_capacity();
}

uint64_t Reassembler::bytes_pending() const
{
  cout << "call bytes_pending " << endl;
  return total_bytes_pending;
}

bool Reassembler::outOfBound( const uint64_t first_index, const string& data )
{
  cout << "call outOfBound " << endl;
  // The pipe is full or index out of bound.
  return ( !data.empty() && lower_bound == upper_bound ) || ( first_index >= upper_bound )
         || ( first_index + data.length() < lower_bound );
}

bool Reassembler::sendNow( const uint64_t first_index, string& data )
{
  cout << "call sendNow " << endl;
  if ( first_index > lower_bound )
    return false;
  if ( first_index == lower_bound )
    return true; // 直接发送
  else {
    // 更改segment, 再发送。只用截取前部分，后部分会被 Writer 截取
    data = data.substr( lower_bound - first_index );
    return true;
  }
}

void Reassembler::insertBuffer( uint64_t first_index, string& data, bool is_last_substring )
{
  // total_bytes_pending 由函数 mergeBuffer 写入
  cout << "call insertBuffer " << endl;
  // [start, end)
  uint64_t start = first_index;
  uint64_t end = start + data.length();

  // 空直接插入
  if ( buffer_domains.empty() ) {
    // cout << "buffer is empty." << endl;
    total_bytes_pending += data.length();
    buffer_domains.insert( buffer_domains.end(), { start, end } );
    buffer_data.insert( { start, { data, is_last_substring } } );
    return;
  }

  // 非空插入: 查找插入位置
  auto it = buffer_domains.begin();
  while ( it != buffer_domains.end() ) {
    if ( start < it->first ) {
      // 插入区间和map, 修改pending
      it = buffer_domains.insert( it, { start, end } );
      total_bytes_pending += data.length();
      buffer_data.insert( { start, { std::move( data ), is_last_substring } } );
      break;
    } else if ( start == it->first && end > it->second ) {
      // 更新, 扩大旧区间, 修改map和pending
      uint64_t diff_bytes = end - it->second;
      total_bytes_pending += diff_bytes;
      it->second = end;
      buffer_data[start] = { std::move( data ), buffer_data[start].second | is_last_substring };
      break;
    } else if ( start == it->first && end <= it->second )
      // 被包含在已有区间，不做处理
      return;
    else
      ++it;
  }
  if ( it == buffer_domains.end() ) {
    it = buffer_domains.insert( it, { start, end } );
    total_bytes_pending += data.length();
    buffer_data.insert( { start, { std::move(data), is_last_substring } } );
  }
  mergerBuffer( it, is_last_substring );
}

void Reassembler::mergerBuffer( list<pair<uint64_t, uint64_t>>::iterator& pos, const bool is_last )
{
  cout << "Call: mergeBuffer " << endl;
  auto pre = pos;
  // 从pre的节点开始向后检查重叠并合并
  if ( pos != buffer_domains.begin() )
    --pre;
  else
    ++pos;
  while ( pos != buffer_domains.end() ) {
    if ( pre->second >= pos->second ) {
      // 区间被包含, 删除当前区间
      // 修改pending, 删除插入的map
      total_bytes_pending -= buffer_data[pos->first].first.length();
      buffer_data.erase( pos->first );
      buffer_domains.erase( pos );
      break;
    } else if ( pre->second >= pos->first && pre->second < pos->second ) {
      // 与前一区间合并, 更新前区间，删除当前区间。
      // 修改前区间对应的map数据, 修改pending, 不需要插入map
      
      string joint_data = std::move( buffer_data[pos->first].first );
      buffer_data.erase( pos->first );

      bool last = buffer_data[pre->first].second | is_last;
      string new_data = std::move( buffer_data[pre->first].first );
      joint_data = joint_data.substr( pre->second - pos->first, pos->second - pre->second );
      new_data += joint_data;

      pre->second = pos->second;
      pos = buffer_domains.erase( pos ); // 删除原区间并移动迭代器

      buffer_data[pre->first].first = std::move( new_data );
      buffer_data[pre->first].second = last;
      total_bytes_pending += joint_data.length();
    } else
      ++pre, ++pos;
  }
}

void Reassembler::popValidDomains( Writer& output )
{
  cout << "call popValidDomains " << endl;
  // 删除无效区间
  while ( !buffer_domains.empty() && buffer_domains.front().second <= lower_bound ) {
    // 整个删除
    buffer_data.erase( buffer_domains.front().first );
    total_bytes_pending -= buffer_domains.front().second - buffer_domains.front().first;
    buffer_domains.pop_front();
  }

  while ( !buffer_domains.empty() && buffer_domains.front().first <= lower_bound ) {
    uint64_t start = buffer_domains.front().first;
    uint64_t end = buffer_domains.front().second; // [ )
    if ( buffer_data[start].first.length() < lower_bound - start )
      lower_bound = lower_bound;
    bool last = buffer_data[start].second;
    string to_send = std::move( buffer_data[start].first );
    // 处理要传输的字符串，存储的都是有效区间
    if ( lower_bound > start ) // 前部截断
      to_send = to_send.substr( lower_bound - start );

    pushToWriter( to_send, output, last );
    buffer_data.erase( start );
    total_bytes_pending -= end - start;
    buffer_domains.pop_front();
  }
}