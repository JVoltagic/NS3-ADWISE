/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 University of Campinas (Unicamp)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:  Luciano Chaves <luciano@lrc.ic.unicamp.br>
 * Modified by: Oscar Bautista <obaut004@fiu.edu>
 */

#include <ns3/traffic-type-controller.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>

NS_LOG_COMPONENT_DEFINE ("TrafficTypeController");
NS_OBJECT_ENSURE_REGISTERED (TrafficTypeController);

TrafficTypeController::TrafficTypeController ()
{
  NS_LOG_FUNCTION (this);
}

TrafficTypeController::~TrafficTypeController ()
{
  NS_LOG_FUNCTION (this);
}

void
TrafficTypeController::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  m_arpTable.clear ();
  OFSwitch13Controller::DoDispose ();
}

TypeId
TrafficTypeController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TrafficTypeController")
    .SetParent<OFSwitch13Controller> ()
    .SetGroupName ("OFSwitch13")
    .AddConstructor<TrafficTypeController> ()
    .AddAttribute ("EnableMeter",
                   "Enable per-flow mettering.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&TrafficTypeController::m_meterEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("MeterRate",
                   "Per-flow meter rate.",
                   DataRateValue (DataRate ("256Kbps")),
                   MakeDataRateAccessor (&TrafficTypeController::m_meterRate),
                   MakeDataRateChecker ())
    .AddAttribute ("ServerIpAddr",
                   "Server IPv4 address.",
                   AddressValue (Address (Ipv4Address ("10.1.1.1"))),
                   MakeAddressAccessor (&TrafficTypeController::m_serverIpAddress),
                   MakeAddressChecker ())
    .AddAttribute ("ServerTcpPort",
                   "Server TCP port.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&TrafficTypeController::m_serverTcpPort),
                   MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("ServerMacAddr",
                   "Server MAC address.",
                   AddressValue (Address (Mac48Address ("00:00:00:00:00:01"))),
                   MakeAddressAccessor (&TrafficTypeController::m_serverMacAddress),
                   MakeAddressChecker ())
  ;
  return tid;
}

ofl_err
TrafficTypeController::HandlePacketIn (
  struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch,
  uint32_t xid)
{
  NS_LOG_FUNCTION (this << swtch << xid);

  char *msgStr =
    ofl_structs_match_to_string ((struct ofl_match_header*)msg->match, 0);
  NS_LOG_DEBUG ("Packet in match: " << msgStr);
  free (msgStr);

  if (msg->reason == OFPR_ACTION)
    {
      // Get Ethernet frame type
      uint16_t ethType;
      struct ofl_match_tlv *tlv;
      tlv = oxm_match_lookup (OXM_OF_ETH_TYPE, (struct ofl_match*)msg->match);
      memcpy (&ethType, tlv->value, OXM_LENGTH (OXM_OF_ETH_TYPE));

      if (ethType == ArpL3Protocol::PROT_NUMBER)
        {
          // ARP packet
          return HandleArpPacketIn (msg, swtch, xid);
        }
      else if (ethType == Ipv4L3Protocol::PROT_NUMBER)
        {
          // Must be a UDP/TCP packet for connection request
          return HandleConnectionRequest (msg, swtch, xid);
        }
    }
  // All handlers must free the message when everything is ok
  ofl_msg_free ((struct ofl_msg_header*)msg, 0);
  return 0;
}

void
TrafficTypeController::HandshakeSuccessful (Ptr<const RemoteSwitch> swtch)
{
  NS_LOG_FUNCTION (this << swtch);

  // This function is called after a successfully handshake between controller
  // and each switch. Let's check the switch for proper configuration.
  if (swtch->GetDpId () == 1)
    {
      ConfigureCarSwitch (swtch);
    }
}

