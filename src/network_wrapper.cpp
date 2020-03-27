//
// Created by nni on 22.11.18.
//
#define LOG_TAG "NetworkWrapper"
#include <generic_log.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "network_wrapper.h"
#include <errno.h>
#include <array>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>

constexpr size_t NetworkWrapper::_max_udp_frame_size;

NetworkWrapper::NetworkWrapper()
{
    //_multicast_adress = std::string("239.0.0.1");

    //_multicast_adress = std::string("224.0.0.199");
    //_multicast_adress = std::string("239.0.0.190");

    //connect();

}

NetworkWrapper::~NetworkWrapper()
{

}

void NetworkWrapper::set(const std::string & multicast_ip, const int port)
{
    _multicast_adress = multicast_ip;
    _port = port;
}

bool NetworkWrapper::connect()
{
    if(initListner())
    {
        //_multicast_adress = std::string("239.255.255.250");
        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "successfully initialized listener");
        LOGD("successfully initialized listener");

        //startReaderThread();

        if(initSender())
        {
           // __android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "successfully initialized sender");
            LOGD("successfully initialized sender");

        } else {
           // __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed to initialize sender");
            LOGE("failed to initialize sender");
            return false;
        }

    } else {

       // __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed to initialize listener");
        LOGE("failed to initialize listener");

        return false;

    }

    return true;
}

bool NetworkWrapper::initListner()
{
    if(_port < 0)
    {
        LOGE("invalid port or port not set");
        return false;
    }

    if(_multicast_adress.empty())
    {
        LOGE("multicast ip not set");

        return false;
    }

    // create what looks like an ordinary UDP socket
    //
    _listner_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (_listner_fd < 0)
    {
        //__android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed to create listener socket");
        LOGE("failed to create listener socket");

        _receiver_connected = false;
        return false;
    } else {
        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "successfully created listener socket");
        LOGD("successfully created listener socket");
    }

    // allow multiple sockets to use the same PORT number
    //
    uint yes = 1;
    if (setsockopt( _listner_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &yes, sizeof(yes)) < 0)
    {
        // "Reusing ADDR failed"
        //__android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "listener address reuse failed");
        LOGE("listener address reuse failed");

        _receiver_connected = false;
        return false;
    } else {

        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "successfully set address reuse");
        LOGD("successfully set address reuse");

    }

    // set up destination address
    memset(&_destination_addr, 0, sizeof(_destination_addr));
    _destination_addr.sin_family = AF_INET;
    _destination_addr.sin_addr.s_addr = htonl(INADDR_ANY); // differs from sender



    _destination_addr.sin_port = htons(_port);

    // bind to receive address
    //
    int error_code = bind(_listner_fd, (struct sockaddr*) &_destination_addr, sizeof(_destination_addr));

    if (error_code < 0)
    {

        //__android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "listener failed to bind socket with code %d = %s", errno, strerror(errno));
        LOGE("listener failed to bind socket with code %d = %s", errno, strerror(errno));

        _receiver_connected = false;
        return false;
    } else {

       // __android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "listener binded socket");
        LOGD("listener binded socket");

    }

    // use setsockopt() to request that the kernel join a multicast group
    //


    _mreq.imr_multiaddr.s_addr = inet_addr(_multicast_adress.c_str());
    _mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if ( setsockopt( _listner_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &_mreq, sizeof(_mreq)) < 0)
    {
        //__android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed listner setsockopt udp multicast membership");
        LOGE("failed listner setsockopt udp multicast membership");
        _receiver_connected = false;
        return false;
    } else {

       // __android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "listener setsockopt set joined udp multicast group");
        LOGD("listener setsockopt set joined udp multicast group");
       // __android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "multicast ip: %s", inet_ntoa(_mreq.imr_multiaddr));
        LOGD("multicast ip: %s", inet_ntoa(_mreq.imr_multiaddr));

        struct ifaddrs * ifAddrStruct=NULL;
        struct ifaddrs * ifa=NULL;
        void * tmpAddrPtr=NULL;

        getifaddrs(&ifAddrStruct);

        for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }
            if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
                // is a valid IP4 Address
                tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                //printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
               // __android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "%s IP Address %s\n", ifa->ifa_name, addressBuffer);
                LOGD("%s IP Address %s\n", ifa->ifa_name, addressBuffer);

                if(0 != strcmp(ifa->ifa_name, "lo"))
                {
                    _my_ip.sin_addr = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                   // __android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "setting this as my outgoing ip-interface: IP Address %s\n", inet_ntoa(_my_ip.sin_addr));
                    LOGD("setting this as my outgoing ip-interface: IP Address %s\n", inet_ntoa(_my_ip.sin_addr));
                    break;
                }
            }
        }
        if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);

    }

    _receiver_connected = true;

    return true;
}

bool NetworkWrapper::initSender()
{
// create what looks like an ordinary UDP socket
    //

    if(_port < 0)
    {
        LOGE("invalid port or port not set");
        return false;
    }

    if(_multicast_adress.empty())
    {
        LOGE("multicast ip not set");

        return false;
    }

    _sender_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (_sender_fd < 0)
    {
     //   __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed to create sender socket with error code %d = %s", errno, strerror(errno));
        LOGE("failed to create sender socket with error code %d = %s", errno, strerror(errno));
        _sender_connected = false;
        return false;
    } else {
        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "successfully created sender socket");
        LOGD("successfully created sender socket");
    }

    memset(&_multicast_sockaddr, 0, sizeof(_multicast_sockaddr));
    _multicast_sockaddr.sin_family = AF_INET;
    _multicast_sockaddr.sin_addr.s_addr =  inet_addr(_multicast_adress.c_str());//_mreq.imr_multiaddr.s_addr;
    _multicast_sockaddr.sin_port = htons(_port);//_destination_addr.sin_port;

    _sender_connected = true;

    return true;
}

