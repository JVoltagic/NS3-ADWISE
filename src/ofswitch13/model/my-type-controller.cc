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
 * Modified by: Juan Leon <jleon148@fiu.edu>
 */
#ifdef NS3_OFSWITCH13
#include <ns3/my-type-controller.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>

NS_LOG_COMPONENT_DEFINE ("MyTypeController");

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED (MyTypeController);

MyTypeController::MyTypeController ()
{
  NS_LOG_FUNCTION (this);
}

MyTypeController::~MyTypeController ()
{
  NS_LOG_FUNCTION (this);
}



TypeId
MyTypeController::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MyTypeController")
    .SetParent<OFSwitch13Controller> ()
    .SetGroupName ("OFSwitch13")
    .AddConstructor<MyTypeController> ()
  ;
  return tid;
}

void
MyTypeController::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  m_arpTable.clear ();
  OFSwitch13Controller::DoDispose ();
}


ofl_err
MyTypeController::HandlePacketIn (
  struct ofl_msg_packet_in *msg, Ptr<const RemoteSwitch> swtch,
  uint32_t xid)
{
  NS_LOG_FUNCTION (this << swtch << xid);

  //static int prio = 100;
  uint32_t outPort = OFPP_IN_PORT;
  outPort = outPort +1;
  uint64_t dpId = swtch->GetDpId ();
  enum ofp_packet_in_reason reason = msg->reason;

  char *msgStr =
    ofl_structs_match_to_string ((struct ofl_match_header*)msg->match, 0);
  NS_LOG_DEBUG ("Packet in match: " << msgStr);
  free (msgStr);

  if (reason == OFPR_ACTION)
    {
      // Let's get necessary information (input port and mac address)
      uint32_t inPort;
      size_t portLen = OXM_LENGTH (OXM_OF_IN_PORT); // (Always 4 bytes)
      struct ofl_match_tlv *input =
        oxm_match_lookup (OXM_OF_IN_PORT, (struct ofl_match*)msg->match);
      memcpy (&inPort, input->value, portLen);

      Mac48Address src48;
      struct ofl_match_tlv *ethSrc =
        oxm_match_lookup (OXM_OF_ETH_SRC, (struct ofl_match*)msg->match);
      src48.CopyFrom (ethSrc->value);

      Mac48Address dst48;
      struct ofl_match_tlv *ethDst =
        oxm_match_lookup (OXM_OF_ETH_DST, (struct ofl_match*)msg->match);
      dst48.CopyFrom (ethDst->value);

      // Get L2Table for this datapath
      auto it = m_learnedInfo.find (dpId);
      if (it != m_learnedInfo.end ())
        {
          L2Table_t *l2Table = &it->second;

          // Looking for out port based on dst address (except for broadcast)
          if (!dst48.IsBroadcast ())
            {
              auto itDst = l2Table->find (dst48);
              if (itDst != l2Table->end ())
                {
                  outPort = itDst->second;
                }
              else
                {
                  NS_LOG_DEBUG ("No L2 info for mac " << dst48 << ". Flood.");
                }
            }

          // Learning port from source address
          NS_ASSERT_MSG (!src48.IsBroadcast (), "Invalid src broadcast addr");
          auto itSrc = l2Table->find (src48);
          if (itSrc == l2Table->end ())
            {
              std::pair<Mac48Address, uint32_t> entry (src48, inPort);
              auto ret = l2Table->insert (entry);
              if (ret.second == false)
                {
                  NS_LOG_ERROR ("Can't insert mac48address / port pair");
                }
              else
                {
                  NS_LOG_DEBUG ("Learning that mac " << src48 <<
                                " can be found at port " << inPort);

                  // Send a flow-mod to switch creating this flow. Let's
                  // configure the flow entry to 10s idle timeout and to
                  // notify the controller when flow expires. (flags=0x0001)
 
	  uint32_t seq;
      size_t seqLen = OXM_LENGTH (OXM_OF_TS_SEQ); // (Always 4 bytes)
      struct ofl_match_tlv *SEQH =
      oxm_match_lookup (OXM_OF_TS_SEQ, (struct ofl_match*)msg->match);
      memcpy (&seq, SEQH->value, seqLen);
      Ipv4Address IPsrc = swtch->GetIpv4();
     
     DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=100 "
                " in_port=2"
                " apply:output=in_port");
     if(IPsrc == (Ipv4Address("7.0.0.4"))){  
            DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=130 "
                " in_port=2,ts_seq=2"
                ""); 
                 
			 if(seq == 20){
              DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=131 "
                " in_port=2,ts_seq=1"
                "");   
			 }
		 }
	 if(IPsrc == (Ipv4Address("7.0.0.5"))){  
		 if(seq == 0){
             DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=131 "
                " in_port=2,ts_seq=21"
                "");   
			 }
		 
             DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=130 "
                " in_port=2,ts_seq=22"
                "");   
			 
		 }
      	
                }
            }
          else
            {
              NS_ASSERT_MSG (itSrc->second == inPort,
                             "Inconsistent L2 switching table");
            }
        }
      else
        {
          NS_LOG_ERROR ("No L2 table for this datapath id " << dpId);
        }


    }
    
  else
    {
      NS_LOG_WARN ("This controller can't handle the packet. Unkwnon reason.");
    }

  // All handlers must free the message when everything is ok
  ofl_msg_free ((struct ofl_msg_header*)msg, 0);
  return 0;
}

