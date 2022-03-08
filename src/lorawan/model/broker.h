#ifndef BROKER_SERVER_H
#define BROKER_SERVER_H

#include "ns3/object.h"
#include "ns3/application.h"
#include "ns3/net-device.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/packet.h"
#include "ns3/lora-device-address.h"
#include "ns3/gateway-status.h"
#include "ns3/network-status.h"
#include "ns3/network-scheduler.h"
#include "ns3/network-controller.h"
#include "ns3/node-container.h"
#include "ns3/log.h"
#include "ns3/class-a-end-device-lorawan-mac.h"

namespace ns3
{
    namespace lorawan
    {

        /**
         * The BrokerServer is an application standing on top of a node equipped with
         * links that connect it with the gateways.
         *
         * This version of the BrokerServer attempts to closely mimic an actual
         * Network Server, by providing as much functionality as possible.
         */
        class BrokerServer : public Application
        {
        public:
            static TypeId GetTypeId(void);

            BrokerServer();
            virtual ~BrokerServer();

            /**
             * Start the NS application.
             */
            void StartApplication(void);

            /**
             * Stop the NS application.
             */
            void StopApplication(void);

            /**
             * Inform the BrokerServer that these nodes are connected to the network.
             *
             * This method will create a DeviceStatus object for each new node, and add
             * it to the list.
             */
            void AddClients(NodeContainer nodes);

            /**
             * Inform the BrokerServer that this node is connected to the network.
             * This method will create a DeviceStatus object for the new node (if it
             * doesn't already exist).
             */
            void AddClient(Ptr<Node> node);

            /**
             * Add this gateway to the list of gateways connected to this NS.
             * Each GW is identified by its Address in the NS-GWs network.
             */
            void AddGateway(Ptr<Node> gateway, Ptr<NetDevice> netDevice);

            /**
             * A NetworkControllerComponent to this BrokerServer instance.
             */
            void AddComponent(Ptr<NetworkControllerComponent> component);

            /**
             * Receive a packet from a gateway.
             * \param packet the received packet
             */
            bool Receive(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol,
                         const Address &address);

            Ptr<NetworkStatus> GetNetworkStatus(void);

            // NEW METHODS BY ME

            /**
             * Subscribe a client to a topic.
             * \param packet the received packet
             */
            void SubscribeToTopic(std::string topic, LoraDeviceAddress address);

            /**
             * Send message to client subcribed to topic.
             * \param packet the received packet
             */
            void SendToSubscribers(std::string topic, std::string MSG, Ptr<Packet> data);

            void SendLora(Ptr<Packet> data, LoraDeviceAddress deviceAddress);

        protected:
            Ptr<NetworkStatus> m_status;
            Ptr<NetworkController> m_controller;
            Ptr<NetworkScheduler> m_scheduler;
            TracedCallback<Ptr<const Packet>> m_receivedPacket;

        private:
            std::map<std::string, std::vector<LoraDeviceAddress>> m_addresses;
            double m_delay;
        };

    } // namespace lorawan

} // namespace ns3
#endif /* BROKER_SERVER_H */
