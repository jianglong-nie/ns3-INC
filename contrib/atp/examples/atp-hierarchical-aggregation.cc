/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

// Network topology
//
// 四层网络拓扑:
// 第一层(叶子层): n0-n7 (8个节点，运行相同的job1)
// 第二层: s0-s3 (4个节点，每个连接2个叶子节点)
// 第三层: s4-s5 (2个节点，每个连接2个第二层节点)
// 第四层: s6 (1个节点，连接s4和s5)
//
// n0,n1 -> s0
// n2,n3 -> s1
// n4,n5 -> s2
// n6,n7 -> s3
//
// s0,s1 -> s4
// s2,s3 -> s5
//
// s4,s5 -> s6
//
// 所有n0-n7的数据包都发送到s6
// s0-s6都开启聚合功能

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/atp-module.h"
#include "ns3/ptr.h"
#include <fstream>
#include <string>
#include <iostream>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("ATP-Hierarchical-Aggregation");

// 拥塞窗口跟踪，写入txt文件
ofstream cwndStream[8];  // n0-n7的拥塞窗口
ofstream SinkBytesStream;  // s6接收到的字节数

uint64_t lastTimeBytes = 0;

// 记录接收端收到的总字节数
static void
Measurement(Ptr<ATPPacketSink> sink)
{
    Time now = Simulator::Now();
    uint64_t currentTimeBytes = sink->GetTotalRxJob(1);  // job1
    uint64_t ReceivedBytesPer100ms = currentTimeBytes - lastTimeBytes;

    // 记录总字节数和本100ms内接收的字节数
    SinkBytesStream << now.GetSeconds() << "\t" << currentTimeBytes << "\t" << ReceivedBytesPer100ms << std::endl;
    lastTimeBytes = currentTimeBytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MilliSeconds(100), &Measurement, sink);
}

