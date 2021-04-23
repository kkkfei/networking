#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
    return _sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight(); 
}

size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes(); 
}

size_t TCPConnection::time_since_last_segment_received() const { 
    return _time_since_last_segment; 
}

void TCPConnection::segment_received(const TCPSegment &seg) { 
    if(seg.header().rst)
    {
        abortConnect();
        return;
    }

    _time_since_last_segment = 0;
    if(seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
    }
    _receiver.segment_received(seg);

    bool hasSend = false;
    auto ackno = _receiver.ackno();
    while(!_sender.segments_out().empty())
    {
        auto& s = _sender.segments_out().front();

        if(ackno.has_value())
        {
            s.header().ack = true;
            s.header().ackno = *ackno;
            s.header().win = _receiver.window_size();
        }
        _segments_out.push(s);
        _sender.segments_out().pop();
        hasSend = true;
    } 
    if(!hasSend && ackno.has_value()){
        TCPSegment s;
        s.header().ack = true;
        s.header().ackno = *ackno;
        s.header().win = _receiver.window_size();
        _segments_out.push(s);
    }

    if(_receiver.stream_out().eof() && _linger_after_streams_finish && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;
}

void TCPConnection::sendRst()
{
    TCPSegment segment;
    segment.header().rst = true;
    
    auto ackno = _receiver.ackno();
    if(ackno.has_value())
    {
        segment.header().ack = true;
        segment.header().ackno = *ackno;
        segment.header().win = _receiver.window_size();
    }
    _segments_out.push(segment);
}

void TCPConnection::abortConnect()
{
    _receiver.stream_out().error();
    _sender.stream_in().error();
    _active = false; 
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    return _sender.stream_in().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _sender.tick(ms_since_last_tick); 
    _time_since_last_segment += ms_since_last_tick;

    if(_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS)
    {
        sendRst();
        abortConnect();
        return;
    }

    _sender.fill_window();
    while(!_sender.segments_out().empty())
    {
        auto& s = _sender.segments_out().front();
        auto ackno = _receiver.ackno();

        if(ackno.has_value())
        {
            s.header().ack = true;
            s.header().ackno = *ackno;
            s.header().win = _receiver.window_size();
        }
        _segments_out.push(s);
        _sender.segments_out().pop();
    } 
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().input_ended();
}

void TCPConnection::connect() {
    _sender.fill_window();
    TCPSegment segment = _sender.segments_out().front();
    _sender.segments_out().pop();
    _segments_out.push(segment);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            sendRst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
