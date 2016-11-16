/*
 * Copyright (c) 2016, Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <boost/format.hpp>
#include <boost/make_shared.hpp>

#include <string>
#include <vector>

#include "server-entry.hpp"
#include "server-encoder-decoder.hpp"
#include "tablet-server.hpp"
#include "metadata-server.hpp"

using std::string;
using std::vector;
using std::cerr;
using std::cout;
using std::endl;
using boost::format;
using boost::shared_ptr;
using boost::make_shared;

void ServerThreadEntry::server_entry(
    uint channel_id, uint num_channels,
    uint server_id, uint num_machines,
    shared_ptr<zmq::context_t> zmq_ctx,
    const Config& config) {
  uint port = config.tcp_base_port + channel_id;
  string request_url = "tcp://*:" + boost::lexical_cast<std::string>(port);

  uint numa_node_id = 0;
  if (config.affinity) {
    numa_node_id =
      get_numa_node_id(channel_id, num_channels, config.num_zones);
  }

  vector<string> connect_list;   /* Empty connect to */
  vector<string> bind_list;
  bind_list.push_back(request_url);
  string tablet_name = (format("tablet-%i") % server_id).str();
  shared_ptr<RouterHandler> router_handler = make_shared<RouterHandler>(
      channel_id, zmq_ctx, connect_list, bind_list, tablet_name,
      numa_node_id, config);

  shared_ptr<ServerClientEncode> encoder = make_shared<ServerClientEncode>(
      router_handler, num_machines, server_id, config);

  shared_ptr<TabletStorage> storage = make_shared<TabletStorage>(
      channel_id, num_channels, server_id, num_machines, encoder, config);
  shared_ptr<MetadataServer> metadata_server = make_shared<MetadataServer>(
      encoder, channel_id, num_channels, server_id, num_machines, config);

  ClientServerDecode decoder(storage, metadata_server);

  router_handler->do_handler(decoder.get_recv_callback());
}
