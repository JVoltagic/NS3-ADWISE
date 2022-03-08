#ifndef MQTT_TAG_H
#define MQTT_TAG_H

#include "ns3/tag.h"

namespace ns3
{
    namespace lorawan
    {

        class mqttTag : public Tag
        {
        public:
            // must be implemented to become a valid new header.
            static TypeId GetTypeId(void);
            virtual TypeId GetInstanceTypeId(void) const;

            mqttTag(double mType = 0);

            virtual ~mqttTag();

            virtual uint32_t GetSerializedSize() const;
            virtual void Serialize(TagBuffer i) const;
            virtual void Deserialize(TagBuffer i);
            virtual void Print(std::ostream &os) const;

            // Access the type of message
            void SetType(double mtype);
            double GetType(void) const;

        private:
            double m_type;
            // double m_control;   add later to see how to make the exact mqtt header
        };
    }
}
#endif