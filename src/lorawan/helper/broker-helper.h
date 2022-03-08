#ifndef BROKER_HELPER_H
#define BROKER_HELPER_H

#include "ns3/object-factory.h"
#include "ns3/address.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/broker.h"
#include <stdint.h>
#include <string>

namespace ns3 {
namespace lorawan {

/**
 * This class can install Broker Server applications on multiple nodes at once.
 */
class BrokerServerHelper
{
public:
  BrokerServerHelper ();

  ~BrokerServerHelper ();

  void SetAttribute (std::string name, const AttributeValue &value);

  ApplicationContainer Install (NodeContainer c);

  ApplicationContainer Install (Ptr<Node> node);

  /**
   * Set which gateways will need to be connected to this NS.
   */
  void SetGateways (NodeContainer gateways);

  /**
   * Set which end devices will be managed by this NS.
   */
  void SetEndDevices (NodeContainer endDevices);

  /**
   * Enable (true) or disable (false) the ADR component in the Network
   * Server created by this helper.
   */
  void EnableAdr (bool enableAdr);

  /**
   * Set the ADR implementation to use in the Broker Server created
   * by this helper.
   */
  void SetAdr (std::string type);

private:
  void InstallComponents (Ptr<BrokerServer> netServer);
  Ptr<Application> InstallPriv (Ptr<Node> node);

  ObjectFactory m_factory;

  NodeContainer m_gateways;   //!< Set of gateways to connect to this NS

  NodeContainer m_endDevices;   //!< Set of endDevices to connect to this NS

  PointToPointHelper p2pHelper; //!< Helper to create PointToPoint links

  bool m_adrEnabled;

  ObjectFactory m_adrSupportFactory;
};

} // namespace ns3

}
#endif /* BROKER_HELPER_H */
