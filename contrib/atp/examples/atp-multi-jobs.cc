/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
//
//        n0     n1
//        |      |
//       ----------
//       | Switch |
//       |        |
//       ----------
//        |      
//        n2                    
//
//
// - Flow from n0 to n1 using BulkSendApplication.
// - Two jobs (job1 and job2) running simultaneously on both n0 and n1
// - Both jobs send data to n2

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/atp-module.h"
#include "ns3/atp-l4-protocol.h"
#include "ns3/atp-socket.h"
#include "ns3/atp-socket-factory.h"

#include <fstream>
#include <string>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ATP-Multi-Jobs");

// 拥塞窗口跟踪流
std::ofstream cwndStream_n0_job1;
std::ofstream cwndStream_n1_job1;
std::ofstream cwndStream_n0_job2;
std::ofstream cwndStream_n1_job2;

// PacketSink端接收到的job1和job2的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;

std::ofstream SinkBytesStream_job1;
std::ofstream SinkBytesStream_job2;

// 记录接收端收到的总字节数
static void
Measurement(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = sink->GetTotalRxJob(1);  // job1
    uint64_t currentTimeJob2Bytes = sink->GetTotalRxJob(2);  // job2
        
    // 100ms内接收到的字节数
    uint64_t ReceivedJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;
    uint64_t ReceivedJob2BytesPer100ms = currentTimeJob2Bytes - lastTimeJob2Bytes;

    // 记录总字节数和本100ms内接收的字节数
    SinkBytesStream_job1 << now.GetSeconds() << "\t" << currentTimeJob1Bytes << "\t" << ReceivedJob1BytesPer100ms << std::endl;
    SinkBytesStream_job2 << now.GetSeconds() << "\t" << currentTimeJob2Bytes << "\t" << ReceivedJob2BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
    lastTimeJob2Bytes = currentTimeJob2Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MilliSeconds(100), &Measurement, sink);
}

static void
CwndChange_n0_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n0_job1 << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_n1_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n1_job1 << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_n0_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n0_job2 << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_n1_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_n1_job2 << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

