#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/atp-module.h"
#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ATP-V1-2jobs");

std::ofstream sendBytesStream_job1;
std::ofstream sendBytesStream_job2;
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;

// 记录job1发送的总字节数
static void
MeasurementTxJob1(Ptr<ATPSocket> socket)
{
    Time now = Simulator::Now();
    uint64_t currentTimeJob1Bytes = socket->GetTotalTxBytes();
    uint64_t SendJob1BytesPer100us = currentTimeJob1Bytes - lastTimeJob1Bytes;
    
    sendBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << SendJob1BytesPer100us << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
    Simulator::Schedule(MicroSeconds(100), &MeasurementTxJob1, socket);
}

// 记录job2发送的总字节数
static void
MeasurementTxJob2(Ptr<ATPSocket> socket)
{
    Time now = Simulator::Now();
    uint64_t currentTimeJob2Bytes = socket->GetTotalTxBytes();
    uint64_t SendJob2BytesPer100us = currentTimeJob2Bytes - lastTimeJob2Bytes;
    
    sendBytesStream_job2 << now.GetMicroSeconds() << "\t" << currentTimeJob2Bytes << "\t" << SendJob2BytesPer100us << std::endl;
    
    lastTimeJob2Bytes = currentTimeJob2Bytes;
    Simulator::Schedule(MicroSeconds(100), &MeasurementTxJob2, socket);
}

