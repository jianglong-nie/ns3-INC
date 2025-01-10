#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/atp-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ATPSocketTest");

// 添加统计信息打印函数s
void PrintStats(Ptr<ATPPacketSink> sink)
{
    std::cout << "\nATP传输统计:\n";
    std::cout << "总接收字节数: " << sink->GetTotalRx() << " bytes\n";
}

int main(int argc, char *argv[])
{
    // 启用更多的日志来帮助调试
    //LogComponentEnable("ATPSocketTest", LOG_LEVEL_ALL);
    //LogComponentEnable("ATPSocketFactory", LOG_LEVEL_ALL); // ATP socket factory的日志
    LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);       // ATP socket的日志
    //LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);   // ATP协议的日志
    LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL); // ATPBulkSendApplication的日志
    LogComponentEnable("ATPPacketSink", LOG_LEVEL_ALL);      // ATPPacketSink的日志
    //LogComponentEnable("ATPTxBuffer", LOG_LEVEL_ALL);     // ATPTxBuffer的日志

    // 创建两个节点
    NodeContainer nodes;
    nodes.Create(2);

    // 配置点对点链路
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 安装Internet协议栈
    InternetStackHelper stack;
    stack.Install(nodes);
    // 分配IP地址
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 在接收端创建ATPPacketSink应用
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    ATPPacketSinkHelper atpPacketSinkHelper("ns3::ATPSocketFactory", 
        InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = atpPacketSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(20.0));

    // 在发送端创建ATPBulkSend应用
    uint32_t maxBytes = 100;
    ATPBulkSendHelper source("ns3::ATPSocketFactory",
        InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    source.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    // 添加统计信息收集
    Ptr<ATPPacketSink> sink = DynamicCast<ATPPacketSink> (sinkApps.Get(0));
    
    // 添加回调来打印接收到的字节数
    Simulator::Schedule(Seconds(19.0), &PrintStats, sink);

    // 运行仿真
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}