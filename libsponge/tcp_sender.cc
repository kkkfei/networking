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
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    TCPSegment tcp;
    uint64_t dataSize = 0;
    bool sendSeg = false;

    switch(_tcpState)
    {
        case TCPState::CLOSED:
        {
            tcp.header().seqno = _isn;
            tcp.header().syn = true;
            dataSize = 1;
            _tcpState = TCPState::SYN_SENT;
            sendSeg = true;
            break;
        }

        case TCPState::SYN_SENT:
            cout << "SYN_SENT";
            break;

        case TCPState::SYN_ACKED:
        {
            if(_stream.buffer_size() >= _window_size)
            {
                tcp.header().seqno = next_seqno();
                string s = _stream.read(size_t(_window_size)) ;
                dataSize = s.size();
                tcp.payload() = Buffer(std::move(s));
                sendSeg = true;
            }

            break;
        }

        case TCPState::FIN_SENT:
            break;

        case TCPState::FIN_ACKED:
            break;
    }



    if(sendSeg)
    {
            cout << int(_tcpState) << "[send]";
        _segments_out.push(tcp);
        _next_seqno =  _next_seqno + dataSize;
        _bytes_in_flight = dataSize;
    }

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {

    
     if(next_seqno() == ackno) {
        _window_size = min(window_size, uint16_t(TCPConfig::MAX_PAYLOAD_SIZE));
        _bytes_in_flight = 0;

        switch(_tcpState)
        {
            case TCPState::SYN_SENT:
            {
                _tcpState = TCPState::SYN_ACKED;
                break;
            }

            
            case TCPState::SYN_ACKED:
            {
                //std::cout << "[" << (next_seqno_absolute() > bytes_in_flight()) << "]" << endl;
                //fill_window();
                
                break;
            }


            case TCPState::FIN_SENT:
            {
                _tcpState = TCPState::FIN_ACKED;
                break;
            }

            case TCPState::CLOSED:
            case TCPState::FIN_ACKED:
                break;
        }

     }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { DUMMY_CODE(ms_since_last_tick); }

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}