void
TrafficTypeController::ConfigureCarSwitch (Ptr<const RemoteSwitch> swtch)
{
  NS_LOG_FUNCTION (this << swtch);

  // For packet-in messages, send only the first 128 bytes to the controller
  DpctlExecute (swtch, "set-config miss=128");

  // Redirect ARP requests to the controller
  DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=20 "
                "eth_type=0x0806,arp_op=1 apply:output=ctrl");

  // Using group #3 for rewriting headers and forwarding packets to clients
  // if (m_linkAggregation)
  //   {
  //     // Configure Group #3 for aggregating links 1 and 2
  //     DpctlExecute (swtch, "group-mod cmd=add,type=sel,group=3 "
  //                   "weight=1,port=any,group=any set_field=ip_src:10.1.1.1"
  //                   ",set_field=eth_src:00:00:00:00:00:01,output=1 "
  //                   "weight=1,port=any,group=any set_field=ip_src:10.1.1.1"
  //                   ",set_field=eth_src:00:00:00:00:00:01,output=2");
  //   }
  // else
  //   {
  //     // Configure Group #3 for sending packets only over link 1
  //     DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=3 "
  //                   "weight=0,port=any,group=any set_field=ip_src:10.1.1.1"
  //                   ",set_field=eth_src:00:00:00:00:00:01,output=1");
  //   }

  // Group #1 to send traffic out from LTE interface (port 4)
  DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=1 "
                "weight=0,port=any,group=any "
                "output=4");

  // Group #2 to send traffic out from mmWave interface (port 5)
  DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=2 "
                "weight=0,port=any,group=any "
                "output=5");

  // DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=3 "
  //               "weight=0,port=any,group=any set_field=ip_dst:10.1.1.4"
  //               ",set_field=eth_dst:00:00:00:00:00:15"
  //               ",output=3");

  // Group #3 to send traffic out from 802.11p interface (port 6)
  // DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=3 "
  //               "weight=0,port=any,group=any set_field=ip_src:20.1.1.2,"
  //               "set_field=eth_dst:00:00:00:00:00:0f,output=6");

  // Group #4 redirect incoming traffic from port 6 to port 1
  // DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=4 "
  //               "weight=0,port=any,group=any set_field=ip_dst:10.1.1.2,"
  //               "set_field=eth_dst:00:00:00:00:00:11,output=1");


  // // Groups #1 and #2 send traffic to internal servers (ports 3 and 4)
  // DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=1 "
  //               "weight=0,port=any,group=any set_field=ip_dst:10.1.1.2,"
  //               "set_field=eth_dst:00:00:00:00:00:08,output=3");
  // DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=2 "
  //               "weight=0,port=any,group=any set_field=ip_dst:10.1.1.3,"
  //               "set_field=eth_dst:00:00:00:00:00:0a,output=4");

  // Incoming connections (ports 1, 2 and 3) are sent to the controller
  DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=500 "
                "in_port=1,eth_type=0x0800,"
                "eth_dst=00:00:00:00:00:01 apply:output=ctrl");
  DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=500 "
                "in_port=2,eth_type=0x0800,"
                "eth_dst=00:00:00:00:00:01 apply:output=ctrl");
  DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=500 "
                "in_port=3,eth_type=0x0800,"
                "eth_dst=00:00:00:00:00:01 apply:output=ctrl");
  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=500 "
  //               "in_port=4,eth_type=0x0800"
  //               " apply:output=ctrl");

  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=1000 "
  //               "eth_type=0x0800,ip_proto=6,ip_src=1.0.0.2,tcp_src=49153 apply:group=3");
                // "eth_type=0x0800,ip_src=1.0.0.2,udp_src=49153,udp_dst=1000, apply:group=3");

  // Packets destined to internal network by MAC addresses
  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=101 "
  //               "eth_type=0x0800,eth_dst=00:00:00:00:00:11 apply:output=1");
  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=101 "
  //               "eth_type=0x0800,eth_dst=00:00:00:00:00:13 apply:output=2");
  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=101 "
  //               "eth_type=0x0800,eth_dst=00:00:00:00:00:15 apply:output=3");

  // Packets to/from an external server
  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=201 "
  //               "eth_type=0x0800,ip_dst=1.0.0.2, apply:group=1");
  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=201 "
  //               "eth_type=0x0800,in_port=4, apply:group=2");
  // Packets to/from remote 802.11p node
  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=201 "
  //               "eth_type=0x0800,ip_dst=20.1.1.1, apply:group=3");
  // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=201 "
  //               "eth_type=0x0800,in_port=6, apply:group=4");

}

