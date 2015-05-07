/*-
 * This file is part of Libight <https://libight.github.io/>.
 *
 * Libight is free software. See AUTHORS and LICENSE for more
 * information on the copying conditions.
 *
 * =========================================================================
 *
 * Portions Copyright (c) 2015, Adriano Faggiani, Enrico Gregori,
 * Luciano Lenzini, Valerio Luconi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef IGHT_PROTOCOLS_TRACEROUTE_ANDROID_HPP
#define IGHT_PROTOCOLS_TRACEROUTE_ANDROID_HPP

/// Android implementation of traceroute interface

// This is meant to run on Android but can run on all Linux systems
#ifdef __linux__

#include <ight/common/constraints.hpp>
#include <ight/protocols/traceroute_interface.hpp>

#include <event2/event.h>

#include <time.h>

struct sock_extended_err; // Forward declaration

namespace ight {
namespace protocols {
namespace traceroute {

using namespace ight::common::constraints;

/// Traceroute prober for Android
class AndroidProber : public NonCopyable,
                      public NonMovable,
                      public ProberInterface {

  private:
    int sockfd = -1;              ///< socket descr
    bool probe_pending = false;   ///< probe is pending
    timespec start_time{0, 0};    ///< start time
    bool use_ipv4 = true;         ///< using IPv4?
    event_base *evbase = nullptr; ///< event base
    event *evp = nullptr;         ///< event pointer

    std::function<void(ProbeResult)> result_cb;       ///< on result callback
    std::function<void()> timeout_cb;                 ///< on timeout callback
    std::function<void(std::runtime_error)> error_cb; ///< on error callback

    /// Returns the source address of the error message.
    /// \param use_ipv4 whether we are using IPv4
    /// \param err socket error structure
    /// \return source address of the error message
    static std::string get_source_addr(bool use_ipv4, sock_extended_err *err);

    /// Returns the Round Trip Time value in milliseconds
    /// \param end ICMP reply arrival time
    /// \param start UDP probe send time
    /// \return RTT value in milliseconds
    static double calculate_rtt(struct timespec end, struct timespec start);

    /// Returns the Time to Live of the error message
    /// \param data CMSG_DATA(cmsg)
    /// \return ttl of the error message
    static int get_ttl(void *data) { return *((int *)data); }

    /// Callback invoked when the socket is readable
    /// \param so Socket descriptor
    /// \param event Event that occurred
    /// \param ptr Opaque pointer to this class
    static void event_callback(int so, short event, void *ptr);

    /// Idempotent cleanup function
    void cleanup();

  public:
    /// Constructor
    /// \param use_ipv4 Whether to use IPv4
    /// \param port The port to bind
    /// \param evbase Event base to use (optional)
    /// \throws Exception on error
    AndroidProber(bool use_ipv4, int port, event_base *evbase = nullptr);

    /// Destructor
    ~AndroidProber() { cleanup(); }

    // --- API of the Linux prober only ---

    /// Get the underlying UDP socket
    /// \throws Exception on error
    int get_socket() {
        if (sockfd < 0)
            throw std::runtime_error("Programmer error");
        return sockfd;
    }

    /// Call this when you don't receive a response within timeout
    void on_timeout() { probe_pending = false; }

    /// Call this as soon as the socket is readable to get
    /// the result ICMP error received by the socket and to
    /// calculate *precisely* the RTT.
    ProbeResult on_socket_readable();

    // --- API shared by all probers ---

    virtual void send_probe(std::string addr, int port, int ttl,
                            std::string payload, double timeout)
                            override final;

    virtual void on_result(std::function<void(ProbeResult)> cb) override final {
        result_cb = cb;
    }

    virtual void on_timeout(std::function<void()> cb) override final {
        timeout_cb = cb;
    }

    virtual void
    on_error(std::function<void(std::runtime_error)> cb) override final {
        error_cb = cb;
    }
};

}}}
#endif
#endif
