#pragma once

/**
 * @file uv_amqp_handler.h
 * @brief Custom AMQP::ConnectionHandler bridging AMQP-CPP raw mode to libuv TCP.
 *
 * Windows/MinGW cannot use AMQP-CPP's built-in TcpHandler (depends on linux_tcp).
 * This handler provides cross-platform TCP via libuv + heartbeat timer.
 *
 * Usage:
 *   uv_loop_t loop;
 *   uv_loop_init(&loop);
 *   UvAmqpHandler handler(&loop);
 *   handler.setReadyCallback([&]() { ... });
 *   handler.setErrorCallback([&](const char* msg) { ... });
 *   handler.connect("localhost", 5672);
 *   AMQP::Connection connection(&handler, AMQP::Login("guest","guest"), "/");
 *   handler.setConnection(&connection);
 *   uv_run(&loop, UV_RUN_DEFAULT);
 */

#include <amqpcpp.h>
#include <atomic>
#include <functional>
#include <iostream>
#include <new>
#include <string>
#include <uv.h>
#include <vector>

class UvAmqpHandler : public AMQP::ConnectionHandler {
    public:
        using ReadyCallback = std::function<void()>;
        using ErrorCallback = std::function<void(const char* message)>;
        using ClosedCallback = std::function<void()>;

        explicit UvAmqpHandler(uv_loop_t* loop)
            : loop_(loop) {
            uv_timer_init(loop_, &heartbeat_timer_);
            heartbeat_timer_.data = this;
        }

        ~UvAmqpHandler() override = default;

        void setConnection(AMQP::Connection* conn) { connection_ = conn; }
        void setReadyCallback(ReadyCallback cb) { ready_cb_ = std::move(cb); }
        void setErrorCallback(ErrorCallback cb) { error_cb_ = std::move(cb); }
        void setClosedCallback(ClosedCallback cb) { closed_cb_ = std::move(cb); }

        /// Initiate async TCP connection to host:port (DNS resolved via libuv)
        void connect(const std::string& host, int port) {
            host_ = host;
            port_ = port;

            auto* req = new uv_getaddrinfo_t;
            req->data = this;

            struct addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

            int r = uv_getaddrinfo(
                loop_,
                req,
                onResolved,
                host.c_str(),
                std::to_string(port).c_str(),
                &hints
            );
            if(r != 0) {
                delete req;
                std::string msg = "DNS resolve failed: " + std::string(uv_strerror(r));
                if(error_cb_) error_cb_(msg.c_str());
            }
        }

        /// Gracefully close the TCP connection
        void close() {
            uv_timer_stop(&heartbeat_timer_);
            read_buffer_.clear();
            if(!tcp_initialized_) return;

            if(!uv_is_closing(reinterpret_cast<uv_handle_t*>(&tcp_handle_))) {
                uv_close(reinterpret_cast<uv_handle_t*>(&tcp_handle_), onClose);
            }
        }

        /// Close heartbeat timer + TCP (call before uv_loop_close)
        void shutdown() {
            shutting_down_ = true;
            uv_timer_stop(&heartbeat_timer_);
            if(!uv_is_closing(reinterpret_cast<uv_handle_t*>(&heartbeat_timer_))) {
                uv_close(reinterpret_cast<uv_handle_t*>(&heartbeat_timer_), nullptr);
            }
            if(tcp_initialized_ && !uv_is_closing(reinterpret_cast<uv_handle_t*>(&tcp_handle_))) {
                uv_close(reinterpret_cast<uv_handle_t*>(&tcp_handle_), nullptr);
            }
        }

    protected:
        // --- AMQP::ConnectionHandler overrides ---

        /// Called by AMQP-CPP when it wants to send data to the broker
        void onData(AMQP::Connection* connection, const char* data, size_t size) override {
            if(shutting_down_) return;
            if(!connected_) {
                output_buffer_.insert(output_buffer_.end(), data, data + size);
                return;
            }
            doWrite(data, size);
        }

        /// AMQP connection is ready (handshake complete)
        void onReady(AMQP::Connection* connection) override {
            std::cout << "[UvAmqp] Connection ready\n";

            // Start heartbeat timer (60s interval)
            uv_timer_start(&heartbeat_timer_, onHeartbeat, 30000, 30000);

            if(ready_cb_) ready_cb_();
        }

        /// AMQP connection error
        void onError(AMQP::Connection* connection, const char* message) override {
            std::cerr << "[UvAmqp] Connection error: " << message << "\n";
            if(error_cb_) error_cb_(message);
        }

        /// AMQP connection gracefully closed
        void onClosed(AMQP::Connection* connection) override {
            std::cout << "[UvAmqp] Connection closed\n";
            uv_timer_stop(&heartbeat_timer_);
            if(closed_cb_) closed_cb_();
        }

    private:
        uv_loop_t* loop_;
        uv_tcp_t tcp_handle_{};
        uv_timer_t heartbeat_timer_{};
        AMQP::Connection* connection_ = nullptr;
        bool tcp_initialized_ = false;
        bool connected_ = false;
        std::atomic<bool> shutting_down_{false};

        std::string host_;
        int port_ = 0;

        std::vector<char> output_buffer_; // buffered data before TCP connect
        std::vector<char> read_buffer_;   // buffered partial AMQP frames between TCP reads

        ReadyCallback ready_cb_;
        ErrorCallback error_cb_;
        ClosedCallback closed_cb_;