// void
// TrafficTypeController::ConfigureAggregationSwitch (Ptr<const RemoteSwitch> swtch)
// {
//   NS_LOG_FUNCTION (this << swtch);
//
//   if (m_linkAggregation)
//     {
//       // Configure Group #1 for aggregating links 1 and 2
//       DpctlExecute (swtch, "group-mod cmd=add,type=sel,group=1 "
//                     "weight=1,port=any,group=any output=1 "
//                     "weight=1,port=any,group=any output=2");
//     }
//   else
//     {
//       // Configure Group #1 for sending packets only over link 1
//       DpctlExecute (swtch, "group-mod cmd=add,type=ind,group=1 "
//                     "weight=0,port=any,group=any output=1");
//     }
//
//   // Packets from input ports 1 and 2 are redirecte to port 3
//   DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=500 "
//                 "in_port=1 write:output=3");
//   DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=500 "
//                 "in_port=2 write:output=3");
//
//   // Packets from input port 3 are redirected to group 1
//   DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=500 "
//                 "in_port=3 write:group=1");
// }

ofl_err
TrafficTypeController::HandleArpPacketIn (
  struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch,
  uint32_t xid)
{
  NS_LOG_FUNCTION (this << swtch << xid);

  struct ofl_match_tlv *tlv;

  // Get ARP operation
  uint16_t arpOp;
  tlv = oxm_match_lookup (OXM_OF_ARP_OP, (struct ofl_match*)msg->match);
  memcpy (&arpOp, tlv->value, OXM_LENGTH (OXM_OF_ARP_OP));

  // Get input port
  uint32_t inPort;
  tlv = oxm_match_lookup (OXM_OF_IN_PORT, (struct ofl_match*)msg->match);
  memcpy (&inPort, tlv->value, OXM_LENGTH (OXM_OF_IN_PORT));

  // Get source and target IP address
  Ipv4Address srcIp, dstIp;
  srcIp = ExtractIpv4Address (OXM_OF_ARP_SPA, (struct ofl_match*)msg->match);
  dstIp = ExtractIpv4Address (OXM_OF_ARP_TPA, (struct ofl_match*)msg->match);

  // Get Source MAC address
  Mac48Address srcMac, dstMac;
  tlv = oxm_match_lookup (OXM_OF_ARP_SHA, (struct ofl_match*)msg->match);
  srcMac.CopyFrom (tlv->value);
  tlv = oxm_match_lookup (OXM_OF_ARP_THA, (struct ofl_match*)msg->match);
  dstMac.CopyFrom (tlv->value);

  // Check for ARP request
  if (arpOp == ArpHeader::ARP_TYPE_REQUEST)
    {
      uint8_t replyData[64];

      // Check for destination IP

      if (dstIp.IsEqual (Ipv4Address ("10.1.1.1")))
        {
          Ptr<Packet> pkt = CreateArpReply (Mac48Address("00:00:00:00:00:01"), dstIp, srcMac, srcIp);
          NS_ASSERT_MSG (pkt->GetSize () == 64, "Invalid packet size.");
          pkt->CopyData (replyData, 64);
        }

      // Send the ARP replay back to the input port
      struct ofl_action_output *action =
        (struct ofl_action_output*)xmalloc (sizeof (struct ofl_action_output));
      action->header.type = OFPAT_OUTPUT;
      action->port = OFPP_IN_PORT;
      action->max_len = 0;

      // Send the ARP reply within an OpenFlow PacketOut message
      struct ofl_msg_packet_out reply;
      reply.header.type = OFPT_PACKET_OUT;
      reply.buffer_id = OFP_NO_BUFFER;
      reply.in_port = inPort;
      reply.data_length = 64;
      reply.data = &replyData[0];
      reply.actions_num = 1;
      reply.actions = (struct ofl_action_header**)&action;

      SendToSwitch (swtch, (struct ofl_msg_header*)&reply, xid);
      free (action);
    }

  // All handlers must free the message when everything is ok
  ofl_msg_free ((struct ofl_msg_header*)msg, 0);
  return 0;
}