// 拥塞窗口变化回调函数
void CwndChange(uint32_t oldCwnd, uint32_t newCwnd, int nodeId)
{
    cwndStream[nodeId] << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

// 创建一个绑定器来帮助我们绑定第三个参数
void CwndChangeRouter(uint32_t oldCwnd, uint32_t newCwnd, uint32_t nodeId)
{
    CwndChange(oldCwnd, newCwnd, static_cast<int>(nodeId));
}

int main(int argc, char* argv[])
{
    // 打开文件流
    for(int i = 0; i < 8; i++) {
        string filename = "atp-result/trace-hierarchical/n" + to_string(i) + "-cwnd.txt";
        cwndStream[i].open(filename, std::ofstream::out | std::ofstream::trunc);
    }
    SinkBytesStream.open("atp-result/trace-hierarchical/sink-bytes.txt", std::ofstream::out | std::ofstream::trunc);

    uint32_t maxBytes = 248;  // 无限发送
    Time stopTime = Seconds(2);

    // 创建所有节点
    NodeContainer leafNodes;  // n0-n7
    leafNodes.Create(8);
    
    NodeContainer level2Nodes;  // s0-s3
    level2Nodes.Create(4);
    
    NodeContainer level3Nodes;  // s4-s5
    level3Nodes.Create(2);
    
    NodeContainer level4Node;  // s6
    level4Node.Create(1);

    NodeContainer destNode;  // m
    destNode.Create(1);

    // 创建点对点链路配置
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2us"));

    // 创建所有的网络设备容器
    vector<NetDeviceContainer> level1to2Links;  // n0-n7到s0-s3的链路
    vector<NetDeviceContainer> level2to3Links;  // s0-s3到s4-s5的链路
    vector<NetDeviceContainer> level3to4Links;  // s4-s5到s6的链路

    // 安装第一层到第二层的链路
    for(uint32_t i = 0; i < 4; i++) {  // 对于每个s0-s3
        for(uint32_t j = 0; j < 2; j++) {  // 连接2个叶子节点
            NodeContainer nc = NodeContainer(leafNodes.Get(i*2 + j), level2Nodes.Get(i));
            level1to2Links.push_back(pointToPoint.Install(nc));
        }
    }

    // 安装第二层到第三层的链路
    for(uint32_t i = 0; i < 2; i++) {  // 对于s4,s5
        for(uint32_t j = 0; j < 2; j++) {  // 连接2个s0-s3
            NodeContainer nc = NodeContainer(level2Nodes.Get(i*2 + j), level3Nodes.Get(i));
            level2to3Links.push_back(pointToPoint.Install(nc));
        }
    }

    // 安装第三层到第四层的链路
    for(uint32_t i = 0; i < 2; i++) {  // s4,s5到s6
        NodeContainer nc = NodeContainer(level3Nodes.Get(i), level4Node.Get(0));
        level3to4Links.push_back(pointToPoint.Install(nc));
    }

    // 安装s6到m的链路
    NodeContainer s6ToM = NodeContainer(level4Node.Get(0), destNode.Get(0));
    NetDeviceContainer s6mLink = pointToPoint.Install(s6ToM);

    // 安装Internet协议栈
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    
    internet.Install(leafNodes);
    internet.Install(level2Nodes);
    internet.Install(level3Nodes);
    internet.Install(level4Node);
    internet.Install(destNode);  // 为m安装协议栈

    // 分配IP地址
    Ipv4AddressHelper ipv4;
    vector<Ipv4InterfaceContainer> level1to2Interfaces;
    vector<Ipv4InterfaceContainer> level2to3Interfaces;
    vector<Ipv4InterfaceContainer> level3to4Interfaces;

    // 为第一层到第二层分配地址
    for(uint32_t i = 0; i < 8; i++) {
        stringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        level1to2Interfaces.push_back(ipv4.Assign(level1to2Links[i]));
    }

    // 为第二层到第三层分配地址
    for(uint32_t i = 0; i < 4; i++) {
        stringstream subnet;
        subnet << "10.2." << (i+1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        level2to3Interfaces.push_back(ipv4.Assign(level2to3Links[i]));
    }

    // 为第三层到第四层分配地址
    for(uint32_t i = 0; i < 2; i++) {
        stringstream subnet;
        subnet << "10.3." << (i+1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        level3to4Interfaces.push_back(ipv4.Assign(level3to4Links[i]));
    }

    // 为s6到m分配地址
    ipv4.SetBase("10.4.1.0", "255.255.255.0");
    Ipv4InterfaceContainer s6mInterface = ipv4.Assign(s6mLink);

    // 在s0-s5上开启聚合并设置fanindegree
    // s0-s3的fanindegree设置
    uint8_t faninDegrees[] = {
        0b00000011,  // s0
        0b00001100,  // s1
        0b00110000,  // s2
        0b11000000,  // s3
        0b00001111,  // s4
        0b11110000,  // s5
        0b11111111   // s6
    };

    // 为s0-s3设置fanindegree
    for(uint32_t i = 0; i < 4; i++) {
        Ptr<Node> node = level2Nodes.Get(i);
        
        // 设置路由聚合
        Ptr<ATPStaticRouting> routing = 
            DynamicCast<ATPStaticRouting>(node->GetObject<Ipv4>()->GetRoutingProtocol());
        routing->SetEnableAggregation(true);

        // 设置L4协议聚合器的fanindegree
        Ptr<ATPL4Protocol> atpl4 = node->GetObject<ATPL4Protocol>();
        if (atpl4) {
            atpl4->SetFaninDegree(faninDegrees[i]);
        }
    }

    // 为s4-s5设置fanindegree
    for(uint32_t i = 0; i < 2; i++) {
        Ptr<Node> node = level3Nodes.Get(i);
        
        // 设置路由聚合
        Ptr<ATPStaticRouting> routing = 
            DynamicCast<ATPStaticRouting>(node->GetObject<Ipv4>()->GetRoutingProtocol());
        routing->SetEnableAggregation(true);

        // 设置L4协议聚合器的fanindegree
        Ptr<ATPL4Protocol> atpl4 = node->GetObject<ATPL4Protocol>();
        if (atpl4) {
            atpl4->SetFaninDegree(faninDegrees[i+4]);
        }
    }

    // 为s6设置fanindegree和路由
    Ptr<Node> s6Node = level4Node.Get(0);
    Ptr<ATPStaticRouting> s6RoutingFanin = 
        DynamicCast<ATPStaticRouting>(s6Node->GetObject<Ipv4>()->GetRoutingProtocol());
    s6RoutingFanin->SetEnableAggregation(true);

    Ptr<ATPL4Protocol> s6Atpl4 = s6Node->GetObject<ATPL4Protocol>();
    if (s6Atpl4) {
        s6Atpl4->SetFaninDegree(faninDegrees[6]);
    }

    // 在m上创建接收应用
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(s6mInterface.GetAddress(1), sinkPort));
    
    Ptr<ATPPacketSink> sinkApp = CreateObject<ATPPacketSink>();
    sinkApp->SetAddressPort(sinkAddress, sinkPort);

    Ptr<Socket> sinkSocket = Socket::CreateSocket(destNode.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket = DynamicCast<ATPSocket>(sinkSocket);
    sinkApp->SetSocket(sinkATPSocket);
    sinkATPSocket->Bind(sinkAddress);
    sinkATPSocket->Listen();

    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(stopTime);
    destNode.Get(0)->AddApplication(sinkApp);

    // 在n0-n7上创建发送应用
    vector<Ptr<ATPBulkSendApplication>> senderApps;
    vector<Ptr<ATPSocket>> senderSockets;
    uint16_t sendPort = 11;

    for(uint32_t i = 0; i < 8; i++) {
        // 创建发送应用
        Ptr<ATPBulkSendApplication> app = CreateObject<ATPBulkSendApplication>();
        
        // 创建socket
        Ptr<Socket> socket = Socket::CreateSocket(leafNodes.Get(i), ATPSocketFactory::GetTypeId());
        Ptr<ATPSocket> atpSocket = DynamicCast<ATPSocket>(socket);
        
        // 设置地址
        Address senderAddress(InetSocketAddress(level1to2Interfaces[i].GetAddress(0), sendPort));
        
        // 配置应用
        app->Setup(sinkAddress, atpSocket, maxBytes, 1);  // jobId = 1
        app->SetEnableATPTag(true);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(stopTime);
        app->SetWorkerId(1 << i);  // 每个发送者有唯一的workerId

        // 设置socket回调
        atpSocket->SetConnectCallback(
            MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, app),
            MakeCallback(&ATPBulkSendApplication::ConnectionFailed, app));
        atpSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, app));
        
        // 绑定和连接socket
        atpSocket->Bind(senderAddress);
        atpSocket->Connect(sinkAddress);

        // 设置拥塞窗口跟踪
        atpSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
            MakeCallback(&CwndChangeRouter).Bind(i));

        // 安装应用
        leafNodes.Get(i)->AddApplication(app);
        
        // 保存应用和socket的引用
        senderApps.push_back(app);
        senderSockets.push_back(atpSocket);

        // 添加地址映射到接收端
        sinkATPSocket->AddAddressMapping(1, level1to2Interfaces[i].GetAddress(0), sendPort);
    }

    // 配置叶子节点(n0-n7)的路由
    // n0,n1 -> s0
    // n2,n3 -> s1
    // n4,n5 -> s2
    // n6,n7 -> s3
    for(uint32_t i = 0; i < 8; i++) {
        Ptr<Node> node = leafNodes.Get(i);
        Ptr<ATPStaticRouting> routing = 
            DynamicCast<ATPStaticRouting>(node->GetObject<Ipv4>()->GetRoutingProtocol());
        // 到m的路由：通过对应的s0-s3转发
        routing->AddHostRouteTo(s6mInterface.GetAddress(1),  // m的地址
                               level1to2Interfaces[i].GetAddress(1),    // 对应的s0-s3的地址
                               1);
    }

    // 配置第二层节点(s0-s3)的路由
    // s0,s1 -> s4
    // s2,s3 -> s5
    for(uint32_t i = 0; i < 4; i++) {
        Ptr<Node> node = level2Nodes.Get(i);
        Ptr<ATPStaticRouting> routing = 
            DynamicCast<ATPStaticRouting>(node->GetObject<Ipv4>()->GetRoutingProtocol());
        routing->SetEnableAggregation(true);

        // 到m的路由：通过s4或s5转发
        if (i < 2) {  // s0,s1 -> s4
            routing->AddHostRouteTo(s6mInterface.GetAddress(1),  // m的地址
                                  level2to3Interfaces[0].GetAddress(1),  // s4的地址
                                  2);  // 使用接口2
        } else {      // s2,s3 -> s5
            routing->AddHostRouteTo(s6mInterface.GetAddress(1),  // m的地址
                                  level2to3Interfaces[1].GetAddress(1),  // s5的地址
                                  2);  // 使用接口2
        }

        // 到n0-n7的返回路由
        for(uint32_t j = 0; j < 2; j++) {
            routing->AddHostRouteTo(level1to2Interfaces[i*2+j].GetAddress(0),  // 叶子节点地址
                                  level1to2Interfaces[i*2+j].GetAddress(0),    // 直接转发
                                  j);  // 使用对应接口
        }
    }

    // 配置第三层节点(s4-s5)的路由
    // s4,s5 -> s6
    for(uint32_t i = 0; i < 2; i++) {
        Ptr<Node> node = level3Nodes.Get(i);
        Ptr<ATPStaticRouting> routing = 
            DynamicCast<ATPStaticRouting>(node->GetObject<Ipv4>()->GetRoutingProtocol());
        routing->SetEnableAggregation(true);

        // 到m的路由：通过s6转发
        routing->AddHostRouteTo(s6mInterface.GetAddress(1),  // m的地址
                               level3to4Interfaces[i].GetAddress(1),  // s6的地址
                               2);  // 使用接口2

        // 到n0-n7的返回路由：通过s0-s3转发
        for(uint32_t j = 0; j < 2; j++) {
            routing->AddHostRouteTo(level2to3Interfaces[i*2+j].GetAddress(0),  // s0-s3地址
                                  level2to3Interfaces[i*2+j].GetAddress(0),    // 直接转发
                                  j);  // 使用对应接口
        }
    }

    // 配置s6的路由
    // s6 -> m
    Ptr<Node> s6NodeRouting = level4Node.Get(0);
    Ptr<ATPStaticRouting> s6Routing = 
        DynamicCast<ATPStaticRouting>(s6NodeRouting->GetObject<Ipv4>()->GetRoutingProtocol());
    s6Routing->SetEnableAggregation(true);

    // s6到m的路由
    s6Routing->AddHostRouteTo(s6mInterface.GetAddress(1),  // m的地址
                             s6mInterface.GetAddress(1),    // 直接转发
                             2);  // 使用接口2

    // s6到s4,s5的返回路由
    for(uint32_t i = 0; i < 2; i++) {
        s6Routing->AddHostRouteTo(level3to4Interfaces[i].GetAddress(0),  // s4或s5地址
                                level3to4Interfaces[i].GetAddress(0),    // 直接转发
                                i);  // 使用对应接口
    }

    // 配置m的路由
    Ptr<Node> mNode = destNode.Get(0);
    Ptr<ATPStaticRouting> mRouting = 
        DynamicCast<ATPStaticRouting>(mNode->GetObject<Ipv4>()->GetRoutingProtocol());
    mRouting->SetEnableAggregation(true);

    // m到s6的返回路由
    mRouting->AddHostRouteTo(s6mInterface.GetAddress(0),  // s6地址
                            s6mInterface.GetAddress(0),    // 直接转发
                            0);  // 使用接口0

    // 开始测量
    Simulator::Schedule(MilliSeconds(100), &Measurement, sinkApp);

    // 运行仿真
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();

    // 关闭文件流
    for(int i = 0; i < 8; i++) {
        cwndStream[i].close();
    }
    SinkBytesStream.close();

    std::cout << "Total Bytes Received: " << sinkApp->GetTotalRx() << std::endl;

    return 0;
}