ofl_err
MyTypeController::HandleFlowRemoved (
  struct ofl_msg_flow_removed *msg, Ptr<const RemoteSwitch> swtch,
  uint32_t xid)
{
  NS_LOG_FUNCTION (this << swtch << xid);

  NS_LOG_DEBUG ( "Flow entry expired. Removing from L2 switch table.");
  uint64_t dpId = swtch->GetDpId ();
  auto it = m_learnedInfo.find (dpId);
  if (it != m_learnedInfo.end ())
    {
      Mac48Address mac48;
      struct ofl_match_tlv *ethSrc =
        oxm_match_lookup (OXM_OF_ETH_DST, (struct ofl_match*)msg->stats->match);
      mac48.CopyFrom (ethSrc->value);

      L2Table_t *l2Table = &it->second;
      auto itSrc = l2Table->find (mac48);
      if (itSrc != l2Table->end ())
        {
          l2Table->erase (itSrc);
        }
    }

  // All handlers must free the message when everything is ok
  ofl_msg_free_flow_removed (msg, true, 0);
  return 0;
}



/********** Private methods **********/
void
MyTypeController::HandshakeSuccessful (
  Ptr<const RemoteSwitch> swtch)
{
  NS_LOG_FUNCTION (this << swtch);

  // After a successfull handshake, let's install the table-miss entry, setting
  // to 128 bytes the maximum amount of data from a packet that should be sent
  // to the controller.
  Ipv4Address IPsrc = swtch->GetIpv4();
  
  //DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=100"
                //" in_port=2"
                 //" apply:output=in_port"); 
                 
          
      
      //CAR1
      if(IPsrc == (Ipv4Address("7.0.0.2"))){   
       
      DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=100 "
                " in_port=2"
                " apply:output=1"); 
      DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=110 "
                " in_port=2,ts_seq=1"
                "");
                 
      
          }
    
    
      //RSU1
      if(IPsrc == (Ipv4Address("7.0.0.4"))){   
       
      DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=100 "
                " in_port=2"
                " apply:output=ctrl:128");     
			 }
          
          
      //RSU2  
      if(IPsrc == (Ipv4Address("7.0.0.5"))){   
       
       DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=100 "
                " in_port=2"
                " apply:output=ctrl:128");      
          }
      //CAR2    
      if(IPsrc == (Ipv4Address("7.0.0.3"))){   
        
      DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=100"
                " in_port=2"
                 " apply:output=1"); 
      DpctlExecute (swtch, "flow-mod cmd=add,table=0,prio=110 "
                " in_port=2,ts_seq=21"
                "");
                 
          }

  // Create an empty L2SwitchingTable and insert it into m_learnedInfo
  L2Table_t l2Table;
  uint64_t dpId = swtch->GetDpId ();

  std::pair<uint64_t, L2Table_t> entry (dpId, l2Table);
  auto ret = m_learnedInfo.insert (entry);
  if (ret.second == false)
    {
      NS_LOG_ERROR ("Table exists for this datapath.");
    }
}


} // namespace ns3
#endif