ofl_err
TrafficTypeController::HandleConnectionRequest (
  struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch,
  uint32_t xid)
{
  NS_LOG_FUNCTION (this << swtch << xid);

  struct ofl_match_tlv *tlv;

  // Get input port
  uint32_t inPort;
  tlv = oxm_match_lookup (OXM_OF_IN_PORT, (struct ofl_match*)msg->match);
  memcpy (&inPort, tlv->value, OXM_LENGTH (OXM_OF_IN_PORT));

  // Get Source MAC address
  Mac48Address srcMac;
  tlv = oxm_match_lookup (OXM_OF_ETH_SRC, (struct ofl_match*)msg->match);
  srcMac.CopyFrom (tlv->value);

  // Get source and destination IP address
  Ipv4Address srcIp, dstIp;
  srcIp = ExtractIpv4Address (OXM_OF_IPV4_SRC, (struct ofl_match*)msg->match);
  dstIp = ExtractIpv4Address (OXM_OF_IPV4_DST, (struct ofl_match*)msg->match);

  // Get source and destination TCP ports
  uint16_t srcPort, dstPort;
  tlv = oxm_match_lookup (OXM_OF_UDP_SRC, (struct ofl_match*)msg->match);
  memcpy (&srcPort, tlv->value, OXM_LENGTH (OXM_OF_UDP_SRC));
  tlv = oxm_match_lookup (OXM_OF_UDP_DST, (struct ofl_match*)msg->match);
  memcpy (&dstPort, tlv->value, OXM_LENGTH (OXM_OF_UDP_DST));

  if (dstIp==Ipv4Address("1.0.0.2"))
    {
    // Select a  route number to assign this connection
    uint16_t routeNumber = 2;
    NS_LOG_INFO ("Connection assigned to route " << routeNumber);

    // If enable, install the metter entry for this connection
    if (m_meterEnable)
      {
        std::ostringstream meterCmd;
        meterCmd << "meter-mod cmd=add,flags=1,meter=" << routeNumber
                 << " drop:rate=" << m_meterRate.GetBitRate () / 1000;
        DpctlExecute (swtch, meterCmd.str ());
      }

    // Install the group entry for the return path
    std::ostringstream flowCmd;
    flowCmd << "group-mod cmd=add,type=ind,group=" << routeNumber+2
            << " weight=0,port=any,group=any set_field=ip_dst:" << srcIp
            << ",set_field=eth_dst:" << srcMac
            << ",output=" << inPort;
    DpctlExecute (swtch, flowCmd.str ());

    // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=201 "
    //               "eth_type=0x0800,in_port=6, apply:group=4");
    //
    // DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=500 "
    //               "in_port=3 write:group=1");

    // Install the flow entry for the return path
    flowCmd.str(std::string());
    flowCmd << "flow-mod cmd=add,table=0,prio=1000"
            << " eth_type=0x0800,ip_proto=17"
            << ",ip_src=" << dstIp
            << ",udp_dst=" << srcPort
            << ",udp_src=" << dstPort;
    if (m_meterEnable)
      {
        flowCmd << " meter:" << routeNumber;
      }
    flowCmd << " apply:group=" << routeNumber+2;
    DpctlExecute (swtch, flowCmd.str ());

    // Install the flow entry for this UDP connection
    flowCmd.str(std::string());
    flowCmd << "flow-mod cmd=add,table=0,prio=800 "
            << " eth_type=0x0800,ip_proto=17"
            << ",ip_src=" << srcIp
            << ",ip_dst=" << dstIp
            << ",udp_dst=" << dstPort
            << ",udp_src=" << srcPort
            << " apply:group=" << routeNumber;
    DpctlExecute (swtch, flowCmd.str ());

    // Create group action with routeNumber
    struct ofl_action_group *action =
      (struct ofl_action_group*)xmalloc (sizeof (struct ofl_action_group));
    action->header.type = OFPAT_GROUP;
    action->group_id = routeNumber;

    // Send the packet out to the switch.
    struct ofl_msg_packet_out reply;
    reply.header.type = OFPT_PACKET_OUT;
    reply.buffer_id = msg->buffer_id;
    reply.in_port = inPort;
    reply.actions_num = 1;
    reply.actions = (struct ofl_action_header**)&action;
    reply.data_length = 0;
    reply.data = 0;
    if (msg->buffer_id == NO_BUFFER)
      {
        // No packet buffer. Send data back to switch
        reply.data_length = msg->data_length;
        reply.data = msg->data;
      }

    SendToSwitch (swtch, (struct ofl_msg_header*)&reply, xid);
    free (action);
    }
  // All handlers must free the message when everything is ok
  ofl_msg_free ((struct ofl_msg_header*)msg, 0);

  Simulator::Schedule (Seconds(5.0), &TrafficTypeController::SwitchInterface, this, swtch, xid, 1);
  Simulator::Schedule (Seconds(11.0), &TrafficTypeController::SwitchInterface, this, swtch, xid, 2);
  std::cout << "Scheduled call to method SwitchInterface" << std::endl;
  return 0;
}

