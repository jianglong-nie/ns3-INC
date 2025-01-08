#ifndef ATP_SOCKET_FACTORY_H
#define ATP_SOCKET_FACTORY_H

#include "ns3/socket-factory.h"

#include "ns3/ptr.h"

namespace ns3 {

class ATPL4Protocol;

/**
 * \brief ATP Socket工厂类
 */
class ATPSocketFactory : public SocketFactory
{
  public:
    static TypeId GetTypeId();

    ATPSocketFactory();
    ~ATPSocketFactory() override;

    void SetATP(Ptr<ATPL4Protocol> atp);

    Ptr<Socket> CreateSocket() override;

  protected:
    void DoDispose() override;

  private:
    Ptr<ATPL4Protocol> m_atp; //!< ATP协议实例
};

} // namespace ns3

#endif /* ATP_SOCKET_FACTORY_H */ 