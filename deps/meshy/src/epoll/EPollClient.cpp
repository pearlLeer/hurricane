/**
 * licensed to the apache software foundation (asf) under one
 * or more contributor license agreements.  see the notice file
 * distributed with this work for additional information
 * regarding copyright ownership.  the asf licenses this file
 * to you under the apache license, version 2.0 (the
 * "license"); you may not use this file except in compliance
 * with the license.  you may obtain a copy of the license at
 *
 * http://www.apache.org/licenses/license-2.0
 *
 * unless required by applicable law or agreed to in writing, software
 * distributed under the license is distributed on an "as is" basis,
 * without warranties or conditions of any kind, either express or implied.
 * see the license for the specific language governing permissions and
 * limitations under the license.
 */

#include "epoll/EPollClient.h"
#include "utils/CommonUtils.h"
#include "epoll/EPollLoop.h"

#include <unistd.h>
#include <logging/Logging.h>
#include <epoll/EPollLoop.h>

namespace meshy {
    void EPollClient::Connect(const std::string& host, int32_t port) {
        struct sockaddr_in serv_addr;

        bzero((char*)&serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(host.c_str());
        serv_addr.sin_port = htons(port);

        meshy::SetNonBlocking(GetNativeSocket());

        connect(GetNativeSocket(), (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    }

    EPollClientPtr EPollClient::Connect(const std::string& ip, int32_t port, DataSink* dataSink) {
        int32_t clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        // Connect
        EPollClientPtr client = EPollClientPtr(new EPollClient(clientSocket));
        client->SetDataSink(dataSink);
        client->Connect(ip, port);

        // TODO: Add to epoll loop
        EPollLoop* ePollLoop = EPollLoop::Get();

        client->_events = EPOLLIN | EPOLLET;
        if ( ePollLoop->AddEpollEvents(client->_events, clientSocket) == -1 ) {
            perror("epoll_ctl: add");
            exit(EXIT_FAILURE);
        }

        ePollLoop->AddStream(client);

        return client;
    }

    int32_t EPollClient::Receive(char* buffer, int32_t bufferSize, int32_t& readSize) {
        readSize = 0;
        int32_t nread = 0;
        NativeSocketEvent ev;

        while ((nread = read(GetNativeSocket(), buffer + readSize, bufferSize - 1)) > 0) {
            readSize += nread;
        }

        return nread;
    }

    int32_t EPollClient::Send(const meshy::ByteArray& byteArray) {
        LOG(LOG_DEBUG) << "EPollConnection::Send";

        struct epoll_event ev;
        NativeSocket clientSocket = GetNativeSocket();

        if ( EPollLoop::Get()->ModifyEpollEvents(_events | EPOLLOUT, clientSocket) ) {
            // TODO: MARK ERASE
            LOG(LOG_ERROR) << "FATAL epoll_ctl: mod failed!";
        }

        const char* buf = byteArray.data();
        int32_t size = byteArray.size();
        int32_t n = size;

        while (n > 0) {
            int32_t nwrite;
            nwrite = write(clientSocket, buf + size - n, n);
            if (nwrite < n) {
                if (nwrite == -1 && errno != EAGAIN) {
                    LOG(LOG_ERROR) << "FATAL write data to peer failed!";
                }
                break;
            }
            n -= nwrite;
        }

        return 0;
    }
}