        // --- Write helper ---
        struct WriteReq {
            uv_write_t req;
            std::vector<char> data;
        };

        void doWrite(const char* data, size_t size) {
            auto* wr = new WriteReq;
            wr->data.assign(data, data + size);
            wr->req.data = wr;

            uv_buf_t buf = uv_buf_init(wr->data.data(), static_cast<unsigned int>(wr->data.size()));
            int r = uv_write(
                &wr->req,
                reinterpret_cast<uv_stream_t*>(&tcp_handle_),
                &buf,
                1,
                onWrite
            );
            if(r != 0) {
                std::cerr << "[UvAmqp] Write error: " << uv_strerror(r) << "\n";
                delete wr;
            }
        }

        void flushOutputBuffer() {
            if(output_buffer_.empty()) return;
            doWrite(output_buffer_.data(), output_buffer_.size());
            output_buffer_.clear();
        }

        // --- libuv static callbacks ---

        static void onResolved(uv_getaddrinfo_t* req, int status, addrinfo* res) {
            auto* self = static_cast<UvAmqpHandler*>(req->data);
            delete req;

            if(status != 0) {
                uv_freeaddrinfo(res);
                std::string msg = "DNS resolve failed: " + std::string(uv_strerror(status));
                if(self->error_cb_) self->error_cb_(msg.c_str());
                return;
            }

            // Initialize TCP handle
            uv_tcp_init(self->loop_, &self->tcp_handle_);
            self->tcp_handle_.data = self;
            self->tcp_initialized_ = true;

            // Connect
            self->connect_req_.data = self;
            int r = uv_tcp_connect(
                &self->connect_req_,
                &self->tcp_handle_,
                res->ai_addr,
                onTcpConnect
            );

            uv_freeaddrinfo(res);

            if(r != 0) {
                std::string msg = "TCP connect failed: " + std::string(uv_strerror(r));
                if(self->error_cb_) self->error_cb_(msg.c_str());
            }
        }

        static void onTcpConnect(uv_connect_t* req, int status) {
            auto* self = static_cast<UvAmqpHandler*>(req->data);

            if(status != 0) {
                std::string msg = "TCP connect failed: " + std::string(uv_strerror(status));
                std::cerr << "[UvAmqp] " << msg << "\n";
                if(self->error_cb_) self->error_cb_(msg.c_str());
                return;
            }

            std::cout << "[UvAmqp] TCP connected to " << self->host_ << ":" << self->port_ << "\n";
            self->connected_ = true;

            // Flush any buffered AMQP protocol data
            self->flushOutputBuffer();

            // Start reading from socket
            int r = uv_read_start(
                reinterpret_cast<uv_stream_t*>(&self->tcp_handle_),
                allocBuffer,
                onRead
            );
            if(r != 0) {
                std::string msg = "Read start failed: " + std::string(uv_strerror(r));
                if(self->error_cb_) self->error_cb_(msg.c_str());
            }
        }

        static void allocBuffer(uv_handle_t*, size_t suggested, uv_buf_t* buf) {
            buf->base = new(std::nothrow) char[suggested];
            buf->len = buf->base ? static_cast<unsigned int>(suggested) : 0;
        }

        static void onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
            auto* self = static_cast<UvAmqpHandler*>(stream->data);

            if(nread > 0 && self->connection_) {
                // Append new TCP data to the read buffer (may contain partial AMQP frames)
                self->read_buffer_.insert(
                    self->read_buffer_.end(),
                    buf->base, buf->base + nread
                );

                // Parse as many complete frames as possible
                size_t parsed = self->connection_->parse(
                    self->read_buffer_.data(), self->read_buffer_.size()
                );

                if(parsed > 0) {
                    // Remove consumed bytes, keep any partial frame for next read
                    self->read_buffer_.erase(
                        self->read_buffer_.begin(),
                        self->read_buffer_.begin() + static_cast<std::ptrdiff_t>(parsed)
                    );
                } else if(!self->read_buffer_.empty()) {
                    // parse() returned 0: either need more data (partial frame)
                    // or connection entered error state (onError already called).
                    // If the connection is no longer usable, clear the buffer and close.
                    if(!self->connection_->usable()) {
                        self->read_buffer_.clear();
                        self->close();
                    }
                    // Otherwise: partial frame, wait for more TCP data.
                }
            } else if(nread < 0) {
                if(nread != UV_EOF) {
                    std::cerr << "[UvAmqp] Read error: " << uv_strerror(static_cast<int>(nread)) << "\n";
                }
                self->read_buffer_.clear();
                self->connected_ = false;
                std::string msg = "Connection lost";
                if(self->error_cb_) self->error_cb_(msg.c_str());
            }

            delete[] buf->base;
        }

        static void onWrite(uv_write_t* req, int status) {
            auto* wr = static_cast<WriteReq*>(req->data);
            if(status != 0) {
                std::cerr << "[UvAmqp] Write callback error: " << uv_strerror(status) << "\n";
            }
            delete wr;
        }

        static void onHeartbeat(uv_timer_t* timer) {
            auto* self = static_cast<UvAmqpHandler*>(timer->data);
            if(self->connection_ && self->connected_) {
                self->connection_->heartbeat();
            }
        }

        static void onClose(uv_handle_t* handle) {
            // TCP handle closed — nothing to free, it's embedded in UvAmqpHandler
        }

        uv_connect_t connect_req_{};
};
