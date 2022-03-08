#include "ns3/mqtt-tag.h"
#include "ns3/tag.h"
#include "ns3/uinteger.h"

namespace ns3
{
    namespace lorawan
    {

        NS_OBJECT_ENSURE_REGISTERED(mqttTag);

        TypeId
        mqttTag::GetTypeId(void)
        {
            static TypeId tid = TypeId("ns3::mqttTag")
                                    .SetParent<Tag>()
                                    .SetGroupName("lorawan")
                                    .AddConstructor<mqttTag>();
            return tid;
        }

        TypeId
        mqttTag::GetInstanceTypeId(void) const
        {
            return GetTypeId();
        }

        mqttTag::mqttTag(double mType) :
        m_type(mType)
        {
        }

        mqttTag::~mqttTag()
        {
        }

        uint32_t
        mqttTag::GetSerializedSize(void) const
        {
            return sizeof(double);
        }
        void
        mqttTag::Serialize(TagBuffer i) const
        {
            // The 1 byte type of message
            i.WriteDouble(m_type);
        }
        void
        mqttTag::Deserialize(TagBuffer i)
        {   
            m_type = i.ReadDouble();
        }

        void mqttTag::SetType(double mtype)
        {
            m_type = mtype;
        }

        double mqttTag::GetType() const
        { 
            return m_type;
        }

        void
        mqttTag::Print(std::ostream &os) const
        {
            os << "type=" << m_type;
        };
    }
}