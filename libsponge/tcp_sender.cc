#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _RTO(retx_timeout){}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    TCPSegment tcp;
    uint64_t dataSize = 0;

    switch(_tcpState)
    {
        case TCPState::CLOSED:
        {
            tcp.header().seqno = _isn;
            tcp.header().syn = true;
            dataSize = 1;
            _tcpState = TCPState::SYN_SENT;
            _check_point = _isn.raw_value();
            break;
        }

        case TCPState::SYN_ACKED:
        {
            uint16_t win_sz = _window_size == 0 ? 1 : _window_size;
            if(win_sz <= _bytes_in_flight) return;

            if(_stream.eof()) 
            {
                tcp.header().seqno = next_seqno();
                tcp.header().fin = true;
                dataSize = 1;
                _tcpState = TCPState::FIN_SENT;

            } else {
                //if(_window_size <= _bytes_in_flight || _stream.eof()) return;

                size_t sz = min(TCPConfig::MAX_PAYLOAD_SIZE, win_sz - _bytes_in_flight);
                string s = _stream.read(size_t(sz)) ;
                dataSize = s.size();
                if(dataSize == 0) return;

                tcp.payload() = Buffer(std::move(s));
                tcp.header().seqno = next_seqno();
                if(_stream.eof() && dataSize < win_sz - _bytes_in_flight) {
                    tcp.header().fin = true;
                    dataSize += 1;
                    _tcpState = TCPState::FIN_SENT;
                }
                _check_point = unwrap(tcp.header().seqno, _isn, _check_point);
            }

            break;
        }

        case TCPState::SYN_SENT:
        case TCPState::FIN_SENT:
        case TCPState::FIN_ACKED:
            return;
    }


    _segments_out.push(tcp);
    _next_seqno =  _next_seqno + dataSize;
    _bytes_in_flight += dataSize;

    OutstandingSegment os;
    os.segment = tcp;
    os.expTime = time + size_t(_RTO);
    os.nobackoff = _window_size == 0;

    q.push_back(std::move(os));

    if(_window_size > _bytes_in_flight && _stream.buffer_size() > 0)
        fill_window();
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {

    uint64_t ackno_ab = unwrap(ackno, _isn, _check_point);

    switch(_tcpState)
    {
        case TCPState::SYN_SENT:
        {
            if(ackno != next_seqno()) return;

            _window_left = ackno_ab;
            _window_size = window_size;
            q.pop_front();
            _bytes_in_flight = 0;

            _tcpState = TCPState::SYN_ACKED;
            break;
        }

        case TCPState::FIN_SENT:
        case TCPState::SYN_ACKED:
        {
            if(ackno_ab >= _window_left) _window_size = window_size;
            if(_window_left >= ackno_ab) return;

            _window_left = ackno_ab;

            while(!q.empty())
            {
                OutstandingSegment& os = q.front();
                uint64_t seqno = unwrap(os.segment.header().seqno, _isn, _check_point); 
                
                if(seqno + uint64_t(os.segment.length_in_sequence_space()) <= ackno_ab)
                {
                    _bytes_in_flight -=  uint64_t(os.segment.length_in_sequence_space());
                    q.pop_front();
                } else {
                    break;
                }
            }
            _consecutive_tx = 0;
            _RTO = _initial_retransmission_timeout;

            if(_tcpState == TCPState::FIN_SENT && _bytes_in_flight == 0) 
                _tcpState = TCPState::FIN_ACKED;

            if(!q.empty())
            {
                OutstandingSegment& os = q.front();
 
                os.expTime = time + size_t(_RTO) ;
 
            }
            // if(_stream.eof() && _bytes_in_flight == 0)
            //     fill_window();
            break;
        }

        case TCPState::CLOSED:
        case TCPState::FIN_ACKED:
            break;
    }

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    time += ms_since_last_tick;


    if(!q.empty())
    {
        OutstandingSegment& os = q.front();

        if(os.expTime <= time)
        {
            //retx
            _segments_out.push(os.segment);
            _consecutive_tx += 1;
            if(!os.nobackoff) _RTO *= 2;
            os.expTime = time + _RTO ;
        }
    }

    // for(auto ite = q.begin(); ite < q.end(); ++ite)
    // {
    //     OutstandingSegment& os = *ite;
    //     if(os.sendTime + size_t(_RTO) <= time)
    //     {
    //         //retx
    //         _segments_out.push(os.segment);
    //         os.sendTime = time;
    //         _consecutive_tx += 1;
    //         _RTO *= 2;
    //     }
    // }
 }

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_tx; }

void TCPSender::send_empty_segment() {
    _segments_out.push(TCPSegment{});
}
