#include "reassembler.hh"
#include <iostream>

using namespace std;

/* Class Reassembler functions */
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  cout << "call insert: " << data << endl; // debug
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
  // printBufferDomains();
  popValidDomains( output );
  updateBounds( output );
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
  return ( lower_bound == upper_bound ) || ( first_index >= upper_bound )
         || ( first_index + data.length() - 1 < lower_bound );
}

bool Reassembler::sendNow( const uint64_t first_index, string& data )
{
  cout << "call sendNow " << endl;
  if ( data.empty() )
    return true;
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
  // [start, end]
  uint64_t start = first_index;
  uint64_t end = start + data.length() - 1;

  // 空直接插入
  if ( buffer_domains.empty() ) {
    buffer_domains.insert( buffer_domains.end(), { start, end } );
    buffer_data.insert( { start, { std::move( data ), is_last_substring } } );
    return;
  }

  // 非空插入: 查找插入位置
  auto it = buffer_domains.begin();
  auto pos = buffer_domains.end(); 
  bool need_merge = false;
  while ( it != buffer_domains.end() ) {
    if ( start <= it->first ) {
      pos = buffer_domains.insert( it, { start, end } );
      break;
    }
    ++it;
  }

  if ( it == buffer_domains.end() )
    pos = buffer_domains.insert( it, { start, end } );

  mergerBuffer( pos, data, is_last_substring ); // 也负责修改map
}

void Reassembler::mergerBuffer( list<pair<uint64_t, uint64_t>>::iterator& pos, string& data, const bool is_last )
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
      // 不修改pending, 不需要插入map
      buffer_domains.erase( pos );
      break;
    } else if ( pre->second >= pos->first && pre->second < pos->second ) {
      // 与前一区间合并, 更新前区间，删除当前区间。
      // 修改前区间对应的map数据, 修改pengding, 不需要插入map
      bool last = buffer_data[pre->first].second | is_last;
      string new_data = std::move( buffer_data[pre->first].first );
      string joint_data = data.substr( pre->second - pos->first + 1, pos->second - pre->second );
      new_data += joint_data;

      pre->second = pos->second;
      pos = buffer_domains.erase( pos );

      buffer_data[pre->first].first = std::move( new_data );
      buffer_data[pre->first].second = last;
      total_bytes_pending += joint_data.length();
    }
  }
}

void Reassembler::popValidDomains( Writer& output )
{
  cout << "call popValidDomains " << endl;

  // 删除无效区间
  while ( !buffer_domains.empty() && buffer_domains.front().second < lower_bound ) {
    // 整个删除
    buffer_data.erase( buffer_domains.front().first );
    total_bytes_pending -= buffer_domains.front().second - buffer_domains.front().first + 1;
    buffer_domains.pop_front();
  }

  // 发送有效区间
  while ( !buffer_domains.empty() && buffer_domains.front().first <= lower_bound ) {
    auto& first_domain = buffer_domains.front(); // [ ]

    bool last = buffer_data[first_domain.first].second;
    string to_send = std::move( buffer_data[first_domain.first].first );
    uint64_t send_length = first_domain.second - lower_bound + 1;
    // 处理要传输的字符串，存储的都是有效区间
    if ( lower_bound > first_domain.first ) // 裁小
      to_send = to_send.substr( lower_bound - first_domain.first, send_length );

    pushToWriter( to_send, output, last );
    buffer_data.erase( first_domain.first );
    total_bytes_pending -= send_length;

    buffer_domains.pop_front();
  }
}

void Reassembler::printBufferDomains()
{
  // debug: 打印容器内容
  for ( auto it = buffer_domains.begin(); it != buffer_domains.end(); ++it ) {
    auto next = it;
    ++next;
    cout << "[" << it->first << " , " << it->second << "]";
    if ( next != buffer_domains.end() )
      cout << " -> ";
    else
      cout << endl;
  }
}
