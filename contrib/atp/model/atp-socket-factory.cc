#include "atp-socket-factory.h"
#include "atp-l4-protocol.h"
#include "ns3/socket.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ATPSocketFactory");
NS_OBJECT_ENSURE_REGISTERED(ATPSocketFactory);

TypeId
ATPSocketFactory::GetTypeId()
{
  static TypeId tid = TypeId("ns3::ATPSocketFactory")
    .SetParent<SocketFactory>()
    .SetGroupName("Internet")
    .AddConstructor<ATPSocketFactory>();
  return tid;
}

ATPSocketFactory::ATPSocketFactory()
  : m_atp(nullptr)
{
    NS_LOG_FUNCTION(this);
}

ATPSocketFactory::~ATPSocketFactory()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(!m_atp);
}

void
ATPSocketFactory::SetATP(Ptr<ATPL4Protocol> atp)
{
    m_atp = atp;
}

Ptr<Socket>
ATPSocketFactory::CreateSocket()
{
    NS_LOG_FUNCTION(this);
    return m_atp->CreateSocket();
}

void
ATPSocketFactory::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_atp = nullptr;
    SocketFactory::DoDispose();
}

} // namespace ns3 