int
main(int argc, char* argv[])
{
    LogComponentEnable("ATP-V1-2jobs", LOG_LEVEL_INFO);

    sendBytesStream_job1.open("atp-result/trace-atp-result/job1-sendBytes-v1-2jobs.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_job2.open("atp-result/trace-atp-result/job2-sendBytes-v1-2jobs.txt", std::ofstream::out | std::ofstream::trunc);

    Time startTime = Seconds(1.0);
    Time stopTime = Seconds(1.0) + MicroSeconds(200);

    NS_LOG_INFO("Build topology");
    NodeContainer nodes;
    nodes.Create(13);  // n0-n12

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1us"));

    // 创建各个连接的设备容器
    // n0-n3 连接到 n8
    NetDeviceContainer link0_8 = pointToPoint.Install(nodes.Get(0), nodes.Get(8));
    NetDeviceContainer link1_8 = pointToPoint.Install(nodes.Get(1), nodes.Get(8));
    NetDeviceContainer link2_8 = pointToPoint.Install(nodes.Get(2), nodes.Get(8));
    NetDeviceContainer link3_8 = pointToPoint.Install(nodes.Get(3), nodes.Get(8));
    
    // n4-n7 连接到 n9
    NetDeviceContainer link4_9 = pointToPoint.Install(nodes.Get(4), nodes.Get(9));
    NetDeviceContainer link5_9 = pointToPoint.Install(nodes.Get(5), nodes.Get(9));
    NetDeviceContainer link6_9 = pointToPoint.Install(nodes.Get(6), nodes.Get(9));
    NetDeviceContainer link7_9 = pointToPoint.Install(nodes.Get(7), nodes.Get(9));
    
    // n8, n9 连接到 n10
    NetDeviceContainer link8_10 = pointToPoint.Install(nodes.Get(8), nodes.Get(10));
    NetDeviceContainer link9_10 = pointToPoint.Install(nodes.Get(9), nodes.Get(10));
    
    // n10 连接到 n11, n12
    NetDeviceContainer link10_11 = pointToPoint.Install(nodes.Get(10), nodes.Get(11));
    NetDeviceContainer link10_12 = pointToPoint.Install(nodes.Get(10), nodes.Get(12));

    // 为关键链路设置ECN和阈值
    Ptr<PointToPointNetDevice> n8_n10_dev = DynamicCast<PointToPointNetDevice>(link8_10.Get(0));
    n8_n10_dev->SetThreshold(80);
    n8_n10_dev->SetEnableEcn(true);

    Ptr<PointToPointNetDevice> n9_n10_dev = DynamicCast<PointToPointNetDevice>(link9_10.Get(0));
    n9_n10_dev->SetThreshold(80);
    n9_n10_dev->SetEnableEcn(true);

    Ptr<PointToPointNetDevice> n10_n11_dev = DynamicCast<PointToPointNetDevice>(link10_11.Get(0));
    n10_n11_dev->SetThreshold(80);
    n10_n11_dev->SetEnableEcn(true);

    Ptr<PointToPointNetDevice> n10_n12_dev = DynamicCast<PointToPointNetDevice>(link10_12.Get(0));
    n10_n12_dev->SetThreshold(80);
    n10_n12_dev->SetEnableEcn(true);

    NS_LOG_INFO("Install internet stack");
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    internet.Install(nodes);

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Helper;
    
    // 为每个链路分配IP地址
    Ipv4InterfaceContainer interfaces0_8, interfaces1_8, interfaces2_8, interfaces3_8;
    Ipv4InterfaceContainer interfaces4_9, interfaces5_9, interfaces6_9, interfaces7_9;
    Ipv4InterfaceContainer interfaces8_10, interfaces9_10;
    Ipv4InterfaceContainer interfaces10_11, interfaces10_12;
    
    ipv4Helper.SetBase("10.1.1.0", "255.255.255.0");
    interfaces0_8 = ipv4Helper.Assign(link0_8);
    
    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    interfaces1_8 = ipv4Helper.Assign(link1_8);
    
    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    interfaces2_8 = ipv4Helper.Assign(link2_8);
    
    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    interfaces3_8 = ipv4Helper.Assign(link3_8);
    
    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    interfaces4_9 = ipv4Helper.Assign(link4_9);
    
    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    interfaces5_9 = ipv4Helper.Assign(link5_9);
    
    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    interfaces6_9 = ipv4Helper.Assign(link6_9);
    
    ipv4Helper.SetBase("10.1.8.0", "255.255.255.0");
    interfaces7_9 = ipv4Helper.Assign(link7_9);
    
    ipv4Helper.SetBase("10.1.9.0", "255.255.255.0");
    interfaces8_10 = ipv4Helper.Assign(link8_10);
    
    ipv4Helper.SetBase("10.1.10.0", "255.255.255.0");
    interfaces9_10 = ipv4Helper.Assign(link9_10);
    
    ipv4Helper.SetBase("10.1.11.0", "255.255.255.0");
    interfaces10_11 = ipv4Helper.Assign(link10_11);
    
    ipv4Helper.SetBase("10.1.12.0", "255.255.255.0");
    interfaces10_12 = ipv4Helper.Assign(link10_12);

    NS_LOG_INFO("Create PacketSink Applications for Job1 and Job2.");
    
    // Job1 Sink on n11
    Ptr<ATPPacketSink> sinkApp_job1 = CreateObject<ATPPacketSink>();
    uint16_t sinkPort1 = 9;
    Address sinkAddress1(InetSocketAddress(interfaces10_11.GetAddress(1), sinkPort1));
    sinkApp_job1->SetAddressPort(sinkAddress1, sinkPort1);
    
    Ptr<Socket> sinkSocket1 = Socket::CreateSocket(nodes.Get(11), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket1 = DynamicCast<ATPSocket>(sinkSocket1);
    sinkApp_job1->SetSocket(sinkATPSocket1);
    sinkATPSocket1->Bind(sinkAddress1);
    sinkATPSocket1->Listen();
    
    // 添加Job1的地址映射，用于发送multi-ack
    uint16_t sendPort = 11;
    sinkATPSocket1->AddAddressMapping(1, interfaces0_8.GetAddress(0), sendPort);  // job1 - n0
    sinkATPSocket1->AddAddressMapping(1, interfaces1_8.GetAddress(0), sendPort);  // job1 - n1
    sinkATPSocket1->AddAddressMapping(1, interfaces4_9.GetAddress(0), sendPort);  // job1 - n4
    sinkATPSocket1->AddAddressMapping(1, interfaces5_9.GetAddress(0), sendPort);  // job1 - n5
    
    sinkApp_job1->SetStartTime(Seconds(0.0));
    sinkApp_job1->SetStopTime(stopTime);
    nodes.Get(11)->AddApplication(sinkApp_job1);

    // Job2 Sink on n12
    Ptr<ATPPacketSink> sinkApp_job2 = CreateObject<ATPPacketSink>();
    uint16_t sinkPort2 = 10;
    Address sinkAddress2(InetSocketAddress(interfaces10_12.GetAddress(1), sinkPort2));
    sinkApp_job2->SetAddressPort(sinkAddress2, sinkPort2);
    
    Ptr<Socket> sinkSocket2 = Socket::CreateSocket(nodes.Get(12), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket2 = DynamicCast<ATPSocket>(sinkSocket2);
    sinkApp_job2->SetSocket(sinkATPSocket2);
    sinkATPSocket2->Bind(sinkAddress2);
    sinkATPSocket2->Listen();
    
    // 添加Job2的地址映射，用于发送multi-ack
    sinkATPSocket2->AddAddressMapping(2, interfaces2_8.GetAddress(0), sendPort);  // job2 - n2
    sinkATPSocket2->AddAddressMapping(2, interfaces3_8.GetAddress(0), sendPort);  // job2 - n3
    sinkATPSocket2->AddAddressMapping(2, interfaces6_9.GetAddress(0), sendPort);  // job2 - n6
    sinkATPSocket2->AddAddressMapping(2, interfaces7_9.GetAddress(0), sendPort);  // job2 - n7
    
    sinkApp_job2->SetStartTime(Seconds(0.0));
    sinkApp_job2->SetStopTime(stopTime);
    nodes.Get(12)->AddApplication(sinkApp_job2);

    NS_LOG_INFO("Create BulkSend Applications for Job1 and Job2.");
    
    // Job1的发送节点: n0, n1, n4, n5
    vector<Ptr<Socket>> socketPtrList_job1;
    vector<Ptr<ATPSocket>> atpSocketPtrList_job1;
    vector<Ptr<ATPBulkSendApplication>> atpBulkSendAppPtrList_job1;
    
    uint32_t initCwnd = 1;
    uint32_t maxBytes = 0;
    
    // Job1 senders配置
    int job1_nodes[] = {0, 1, 4, 5};
    Ipv4InterfaceContainer* job1_interfaces[] = {&interfaces0_8, &interfaces1_8, &interfaces4_9, &interfaces5_9};
    
    uint8_t fanInDegree0_job1 = 2;  // n8和n9各2个
    uint8_t fanInDegree1_job1 = 2;  // n10有2个来源(n8, n9)
    
    for (int i = 0; i < 4; i++) {
        int nodeId = job1_nodes[i];
        Ptr<Socket> sock = Socket::CreateSocket(nodes.Get(nodeId), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> atpSock = DynamicCast<ATPSocket>(sock);
        Ptr<ATPBulkSendApplication> bulkApp = CreateObject<ATPBulkSendApplication>();
        
        bulkApp->Setup(sinkAddress1, atpSock, maxBytes, 1);  // jobId = 1
        bulkApp->SetEnableATPTag(true);
        bulkApp->SetStartTime(startTime);
        bulkApp->SetFaninDegree0(fanInDegree0_job1);
        bulkApp->SetFaninDegree1(fanInDegree1_job1);
        
        // bitmap配置: 前两个节点(0,1)连到n8, 后两个(4,5)连到n9
        uint32_t bitmap0 = 1u << (i % 2);  // 在各自聚合点的位置
        uint32_t bitmap1 = 1u << (i / 2);  // 0,1属于第0组(n8), 4,5属于第1组(n9)
        bulkApp->SetBitmap0(bitmap0);
        bulkApp->SetBitmap1(bitmap1);

        // 设置超时参数（可选）
        atpSock->SetRetxTimeout(MicroSeconds(12));  // rtt = 6us
        atpSock->SetRetxCheckInterval(MicroSeconds(3));  // 每3us检查一次
        
        atpSock->SetInitCwnd(initCwnd);
        atpSock->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, bulkApp),
                                     MakeCallback(&ATPBulkSendApplication::ConnectionFailed, bulkApp));
        atpSock->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, bulkApp));
        // 必须绑定IP地址和端口，这样ACK才能正确返回
        Address bindAddr = InetSocketAddress(job1_interfaces[i]->GetAddress(0), sendPort);
        atpSock->Bind(bindAddr);
        atpSock->Connect(sinkAddress1);
        
        nodes.Get(nodeId)->AddApplication(bulkApp);
        
        socketPtrList_job1.push_back(sock);
        atpSocketPtrList_job1.push_back(atpSock);
        atpBulkSendAppPtrList_job1.push_back(bulkApp);
    }
    
    // Job2的发送节点: n2, n3, n6, n7
    vector<Ptr<Socket>> socketPtrList_job2;
    vector<Ptr<ATPSocket>> atpSocketPtrList_job2;
    vector<Ptr<ATPBulkSendApplication>> atpBulkSendAppPtrList_job2;
    
    int job2_nodes[] = {2, 3, 6, 7};
    Ipv4InterfaceContainer* job2_interfaces[] = {&interfaces2_8, &interfaces3_8, &interfaces6_9, &interfaces7_9};
    
    uint8_t fanInDegree0_job2 = 2;
    uint8_t fanInDegree1_job2 = 2;
    
    for (int i = 0; i < 4; i++) {
        int nodeId = job2_nodes[i];
        Ptr<Socket> sock = Socket::CreateSocket(nodes.Get(nodeId), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> atpSock = DynamicCast<ATPSocket>(sock);
        Ptr<ATPBulkSendApplication> bulkApp = CreateObject<ATPBulkSendApplication>();
        
        bulkApp->Setup(sinkAddress2, atpSock, maxBytes, 2);  // jobId = 2
        bulkApp->SetEnableATPTag(true);
        bulkApp->SetStartTime(startTime);
        bulkApp->SetFaninDegree0(fanInDegree0_job2);
        bulkApp->SetFaninDegree1(fanInDegree1_job2);
        
        // bitmap配置: 前两个节点(2,3)连到n8, 后两个(6,7)连到n9
        uint32_t bitmap0 = 1u << (i % 2);
        uint32_t bitmap1 = 1u << (i / 2);
        bulkApp->SetBitmap0(bitmap0);
        bulkApp->SetBitmap1(bitmap1);
        
        // 设置超时参数（可选）
        atpSock->SetRetxTimeout(MicroSeconds(12));  // rtt = 6us
        atpSock->SetRetxCheckInterval(MicroSeconds(3));  // 每3us检查一次
        
        atpSock->SetInitCwnd(initCwnd);
        atpSock->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, bulkApp),
                                     MakeCallback(&ATPBulkSendApplication::ConnectionFailed, bulkApp));
        atpSock->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, bulkApp));
        // 必须绑定IP地址和端口，这样ACK才能正确返回
        Address bindAddr = InetSocketAddress(job2_interfaces[i]->GetAddress(0), sendPort);
        atpSock->Bind(bindAddr);
        atpSock->Connect(sinkAddress2);
        
        nodes.Get(nodeId)->AddApplication(bulkApp);
        
        socketPtrList_job2.push_back(sock);
        atpSocketPtrList_job2.push_back(atpSock);
        atpBulkSendAppPtrList_job2.push_back(bulkApp);
    }

    NS_LOG_INFO("Configure Static Routing.");
    ATPStaticRoutingHelper staticRoutingHelper;
    
    // 获取所有节点的路由表
    Ptr<ATPStaticRouting> staticRouting[13];
    for (int i = 0; i < 13; i++) {
        staticRouting[i] = staticRoutingHelper.GetStaticRouting(nodes.Get(i)->GetObject<Ipv4>());
    }
    
    // 在n8, n9, n10上开启聚合
    staticRouting[8]->SetEnableAggregation(true);
    staticRouting[9]->SetEnableAggregation(true);
    staticRouting[10]->SetEnableAggregation(true);
    
    // 配置n0-n7到各自sink的路由
    // n0, n1 -> n8 -> n10 -> n11 (job1)
    staticRouting[0]->AddHostRouteTo(interfaces10_11.GetAddress(1), interfaces0_8.GetAddress(1), 1);
    staticRouting[1]->AddHostRouteTo(interfaces10_11.GetAddress(1), interfaces1_8.GetAddress(1), 1);
    
    // n2, n3 -> n8 -> n10 -> n12 (job2)
    staticRouting[2]->AddHostRouteTo(interfaces10_12.GetAddress(1), interfaces2_8.GetAddress(1), 1);
    staticRouting[3]->AddHostRouteTo(interfaces10_12.GetAddress(1), interfaces3_8.GetAddress(1), 1);
    
    // n4, n5 -> n9 -> n10 -> n11 (job1)
    staticRouting[4]->AddHostRouteTo(interfaces10_11.GetAddress(1), interfaces4_9.GetAddress(1), 1);
    staticRouting[5]->AddHostRouteTo(interfaces10_11.GetAddress(1), interfaces5_9.GetAddress(1), 1);
    
    // n6, n7 -> n9 -> n10 -> n12 (job2)
    staticRouting[6]->AddHostRouteTo(interfaces10_12.GetAddress(1), interfaces6_9.GetAddress(1), 1);
    staticRouting[7]->AddHostRouteTo(interfaces10_12.GetAddress(1), interfaces7_9.GetAddress(1), 1);
    
    // 配置n8的路由
    staticRouting[8]->AddHostRouteTo(interfaces0_8.GetAddress(0), interfaces0_8.GetAddress(0), 1);
    staticRouting[8]->AddHostRouteTo(interfaces1_8.GetAddress(0), interfaces1_8.GetAddress(0), 2);
    staticRouting[8]->AddHostRouteTo(interfaces2_8.GetAddress(0), interfaces2_8.GetAddress(0), 3);
    staticRouting[8]->AddHostRouteTo(interfaces3_8.GetAddress(0), interfaces3_8.GetAddress(0), 4);
    staticRouting[8]->AddHostRouteTo(interfaces10_11.GetAddress(1), interfaces8_10.GetAddress(1), 5);
    staticRouting[8]->AddHostRouteTo(interfaces10_12.GetAddress(1), interfaces8_10.GetAddress(1), 5);
    
    // 配置n9的路由
    staticRouting[9]->AddHostRouteTo(interfaces4_9.GetAddress(0), interfaces4_9.GetAddress(0), 1);
    staticRouting[9]->AddHostRouteTo(interfaces5_9.GetAddress(0), interfaces5_9.GetAddress(0), 2);
    staticRouting[9]->AddHostRouteTo(interfaces6_9.GetAddress(0), interfaces6_9.GetAddress(0), 3);
    staticRouting[9]->AddHostRouteTo(interfaces7_9.GetAddress(0), interfaces7_9.GetAddress(0), 4);
    staticRouting[9]->AddHostRouteTo(interfaces10_11.GetAddress(1), interfaces9_10.GetAddress(1), 5);
    staticRouting[9]->AddHostRouteTo(interfaces10_12.GetAddress(1), interfaces9_10.GetAddress(1), 5);
    
    // 配置n10的路由
    staticRouting[10]->AddHostRouteTo(interfaces0_8.GetAddress(0), interfaces8_10.GetAddress(0), 1);
    staticRouting[10]->AddHostRouteTo(interfaces1_8.GetAddress(0), interfaces8_10.GetAddress(0), 1);
    staticRouting[10]->AddHostRouteTo(interfaces2_8.GetAddress(0), interfaces8_10.GetAddress(0), 1);
    staticRouting[10]->AddHostRouteTo(interfaces3_8.GetAddress(0), interfaces8_10.GetAddress(0), 1);
    staticRouting[10]->AddHostRouteTo(interfaces4_9.GetAddress(0), interfaces9_10.GetAddress(0), 2);
    staticRouting[10]->AddHostRouteTo(interfaces5_9.GetAddress(0), interfaces9_10.GetAddress(0), 2);
    staticRouting[10]->AddHostRouteTo(interfaces6_9.GetAddress(0), interfaces9_10.GetAddress(0), 2);
    staticRouting[10]->AddHostRouteTo(interfaces7_9.GetAddress(0), interfaces9_10.GetAddress(0), 2);
    staticRouting[10]->AddHostRouteTo(interfaces10_11.GetAddress(1), interfaces10_11.GetAddress(1), 3);
    staticRouting[10]->AddHostRouteTo(interfaces10_12.GetAddress(1), interfaces10_12.GetAddress(1), 4);
    
    // 配置n11的反向路由(ACK)
    staticRouting[11]->AddHostRouteTo(interfaces0_8.GetAddress(0), interfaces10_11.GetAddress(0), 1);
    staticRouting[11]->AddHostRouteTo(interfaces1_8.GetAddress(0), interfaces10_11.GetAddress(0), 1);
    staticRouting[11]->AddHostRouteTo(interfaces4_9.GetAddress(0), interfaces10_11.GetAddress(0), 1);
    staticRouting[11]->AddHostRouteTo(interfaces5_9.GetAddress(0), interfaces10_11.GetAddress(0), 1);
    
    // 配置n12的反向路由(ACK)
    staticRouting[12]->AddHostRouteTo(interfaces2_8.GetAddress(0), interfaces10_12.GetAddress(0), 1);
    staticRouting[12]->AddHostRouteTo(interfaces3_8.GetAddress(0), interfaces10_12.GetAddress(0), 1);
    staticRouting[12]->AddHostRouteTo(interfaces6_9.GetAddress(0), interfaces10_12.GetAddress(0), 1);
    staticRouting[12]->AddHostRouteTo(interfaces7_9.GetAddress(0), interfaces10_12.GetAddress(0), 1);

    // 调度测量函数
    Simulator::Schedule(Seconds(1.0), &MeasurementTxJob1, atpSocketPtrList_job1[0]);
    Simulator::Schedule(Seconds(1.0), &MeasurementTxJob2, atpSocketPtrList_job2[0]);

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    std::cout << "Job1 Total Bytes Received: " << sinkApp_job1->GetTotalRx() << std::endl;
    std::cout << "Job2 Total Bytes Received: " << sinkApp_job2->GetTotalRx() << std::endl;

    sendBytesStream_job1.close();
    sendBytesStream_job2.close();

    return 0;
}

