# Copyright (C) 2011 Nippon Telegraph and Telephone Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#	http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from ryu.base import app_manager
from ryu.controller import ofp_event
from ryu.controller.handler import CONFIG_DISPATCHER, MAIN_DISPATCHER, DEAD_DISPATCHER
from ryu.controller.handler import set_ev_cls
from ryu.ofproto import ofproto_v1_3
from ryu.lib.packet import packet
from ryu.ofproto import ether
from ryu.lib.packet import ethernet
from ryu.lib.packet import ipv4
from ryu.lib.packet import lldp
from ryu.lib.packet import ether_types
from ryu.lib.packet import arp
from ryu.lib.packet import tcp
from ryu.lib.packet import udp

from ryu.topology import event, switches
from ryu.topology.api import get_switch, get_link
import networkx as nx
from matplotlib import pyplot

import sys
from ryu.lib import hub
from operator import attrgetter


class SimpleSwitch13(app_manager.RyuApp):
	OFP_VERSIONS = [ofproto_v1_3.OFP_VERSION]

	def __init__(self, *args, **kwargs):
		super(SimpleSwitch13, self).__init__(*args, **kwargs)
		self.mac_to_port = {}
		self.mac_to_ip = {}
		#self.mac_to_ip = {"10.0.0.1": "08:00:27:21:7d:64", "10.0.0.2": "08:00:27:de:9e:3e"}
		self.topology_api_app = self
		self.special_char = {"a": 7, "b": 8, "f": 12, "n": 10, "r": 13, "t": 9, "v": 11}
		self.net = nx.DiGraph()
		self.switch_list = {}
		self.switch_dict = {}
		self.link_list = {}
		self.no_of_nodes = 0
		self.no_of_links = 0
		self.known_hosts = []
		self.last_pair = []
		self.monitor_dpid = 1
		self.monitor_port = 5
		self.local_port = 65535
		self.datapaths = {}
		self.flow_pkt_count_pre = {}
		self.flow_pkt_count_cur = {}
		self.switch_num = 0
		self.switch_i = 0
		self.flow_rate = 0
		self.flows = {}
		self.flow_num_all = 0
		self.flow_num_me = 0
		self.war_time = 0
		self.war_period = 10
		self.idle_timeout = 500
		self.white_list = []
		self.ports_initialized = 0
		self.ports_pkt_count_pre = {}
		self.ports_pkt_count_cur = {}
		self.port_rate = 0
		self.very_big = 99999

	
	@set_ev_cls(ofp_event.EventOFPStateChange, [MAIN_DISPATCHER, DEAD_DISPATCHER])
	def _state_change_handler(self, ev):
		datapath = ev.datapath
		dpid = datapath.id
		if ev.state == MAIN_DISPATCHER:
			if not dpid in self.datapaths:
				self.logger.debug('register datapath: %016x', dpid)
				self.switch_num += 1
		elif ev.state == DEAD_DISPATCHER:
			if dpid in self.datapaths:
				self.logger.debug('unregister datapath: %016x', dpid)
				del self.datapaths[dpid]
				del self.flows[dpid]
				del self.flow_pkt_count_pre[dpid]
				del self.flow_pkt_count_cur[dpid]
				self.switch_num -= 1


	def _request_stats(self, datapath):
		ofproto = datapath.ofproto
		parser = datapath.ofproto_parser

		#req = parser.OFPFlowStatsRequest(datapath)
		#datapath.send_msg(req)

		req = parser.OFPPortStatsRequest(datapath, 0, ofproto.OFPP_ANY)
		datapath.send_msg(req)

		#req = parser.OFPPortDescStatsRequest(datapath, 0)
		#datapath.send_msg(req)

	@set_ev_cls(ofp_event.EventOFPFlowStatsReply, MAIN_DISPATCHER)
	def _flow_stats_reply_handler(self, ev):
		body = ev.msg.body

		dpid = ev.msg.datapath.id
		self.flow_num_all += len(body)
		for stat in [flow for flow in body]:
			match = stat.match
			if not('ipv4_src' in match and 'ipv4_dst' in match):
				continue
			self.flow_num_me += 1
			index = self.flows[dpid].index([match['ipv4_src'], match['ipv4_dst']])
				
			self.flow_pkt_count_cur[dpid][index] = stat.packet_count
			self.flow_rate = self.flow_pkt_count_cur[dpid][index] - self.flow_pkt_count_pre[dpid][index]
			self.flow_pkt_count_pre[dpid][index] = self.flow_pkt_count_cur[dpid][index]

	@set_ev_cls(ofp_event.EventOFPPortStatsReply, MAIN_DISPATCHER)
	def _port_stats_reply_handler(self, ev):
		body = ev.msg.body
		dpid = ev.msg.datapath.id

		for stat in body:
			port = stat.port_no
			if self.ports_initialized == 0:
				self.ports_pkt_count_pre[dpid, port] = 0
			all_packets = stat.rx_packets + stat.tx_packets
			self.ports_pkt_count_cur[dpid, port] = all_packets
			self.port_rate = self.ports_pkt_count_cur[dpid, port] - self.ports_pkt_count_pre[dpid, port]
			self.ports_pkt_count_pre[dpid, port] = all_packets
			for node in self.net[dpid]:
				if self.net[dpid][node]['port_src'] == port:
					self.net[dpid][node]['weight'] = self.port_rate

		self.switch_i += 1
		if self.ports_initialized == 0 and self.switch_i >= len(self.switch_list):
			self.ports_initialized = 1

	@set_ev_cls(ofp_event.EventOFPPortDescStatsReply, MAIN_DISPATCHER)
	def port_desc_stats_reply_handler(self, ev):
		ports = []
		for p in ev.msg.body:
			ports.append('port_no=%d hw_addr=%s name=%s config=0x%08x '
					'state=0x%08x curr=0x%08x advertised=0x%08x '
					'supported=0x%08x peer=0x%08x curr_speed=%d '
					'max_speed=%d' %
					(p.port_no, p.hw_addr,
						p.name, p.config,
						p.state, p.curr, p.advertised,
						p.supported, p.peer, p.curr_speed,
						p.max_speed))


	@set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
	def switch_features_handler(self, ev):								### this section takes effect earlier than _state_change_handler
		msg = ev.msg
		datapath = ev.msg.datapath
		dpid = datapath.id
		if datapath.id not in self.datapaths:
			self.datapaths[dpid] = datapath
			self.flows[dpid] = []
			self.flow_pkt_count_pre[dpid] = []
			self.flow_pkt_count_cur[dpid] = []
		ofproto = datapath.ofproto
		parser = datapath.ofproto_parser
		req = parser.OFPPortDescStatsRequest(datapath, 0)
		datapath.send_msg(req)

		# install table-miss flow entry
		#
		# We specify NO BUFFER to max_len of the output action due to
		# OVS bug. At this moment, if we specify a lesser number, e.g.,
		# 128, OVS will send Packet-In with invalid buffer_id and
		# truncated packet data. In that case, we cannot output packets
		# correctly.  The bug has been fixed in OVS v2.1.0.
		match = parser.OFPMatch()
		actions = [parser.OFPActionOutput(ofproto.OFPP_CONTROLLER,
										  ofproto.OFPCML_NO_BUFFER)]
		print("add table miss")
		self.add_flow(datapath, 0, match, actions, 500)

		if datapath.id == self.monitor_dpid:
			match = parser.OFPMatch(in_port=self.monitor_port)
			actions = []
			self.add_flow(datapath, 999, match, actions, 5)

	def add_flow(self, datapath, priority, match, actions, buffer_id=None):
		ofproto = datapath.ofproto
		parser = datapath.ofproto_parser
	
		inst = [parser.OFPInstructionActions(ofproto.OFPIT_APPLY_ACTIONS,
											 actions)]
		if buffer_id:
			mod = parser.OFPFlowMod(datapath=datapath, buffer_id=buffer_id,
									priority=priority, match=match,
									instructions=inst)
		else:
			mod = parser.OFPFlowMod(datapath=datapath, priority=priority,
									match=match, instructions=inst)
		datapath.send_msg(mod)
		dpid = datapath.id

		if 'ipv4_src' in match and 'ipv4_dst' in match:
			if [match['ipv4_src'], match['ipv4_dst']] not in self.flows[dpid]:
				if 'ip_proto' in match:
					self.flows[dpid].append([match['ipv4_src'], match['ipv4_dst'], match['ip_proto']])
				else:
					self.flows[dpid].append([match['ipv4_src'], match['ipv4_dst'], "None"])
				self.flow_pkt_count_pre[dpid].append(0)
				self.flow_pkt_count_cur[dpid].append(0)
			else:
				pass

	def delete_flow(self, datapath, match):
		ofproto = datapath.ofproto
		parser = datapath.ofproto_parser
  
		mod = parser.OFPFlowMod(
			datapath, command=ofproto.OFPFC_DELETE, out_port=ofproto.OFPP_ANY, out_group=ofproto.OFPG_ANY,
			match=match)
		datapath.send_msg(mod)
		dpid = datapath.id
		for each in self.flows[dpid]:
			if [match['ipv4_src'], match['ipv4_dst']] == each:
				index = self.flows[dpid].index(each)
				del(self.flows[dpid][index])
				del(self.flow_pkt_count_pre[dpid][index])
				del(self.flow_pkt_count_cur[dpid][index])
	

	@set_ev_cls(event.EventSwitchEnter)
	def get_topology_data(self, ev):
		self.switch_list = get_switch(self.topology_api_app, None)
		print("get %d switches" % len(self.switch_list))
		for switch in self.switch_list:
			self.net.add_node(switch.dp.id, {'name': switch.dp.id})
			datapath = switch.dp
			dpid = datapath.id
			self.switch_dict[dpid] = datapath
			
		self.link_list = get_link(self.topology_api_app, None)
		print("get %d links" % len(self.link_list))
		for link in self.link_list:
			self.net.add_edge(link.src.dpid, link.dst.dpid, {'port_src': link.src.port_no, 'port_dst': link.dst.port_no, 'weight': 1})
	


	def _handle_arp(self, datapath, port, pkt_ethernet, pkt_arp, the_mac):
		if pkt_arp.opcode != arp.ARP_REQUEST:
			return
		pkt = packet.Packet()
		pkt.add_protocol(ethernet.ethernet(ethertype=pkt_ethernet.ethertype,
											dst=pkt_ethernet.src,
											src=pkt_ethernet.dst))
		pkt.add_protocol(arp.arp(opcode=arp.ARP_REPLY,
									src_mac=the_mac,
									src_ip=pkt_arp.dst_ip,
									dst_mac=pkt_arp.src_mac,
									dst_ip=pkt_arp.src_ip))
		self._send_packet(datapath, port, pkt)

	def _send_packet(self, datapath, port, pkt):
		ofproto = datapath.ofproto
		parser = datapath.ofproto_parser
		pkt.serialize()
		data = pkt.data
		actions = [parser.OFPActionOutput(port=port)]
		out = parser.OFPPacketOut(datapath=datapath,
									buffer_id=ofproto.OFP_NO_BUFFER,
									in_port=ofproto.OFPP_CONTROLLER,
									actions=actions,
									data=data)
		datapath.send_msg(out)


	@set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
	def _packet_in_handler(self, ev):
		# If you hit this you might want to increase
		# the "miss_send_length" of your switch
		if ev.msg.msg_len < ev.msg.total_len:
			self.logger.debug("packet truncated: only %s of %s bytes",
							  ev.msg.msg_len, ev.msg.total_len)
		msg = ev.msg
		datapath = msg.datapath
		ofproto = datapath.ofproto
		parser = datapath.ofproto_parser
		in_port = msg.match['in_port']

		pkt = packet.Packet(msg.data)
		eth = pkt.get_protocols(ethernet.ethernet)[0]
		
		if not eth:
			return

		dst = eth.dst
		src = eth.src
		dpid = datapath.id

		msg_arp = pkt.get_protocol(arp.arp)
		if msg_arp:
			if msg_arp.opcode == arp.ARP_REQUEST:
				if msg_arp.dst_ip in self.mac_to_ip:
					the_mac = self.mac_to_ip[msg_arp.dst_ip]
					self._handle_arp(datapath, in_port, eth, msg_arp, the_mac)
			else:
				pass
				
		msg_ip = pkt.get_protocol(ipv4.ipv4)
		src_ip = None
		dst_ip = None

		msg_tcp = pkt.get_protocol(tcp.tcp)
		tcp_src_port = None
		tcp_dst_port = None
		ip_proto = -1
		if msg_ip:
			src_ip = msg_ip.src
			dst_ip = msg_ip.dst
			ip_proto = None
			
		if msg_tcp:
			tcp_src_port = msg_tcp.src_port
			tcp_dst_port = msg_tcp.dst_port
			ip_proto = 6

		msg_udp = pkt.get_protocol(udp.udp)
		udp_src_port = None
		udp_dst_port = None
		if msg_udp:
			udp_src_port = msg_udp.src_port
			udp_dst_port = msg_udp.dst_port
			ip_proto = 17

		if self.war_time == 1:
			if src_ip not in self.white_list:
				return

		if src not in self.known_hosts:
			if "00:00:00:00" not in src:
				return
			self.known_hosts.append(src)
			if src not in self.net:
				self.net.add_node(src, {'name': src})
				self.net.add_edge(dpid, src, {'port_src':in_port, 'weight':0})
				self.net.add_edge(src, dpid, {'port_src': "X", 'weight':0})

		if(src in self.net and dst in self.net):
			if [src_ip, dst_ip, ip_proto] in self.flows[dpid]:
				return
			
			pos = nx.spring_layout(self.net)
			nx.draw(self.net, pos,node_color='k')
			path = nx.dijkstra_path(self.net,src,dst, 'weight')
			path_edges = zip(path,path[1:])
			port_src = nx.get_edge_attributes(self.net, "port_src")
			if src_ip == None or dst_ip == None:
				return
			prior = 0
			for edge in path_edges:
				edge_src, edge_dst = edge
				if str(edge_src).isdigit():
					the_dp = self.switch_dict[int(edge_src)]
					the_outport = port_src[(edge_src, edge_dst)]
					if tcp_src_port and tcp_dst_port:
						match = parser.OFPMatch(eth_type=ether.ETH_TYPE_IP, ipv4_src=src_ip, ipv4_dst=dst_ip, ip_proto=6)
						actions = [parser.OFPActionSetQueue(0), parser.OFPActionOutput(the_outport)]
						prior = 10
					elif udp_src_port and udp_dst_port:
						match = parser.OFPMatch(eth_type=ether.ETH_TYPE_IP, ipv4_src=src_ip, ipv4_dst=dst_ip, ip_proto=17)
						actions = [parser.OFPActionSetQueue(1), parser.OFPActionOutput(the_outport)]
						prior = 10
					else:
						match = parser.OFPMatch(eth_type=ether.ETH_TYPE_IP, ipv4_src=src_ip, ipv4_dst=dst_ip)
						actions = [parser.OFPActionSetQueue(0), parser.OFPActionOutput(the_outport)]
						prior = 3
					if the_dp.id == self.monitor_dpid:
						actions.append(parser.OFPActionOutput(self.monitor_port))
					
					self.add_flow(the_dp, prior, match, actions)
					#self.net[edge_src][edge_dst]['weight'] = self.very_big	# If you want to disable dynamic routing, comment here.
					#self.very_big += 1										#
					data = None
					if msg.buffer_id == ofproto.OFP_NO_BUFFER:
						data = msg.data
					out = parser.OFPPacketOut(datapath=datapath, buffer_id=msg.buffer_id, in_port=in_port, actions=actions, data=data)
					datapath.send_msg(out)
				else:
					continue
			
		
		if eth.ethertype == ether_types.ETH_TYPE_LLDP:
			temp = str(pkt)
			if "ManagementAddress" not in temp:
				return
			ip_part = temp.split("ManagementAddress(addr='")[1].split("',addr_len")[0]
			state = 0
			i = 0
			decoded_ip = ""
			while(i < len(ip_part)):
				if(ip_part[i:i+2] == '\\x'):
					hex_string = ip_part[i+2:i+4]
					decoded_ip = decoded_ip + str(int(hex_string, 16))
					i = i + 4
				elif(ip_part[i] == '\\'):
					decoded_ip = decoded_ip + str(self.special_char[ip_part[i+1]])
					i = i + 2
				else:
					decoded_ip = decoded_ip + str(ord(ip_part[i]))
					i = i + 1
				if state < 3:
					state = state + 1
					decoded_ip = decoded_ip + "."
				else:
					if eth.src in self.mac_to_ip:
						pass
					else:
						self.mac_to_ip[decoded_ip] = eth.src
					state = 0
					decoded_ip = ""
					break