ofl_err
TrafficTypeController::SwitchInterface (
  Ptr<const RemoteSwitch> swtch, uint32_t xid, uint16_t altRouteNumber)
{
  std::cout << "Successfull call to method" << std::endl;
  // Mac48Address hostMac = Mac48Address ("00:00:00:00:00:15");
  Ipv4Address hostIp = Ipv4Address ("10.1.1.4");
  Ipv4Address serverIp = Ipv4Address ("1.0.0.2");
  uint16_t localPort = 1000;
  uint16_t remotePort = 2000;
  // uint16_t ofSwitchHostPort = 3;
  // uint16_t routeNumber = 1;
  // uint16_t altRouteNumber = 2;

  // Add the group entry for the return path  /* (Not necessary actually) */
  std::ostringstream flowCmd;
  // flowCmd << "group-mod cmd=add,type=ind,group=" << altRouteNumber + 2
  //         << " weight=0,port=any,group=any set_field=ip_dst:" << hostIp
  //         << ",set_field=eth_dst:" << hostMac
  //         << ",output=" << ofSwitchHostPort;
  // DpctlExecute (swtch, flowCmd.str ());

  // Modify the flow entry for the return path /* (Not necessary if redirecting to existing OF group) */
  // flowCmd.str(std::string());
  // flowCmd << "flow-mod cmd=mod,table=0,prio=1000"
  //         << " eth_type=0x0800,ip_proto=17"
  //         << ",ip_src=" << serverIp
  //         << ",udp_dst=" << localPort
  //         << ",udp_src=" << remotePort;
  // if (m_meterEnable)
  //   {
  //     flowCmd << " meter:" << altRouteNumber + 2;
  //   }
  //
  // /* Because we are dealing with traffic from/to same devices with same udp port Numbers
  // This flow can be kept redirecting to existing group 4 */
  // flowCmd << " apply:group=" << routeNumber + 2;
  // DpctlExecute (swtch, flowCmd.str ());

  // Modify the flow entry for this UDP connection
  flowCmd.str(std::string());
  flowCmd << "flow-mod cmd=mod,table=0,prio=800 "
          << " eth_type=0x0800,ip_proto=17"
          << ",ip_src=" << hostIp
          << ",ip_dst=" << serverIp
          << ",udp_dst=" << remotePort
          << ",udp_src=" << localPort
          << " apply:group=" << altRouteNumber;
  DpctlExecute (swtch, flowCmd.str ());

  return 0;
}

