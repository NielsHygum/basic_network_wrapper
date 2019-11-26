//
// Created by nni on 22.11.18.
//

#pragma once

#include <string>

#include <netinet/in.h>
#include <future>
#include <basic_ring_buffer.hpp>

class NetworkWrapper {

private:

    std::string _multicast_adress;

    struct sockaddr_in _destination_addr;
    struct ip_mreq _mreq;

    struct sockaddr_in _multicast_sockaddr;

    struct sockaddr_in _my_ip;


    int _listner_fd;
    int _sender_fd;
    int _port = 6000;

    bool initListner();
    bool initSender();

    std::thread * _reader_thread = nullptr;

    void readPrintLoop();
    void startReaderThread();


    bool _receive_data_from_yourself = false;

    bool connect();
    bool _receiver_connected = false;
    bool _sender_connected = false;

public:

    static constexpr size_t _max_udp_frame_size = 1500;

    std::mutex _ring_buffer_mutex;
    RingBuffer<1024*1024> _output_audio_ring_buffer;
    bool send(const char * data, size_t size_of_data);
    std::pair<size_t, sockaddr_in> receiveData(char * data, size_t max_size);

    bool isDataReceived(); //non-blocking call

    NetworkWrapper();
    ~NetworkWrapper();
};

