#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/atp-module.h" // atp module
#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ATP-V1-1job");

int
main(int argc, char* argv[])
{
    LogComponentEnable("ATP-V1-1job", LOG_LEVEL_INFO);
    // LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
   // LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);


    Time startTime = Seconds(1.0);
    Time stopTime = Seconds(10.0);
    

    NS_LOG_INFO("Build topology");
    NodeContainer nodes;
    nodes.Create(8);
 
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // 创建各个连接的设备容器
    NetDeviceContainer link0_4 = pointToPoint.Install(nodes.Get(0), nodes.Get(4));
    NetDeviceContainer link1_4 = pointToPoint.Install(nodes.Get(1), nodes.Get(4));
    NetDeviceContainer link2_5 = pointToPoint.Install(nodes.Get(2), nodes.Get(5));
    NetDeviceContainer link3_5 = pointToPoint.Install(nodes.Get(3), nodes.Get(5));
    NetDeviceContainer link4_6 = pointToPoint.Install(nodes.Get(4), nodes.Get(6));
    NetDeviceContainer link5_6 = pointToPoint.Install(nodes.Get(5), nodes.Get(6));
    NetDeviceContainer link6_7 = pointToPoint.Install(nodes.Get(6), nodes.Get(7));

    // 为关键链路设置ECN和阈值（节点4-6, 5-6, 6-7的链路）
    Ptr<PointToPointNetDevice> n4_n6_dev = DynamicCast<PointToPointNetDevice>(link4_6.Get(0)); // 节点4侧设备
    n4_n6_dev->SetThreshold(80);
    n4_n6_dev->SetEnableEcn(true);

    Ptr<PointToPointNetDevice> n5_n6_dev = DynamicCast<PointToPointNetDevice>(link5_6.Get(0)); // 节点5侧设备
    n5_n6_dev->SetThreshold(80);
    n5_n6_dev->SetEnableEcn(true);

    Ptr<PointToPointNetDevice> n6_n7_dev = DynamicCast<PointToPointNetDevice>(link6_7.Get(0)); // 节点6侧设备
    n6_n7_dev->SetThreshold(80);
    n6_n7_dev->SetEnableEcn(true);


    NS_LOG_INFO("Install internet stack");
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    internet.Install(nodes);

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Helper;
    
    // 为每个节点创建独立的接口容器，避免索引混乱
    Ipv4InterfaceContainer interfaces0_4, interfaces1_4, interfaces2_5, interfaces3_5;
    Ipv4InterfaceContainer interfaces4_6, interfaces5_6, interfaces6_7;
    
    // 为每个链路分配IP地址
    ipv4Helper.SetBase("10.1.1.0", "255.255.255.0");
    interfaces0_4 = ipv4Helper.Assign(link0_4);
    
    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    interfaces1_4 = ipv4Helper.Assign(link1_4);
    
    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    interfaces2_5 = ipv4Helper.Assign(link2_5);
    
    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    interfaces3_5 = ipv4Helper.Assign(link3_5);
    
    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    interfaces4_6 = ipv4Helper.Assign(link4_6);
    
    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    interfaces5_6 = ipv4Helper.Assign(link5_6);
    
    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    interfaces6_7 = ipv4Helper.Assign(link6_7);

    NS_LOG_INFO("Create PacketSink Applications.");
    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces6_7.GetAddress(1), sinkPort)); // 节点7的地址（link6_7的第二个地址）
    sinkApp->SetAddressPort(sinkAddress, sinkPort);

    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(7), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    // start sinkApp
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    nodes.Get(7)->AddApplication(sinkApp);

    NS_LOG_INFO("Create BulkSend Applications.");
    
    // 构建socket和bulkapp 的ptr list
    vector<Ptr<Socket>> socketPtrList;
    vector<Ptr<ATPSocket>> atpSocketPtrList;
    vector<Ptr<ATPBulkSendApplication>> atpBulkSendAppPtrList;
    vector<Address> addrList;

    uint16_t sendPort = 11;
    // 为每个发送节点获取正确的地址
    Ipv4InterfaceContainer* nodeInterfaces[4] = {&interfaces0_4, &interfaces1_4, &interfaces2_5, &interfaces3_5};
    for (int i = 0; i < 4; i++) {
        Ptr<Socket> sock = Socket::CreateSocket(nodes.Get(i), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> ATPsock = DynamicCast<ATPSocket>(sock);
        Ptr<ATPBulkSendApplication> bulkApp = CreateObject<ATPBulkSendApplication>();
        // 获取节点i在其对应链路上的地址（索引0）
        Address addr = (InetSocketAddress(nodeInterfaces[i]->GetAddress(0), sendPort));

        socketPtrList.push_back(sock);
        atpSocketPtrList.push_back(ATPsock);
        atpBulkSendAppPtrList.push_back(bulkApp);
        addrList.push_back(addr);
    }

    // 进行各种参数的设置
    uint32_t initCwnd = 1;
    uint32_t jobId = 1;
    uint32_t maxBytes = 100;
    uint8_t fanInDegree0 = 2;
    uint8_t fanInDegree1 = 2;
    uint32_t bitmap0 = 0;
    uint32_t bitmap1 = 0;
    for (int i = 0; i < 4; i++) {
        Ptr<ATPSocket> atpSocketPtr = atpSocketPtrList[i];
        Ptr<ATPBulkSendApplication> atpBulkPtr = atpBulkSendAppPtrList[i];

        atpBulkPtr->Setup(sinkAddress, atpSocketPtr, maxBytes, jobId);
        atpBulkPtr->SetEnableATPTag(true);
        atpBulkPtr->SetStartTime(startTime);
        atpBulkPtr->SetFaninDegree0(fanInDegree0);
        atpBulkPtr->SetFaninDegree1(fanInDegree1);
        bitmap0 = 1u << (i % fanInDegree0);
        bitmap1 = 1u << (i / fanInDegree1);
        atpBulkPtr->SetBitmap0(bitmap0);
        atpBulkPtr->SetBitmap1(bitmap1);

        atpSocketPtr->SetInitCwnd(initCwnd);
        atpSocketPtr->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, atpBulkPtr),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, atpBulkPtr));
        atpSocketPtr->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, atpBulkPtr));
        atpSocketPtr->Bind(nodeInterfaces[i]->GetAddress(0));
        atpSocketPtr->Connect(sinkAddress);

        nodes.Get(i)->AddApplication(atpBulkPtr);
    }

    // 连接拥塞窗口跟踪，待实现

    // 怎么实现路由表的配置
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_n0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n4 = staticRoutingHelper.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n5 = staticRoutingHelper.GetStaticRouting(nodes.Get(5)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n6 = staticRoutingHelper.GetStaticRouting(nodes.Get(6)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_n7 = staticRoutingHelper.GetStaticRouting(nodes.Get(7)->GetObject<Ipv4>());

    staticRouting_n4->SetEnableAggregation(true);
    staticRouting_n5->SetEnableAggregation(true);
    staticRouting_n6->SetEnableAggregation(true);

    // 使用清晰的接口地址引用，避免索引混乱
    // 每个接口容器的索引0是第一个节点，索引1是第二个节点
    
    // 配置n0-n3 到 n7的路由表
    staticRouting_n0->AddHostRouteTo(interfaces6_7.GetAddress(1), interfaces0_4.GetAddress(1), 1);  // 到n7经过n4
    staticRouting_n1->AddHostRouteTo(interfaces6_7.GetAddress(1), interfaces1_4.GetAddress(1), 1);  // 到n7经过n4
    staticRouting_n2->AddHostRouteTo(interfaces6_7.GetAddress(1), interfaces2_5.GetAddress(1), 1);  // 到n7经过n5
    staticRouting_n3->AddHostRouteTo(interfaces6_7.GetAddress(1), interfaces3_5.GetAddress(1), 1);  // 到n7经过n5
    
    staticRouting_n4->AddHostRouteTo(interfaces0_4.GetAddress(0), interfaces0_4.GetAddress(0), 1);   // 到n0
    staticRouting_n4->AddHostRouteTo(interfaces1_4.GetAddress(0), interfaces1_4.GetAddress(0), 2);   // 到n1
    staticRouting_n4->AddHostRouteTo(interfaces6_7.GetAddress(1), interfaces4_6.GetAddress(1), 3);   // 到n7经过n6

    staticRouting_n5->AddHostRouteTo(interfaces2_5.GetAddress(0), interfaces2_5.GetAddress(0), 1);   // 到n2
    staticRouting_n5->AddHostRouteTo(interfaces3_5.GetAddress(0), interfaces3_5.GetAddress(0), 2);   // 到n3
    staticRouting_n5->AddHostRouteTo(interfaces6_7.GetAddress(1), interfaces5_6.GetAddress(1), 3);   // 到n7经过n6

    staticRouting_n6->AddHostRouteTo(interfaces0_4.GetAddress(0), interfaces4_6.GetAddress(0), 1);   // 到n0经过n4
    staticRouting_n6->AddHostRouteTo(interfaces1_4.GetAddress(0), interfaces4_6.GetAddress(0), 1);   // 到n1经过n4 (同一接口)
    staticRouting_n6->AddHostRouteTo(interfaces2_5.GetAddress(0), interfaces5_6.GetAddress(0), 2);   // 到n2经过n5
    staticRouting_n6->AddHostRouteTo(interfaces3_5.GetAddress(0), interfaces5_6.GetAddress(0), 2);   // 到n3经过n5 (同一接口)
    staticRouting_n6->AddHostRouteTo(interfaces6_7.GetAddress(1), interfaces6_7.GetAddress(1), 3);   // 到n7

    staticRouting_n7->AddHostRouteTo(interfaces0_4.GetAddress(0), interfaces6_7.GetAddress(0), 1);   // 到n0经过n6
    staticRouting_n7->AddHostRouteTo(interfaces1_4.GetAddress(0), interfaces6_7.GetAddress(0), 1);   // 到n1经过n6
    staticRouting_n7->AddHostRouteTo(interfaces2_5.GetAddress(0), interfaces6_7.GetAddress(0), 1);   // 到n2经过n6
    staticRouting_n7->AddHostRouteTo(interfaces3_5.GetAddress(0), interfaces6_7.GetAddress(0), 1);   // 到n3经过n6
    
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