Ipv4Address
TrafficTypeController::ExtractIpv4Address (uint32_t oxm_of, struct ofl_match* match)
{
  switch (oxm_of)
    {
    case OXM_OF_ARP_SPA:
    case OXM_OF_ARP_TPA:
    case OXM_OF_IPV4_DST:
    case OXM_OF_IPV4_SRC:
      {
        uint32_t ip;
        int size = OXM_LENGTH (oxm_of);
        struct ofl_match_tlv *tlv = oxm_match_lookup (oxm_of, match);
        memcpy (&ip, tlv->value, size);
        return Ipv4Address (ntohl (ip));
      }
    default:
      NS_ABORT_MSG ("Invalid IP field.");
    }
}

Ptr<Packet>
TrafficTypeController::CreateArpRequest (Mac48Address srcMac, Ipv4Address srcIp,
                                 Ipv4Address dstIp)
{
  NS_LOG_FUNCTION (this << srcMac << srcIp << dstIp);

  Ptr<Packet> packet = Create<Packet> ();

  // ARP header
  ArpHeader arp;
  arp.SetRequest (srcMac, srcIp, Mac48Address::GetBroadcast (), dstIp);
  packet->AddHeader (arp);

  // Ethernet header
  EthernetHeader eth (false);
  eth.SetSource (srcMac);
  eth.SetDestination (Mac48Address::GetBroadcast ());
  if (packet->GetSize () < 46)
    {
      uint8_t buffer[46];
      memset (buffer, 0, 46);
      Ptr<Packet> padd = Create<Packet> (buffer, 46 - packet->GetSize ());
      packet->AddAtEnd (padd);
    }
  eth.SetLengthType (ArpL3Protocol::PROT_NUMBER);
  packet->AddHeader (eth);

  // Ethernet trailer
  EthernetTrailer trailer;
  if (Node::ChecksumEnabled ())
    {
      trailer.EnableFcs (true);
    }
  trailer.CalcFcs (packet);
  packet->AddTrailer (trailer);

  return packet;
}

Ptr<Packet>
TrafficTypeController::CreateArpReply (Mac48Address srcMac, Ipv4Address srcIp,
                               Mac48Address dstMac, Ipv4Address dstIp)
{
  NS_LOG_FUNCTION (this << srcMac << srcIp << dstMac << dstIp);

  Ptr<Packet> packet = Create<Packet> ();

  // ARP header
  ArpHeader arp;
  arp.SetReply (srcMac, srcIp, dstMac, dstIp);
  packet->AddHeader (arp);

  // Ethernet header
  EthernetHeader eth (false);
  eth.SetSource (srcMac);
  eth.SetDestination (dstMac);
  if (packet->GetSize () < 46)
    {
      uint8_t buffer[46];
      memset (buffer, 0, 46);
      Ptr<Packet> padd = Create<Packet> (buffer, 46 - packet->GetSize ());
      packet->AddAtEnd (padd);
    }
  eth.SetLengthType (ArpL3Protocol::PROT_NUMBER);
  packet->AddHeader (eth);

  // Ethernet trailer
  EthernetTrailer trailer;
  if (Node::ChecksumEnabled ())
    {
      trailer.EnableFcs (true);
    }
  trailer.CalcFcs (packet);
  packet->AddTrailer (trailer);

  return packet;
}

void
TrafficTypeController::SaveArpEntry (Ipv4Address ipAddr, Mac48Address macAddr)
{
  std::pair<Ipv4Address, Mac48Address> entry (ipAddr, macAddr);
  std::pair <IpMacMap_t::iterator, bool> ret;
  ret = m_arpTable.insert (entry);
  if (ret.second == true)
    {
      NS_LOG_INFO ("New ARP entry: " << ipAddr << " - " << macAddr);
      return;
    }
}

Mac48Address
TrafficTypeController::GetArpEntry (Ipv4Address ip)
{
  IpMacMap_t::iterator ret;
  ret = m_arpTable.find (ip);
  if (ret != m_arpTable.end ())
    {
      NS_LOG_INFO ("Found ARP entry: " << ip << " - " << ret->second);
      return ret->second;
    }
  NS_ABORT_MSG ("No ARP information for this IP.");
}