int
main(int argc, char* argv[])
{
    // 抓取cwnd和datasize和throughput
    cwndStream_n0_job1.open("atp-result/trace-multi-jobs/n0-job1-cwnd-multi-jobs.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n1_job1.open("atp-result/trace-multi-jobs/n1-job1-cwnd-multi-jobs.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n0_job2.open("atp-result/trace-multi-jobs/n0-job2-cwnd-multi-jobs.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_n1_job2.open("atp-result/trace-multi-jobs/n1-job2-cwnd-multi-jobs.txt", std::ofstream::out | std::ofstream::trunc);

    SinkBytesStream_job1.open("atp-result/trace-multi-jobs/n0-job1-sinkBytes-multi-jobs.txt", std::ofstream::out | std::ofstream::trunc);
    SinkBytesStream_job2.open("atp-result/trace-multi-jobs/n0-job2-sinkBytes-multi-jobs.txt", std::ofstream::out | std::ofstream::trunc);

    // 设置最大发送字节数
    uint64_t maxBytes = 0;
    // 设置停止时间
    Time stopTime = Seconds(10.0);

    // Create nodes
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(3);

    NodeContainer csmaSwitch;
    csmaSwitch.Create(1);

    NS_LOG_INFO("Build Topology");
    ATPCsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2us"));

    NetDeviceContainer nodesDevices;
    NetDeviceContainer switchDevices;

    for (int i = 0; i < 3; i++)
    {
        NetDeviceContainer link = csma.Install(NodeContainer(nodes.Get(i), csmaSwitch));
        nodesDevices.Add(link.Get(0));
        switchDevices.Add(link.Get(1));
    }

    // 设置switchDevices的m_threshold
    for (int i = 0; i < 3; i++)
    {
        Ptr<ATPCsmaNetDevice> device = DynamicCast<ATPCsmaNetDevice>(switchDevices.Get(i));
        device->SetThreshold(40);
    }

    // 设置nodesDevices的m_threshold
    for (int i = 0; i < 3; i++)
    {
        Ptr<ATPCsmaNetDevice> device = DynamicCast<ATPCsmaNetDevice>(nodesDevices.Get(i));
        device->SetThreshold(2000);
    }

    // Create the bridge netdevice
    Ptr<Node> switchNode = csmaSwitch.Get(0);
    ATPBridgeHelper bridge;
    bridge.Install(switchNode, switchDevices);

    // 安装internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(nodesDevices);

    NS_LOG_INFO("Create Applications.");
    
    // Create sink application and its socket
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(Ipv4Address("10.1.1.3"), sinkPort));
    
    Ptr<Socket> sinkSocket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);

    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();
    sinkApp->SetSocket(sinkATPSocket);
    sinkApp->SetAddressPort(sinkAddress, sinkPort);
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);

    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    nodes.Get(2)->AddApplication(sinkApp);

    // Create sockets for job1 and job2 on n0 and n1
    Ptr<Socket> n0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n0job2socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> n1job2socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    
    Ptr<ATPSocket> n0job1_ATPSocket = DynamicCast<ATPSocket>(n0job1socket);
    Ptr<ATPSocket> n1job1_ATPSocket = DynamicCast<ATPSocket>(n1job1socket);
    Ptr<ATPSocket> n0job2_ATPSocket = DynamicCast<ATPSocket>(n0job2socket);
    Ptr<ATPSocket> n1job2_ATPSocket = DynamicCast<ATPSocket>(n1job2socket);

    // Create and configure applications for job1
    Ptr<ATPBulkSendApplication> n0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n1job1App = CreateObject<ATPBulkSendApplication>();

    // Configure n0job1App
    n0job1App->Setup(sinkAddress, n0job1_ATPSocket, maxBytes, 1);
    n0job1App->SetEnableATPTag(true);
    n0job1App->SetStartTime(Seconds(1.0));
    n0job1App->SetStopTime(stopTime);
    n0job1App->SetWorkerId(0b00000001);

    n0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n0job1App),
                                    MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n0job1App));
    n0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n0job1App));
    n0job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(0)->AddApplication(n0job1App);

    // Configure n1job1App
    n1job1App->Setup(sinkAddress, n1job1_ATPSocket, maxBytes, 1);
    n1job1App->SetEnableATPTag(true);
    n1job1App->SetStartTime(Seconds(1.0));
    n1job1App->SetStopTime(stopTime);
    n1job1App->SetWorkerId(0b00000010);

    n1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n1job1App),
                                    MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n1job1App));
    n1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n1job1App));
    n1job1_ATPSocket->Connect(sinkAddress);

    nodes.Get(1)->AddApplication(n1job1App);

    // Create and configure applications for job2
    Ptr<ATPBulkSendApplication> n0job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> n1job2App = CreateObject<ATPBulkSendApplication>();

    // Configure n0job2App
    n0job2App->Setup(sinkAddress, n0job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    n0job2App->SetEnableATPTag(true);
    n0job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    n0job2App->SetStopTime(stopTime);
    n0job2App->SetWorkerId(0b00000001);  // Different worker ID for job2

    n0job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n0job2App),
                                    MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n0job2App));
    n0job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n0job2App));
    n0job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(0)->AddApplication(n0job2App);

    // Configure n1job2App
    n1job2App->Setup(sinkAddress, n1job2_ATPSocket, maxBytes, 2);  // Note jobId = 2
    n1job2App->SetEnableATPTag(true);
    n1job2App->SetStartTime(Seconds(1.0));  // Start at the same time as job1
    n1job2App->SetStopTime(stopTime);
    n1job2App->SetWorkerId(0b00000010);  // Different worker ID for job2

    n1job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, n1job2App),
                                    MakeCallback(&ATPBulkSendApplication::ConnectionFailed, n1job2App));
    n1job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, n1job2App));
    n1job2_ATPSocket->Connect(sinkAddress);

    nodes.Get(1)->AddApplication(n1job2App);

    // 连接拥塞窗口跟踪
    n0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n0_job1));
    n1job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n1_job1));
    n0job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n0_job2));
    n1job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_n1_job2));

    // 设置地址映射
    NS_LOG_INFO("Setting up address mapping for jobs");
    Ipv4Address n0Addr("10.1.1.1");
    Ipv4Address n1Addr("10.1.1.2");

    uint16_t senderPort = 11;  // Port for job1
    uint16_t senderPort2 = 12; // Different port for job2

    // 绑定job1的socket
    Address n0Address(InetSocketAddress(n0Addr, senderPort));
    n0job1_ATPSocket->Bind(n0Address);
    n0job1_ATPSocket->Connect(sinkAddress);

    Address n1Address(InetSocketAddress(n1Addr, senderPort));
    n1job1_ATPSocket->Bind(n1Address);
    n1job1_ATPSocket->Connect(sinkAddress);

    // 绑定job2的socket
    Address n0Address2(InetSocketAddress(n0Addr, senderPort2));
    n0job2_ATPSocket->Bind(n0Address2);
    n0job2_ATPSocket->Connect(sinkAddress);

    Address n1Address2(InetSocketAddress(n1Addr, senderPort2));
    n1job2_ATPSocket->Bind(n1Address2);
    n1job2_ATPSocket->Connect(sinkAddress);

    // 更新地址映射
    sinkATPSocket->AddAddressMapping(1, n0Addr, senderPort);  // job1
    sinkATPSocket->AddAddressMapping(1, n1Addr, senderPort);  // job1
    sinkATPSocket->AddAddressMapping(2, n0Addr, senderPort2); // job2
    sinkATPSocket->AddAddressMapping(2, n1Addr, senderPort2); // job2
    NS_LOG_INFO("Address mapping completed");

    // 开始测量
    Simulator::Schedule(MilliSeconds(100), &Measurement, sinkApp);

    // 运行仿真
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime + Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    // 关闭trace文件
    cwndStream_n0_job1.close();
    cwndStream_n1_job1.close();
    cwndStream_n0_job2.close();
    cwndStream_n1_job2.close();
    SinkBytesStream_job1.close();
    SinkBytesStream_job2.close();

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
