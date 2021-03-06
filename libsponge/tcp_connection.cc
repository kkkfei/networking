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
    //cerr << "segment_received " << seg.header().ack;
    if(!_active) return;


    _time_since_last_segment = 0;

    if(!_connect) {
        if(!seg.header().syn) return;

        _sender.fill_window();
        _connect = true;
    }

    if(seg.header().rst)
    {
        abortConnect();
        return;
    }

    if(seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
    }

    if(seg.length_in_sequence_space() > 0)
        _receiver.segment_received(seg);

  
    if(_receiver.stream_out().input_ended() && _linger_after_streams_finish && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;

    if(_sender.segments_out().empty() && seg.length_in_sequence_space() > 0 && _receiver.ackno().has_value())
    {
        //send ack
        _sender.send_empty_segment();
    }
    trySendSegmet();

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
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _active = false; 
}

void TCPConnection::trySendSegmet()
{
    while(!_sender.segments_out().empty())
    {
        auto ackno = _receiver.ackno();
        auto& s = _sender.segments_out().front();

        if(ackno.has_value())
        {
            s.header().ack = true;
            s.header().ackno = *ackno;
            s.header().win = _receiver.window_size();
        }
        _segments_out.push(std::move(s));
        _sender.segments_out().pop();
    } 
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {

    if(data.size() == 0) return 0;
    size_t sz = _sender.stream_in().write(data);

    _sender.fill_window();
    trySendSegmet();

    return sz;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if(!_connect || !_active) return;
    _sender.tick(ms_since_last_tick); 
    _time_since_last_segment += ms_since_last_tick;

    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
    {
        sendRst();
        abortConnect();
        return;
    }

    trySendSegmet();
 

    if(_receiver.stream_out().eof() && _sender.stream_in().eof() 
        && _sender.bytes_in_flight() == 0 && _linger_after_streams_finish)
    {
         _linger_time += ms_since_last_tick;
        if(_linger_time >= 10 * _cfg.rt_timeout) {
            _linger_after_streams_finish = false;
            _active = false;
        } 
    }

    if(_receiver.stream_out().eof() && _sender.stream_in().eof() 
        && _sender.bytes_in_flight() == 0 && !_linger_after_streams_finish) 
        _active = false;

}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();

    _sender.fill_window();
    trySendSegmet();
}

void TCPConnection::connect() {
    _sender.fill_window();
    trySendSegmet();
    _connect = true;
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
