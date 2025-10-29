#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/atp-module.h" // atp module
#include "ns3/ptr.h"
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
    Time startTime = Seconds(1.0);
    Time stopTime = Seconds(10.0);
    

    NS_LOG_INFO("Build topology");
    NodeContainer nodes;
    nodes.Create(8);
 
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer deviceList;

    deviceList.Add(pointToPoint.Install(nodes.Get(0), nodes.Get(4)));
    deviceList.Add(pointToPoint.Install(nodes.Get(1), nodes.Get(4)));
    deviceList.Add(pointToPoint.Install(nodes.Get(2), nodes.Get(5)));
    deviceList.Add(pointToPoint.Install(nodes.Get(3), nodes.Get(5)));
    deviceList.Add(pointToPoint.Install(nodes.Get(4), nodes.Get(6)));
    deviceList.Add(pointToPoint.Install(nodes.Get(5), nodes.Get(6)));
    deviceList.Add(pointToPoint.Install(nodes.Get(6), nodes.Get(7)));

    Ptr<PointToPointNetDevice> n4Device = DynamicCast<PointToPointNetDevice>(deviceList.Get(4));
    n4Device->SetThreshold(80);
    n4Device->SetEnableEcn(true);

    Ptr<PointToPointNetDevice> n5Device = DynamicCast<PointToPointNetDevice>(deviceList.Get(5));
    n5Device->SetThreshold(80);
    n5Device->SetEnableEcn(true);

    Ptr<PointToPointNetDevice> n6Device = DynamicCast<PointToPointNetDevice>(deviceList.Get(6));
    n6Device->SetThreshold(80);
    n6Device->SetEnableEcn(true);


    NS_LOG_INFO("Install internet stack");
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    internet.Install(nodes);

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Helper;
    Ipv4InterfaceContainer interfaceList;
    for (int i = 0; i < deviceList.GetN(); i++) {
        char  base[9];
        sprintf(base, "10.1.%d.0", i + 1);
        ipv4Helper.SetBase(base, "255.255.255.0");
        interfaceList.Add(ipv4Helper.Assign(deviceList.Get(i)));
    }

    NS_LOG_INFO("Create PacketSink Applications.");
    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaceList.GetAddress(7), sinkPort));
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
    for (int i = 0; i < 4; i++) {
        Ptr<Socket> sock = Socket::CreateSocket(nodes.Get(i), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> ATPsock = DynamicCast<ATPSocket>(sock);
        Ptr<ATPBulkSendApplication> bulkApp = CreateObject<ATPBulkSendApplication>();
        Address addr = (InetSocketAddress(interfaceList.GetAddress(i), sendPort));

        socketPtrList.push_back(sock);
        atpSocketPtrList.push_back(ATPsock);
        atpBulkSendAppPtrList.push_back(bulkApp);
        addrList.push_back(addr);
    }

    // 进行各种参数的设置
    uint32_t initCwnd = 1;
    uint32_t jobId = 1;
    uint32_t maxBytes = 100;
    uint8_t fanInDegree = 0b00001111;
    for (int i = 0; i < 4; i++) {
        Ptr<ATPSocket> atpSocketPtr = atpSocketPtrList[i];
        Ptr<ATPBulkSendApplication> atpBulkPtr = atpBulkSendAppPtrList[i];

        atpBulkPtr->Setup(sinkAddress, atpSocketPtr, maxBytes, jobId);
        atpBulkPtr->SetEnableATPTag(true);
        atpBulkPtr->SetStartTime(startTime);
        atpBulkPtr->SetFaninDegree(fanInDegree);
        atpBulkPtr->SetWorkerId(1 << i);

        atpSocketPtr->SetInitCwnd(initCwnd);
        atpSocketPtr->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, atpBulkPtr),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, atpBulkPtr));
        atpSocketPtr->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, atpBulkPtr));
        atpSocketPtr->Bind(interfaceList.GetAddress(i));
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

    // 配置n0-n3 到 n7的路由表
    staticRouting_n0->AddHostRouteTo(interfaceList.GetAddress(7), interfaceList.GetAddress(4), 1);
    staticRouting_n1->AddHostRouteTo(interfaceList.GetAddress(7), interfaceList.GetAddress(4), 1);
    staticRouting_n2->AddHostRouteTo(interfaceList.GetAddress(7), interfaceList.GetAddress(5), 1);
    staticRouting_n3->AddHostRouteTo(interfaceList.GetAddress(7), interfaceList.GetAddress(5), 1);
    
    staticRouting_n4->AddHostRouteTo(interfaceList.GetAddress(0), interfaceList.GetAddress(0), 1);
    staticRouting_n4->AddHostRouteTo(interfaceList.GetAddress(1), interfaceList.GetAddress(1), 2);
    staticRouting_n4->AddHostRouteTo(interfaceList.GetAddress(7), interfaceList.GetAddress(6), 3);

    staticRouting_n5->AddHostRouteTo(interfaceList.GetAddress(2), interfaceList.GetAddress(2), 1);
    staticRouting_n5->AddHostRouteTo(interfaceList.GetAddress(3), interfaceList.GetAddress(3), 2);
    staticRouting_n5->AddHostRouteTo(interfaceList.GetAddress(7), interfaceList.GetAddress(6), 3);

    staticRouting_n6->AddHostRouteTo(interfaceList.GetAddress(0), interfaceList.GetAddress(4), 1);
    staticRouting_n6->AddHostRouteTo(interfaceList.GetAddress(1), interfaceList.GetAddress(4), 2);
    staticRouting_n6->AddHostRouteTo(interfaceList.GetAddress(2), interfaceList.GetAddress(5), 3);
    staticRouting_n6->AddHostRouteTo(interfaceList.GetAddress(3), interfaceList.GetAddress(5), 4);
    staticRouting_n6->AddHostRouteTo(interfaceList.GetAddress(7), interfaceList.GetAddress(6), 5);

    staticRouting_n7->AddHostRouteTo(interfaceList.GetAddress(0), interfaceList.GetAddress(6), 1);
    staticRouting_n7->AddHostRouteTo(interfaceList.GetAddress(1), interfaceList.GetAddress(6), 1);
    staticRouting_n7->AddHostRouteTo(interfaceList.GetAddress(2), interfaceList.GetAddress(6), 1);
    staticRouting_n7->AddHostRouteTo(interfaceList.GetAddress(3), interfaceList.GetAddress(6), 1);
    
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}