void NetworkWrapper::readPrintLoop()
{
    struct sockaddr_in source_addr;
    socklen_t addrlen = sizeof(source_addr);
    char msgbuf[_max_udp_frame_size];

    while(true)
    {
        int nbytes = recvfrom( _listner_fd, msgbuf, _max_udp_frame_size, 0, (struct sockaddr *) &source_addr, &addrlen);

        if (nbytes < 0)
        {
          //  __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed rescfrom call, returned code: %d", nbytes);
            LOGE("failed rescfrom call, returned code: %d", nbytes);
        } else {
            //todo: maybe we should lock
           // __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "reveived %d bytes of data", nbytes);
           LOGD("failed rescfrom call, returned code: %d", nbytes);
            std::lock_guard<std::mutex> lock(_ring_buffer_mutex);
            _output_audio_ring_buffer.writeNewestDataToBuffer(msgbuf, nbytes);
        }

        //msgbuf[nbytes] = '\0';
    }

}

std::pair<size_t, sockaddr_in> NetworkWrapper::receiveData(char * data, size_t max_size)
{
    struct sockaddr_in source_addr;
    socklen_t addrlen = sizeof(source_addr);

    if(!_receiver_connected)
        return std::make_pair(0, source_addr);

    size_t total_bytes_received = 0;
    do
    {
        int nbytes = recvfrom( _listner_fd, data+total_bytes_received, max_size-total_bytes_received, 0, (struct sockaddr *) &source_addr, &addrlen);

        if (nbytes < 0)
        {
           // __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed rescvfrom call, returned code: %d", nbytes);
            LOGE("failed rescvfrom call, returned code: %d", nbytes);

            return std::make_pair(0, source_addr);
        } else {
            total_bytes_received += static_cast<size_t > (nbytes);
        }

    } while (isDataReceived() && max_size >= total_bytes_received); //todo: also check that data is from same source!


    // source is in network byte order

    if(_receive_data_from_yourself || (source_addr.sin_addr.s_addr != _my_ip.sin_addr.s_addr))
    {
        //char addressBuffer[INET_ADDRSTRLEN];
        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "received data from ip %s\n", inet_ntop(AF_INET, &source_addr, addressBuffer, INET_ADDRSTRLEN));
        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "received data from ip %s\n", inet_ntoa(source_addr.sin_addr));
        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "my ip is: %s\n", inet_ntoa(_my_ip.sin_addr));
        return std::make_pair(total_bytes_received, source_addr);
    }
    else {
        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "received data from yourself - throwing data away");
        return std::make_pair(0, source_addr);
    }

    /*
     *
    struct sockaddr_in source_addr;
    socklen_t addrlen = sizeof(source_addr);


        int nbytes = recvfrom( _listner_fd, data, max_size, 0, (struct sockaddr *) &source_addr, &addrlen);

        if (nbytes < 0)
        {
            __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed rescvfrom call, returned code: %d", nbytes);

            return 0;
        }

    return static_cast<size_t >(nbytes);*/
}

void NetworkWrapper::startReaderThread()
{
    if(_reader_thread == nullptr)
    {
        //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "starting reader thread");
        LOGD("starting reader thread");
        _reader_thread = new std::thread(&NetworkWrapper::readPrintLoop, this);
    } else {

       // __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "cannot initialize new thread as reader thread is already initialized");
        LOGE("cannot initialize new thread as reader thread is already initialized");

    }
}

bool NetworkWrapper::send(const char * data, size_t size_of_data)
{
    if(!_sender_connected)
    {
//        __android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "No sender socket connected - failed sending data");
        LOGE("No sender socket connected - failed sending data");

        return false;
    }


    size_t total_bytes_send = 0;

    while(total_bytes_send < size_of_data)
    {
        int nbytes = sendto(_sender_fd, data+total_bytes_send, size_of_data-total_bytes_send, 0, (struct sockaddr*) &_multicast_sockaddr, sizeof(_multicast_sockaddr));

        if (nbytes < 0)
        {
            //__android_log_print(ANDROID_LOG_ERROR, "NetworkWrapper", "failed sending data, error code %d = %s", errno, strerror(errno));
            LOGE("failed sending data, error code %d = %s", errno, strerror(errno));
            return false;
        } else {

            total_bytes_send += static_cast<size_t >(nbytes);

            //__android_log_print(ANDROID_LOG_DEBUG, "NetworkWrapper", "successfully send: %.*s", nbytes, data);
        }
    }


    return true;
}

bool NetworkWrapper::isDataReceived()
{
    struct timeval tv;

    fd_set temp_fd_set;

    FD_ZERO(&temp_fd_set);
    FD_SET(_listner_fd,&temp_fd_set);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    int retval = select(_listner_fd+1, &temp_fd_set, NULL, NULL, &tv);

    if ( retval > 0){
        return true;
    }

    return false;
}