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

NS_LOG_COMPONENT_DEFINE("ATP-P2P-2w2w");

// 拥塞窗口跟踪，写入txt文件
std::ofstream cwndStream_job1;
std::ofstream cwndStream_job2;

// PacketSink端接收到的job1和job2的字节数
uint64_t lastTimeJob1Bytes = 0;
uint64_t lastTimeJob2Bytes = 0;

std::ofstream sendBytesStream_job1;
std::ofstream sendBytesStream_job2;

// 记录发送端发送的总字节数
static void
MeasurementTxJob1(Ptr<ATPSocket> socket)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob1Bytes = socket->GetTotalTxBytes();  // job1
        
    // 100us内发送的字节数
    uint64_t SendJob1BytesPer100ms = currentTimeJob1Bytes - lastTimeJob1Bytes;

    // 记录总字节数和本100us内发送的字节数
    sendBytesStream_job1 << now.GetMicroSeconds() << "\t" << currentTimeJob1Bytes << "\t" << SendJob1BytesPer100ms << std::endl;
    
    lastTimeJob1Bytes = currentTimeJob1Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &MeasurementTxJob1, socket);
}

static void
MeasurementTxJob2(Ptr<ATPSocket> socket)
{
    Time now = Simulator::Now();

    uint64_t currentTimeJob2Bytes = socket->GetTotalTxBytes();  // job2

    // 100us内接收到的字节数
    uint64_t SendJob2BytesPer100ms = currentTimeJob2Bytes - lastTimeJob2Bytes;

    // 记录总字节数和本100us内发送的字节数
    sendBytesStream_job2 << now.GetMicroSeconds() << "\t" << currentTimeJob2Bytes << "\t" << SendJob2BytesPer100ms << std::endl;
    
    lastTimeJob2Bytes = currentTimeJob2Bytes;
                            
    // 调度下一个测量
    Simulator::Schedule(MicroSeconds(100), &MeasurementTxJob2, socket);
}

static void
CwndChange_job1(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_job1 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

static void
CwndChange_job2(uint32_t oldCwnd, uint32_t newCwnd)
{
    cwndStream_job2 << Simulator::Now().GetMicroSeconds() << "\t" << newCwnd << std::endl;
}

int
main(int argc, char* argv[])
{
    // LogComponentEnable("ATPBulkSendApplication", LOG_LEVEL_ALL);
    // LogComponentEnable("PacketSink", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPSocket", LOG_LEVEL_ALL);
    // LogComponentEnable("ATPL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);

    cwndStream_job1.open("atp-result/trace-ha-twojobs/job1-cwnd-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);
    cwndStream_job2.open("atp-result/trace-ha-twojobs/job2-cwnd-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_job1.open("atp-result/trace-ha-twojobs/job1-sendBytes-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);
    sendBytesStream_job2.open("atp-result/trace-ha-twojobs/job2-sendBytes-trace-ha-twojobs.txt", std::ofstream::out | std::ofstream::trunc);

    uint32_t maxBytes = 100;
    Time stopTime = Seconds(1.0) + MicroSeconds(20000); // 约8us为一个rtt时间

    // 设置job1和job2初始拥塞窗口
    uint64_t initialTimestamp = 1000000;
    uint32_t job1_initCwnd = 1;
    uint32_t job2_initCwnd = 1;

    // 在文件打开后，写入初始拥塞窗口值
    cwndStream_job1 << initialTimestamp << "\t" << job1_initCwnd << std::endl;
    cwndStream_job2 << initialTimestamp << "\t" << job2_initCwnd << std::endl;


    //
    // Explicitly create the nodes required by the topology (shown above).
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(80); // 80 nodes: w0 - w63, s0 - s7, s8 - s15

    NodeContainer w0s0 = NodeContainer(nodes.Get(0), nodes.Get(64));
    NodeContainer w1s0 = NodeContainer(nodes.Get(1), nodes.Get(64));
    NodeContainer w2s0 = NodeContainer(nodes.Get(2), nodes.Get(64));
    NodeContainer w3s0 = NodeContainer(nodes.Get(3), nodes.Get(64));
    NodeContainer w4s0 = NodeContainer(nodes.Get(4), nodes.Get(64));
    NodeContainer w5s0 = NodeContainer(nodes.Get(5), nodes.Get(64));
    NodeContainer w6s0 = NodeContainer(nodes.Get(6), nodes.Get(64));
    NodeContainer w7s0 = NodeContainer(nodes.Get(7), nodes.Get(64));

    NodeContainer w8s1 = NodeContainer(nodes.Get(8), nodes.Get(65));
    NodeContainer w9s1 = NodeContainer(nodes.Get(9), nodes.Get(65));
    NodeContainer w10s1 = NodeContainer(nodes.Get(10), nodes.Get(65));
    NodeContainer w11s1 = NodeContainer(nodes.Get(11), nodes.Get(65));
    NodeContainer w12s1 = NodeContainer(nodes.Get(12), nodes.Get(65));
    NodeContainer w13s1 = NodeContainer(nodes.Get(13), nodes.Get(65));
    NodeContainer w14s1 = NodeContainer(nodes.Get(14), nodes.Get(65));
    NodeContainer w15s1 = NodeContainer(nodes.Get(15), nodes.Get(65));

    NodeContainer w16s2 = NodeContainer(nodes.Get(16), nodes.Get(66));
    NodeContainer w17s2 = NodeContainer(nodes.Get(17), nodes.Get(66));
    NodeContainer w18s2 = NodeContainer(nodes.Get(18), nodes.Get(66));
    NodeContainer w19s2 = NodeContainer(nodes.Get(19), nodes.Get(66));
    NodeContainer w20s2 = NodeContainer(nodes.Get(20), nodes.Get(66));
    NodeContainer w21s2 = NodeContainer(nodes.Get(21), nodes.Get(66));
    NodeContainer w22s2 = NodeContainer(nodes.Get(22), nodes.Get(66));
    NodeContainer w23s2 = NodeContainer(nodes.Get(23), nodes.Get(66));

    NodeContainer w24s3 = NodeContainer(nodes.Get(24), nodes.Get(67));
    NodeContainer w25s3 = NodeContainer(nodes.Get(25), nodes.Get(67));
    NodeContainer w26s3 = NodeContainer(nodes.Get(26), nodes.Get(67));
    NodeContainer w27s3 = NodeContainer(nodes.Get(27), nodes.Get(67));
    NodeContainer w28s3 = NodeContainer(nodes.Get(28), nodes.Get(67));
    NodeContainer w29s3 = NodeContainer(nodes.Get(29), nodes.Get(67));
    NodeContainer w30s3 = NodeContainer(nodes.Get(30), nodes.Get(67));
    NodeContainer w31s3 = NodeContainer(nodes.Get(31), nodes.Get(67));

    NodeContainer w32s4 = NodeContainer(nodes.Get(32), nodes.Get(68));
    NodeContainer w33s4 = NodeContainer(nodes.Get(33), nodes.Get(68));
    NodeContainer w34s4 = NodeContainer(nodes.Get(34), nodes.Get(68));
    NodeContainer w35s4 = NodeContainer(nodes.Get(35), nodes.Get(68));
    NodeContainer w36s4 = NodeContainer(nodes.Get(36), nodes.Get(68));
    NodeContainer w37s4 = NodeContainer(nodes.Get(37), nodes.Get(68));
    NodeContainer w38s4 = NodeContainer(nodes.Get(38), nodes.Get(68));
    NodeContainer w39s4 = NodeContainer(nodes.Get(39), nodes.Get(68));

    NodeContainer w40s5 = NodeContainer(nodes.Get(40), nodes.Get(69));
    NodeContainer w41s5 = NodeContainer(nodes.Get(41), nodes.Get(69));
    NodeContainer w42s5 = NodeContainer(nodes.Get(42), nodes.Get(69));
    NodeContainer w43s5 = NodeContainer(nodes.Get(43), nodes.Get(69));
    NodeContainer w44s5 = NodeContainer(nodes.Get(44), nodes.Get(69));
    NodeContainer w45s5 = NodeContainer(nodes.Get(45), nodes.Get(69));
    NodeContainer w46s5 = NodeContainer(nodes.Get(46), nodes.Get(69));
    NodeContainer w47s5 = NodeContainer(nodes.Get(47), nodes.Get(69));

    NodeContainer w48s6 = NodeContainer(nodes.Get(48), nodes.Get(70));
    NodeContainer w49s6 = NodeContainer(nodes.Get(49), nodes.Get(70));
    NodeContainer w50s6 = NodeContainer(nodes.Get(50), nodes.Get(70));
    NodeContainer w51s6 = NodeContainer(nodes.Get(51), nodes.Get(70));
    NodeContainer w52s6 = NodeContainer(nodes.Get(52), nodes.Get(70));
    NodeContainer w53s6 = NodeContainer(nodes.Get(53), nodes.Get(70));
    NodeContainer w54s6 = NodeContainer(nodes.Get(54), nodes.Get(70));
    NodeContainer w55s6 = NodeContainer(nodes.Get(55), nodes.Get(70));

    NodeContainer w56s7 = NodeContainer(nodes.Get(56), nodes.Get(71));
    NodeContainer w57s7 = NodeContainer(nodes.Get(57), nodes.Get(71));
    NodeContainer w58s7 = NodeContainer(nodes.Get(58), nodes.Get(71));
    NodeContainer w59s7 = NodeContainer(nodes.Get(59), nodes.Get(71));
    NodeContainer w60s7 = NodeContainer(nodes.Get(60), nodes.Get(71));
    NodeContainer w61s7 = NodeContainer(nodes.Get(61), nodes.Get(71));
    NodeContainer w62s7 = NodeContainer(nodes.Get(62), nodes.Get(71));
    NodeContainer w63s7 = NodeContainer(nodes.Get(63), nodes.Get(71));

    NodeContainer s0s8 = NodeContainer(nodes.Get(64), nodes.Get(72));
    NodeContainer s0s9 = NodeContainer(nodes.Get(64), nodes.Get(73));
    NodeContainer s0s10 = NodeContainer(nodes.Get(64), nodes.Get(74));
    NodeContainer s0s11 = NodeContainer(nodes.Get(64), nodes.Get(75));
    NodeContainer s0s12 = NodeContainer(nodes.Get(64), nodes.Get(76));
    NodeContainer s0s13 = NodeContainer(nodes.Get(64), nodes.Get(77));
    NodeContainer s0s14 = NodeContainer(nodes.Get(64), nodes.Get(78));
    NodeContainer s0s15 = NodeContainer(nodes.Get(64), nodes.Get(79));

    NodeContainer s1s8 = NodeContainer(nodes.Get(65), nodes.Get(72));
    NodeContainer s1s9 = NodeContainer(nodes.Get(65), nodes.Get(73));
    NodeContainer s1s10 = NodeContainer(nodes.Get(65), nodes.Get(74));
    NodeContainer s1s11 = NodeContainer(nodes.Get(65), nodes.Get(75));
    NodeContainer s1s12 = NodeContainer(nodes.Get(65), nodes.Get(76));
    NodeContainer s1s13 = NodeContainer(nodes.Get(65), nodes.Get(77));
    NodeContainer s1s14 = NodeContainer(nodes.Get(65), nodes.Get(78));
    NodeContainer s1s15 = NodeContainer(nodes.Get(65), nodes.Get(79));

    NodeContainer s2s8 = NodeContainer(nodes.Get(66), nodes.Get(72));
    NodeContainer s2s9 = NodeContainer(nodes.Get(66), nodes.Get(73));
    NodeContainer s2s10 = NodeContainer(nodes.Get(66), nodes.Get(74));
    NodeContainer s2s11 = NodeContainer(nodes.Get(66), nodes.Get(75));
    NodeContainer s2s12 = NodeContainer(nodes.Get(66), nodes.Get(76));
    NodeContainer s2s13 = NodeContainer(nodes.Get(66), nodes.Get(77));
    NodeContainer s2s14 = NodeContainer(nodes.Get(66), nodes.Get(78));
    NodeContainer s2s15 = NodeContainer(nodes.Get(66), nodes.Get(79));

    NodeContainer s3s8 = NodeContainer(nodes.Get(67), nodes.Get(72));
    NodeContainer s3s9 = NodeContainer(nodes.Get(67), nodes.Get(73));
    NodeContainer s3s10 = NodeContainer(nodes.Get(67), nodes.Get(74));
    NodeContainer s3s11 = NodeContainer(nodes.Get(67), nodes.Get(75));
    NodeContainer s3s12 = NodeContainer(nodes.Get(67), nodes.Get(76));
    NodeContainer s3s13 = NodeContainer(nodes.Get(67), nodes.Get(77));
    NodeContainer s3s14 = NodeContainer(nodes.Get(67), nodes.Get(78));
    NodeContainer s3s15 = NodeContainer(nodes.Get(67), nodes.Get(79));

    NodeContainer s4s8 = NodeContainer(nodes.Get(68), nodes.Get(72));
    NodeContainer s4s9 = NodeContainer(nodes.Get(68), nodes.Get(73));
    NodeContainer s4s10 = NodeContainer(nodes.Get(68), nodes.Get(74));
    NodeContainer s4s11 = NodeContainer(nodes.Get(68), nodes.Get(75));
    NodeContainer s4s12 = NodeContainer(nodes.Get(68), nodes.Get(76));
    NodeContainer s4s13 = NodeContainer(nodes.Get(68), nodes.Get(77));
    NodeContainer s4s14 = NodeContainer(nodes.Get(68), nodes.Get(78));
    NodeContainer s4s15 = NodeContainer(nodes.Get(68), nodes.Get(79));

    NodeContainer s5s8 = NodeContainer(nodes.Get(69), nodes.Get(72));
    NodeContainer s5s9 = NodeContainer(nodes.Get(69), nodes.Get(73));
    NodeContainer s5s10 = NodeContainer(nodes.Get(69), nodes.Get(74));
    NodeContainer s5s11 = NodeContainer(nodes.Get(69), nodes.Get(75));
    NodeContainer s5s12 = NodeContainer(nodes.Get(69), nodes.Get(76));
    NodeContainer s5s13 = NodeContainer(nodes.Get(69), nodes.Get(77));
    NodeContainer s5s14 = NodeContainer(nodes.Get(69), nodes.Get(78));
    NodeContainer s5s15 = NodeContainer(nodes.Get(69), nodes.Get(79));

    NodeContainer s6s8 = NodeContainer(nodes.Get(70), nodes.Get(72));
    NodeContainer s6s9 = NodeContainer(nodes.Get(70), nodes.Get(73));
    NodeContainer s6s10 = NodeContainer(nodes.Get(70), nodes.Get(74));
    NodeContainer s6s11 = NodeContainer(nodes.Get(70), nodes.Get(75));
    NodeContainer s6s12 = NodeContainer(nodes.Get(70), nodes.Get(76));
    NodeContainer s6s13 = NodeContainer(nodes.Get(70), nodes.Get(77));
    NodeContainer s6s14 = NodeContainer(nodes.Get(70), nodes.Get(78));
    NodeContainer s6s15 = NodeContainer(nodes.Get(70), nodes.Get(79));

    NodeContainer s7s8 = NodeContainer(nodes.Get(71), nodes.Get(72));
    NodeContainer s7s9 = NodeContainer(nodes.Get(71), nodes.Get(73));
    NodeContainer s7s10 = NodeContainer(nodes.Get(71), nodes.Get(74));
    NodeContainer s7s11 = NodeContainer(nodes.Get(71), nodes.Get(75));
    NodeContainer s7s12 = NodeContainer(nodes.Get(71), nodes.Get(76));
    NodeContainer s7s13 = NodeContainer(nodes.Get(71), nodes.Get(77));
    NodeContainer s7s14 = NodeContainer(nodes.Get(71), nodes.Get(78));
    NodeContainer s7s15 = NodeContainer(nodes.Get(71), nodes.Get(79));

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point links required by the topology (shown above).
    //
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("3us"));

    NetDeviceContainer dev_w0s0, dev_w1s0, dev_w2s0, dev_w3s0, dev_w4s0, dev_w5s0, dev_w6s0, dev_w7s0;
    NetDeviceContainer dev_w8s1, dev_w9s1, dev_w10s1, dev_w11s1, dev_w12s1, dev_w13s1, dev_w14s1, dev_w15s1;
    NetDeviceContainer dev_w16s2, dev_w17s2, dev_w18s2, dev_w19s2, dev_w20s2, dev_w21s2, dev_w22s2, dev_w23s2;
    NetDeviceContainer dev_w24s3, dev_w25s3, dev_w26s3, dev_w27s3, dev_w28s3, dev_w29s3, dev_w30s3, dev_w31s3;
    NetDeviceContainer dev_w32s4, dev_w33s4, dev_w34s4, dev_w35s4, dev_w36s4, dev_w37s4, dev_w38s4, dev_w39s4;
    NetDeviceContainer dev_w40s5, dev_w41s5, dev_w42s5, dev_w43s5, dev_w44s5, dev_w45s5, dev_w46s5, dev_w47s5;
    NetDeviceContainer dev_w48s6, dev_w49s6, dev_w50s6, dev_w51s6, dev_w52s6, dev_w53s6, dev_w54s6, dev_w55s6;
    NetDeviceContainer dev_w56s7, dev_w57s7, dev_w58s7, dev_w59s7, dev_w60s7, dev_w61s7, dev_w62s7, dev_w63s7;
    
    NetDeviceContainer dev_s0s8, dev_s0s9, dev_s0s10, dev_s0s11, dev_s0s12, dev_s0s13, dev_s0s14, dev_s0s15;
    NetDeviceContainer dev_s1s8, dev_s1s9, dev_s1s10, dev_s1s11, dev_s1s12, dev_s1s13, dev_s1s14, dev_s1s15;
    NetDeviceContainer dev_s2s8, dev_s2s9, dev_s2s10, dev_s2s11, dev_s2s12, dev_s2s13, dev_s2s14, dev_s2s15;
    NetDeviceContainer dev_s3s8, dev_s3s9, dev_s3s10, dev_s3s11, dev_s3s12, dev_s3s13, dev_s3s14, dev_s3s15;
    NetDeviceContainer dev_s4s8, dev_s4s9, dev_s4s10, dev_s4s11, dev_s4s12, dev_s4s13, dev_s4s14, dev_s4s15;
    NetDeviceContainer dev_s5s8, dev_s5s9, dev_s5s10, dev_s5s11, dev_s5s12, dev_s5s13, dev_s5s14, dev_s5s15;
    NetDeviceContainer dev_s6s8, dev_s6s9, dev_s6s10, dev_s6s11, dev_s6s12, dev_s6s13, dev_s6s14, dev_s6s15;
    NetDeviceContainer dev_s7s8, dev_s7s9, dev_s7s10, dev_s7s11, dev_s7s12, dev_s7s13, dev_s7s14, dev_s7s15;

    dev_w0s0 = pointToPoint.Install(w0s0);
    dev_w1s0 = pointToPoint.Install(w1s0);
    dev_w2s0 = pointToPoint.Install(w2s0);
    dev_w3s0 = pointToPoint.Install(w3s0);
    dev_w4s0 = pointToPoint.Install(w4s0);
    dev_w5s0 = pointToPoint.Install(w5s0);
    dev_w6s0 = pointToPoint.Install(w6s0);
    dev_w7s0 = pointToPoint.Install(w7s0);

    dev_w8s1 = pointToPoint.Install(w8s1);
    dev_w9s1 = pointToPoint.Install(w9s1);
    dev_w10s1 = pointToPoint.Install(w10s1);
    dev_w11s1 = pointToPoint.Install(w11s1);
    dev_w12s1 = pointToPoint.Install(w12s1);
    dev_w13s1 = pointToPoint.Install(w13s1);
    dev_w14s1 = pointToPoint.Install(w14s1);
    dev_w15s1 = pointToPoint.Install(w15s1);

    dev_w16s2 = pointToPoint.Install(w16s2);
    dev_w17s2 = pointToPoint.Install(w17s2);
    dev_w18s2 = pointToPoint.Install(w18s2);
    dev_w19s2 = pointToPoint.Install(w19s2);
    dev_w20s2 = pointToPoint.Install(w20s2);
    dev_w21s2 = pointToPoint.Install(w21s2);
    dev_w22s2 = pointToPoint.Install(w22s2);
    dev_w23s2 = pointToPoint.Install(w23s2);

    dev_w24s3 = pointToPoint.Install(w24s3);
    dev_w25s3 = pointToPoint.Install(w25s3);
    dev_w26s3 = pointToPoint.Install(w26s3);
    dev_w27s3 = pointToPoint.Install(w27s3);
    dev_w28s3 = pointToPoint.Install(w28s3);
    dev_w29s3 = pointToPoint.Install(w29s3);
    dev_w30s3 = pointToPoint.Install(w30s3);
    dev_w31s3 = pointToPoint.Install(w31s3);

    dev_w32s4 = pointToPoint.Install(w32s4);
    dev_w33s4 = pointToPoint.Install(w33s4);
    dev_w34s4 = pointToPoint.Install(w34s4);
    dev_w35s4 = pointToPoint.Install(w35s4);
    dev_w36s4 = pointToPoint.Install(w36s4);
    dev_w37s4 = pointToPoint.Install(w37s4);
    dev_w38s4 = pointToPoint.Install(w38s4);
    dev_w39s4 = pointToPoint.Install(w39s4);

    dev_w40s5 = pointToPoint.Install(w40s5);
    dev_w41s5 = pointToPoint.Install(w41s5);
    dev_w42s5 = pointToPoint.Install(w42s5);
    dev_w43s5 = pointToPoint.Install(w43s5);
    dev_w44s5 = pointToPoint.Install(w44s5);
    dev_w45s5 = pointToPoint.Install(w45s5);
    dev_w46s5 = pointToPoint.Install(w46s5);
    dev_w47s5 = pointToPoint.Install(w47s5);

    dev_w48s6 = pointToPoint.Install(w48s6);
    dev_w49s6 = pointToPoint.Install(w49s6);
    dev_w50s6 = pointToPoint.Install(w50s6);
    dev_w51s6 = pointToPoint.Install(w51s6);
    dev_w52s6 = pointToPoint.Install(w52s6);
    dev_w53s6 = pointToPoint.Install(w53s6);
    dev_w54s6 = pointToPoint.Install(w54s6);
    dev_w55s6 = pointToPoint.Install(w55s6);

    dev_w56s7 = pointToPoint.Install(w56s7);
    dev_w57s7 = pointToPoint.Install(w57s7);
    dev_w58s7 = pointToPoint.Install(w58s7);
    dev_w59s7 = pointToPoint.Install(w59s7);
    dev_w60s7 = pointToPoint.Install(w60s7);
    dev_w61s7 = pointToPoint.Install(w61s7);
    dev_w62s7 = pointToPoint.Install(w62s7);
    dev_w63s7 = pointToPoint.Install(w63s7);

    dev_s0s8 = pointToPoint.Install(s0s8);
    dev_s0s9 = pointToPoint.Install(s0s9);
    dev_s0s10 = pointToPoint.Install(s0s10);
    dev_s0s11 = pointToPoint.Install(s0s11);
    dev_s0s12 = pointToPoint.Install(s0s12);
    dev_s0s13 = pointToPoint.Install(s0s13);
    dev_s0s14 = pointToPoint.Install(s0s14);
    dev_s0s15 = pointToPoint.Install(s0s15);

    dev_s1s8 = pointToPoint.Install(s1s8);
    dev_s1s9 = pointToPoint.Install(s1s9);
    dev_s1s10 = pointToPoint.Install(s1s10);
    dev_s1s11 = pointToPoint.Install(s1s11);
    dev_s1s12 = pointToPoint.Install(s1s12);
    dev_s1s13 = pointToPoint.Install(s1s13);
    dev_s1s14 = pointToPoint.Install(s1s14);
    dev_s1s15 = pointToPoint.Install(s1s15);

    dev_s2s8 = pointToPoint.Install(s2s8);
    dev_s2s9 = pointToPoint.Install(s2s9);
    dev_s2s10 = pointToPoint.Install(s2s10);
    dev_s2s11 = pointToPoint.Install(s2s11);
    dev_s2s12 = pointToPoint.Install(s2s12);
    dev_s2s13 = pointToPoint.Install(s2s13);
    dev_s2s14 = pointToPoint.Install(s2s14);
    dev_s2s15 = pointToPoint.Install(s2s15);

    dev_s3s8 = pointToPoint.Install(s3s8);
    dev_s3s9 = pointToPoint.Install(s3s9);
    dev_s3s10 = pointToPoint.Install(s3s10);
    dev_s3s11 = pointToPoint.Install(s3s11);
    dev_s3s12 = pointToPoint.Install(s3s12);
    dev_s3s13 = pointToPoint.Install(s3s13);
    dev_s3s14 = pointToPoint.Install(s3s14);
    dev_s3s15 = pointToPoint.Install(s3s15);

    dev_s4s8 = pointToPoint.Install(s4s8);
    dev_s4s9 = pointToPoint.Install(s4s9);
    dev_s4s10 = pointToPoint.Install(s4s10);
    dev_s4s11 = pointToPoint.Install(s4s11);
    dev_s4s12 = pointToPoint.Install(s4s12);
    dev_s4s13 = pointToPoint.Install(s4s13);
    dev_s4s14 = pointToPoint.Install(s4s14);
    dev_s4s15 = pointToPoint.Install(s4s15);

    dev_s5s8 = pointToPoint.Install(s5s8);
    dev_s5s9 = pointToPoint.Install(s5s9);
    dev_s5s10 = pointToPoint.Install(s5s10);
    dev_s5s11 = pointToPoint.Install(s5s11);
    dev_s5s12 = pointToPoint.Install(s5s12);
    dev_s5s13 = pointToPoint.Install(s5s13);
    dev_s5s14 = pointToPoint.Install(s5s14);
    dev_s5s15 = pointToPoint.Install(s5s15);

    dev_s6s8 = pointToPoint.Install(s6s8);
    dev_s6s9 = pointToPoint.Install(s6s9);
    dev_s6s10 = pointToPoint.Install(s6s10);
    dev_s6s11 = pointToPoint.Install(s6s11);
    dev_s6s12 = pointToPoint.Install(s6s12);
    dev_s6s13 = pointToPoint.Install(s6s13);
    dev_s6s14 = pointToPoint.Install(s6s14);
    dev_s6s15 = pointToPoint.Install(s6s15);

    dev_s7s8 = pointToPoint.Install(s7s8);
    dev_s7s9 = pointToPoint.Install(s7s9);
    dev_s7s10 = pointToPoint.Install(s7s10);
    dev_s7s11 = pointToPoint.Install(s7s11);
    dev_s7s12 = pointToPoint.Install(s7s12);
    dev_s7s13 = pointToPoint.Install(s7s13);
    dev_s7s14 = pointToPoint.Install(s7s14);
    dev_s7s15 = pointToPoint.Install(s7s15);
    
    // 设置s0s2, s1s2, s2ps的队列阈值

    Ptr<PointToPointNetDevice> s0s8Device = DynamicCast<PointToPointNetDevice>(dev_s0s8.Get(0));
    Ptr<PointToPointNetDevice> s0s9Device = DynamicCast<PointToPointNetDevice>(dev_s0s9.Get(0));
    Ptr<PointToPointNetDevice> s0s10Device = DynamicCast<PointToPointNetDevice>(dev_s0s10.Get(0));
    Ptr<PointToPointNetDevice> s0s11Device = DynamicCast<PointToPointNetDevice>(dev_s0s11.Get(0));
    Ptr<PointToPointNetDevice> s0s12Device = DynamicCast<PointToPointNetDevice>(dev_s0s12.Get(0));
    Ptr<PointToPointNetDevice> s0s13Device = DynamicCast<PointToPointNetDevice>(dev_s0s13.Get(0));
    Ptr<PointToPointNetDevice> s0s14Device = DynamicCast<PointToPointNetDevice>(dev_s0s14.Get(0));
    Ptr<PointToPointNetDevice> s0s15Device = DynamicCast<PointToPointNetDevice>(dev_s0s15.Get(0));

    Ptr<PointToPointNetDevice> s1s8Device = DynamicCast<PointToPointNetDevice>(dev_s1s8.Get(0));
    Ptr<PointToPointNetDevice> s1s9Device = DynamicCast<PointToPointNetDevice>(dev_s1s9.Get(0));
    Ptr<PointToPointNetDevice> s1s10Device = DynamicCast<PointToPointNetDevice>(dev_s1s10.Get(0));
    Ptr<PointToPointNetDevice> s1s11Device = DynamicCast<PointToPointNetDevice>(dev_s1s11.Get(0));
    Ptr<PointToPointNetDevice> s1s12Device = DynamicCast<PointToPointNetDevice>(dev_s1s12.Get(0));
    Ptr<PointToPointNetDevice> s1s13Device = DynamicCast<PointToPointNetDevice>(dev_s1s13.Get(0));
    Ptr<PointToPointNetDevice> s1s14Device = DynamicCast<PointToPointNetDevice>(dev_s1s14.Get(0));
    Ptr<PointToPointNetDevice> s1s15Device = DynamicCast<PointToPointNetDevice>(dev_s1s15.Get(0));

    Ptr<PointToPointNetDevice> s2s8Device = DynamicCast<PointToPointNetDevice>(dev_s2s8.Get(0));
    Ptr<PointToPointNetDevice> s2s9Device = DynamicCast<PointToPointNetDevice>(dev_s2s9.Get(0));
    Ptr<PointToPointNetDevice> s2s10Device = DynamicCast<PointToPointNetDevice>(dev_s2s10.Get(0));
    Ptr<PointToPointNetDevice> s2s11Device = DynamicCast<PointToPointNetDevice>(dev_s2s11.Get(0));
    Ptr<PointToPointNetDevice> s2s12Device = DynamicCast<PointToPointNetDevice>(dev_s2s12.Get(0));
    Ptr<PointToPointNetDevice> s2s13Device = DynamicCast<PointToPointNetDevice>(dev_s2s13.Get(0));
    Ptr<PointToPointNetDevice> s2s14Device = DynamicCast<PointToPointNetDevice>(dev_s2s14.Get(0));
    Ptr<PointToPointNetDevice> s2s15Device = DynamicCast<PointToPointNetDevice>(dev_s2s15.Get(0));

    Ptr<PointToPointNetDevice> s3s8Device = DynamicCast<PointToPointNetDevice>(dev_s3s8.Get(0));
    Ptr<PointToPointNetDevice> s3s9Device = DynamicCast<PointToPointNetDevice>(dev_s3s9.Get(0));
    Ptr<PointToPointNetDevice> s3s10Device = DynamicCast<PointToPointNetDevice>(dev_s3s10.Get(0));
    Ptr<PointToPointNetDevice> s3s11Device = DynamicCast<PointToPointNetDevice>(dev_s3s11.Get(0));
    Ptr<PointToPointNetDevice> s3s12Device = DynamicCast<PointToPointNetDevice>(dev_s3s12.Get(0));
    Ptr<PointToPointNetDevice> s3s13Device = DynamicCast<PointToPointNetDevice>(dev_s3s13.Get(0));
    Ptr<PointToPointNetDevice> s3s14Device = DynamicCast<PointToPointNetDevice>(dev_s3s14.Get(0));
    Ptr<PointToPointNetDevice> s3s15Device = DynamicCast<PointToPointNetDevice>(dev_s3s15.Get(0));

    Ptr<PointToPointNetDevice> s4s8Device = DynamicCast<PointToPointNetDevice>(dev_s4s8.Get(0));
    Ptr<PointToPointNetDevice> s4s9Device = DynamicCast<PointToPointNetDevice>(dev_s4s9.Get(0));
    Ptr<PointToPointNetDevice> s4s10Device = DynamicCast<PointToPointNetDevice>(dev_s4s10.Get(0));
    Ptr<PointToPointNetDevice> s4s11Device = DynamicCast<PointToPointNetDevice>(dev_s4s11.Get(0));
    Ptr<PointToPointNetDevice> s4s12Device = DynamicCast<PointToPointNetDevice>(dev_s4s12.Get(0));
    Ptr<PointToPointNetDevice> s4s13Device = DynamicCast<PointToPointNetDevice>(dev_s4s13.Get(0));
    Ptr<PointToPointNetDevice> s4s14Device = DynamicCast<PointToPointNetDevice>(dev_s4s14.Get(0));
    Ptr<PointToPointNetDevice> s4s15Device = DynamicCast<PointToPointNetDevice>(dev_s4s15.Get(0));

    Ptr<PointToPointNetDevice> s5s8Device = DynamicCast<PointToPointNetDevice>(dev_s5s8.Get(0));
    Ptr<PointToPointNetDevice> s5s9Device = DynamicCast<PointToPointNetDevice>(dev_s5s9.Get(0));
    Ptr<PointToPointNetDevice> s5s10Device = DynamicCast<PointToPointNetDevice>(dev_s5s10.Get(0));
    Ptr<PointToPointNetDevice> s5s11Device = DynamicCast<PointToPointNetDevice>(dev_s5s11.Get(0));
    Ptr<PointToPointNetDevice> s5s12Device = DynamicCast<PointToPointNetDevice>(dev_s5s12.Get(0));
    Ptr<PointToPointNetDevice> s5s13Device = DynamicCast<PointToPointNetDevice>(dev_s5s13.Get(0));
    Ptr<PointToPointNetDevice> s5s14Device = DynamicCast<PointToPointNetDevice>(dev_s5s14.Get(0));
    Ptr<PointToPointNetDevice> s5s15Device = DynamicCast<PointToPointNetDevice>(dev_s5s15.Get(0));

    Ptr<PointToPointNetDevice> s6s8Device = DynamicCast<PointToPointNetDevice>(dev_s6s8.Get(0));
    Ptr<PointToPointNetDevice> s6s9Device = DynamicCast<PointToPointNetDevice>(dev_s6s9.Get(0));
    Ptr<PointToPointNetDevice> s6s10Device = DynamicCast<PointToPointNetDevice>(dev_s6s10.Get(0));
    Ptr<PointToPointNetDevice> s6s11Device = DynamicCast<PointToPointNetDevice>(dev_s6s11.Get(0));
    Ptr<PointToPointNetDevice> s6s12Device = DynamicCast<PointToPointNetDevice>(dev_s6s12.Get(0));
    Ptr<PointToPointNetDevice> s6s13Device = DynamicCast<PointToPointNetDevice>(dev_s6s13.Get(0));
    Ptr<PointToPointNetDevice> s6s14Device = DynamicCast<PointToPointNetDevice>(dev_s6s14.Get(0));
    Ptr<PointToPointNetDevice> s6s15Device = DynamicCast<PointToPointNetDevice>(dev_s6s15.Get(0));

    Ptr<PointToPointNetDevice> s7s8Device = DynamicCast<PointToPointNetDevice>(dev_s7s8.Get(0));
    Ptr<PointToPointNetDevice> s7s9Device = DynamicCast<PointToPointNetDevice>(dev_s7s9.Get(0));
    Ptr<PointToPointNetDevice> s7s10Device = DynamicCast<PointToPointNetDevice>(dev_s7s10.Get(0));
    Ptr<PointToPointNetDevice> s7s11Device = DynamicCast<PointToPointNetDevice>(dev_s7s11.Get(0));
    Ptr<PointToPointNetDevice> s7s12Device = DynamicCast<PointToPointNetDevice>(dev_s7s12.Get(0));
    Ptr<PointToPointNetDevice> s7s13Device = DynamicCast<PointToPointNetDevice>(dev_s7s13.Get(0));
    Ptr<PointToPointNetDevice> s7s14Device = DynamicCast<PointToPointNetDevice>(dev_s7s14.Get(0));
    Ptr<PointToPointNetDevice> s7s15Device = DynamicCast<PointToPointNetDevice>(dev_s7s15.Get(0));


    NS_ASSERT(s0s8Device != nullptr); // 确保转换成功
    NS_ASSERT(s0s9Device != nullptr); // 确保转换成功
    NS_ASSERT(s0s10Device != nullptr); // 确保转换成功
    NS_ASSERT(s0s11Device != nullptr); // 确保转换成功
    NS_ASSERT(s0s12Device != nullptr); // 确保转换成功
    NS_ASSERT(s0s13Device != nullptr); // 确保转换成功
    NS_ASSERT(s0s14Device != nullptr); // 确保转换成功
    NS_ASSERT(s0s15Device != nullptr); // 确保转换成功

    NS_ASSERT(s1s8Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s9Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s10Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s11Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s12Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s13Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s14Device != nullptr); // 确保转换成功
    NS_ASSERT(s1s15Device != nullptr); // 确保转换成功

    NS_ASSERT(s2s8Device != nullptr); // 确保转换成功
    NS_ASSERT(s2s9Device != nullptr); // 确保转换成功
    NS_ASSERT(s2s10Device != nullptr); // 确保转换成功
    NS_ASSERT(s2s11Device != nullptr); // 确保转换成功
    NS_ASSERT(s2s12Device != nullptr); // 确保转换成功
    NS_ASSERT(s2s13Device != nullptr); // 确保转换成功
    NS_ASSERT(s2s14Device != nullptr); // 确保转换成功
    NS_ASSERT(s2s15Device != nullptr); // 确保转换成功

    NS_ASSERT(s3s8Device != nullptr); // 确保转换成功
    NS_ASSERT(s3s9Device != nullptr); // 确保转换成功
    NS_ASSERT(s3s10Device != nullptr); // 确保转换成功
    NS_ASSERT(s3s11Device != nullptr); // 确保转换成功
    NS_ASSERT(s3s12Device != nullptr); // 确保转换成功
    NS_ASSERT(s3s13Device != nullptr); // 确保转换成功
    NS_ASSERT(s3s14Device != nullptr); // 确保转换成功
    NS_ASSERT(s3s15Device != nullptr); // 确保转换成功

    NS_ASSERT(s4s8Device != nullptr); // 确保转换成功
    NS_ASSERT(s4s9Device != nullptr); // 确保转换成功
    NS_ASSERT(s4s10Device != nullptr); // 确保转换成功
    NS_ASSERT(s4s11Device != nullptr); // 确保转换成功
    NS_ASSERT(s4s12Device != nullptr); // 确保转换成功
    NS_ASSERT(s4s13Device != nullptr); // 确保转换成功
    NS_ASSERT(s4s14Device != nullptr); // 确保转换成功
    NS_ASSERT(s4s15Device != nullptr); // 确保转换成功

    NS_ASSERT(s5s8Device != nullptr); // 确保转换成功
    NS_ASSERT(s5s9Device != nullptr); // 确保转换成功
    NS_ASSERT(s5s10Device != nullptr); // 确保转换成功
    NS_ASSERT(s5s11Device != nullptr); // 确保转换成功
    NS_ASSERT(s5s12Device != nullptr); // 确保转换成功
    NS_ASSERT(s5s13Device != nullptr); // 确保转换成功
    NS_ASSERT(s5s14Device != nullptr); // 确保转换成功
    NS_ASSERT(s5s15Device != nullptr); // 确保转换成功


    NS_ASSERT(s6s8Device != nullptr); // 确保转换成功
    NS_ASSERT(s6s9Device != nullptr); // 确保转换成功
    NS_ASSERT(s6s10Device != nullptr); // 确保转换成功
    NS_ASSERT(s6s11Device != nullptr); // 确保转换成功
    NS_ASSERT(s6s12Device != nullptr); // 确保转换成功
    NS_ASSERT(s6s13Device != nullptr); // 确保转换成功
    NS_ASSERT(s6s14Device != nullptr); // 确保转换成功
    NS_ASSERT(s6s15Device != nullptr); // 确保转换成功

    NS_ASSERT(s7s8Device != nullptr); // 确保转换成功
    NS_ASSERT(s7s9Device != nullptr); // 确保转换成功
    NS_ASSERT(s7s10Device != nullptr); // 确保转换成功
    NS_ASSERT(s7s11Device != nullptr); // 确保转换成功
    NS_ASSERT(s7s12Device != nullptr); // 确保转换成功
    NS_ASSERT(s7s13Device != nullptr); // 确保转换成功
    NS_ASSERT(s7s14Device != nullptr); // 确保转换成功
    NS_ASSERT(s7s15Device != nullptr); // 确保转换成功

    /*
    // 创建一个新的、容量更大的队列
    Ptr<Queue<Packet>> customQueue = CreateObject<DropTailQueue<Packet>>();
    customQueue->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p"))); // 设置容量为 105p (大于100)

    // 直接在这个设备上设置自定义队列
    n2Device->SetQueue(customQueue);
    */

    s0s8Device->SetThreshold(160);
    s0s9Device->SetThreshold(160);
    s0s10Device->SetThreshold(160);
    s0s11Device->SetThreshold(160);
    s0s12Device->SetThreshold(160);
    s0s13Device->SetThreshold(160);
    s0s14Device->SetThreshold(160);
    s0s15Device->SetThreshold(160);

    s1s8Device->SetThreshold(160);
    s1s9Device->SetThreshold(160);
    s1s10Device->SetThreshold(160);
    s1s11Device->SetThreshold(160);
    s1s12Device->SetThreshold(160);
    s1s13Device->SetThreshold(160);
    s1s14Device->SetThreshold(160);
    s1s15Device->SetThreshold(160);

    s2s8Device->SetThreshold(160);
    s2s9Device->SetThreshold(160);
    s2s10Device->SetThreshold(160);
    s2s11Device->SetThreshold(160);
    s2s12Device->SetThreshold(160);
    s2s13Device->SetThreshold(160);
    s2s14Device->SetThreshold(160);
    s2s15Device->SetThreshold(160);


    s3s8Device->SetThreshold(160);
    s3s9Device->SetThreshold(160);
    s3s10Device->SetThreshold(160);
    s3s11Device->SetThreshold(160);
    s3s12Device->SetThreshold(160);
    s3s13Device->SetThreshold(160);
    s3s14Device->SetThreshold(160);
    s3s15Device->SetThreshold(160);

    s4s8Device->SetThreshold(160);
    s4s9Device->SetThreshold(160);
    s4s10Device->SetThreshold(160);
    s4s11Device->SetThreshold(160);
    s4s12Device->SetThreshold(160);
    s4s13Device->SetThreshold(160);
    s4s14Device->SetThreshold(160);
    s4s15Device->SetThreshold(160);
    
    s5s8Device->SetThreshold(160);
    s5s9Device->SetThreshold(160);
    s5s10Device->SetThreshold(160);
    s5s11Device->SetThreshold(160);
    s5s12Device->SetThreshold(160);
    s5s13Device->SetThreshold(160);
    s5s14Device->SetThreshold(160);
    s5s15Device->SetThreshold(160);

    s6s8Device->SetThreshold(160);
    s6s9Device->SetThreshold(160);
    s6s10Device->SetThreshold(160);
    s6s11Device->SetThreshold(160);
    s6s12Device->SetThreshold(160);
    s6s13Device->SetThreshold(160);
    s6s14Device->SetThreshold(160);
    s6s15Device->SetThreshold(160);

    s7s8Device->SetThreshold(160);
    s7s9Device->SetThreshold(160);
    s7s10Device->SetThreshold(160);
    s7s11Device->SetThreshold(160);
    s7s12Device->SetThreshold(160);
    s7s13Device->SetThreshold(160);
    s7s14Device->SetThreshold(160);
    s7s15Device->SetThreshold(160);

    s0s8Device->SetEnableEcn(false);
    s0s9Device->SetEnableEcn(false);
    s0s10Device->SetEnableEcn(false);
    s0s11Device->SetEnableEcn(false);
    s0s12Device->SetEnableEcn(false);
    s0s13Device->SetEnableEcn(false);
    s0s14Device->SetEnableEcn(false);
    s0s15Device->SetEnableEcn(false);

    s1s8Device->SetEnableEcn(false);
    s1s9Device->SetEnableEcn(false);
    s1s10Device->SetEnableEcn(false);
    s1s11Device->SetEnableEcn(false);
    s1s12Device->SetEnableEcn(false);
    s1s13Device->SetEnableEcn(false);
    s1s14Device->SetEnableEcn(false);
    s1s15Device->SetEnableEcn(false);

    s2s8Device->SetEnableEcn(false);
    s2s9Device->SetEnableEcn(false);
    s2s10Device->SetEnableEcn(false);
    s2s11Device->SetEnableEcn(false);
    s2s12Device->SetEnableEcn(false);
    s2s13Device->SetEnableEcn(false);
    s2s14Device->SetEnableEcn(false);
    s2s15Device->SetEnableEcn(false);

    s3s8Device->SetEnableEcn(false);
    s3s9Device->SetEnableEcn(false);
    s3s10Device->SetEnableEcn(false);
    s3s11Device->SetEnableEcn(false);
    s3s12Device->SetEnableEcn(false);
    s3s13Device->SetEnableEcn(false);
    s3s14Device->SetEnableEcn(false);
    s3s15Device->SetEnableEcn(false);


    s4s8Device->SetEnableEcn(false);
    s4s9Device->SetEnableEcn(false);
    s4s10Device->SetEnableEcn(false);
    s4s11Device->SetEnableEcn(false);
    s4s12Device->SetEnableEcn(false);
    s4s13Device->SetEnableEcn(false);
    s4s14Device->SetEnableEcn(false);
    s4s15Device->SetEnableEcn(false);

    s5s8Device->SetEnableEcn(false);
    s5s9Device->SetEnableEcn(false);
    s5s10Device->SetEnableEcn(false);
    s5s11Device->SetEnableEcn(false);
    s5s12Device->SetEnableEcn(false);
    s5s13Device->SetEnableEcn(false);
    s5s14Device->SetEnableEcn(false);
    s5s15Device->SetEnableEcn(false);

    s6s8Device->SetEnableEcn(false);
    s6s9Device->SetEnableEcn(false);
    s6s10Device->SetEnableEcn(false);
    s6s11Device->SetEnableEcn(false);
    s6s12Device->SetEnableEcn(false);
    s6s13Device->SetEnableEcn(false);
    s6s14Device->SetEnableEcn(false);
    s6s15Device->SetEnableEcn(false);

    s7s8Device->SetEnableEcn(false);
    s7s9Device->SetEnableEcn(false);
    s7s10Device->SetEnableEcn(false);
    s7s11Device->SetEnableEcn(false);
    s7s12Device->SetEnableEcn(false);
    s7s13Device->SetEnableEcn(false);
    s7s14Device->SetEnableEcn(false);
    s7s15Device->SetEnableEcn(false);

    //
    // Install the internet stack on the nodes
    //
    InternetStackHelper internet;
    internet.SetRoutingHelper(ATPStaticRoutingHelper());
    internet.Install(nodes);

    //
    // We've got the "hardware" in place.  Now we need to add IP addresses.
    //
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Helper;
    
    ipv4Helper.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w0s0 = ipv4Helper.Assign(dev_w0s0);
    ipv4Helper.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w1s0 = ipv4Helper.Assign(dev_w1s0);
    ipv4Helper.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w2s0 = ipv4Helper.Assign(dev_w2s0);
    ipv4Helper.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w3s0 = ipv4Helper.Assign(dev_w3s0);
    ipv4Helper.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w4s0 = ipv4Helper.Assign(dev_w4s0);
    ipv4Helper.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w5s0 = ipv4Helper.Assign(dev_w5s0);
    ipv4Helper.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w6s0 = ipv4Helper.Assign(dev_w6s0);
    ipv4Helper.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w7s0 = ipv4Helper.Assign(dev_w7s0);

    ipv4Helper.SetBase("10.1.9.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w8s1 = ipv4Helper.Assign(dev_w8s1);
    ipv4Helper.SetBase("10.1.10.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w9s1 = ipv4Helper.Assign(dev_w9s1);
    ipv4Helper.SetBase("10.1.11.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w10s1 = ipv4Helper.Assign(dev_w10s1);
    ipv4Helper.SetBase("10.1.12.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w11s1 = ipv4Helper.Assign(dev_w11s1);
    ipv4Helper.SetBase("10.1.13.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w12s1 = ipv4Helper.Assign(dev_w12s1);
    ipv4Helper.SetBase("10.1.14.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w13s1 = ipv4Helper.Assign(dev_w13s1);
    ipv4Helper.SetBase("10.1.15.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w14s1 = ipv4Helper.Assign(dev_w14s1);
    ipv4Helper.SetBase("10.1.16.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w15s1 = ipv4Helper.Assign(dev_w15s1);

    ipv4Helper.SetBase("10.1.17.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w16s2 = ipv4Helper.Assign(dev_w16s2);
    ipv4Helper.SetBase("10.1.18.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w17s2 = ipv4Helper.Assign(dev_w17s2);
    ipv4Helper.SetBase("10.1.19.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w18s2 = ipv4Helper.Assign(dev_w18s2);
    ipv4Helper.SetBase("10.1.20.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w19s2 = ipv4Helper.Assign(dev_w19s2);
    ipv4Helper.SetBase("10.1.21.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w20s2 = ipv4Helper.Assign(dev_w20s2);
    ipv4Helper.SetBase("10.1.22.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w21s2 = ipv4Helper.Assign(dev_w21s2);
    ipv4Helper.SetBase("10.1.23.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w22s2 = ipv4Helper.Assign(dev_w22s2);
    ipv4Helper.SetBase("10.1.24.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w23s2 = ipv4Helper.Assign(dev_w23s2);

    ipv4Helper.SetBase("10.1.25.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w24s3 = ipv4Helper.Assign(dev_w24s3);
    ipv4Helper.SetBase("10.1.26.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w25s3 = ipv4Helper.Assign(dev_w25s3);
    ipv4Helper.SetBase("10.1.27.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w26s3 = ipv4Helper.Assign(dev_w26s3);
    ipv4Helper.SetBase("10.1.28.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w27s3 = ipv4Helper.Assign(dev_w27s3);
    ipv4Helper.SetBase("10.1.29.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w28s3 = ipv4Helper.Assign(dev_w28s3);
    ipv4Helper.SetBase("10.1.30.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w29s3 = ipv4Helper.Assign(dev_w29s3);
    ipv4Helper.SetBase("10.1.31.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w30s3 = ipv4Helper.Assign(dev_w30s3);
    ipv4Helper.SetBase("10.1.32.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w31s3 = ipv4Helper.Assign(dev_w31s3);

    ipv4Helper.SetBase("10.1.33.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w32s4 = ipv4Helper.Assign(dev_w32s4);
    ipv4Helper.SetBase("10.1.34.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w33s4 = ipv4Helper.Assign(dev_w33s4);
    ipv4Helper.SetBase("10.1.35.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w34s4 = ipv4Helper.Assign(dev_w34s4);
    ipv4Helper.SetBase("10.1.36.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w35s4 = ipv4Helper.Assign(dev_w35s4);
    ipv4Helper.SetBase("10.1.37.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w36s4 = ipv4Helper.Assign(dev_w36s4);
    ipv4Helper.SetBase("10.1.38.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w37s4 = ipv4Helper.Assign(dev_w37s4);
    ipv4Helper.SetBase("10.1.39.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w38s4 = ipv4Helper.Assign(dev_w38s4);
    ipv4Helper.SetBase("10.1.40.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w39s4 = ipv4Helper.Assign(dev_w39s4);

    ipv4Helper.SetBase("10.1.41.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w40s5 = ipv4Helper.Assign(dev_w40s5);
    ipv4Helper.SetBase("10.1.42.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w41s5 = ipv4Helper.Assign(dev_w41s5);
    ipv4Helper.SetBase("10.1.43.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w42s5 = ipv4Helper.Assign(dev_w42s5);
    ipv4Helper.SetBase("10.1.44.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w43s5 = ipv4Helper.Assign(dev_w43s5);
    ipv4Helper.SetBase("10.1.45.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w44s5 = ipv4Helper.Assign(dev_w44s5);
    ipv4Helper.SetBase("10.1.46.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w45s5 = ipv4Helper.Assign(dev_w45s5);
    ipv4Helper.SetBase("10.1.47.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w46s5 = ipv4Helper.Assign(dev_w46s5);
    ipv4Helper.SetBase("10.1.48.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w47s5 = ipv4Helper.Assign(dev_w47s5);

    ipv4Helper.SetBase("10.1.49.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w48s6 = ipv4Helper.Assign(dev_w48s6);
    ipv4Helper.SetBase("10.1.50.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w49s6 = ipv4Helper.Assign(dev_w49s6);
    ipv4Helper.SetBase("10.1.51.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w50s6 = ipv4Helper.Assign(dev_w50s6);
    ipv4Helper.SetBase("10.1.52.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w51s6 = ipv4Helper.Assign(dev_w51s6);
    ipv4Helper.SetBase("10.1.53.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w52s6 = ipv4Helper.Assign(dev_w52s6);
    ipv4Helper.SetBase("10.1.54.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w53s6 = ipv4Helper.Assign(dev_w53s6);
    ipv4Helper.SetBase("10.1.55.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w54s6 = ipv4Helper.Assign(dev_w54s6);
    ipv4Helper.SetBase("10.1.56.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w55s6 = ipv4Helper.Assign(dev_w55s6);

    ipv4Helper.SetBase("10.1.57.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w56s7 = ipv4Helper.Assign(dev_w56s7);
    ipv4Helper.SetBase("10.1.58.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w57s7 = ipv4Helper.Assign(dev_w57s7);
    ipv4Helper.SetBase("10.1.59.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w58s7 = ipv4Helper.Assign(dev_w58s7);
    ipv4Helper.SetBase("10.1.60.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w59s7 = ipv4Helper.Assign(dev_w59s7);
    ipv4Helper.SetBase("10.1.61.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w60s7 = ipv4Helper.Assign(dev_w60s7);
    ipv4Helper.SetBase("10.1.62.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w61s7 = ipv4Helper.Assign(dev_w61s7);
    ipv4Helper.SetBase("10.1.63.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w62s7 = ipv4Helper.Assign(dev_w62s7);
    ipv4Helper.SetBase("10.1.64.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_w63s7 = ipv4Helper.Assign(dev_w63s7);

    ipv4Helper.SetBase("10.1.65.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s8 = ipv4Helper.Assign(dev_s0s8);
    ipv4Helper.SetBase("10.1.66.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s9 = ipv4Helper.Assign(dev_s0s9);
    ipv4Helper.SetBase("10.1.67.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s10 = ipv4Helper.Assign(dev_s0s10);
    ipv4Helper.SetBase("10.1.68.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s11 = ipv4Helper.Assign(dev_s0s11);
    ipv4Helper.SetBase("10.1.69.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s12 = ipv4Helper.Assign(dev_s0s12);
    ipv4Helper.SetBase("10.1.70.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s13 = ipv4Helper.Assign(dev_s0s13);
    ipv4Helper.SetBase("10.1.71.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s14 = ipv4Helper.Assign(dev_s0s14);
    ipv4Helper.SetBase("10.1.72.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s0s15 = ipv4Helper.Assign(dev_s0s15);

    ipv4Helper.SetBase("10.1.73.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s8 = ipv4Helper.Assign(dev_s1s8);
    ipv4Helper.SetBase("10.1.74.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s9 = ipv4Helper.Assign(dev_s1s9);
    ipv4Helper.SetBase("10.1.75.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s10 = ipv4Helper.Assign(dev_s1s10);
    ipv4Helper.SetBase("10.1.76.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s11 = ipv4Helper.Assign(dev_s1s11);
    ipv4Helper.SetBase("10.1.77.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s12 = ipv4Helper.Assign(dev_s1s12);
    ipv4Helper.SetBase("10.1.78.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s13 = ipv4Helper.Assign(dev_s1s13);
    ipv4Helper.SetBase("10.1.79.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s14 = ipv4Helper.Assign(dev_s1s14);
    ipv4Helper.SetBase("10.1.80.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s1s15 = ipv4Helper.Assign(dev_s1s15);

    ipv4Helper.SetBase("10.1.81.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s8 = ipv4Helper.Assign(dev_s2s8);
    ipv4Helper.SetBase("10.1.82.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s9 = ipv4Helper.Assign(dev_s2s9);
    ipv4Helper.SetBase("10.1.83.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s10 = ipv4Helper.Assign(dev_s2s10);
    ipv4Helper.SetBase("10.1.84.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s11 = ipv4Helper.Assign(dev_s2s11);
    ipv4Helper.SetBase("10.1.85.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s12 = ipv4Helper.Assign(dev_s2s12);
    ipv4Helper.SetBase("10.1.86.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s13 = ipv4Helper.Assign(dev_s2s13);
    ipv4Helper.SetBase("10.1.87.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s14 = ipv4Helper.Assign(dev_s2s14);
    ipv4Helper.SetBase("10.1.88.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s2s15 = ipv4Helper.Assign(dev_s2s15);

    ipv4Helper.SetBase("10.1.89.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3s8 = ipv4Helper.Assign(dev_s3s8);
    ipv4Helper.SetBase("10.1.90.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3s9 = ipv4Helper.Assign(dev_s3s9);
    ipv4Helper.SetBase("10.1.91.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3s10 = ipv4Helper.Assign(dev_s3s10);
    ipv4Helper.SetBase("10.1.92.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3s11 = ipv4Helper.Assign(dev_s3s11);
    ipv4Helper.SetBase("10.1.93.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3s12 = ipv4Helper.Assign(dev_s3s12);
    ipv4Helper.SetBase("10.1.94.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3s13 = ipv4Helper.Assign(dev_s3s13);
    ipv4Helper.SetBase("10.1.95.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3s14 = ipv4Helper.Assign(dev_s3s14);
    ipv4Helper.SetBase("10.1.96.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s3s15 = ipv4Helper.Assign(dev_s3s15);

    ipv4Helper.SetBase("10.1.97.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s4s8 = ipv4Helper.Assign(dev_s4s8);
    ipv4Helper.SetBase("10.1.98.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s4s9 = ipv4Helper.Assign(dev_s4s9);
    ipv4Helper.SetBase("10.1.99.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s4s10 = ipv4Helper.Assign(dev_s4s10);
    ipv4Helper.SetBase("10.1.100.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s4s11 = ipv4Helper.Assign(dev_s4s11);
    ipv4Helper.SetBase("10.1.101.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s4s12 = ipv4Helper.Assign(dev_s4s12);
    ipv4Helper.SetBase("10.1.102.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s4s13 = ipv4Helper.Assign(dev_s4s13);
    ipv4Helper.SetBase("10.1.103.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s4s14 = ipv4Helper.Assign(dev_s4s14);
    ipv4Helper.SetBase("10.1.104.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s4s15 = ipv4Helper.Assign(dev_s4s15);

    ipv4Helper.SetBase("10.1.105.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s5s8 = ipv4Helper.Assign(dev_s5s8);
    ipv4Helper.SetBase("10.1.106.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s5s9 = ipv4Helper.Assign(dev_s5s9);
    ipv4Helper.SetBase("10.1.107.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s5s10 = ipv4Helper.Assign(dev_s5s10);
    ipv4Helper.SetBase("10.1.108.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s5s11 = ipv4Helper.Assign(dev_s5s11);
    ipv4Helper.SetBase("10.1.109.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s5s12 = ipv4Helper.Assign(dev_s5s12);
    ipv4Helper.SetBase("10.1.110.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s5s13 = ipv4Helper.Assign(dev_s5s13);
    ipv4Helper.SetBase("10.1.111.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s5s14 = ipv4Helper.Assign(dev_s5s14);
    ipv4Helper.SetBase("10.1.112.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s5s15 = ipv4Helper.Assign(dev_s5s15);

    ipv4Helper.SetBase("10.1.113.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s6s8 = ipv4Helper.Assign(dev_s6s8);
    ipv4Helper.SetBase("10.1.114.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s6s9 = ipv4Helper.Assign(dev_s6s9);
    ipv4Helper.SetBase("10.1.115.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s6s10 = ipv4Helper.Assign(dev_s6s10);
    ipv4Helper.SetBase("10.1.116.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s6s11 = ipv4Helper.Assign(dev_s6s11);
    ipv4Helper.SetBase("10.1.117.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s6s12 = ipv4Helper.Assign(dev_s6s12);
    ipv4Helper.SetBase("10.1.118.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s6s13 = ipv4Helper.Assign(dev_s6s13);
    ipv4Helper.SetBase("10.1.119.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s6s14 = ipv4Helper.Assign(dev_s6s14);
    ipv4Helper.SetBase("10.1.120.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s6s15 = ipv4Helper.Assign(dev_s6s15);

    ipv4Helper.SetBase("10.1.121.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s7s8 = ipv4Helper.Assign(dev_s7s8);
    ipv4Helper.SetBase("10.1.122.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s7s9 = ipv4Helper.Assign(dev_s7s9);
    ipv4Helper.SetBase("10.1.123.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s7s10 = ipv4Helper.Assign(dev_s7s10);
    ipv4Helper.SetBase("10.1.124.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s7s11 = ipv4Helper.Assign(dev_s7s11);
    ipv4Helper.SetBase("10.1.125.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s7s12 = ipv4Helper.Assign(dev_s7s12);
    ipv4Helper.SetBase("10.1.126.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s7s13 = ipv4Helper.Assign(dev_s7s13);
    ipv4Helper.SetBase("10.1.127.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s7s14 = ipv4Helper.Assign(dev_s7s14);
    ipv4Helper.SetBase("10.1.128.0", "255.255.255.0");
    Ipv4InterfaceContainer ip_s7s15 = ipv4Helper.Assign(dev_s7s15);
    
    //
    // Create a PacketSinkApplication and install it on node 3
    //
    NS_LOG_INFO("Create Applications.");

    // Create sink applications for both jobs
    Ptr<ATPPacketSink> sinkApp1 = CreateObject<ATPPacketSink>();
    Ptr<ATPPacketSink> sinkApp2 = CreateObject<ATPPacketSink>();

    // job1 sink (s8)
    uint16_t sinkPort1 = 9;
    Address sinkAddress1(InetSocketAddress(ip_s0s8.GetAddress(1), sinkPort1));
    sinkApp1->SetAddressPort(sinkAddress1, sinkPort1);

    // job2 sink (s8) 
    uint16_t sinkPort2 = 10;
    Address sinkAddress2(InetSocketAddress(ip_s1s8.GetAddress(1), sinkPort2));
    sinkApp2->SetAddressPort(sinkAddress2, sinkPort2);

    // create ATPSocket for sink1 and bind to sinkAddress1
    Ptr<Socket> sinkSocket1 = Socket::CreateSocket(nodes.Get(72), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket1 = DynamicCast<ATPSocket>(sinkSocket1);
    sinkApp1->SetSocket(sinkATPSocket1);
    sinkATPSocket1->Bind(sinkAddress1);
    sinkATPSocket1->Listen();

    // create ATPSocket for sink2 and bind to sinkAddress2
    Ptr<Socket> sinkSocket2 = Socket::CreateSocket(nodes.Get(72), ATPSocketFactory::GetTypeId());
    Ptr<ATPSocket> sinkATPSocket2 = DynamicCast<ATPSocket>(sinkSocket2);
    sinkApp2->SetSocket(sinkATPSocket2);
    sinkATPSocket2->Bind(sinkAddress2);
    sinkATPSocket2->Listen();

    // start sink applications
    sinkApp1->SetStartTime(Seconds(0.0));
    sinkApp1->SetStopTime(stopTime);
    nodes.Get(72)->AddApplication(sinkApp1);  // s8

    sinkApp2->SetStartTime(Seconds(0.0));
    sinkApp2->SetStopTime(stopTime);
    nodes.Get(72)->AddApplication(sinkApp2);  // s8

    //
    // Create sockets for job1 (w0, w1, w2, w3)
    //
    Ptr<Socket> w0job1socket = Socket::CreateSocket(nodes.Get(0), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w1job1socket = Socket::CreateSocket(nodes.Get(1), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w2job1socket = Socket::CreateSocket(nodes.Get(2), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w3job1socket = Socket::CreateSocket(nodes.Get(3), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w0job1_ATPSocket = DynamicCast<ATPSocket>(w0job1socket);
    Ptr<ATPSocket> w1job1_ATPSocket = DynamicCast<ATPSocket>(w1job1socket);
    Ptr<ATPSocket> w2job1_ATPSocket = DynamicCast<ATPSocket>(w2job1socket);
    Ptr<ATPSocket> w3job1_ATPSocket = DynamicCast<ATPSocket>(w3job1socket);

    //
    // Create sockets for job2 (w8, w9, w10, w11)
    //
    Ptr<Socket> w8job2socket = Socket::CreateSocket(nodes.Get(8), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w9job2socket = Socket::CreateSocket(nodes.Get(9), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w10job2socket = Socket::CreateSocket(nodes.Get(10), ATPSocketFactory::GetTypeId());
    Ptr<Socket> w11job2socket = Socket::CreateSocket(nodes.Get(11), ATPSocketFactory::GetTypeId());

    Ptr<ATPSocket> w8job2_ATPSocket = DynamicCast<ATPSocket>(w8job2socket);
    Ptr<ATPSocket> w9job2_ATPSocket = DynamicCast<ATPSocket>(w9job2socket);
    Ptr<ATPSocket> w10job2_ATPSocket = DynamicCast<ATPSocket>(w10job2socket);
    Ptr<ATPSocket> w11job2_ATPSocket = DynamicCast<ATPSocket>(w11job2socket);

    // Create applications for job1
    Ptr<ATPBulkSendApplication> w0job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w1job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w2job1App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w3job1App = CreateObject<ATPBulkSendApplication>();

    // Create applications for job2
    Ptr<ATPBulkSendApplication> w8job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w9job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w10job2App = CreateObject<ATPBulkSendApplication>();
    Ptr<ATPBulkSendApplication> w11job2App = CreateObject<ATPBulkSendApplication>();

    uint16_t sendPort1 = 11;  // Port for job1
    uint16_t sendPort2 = 12;  // Port for job2

    // Addresses for job1 workers
    Address w0Address(InetSocketAddress(ip_w0s0.GetAddress(0), sendPort1));
    Address w1Address(InetSocketAddress(ip_w1s0.GetAddress(0), sendPort1));
    Address w2Address(InetSocketAddress(ip_w2s0.GetAddress(0), sendPort1));
    Address w3Address(InetSocketAddress(ip_w3s0.GetAddress(0), sendPort1));

    // Addresses for job2 workers
    Address w8Address(InetSocketAddress(ip_w8s1.GetAddress(0), sendPort2));
    Address w9Address(InetSocketAddress(ip_w9s1.GetAddress(0), sendPort2));
    Address w10Address(InetSocketAddress(ip_w10s1.GetAddress(0), sendPort2));
    Address w11Address(InetSocketAddress(ip_w11s1.GetAddress(0), sendPort2));

    // 设置job1初始拥塞窗口
    w0job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w1job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w2job1_ATPSocket->SetInitCwnd(job1_initCwnd);
    w3job1_ATPSocket->SetInitCwnd(job1_initCwnd);

    // 设置job2初始拥塞窗口
    w8job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w9job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w10job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    w11job2_ATPSocket->SetInitCwnd(job2_initCwnd);
    
    // Configure w0job1App
    uint8_t job1Id = 1;
    w0job1App->Setup(sinkAddress1, w0job1_ATPSocket, maxBytes, job1Id);
    w0job1App->SetEnableATPTag(true);
    w0job1App->SetStartTime(Seconds(1.0));
    w0job1App->SetStopTime(stopTime);
    w0job1App->SetFaninDegree(0b00001111); // 4个发送端 job1
    w0job1App->SetWorkerId(0b00000001);

    w0job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w0job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w0job1App));
    w0job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w0job1App));
    w0job1_ATPSocket->Bind(w0Address);
    w0job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(0)->AddApplication(w0job1App);

    // Configure w1job1App
    w1job1App->Setup(sinkAddress1, w1job1_ATPSocket, maxBytes, job1Id);
    w1job1App->SetEnableATPTag(true);
    w1job1App->SetStartTime(Seconds(1.0));
    w1job1App->SetStopTime(stopTime);
    w1job1App->SetFaninDegree(0b00001111); // 4个发送端 job1
    w1job1App->SetWorkerId(0b00000010);

    w1job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w1job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w1job1App));
    w1job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w1job1App));
    w1job1_ATPSocket->Bind(w1Address);
    w1job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(1)->AddApplication(w1job1App);

    // Configure w2job1App
    w2job1App->Setup(sinkAddress1, w2job1_ATPSocket, maxBytes, job1Id);
    w2job1App->SetEnableATPTag(true);
    w2job1App->SetStartTime(Seconds(1.0));
    w2job1App->SetStopTime(stopTime);
    w2job1App->SetFaninDegree(0b00001111); // 4个发送端 job1
    w2job1App->SetWorkerId(0b00000100);

    w2job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w2job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w2job1App));
    w2job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w2job1App));
    w2job1_ATPSocket->Bind(w2Address);
    w2job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(2)->AddApplication(w2job1App);

    // Configure w3job1App
    w3job1App->Setup(sinkAddress1, w3job1_ATPSocket, maxBytes, job1Id);
    w3job1App->SetEnableATPTag(true);
    w3job1App->SetStartTime(Seconds(1.0));
    w3job1App->SetStopTime(stopTime);
    w3job1App->SetFaninDegree(0b00001111); // 4个发送端 job1
    w3job1App->SetWorkerId(0b00001000);

    w3job1_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w3job1App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w3job1App));
    w3job1_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w3job1App));
    w3job1_ATPSocket->Bind(w3Address);
    w3job1_ATPSocket->Connect(sinkAddress1);
    nodes.Get(3)->AddApplication(w3job1App);  

    // Configure w8job2App
    uint8_t job2Id = 2;
    w8job2App->Setup(sinkAddress2, w8job2_ATPSocket, maxBytes, job2Id);
    w8job2App->SetEnableATPTag(true);
    w8job2App->SetStartTime(Seconds(1.0));
    w8job2App->SetStopTime(stopTime);
    w8job2App->SetFaninDegree(0b00001111); // 4个发送端 job2
    w8job2App->SetWorkerId(0b00000001);

    w8job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w8job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w8job2App));
    w8job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w8job2App));
    w8job2_ATPSocket->Bind(w8Address);
    w8job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(8)->AddApplication(w8job2App);

    // Configure w9job2App
    w9job2App->Setup(sinkAddress2, w9job2_ATPSocket, maxBytes, job2Id);
    w9job2App->SetEnableATPTag(true);
    w9job2App->SetStartTime(Seconds(1.0));
    w9job2App->SetStopTime(stopTime);
    w9job2App->SetFaninDegree(0b00001111); // 4个发送端 job2
    w9job2App->SetWorkerId(0b00000010);

    w9job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w9job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w9job2App));
    w9job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w9job2App));
    w9job2_ATPSocket->Bind(w9Address);
    w9job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(9)->AddApplication(w9job2App);

    // Configure w10job2App
    w10job2App->Setup(sinkAddress2, w10job2_ATPSocket, maxBytes, job2Id);
    w10job2App->SetEnableATPTag(true);
    w10job2App->SetStartTime(Seconds(1.0));
    w10job2App->SetStopTime(stopTime);
    w10job2App->SetFaninDegree(0b00001111); // 4个发送端 job2
    w10job2App->SetWorkerId(0b00000100);

    w10job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w10job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w10job2App));
    w10job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w10job2App));
    w10job2_ATPSocket->Bind(w10Address);
    w10job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(10)->AddApplication(w10job2App);

    // Configure w11job2App
    w11job2App->Setup(sinkAddress2, w11job2_ATPSocket, maxBytes, job2Id);
    w11job2App->SetEnableATPTag(true);
    w11job2App->SetStartTime(Seconds(1.0));
    w11job2App->SetStopTime(stopTime);
    w11job2App->SetFaninDegree(0b00001111); // 4个发送端 job2
    w11job2App->SetWorkerId(0b00001000);

    w11job2_ATPSocket->SetConnectCallback(MakeCallback(&ATPBulkSendApplication::ConnectionSucceeded, w11job2App),
                                MakeCallback(&ATPBulkSendApplication::ConnectionFailed, w11job2App));
    w11job2_ATPSocket->SetSendCallback(MakeCallback(&ATPBulkSendApplication::DataSend, w11job2App));
    w11job2_ATPSocket->Bind(w11Address);
    w11job2_ATPSocket->Connect(sinkAddress2);
    nodes.Get(11)->AddApplication(w11job2App);

    // 连接拥塞窗口跟踪
    w0job1_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_job1));
    w8job2_ATPSocket->GetTxBuffer()->TraceConnectWithoutContext("cwndTrace",
         MakeCallback(&CwndChange_job2));

    // 获取每个节点的静态路由对象
    ATPStaticRoutingHelper staticRoutingHelper;
    Ptr<ATPStaticRouting> staticRouting_w0 = staticRoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w1 = staticRoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w2 = staticRoutingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w3 = staticRoutingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w8 = staticRoutingHelper.GetStaticRouting(nodes.Get(8)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w9 = staticRoutingHelper.GetStaticRouting(nodes.Get(9)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w10 = staticRoutingHelper.GetStaticRouting(nodes.Get(10)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_w11 = staticRoutingHelper.GetStaticRouting(nodes.Get(11)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s0 = staticRoutingHelper.GetStaticRouting(nodes.Get(64)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s1 = staticRoutingHelper.GetStaticRouting(nodes.Get(65)->GetObject<Ipv4>());
    Ptr<ATPStaticRouting> staticRouting_s8 = staticRoutingHelper.GetStaticRouting(nodes.Get(72)->GetObject<Ipv4>());

    staticRouting_s0->SetEnableAggregation(true);
    staticRouting_s1->SetEnableAggregation(true);
    staticRouting_s8->SetEnableAggregation(true);

    Ptr<ATPL4Protocol> atpl4_s0 = staticRouting_s0->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s1 = staticRouting_s1->GetATPL4Protocol();
    Ptr<ATPL4Protocol> atpl4_s8 = staticRouting_s8->GetATPL4Protocol();

    // 这里得修改。还得改aggregator里的fanindegree
    // job1
    atpl4_s0->SetAggregatorFaninDegree(job1Id, 0b00001111);
    atpl4_s8->SetAggregatorFaninDegree(job1Id, 0b00001111);
    // job2
    atpl4_s1->SetAggregatorFaninDegree(job2Id, 0b00001111);
    atpl4_s8->SetAggregatorFaninDegree(job2Id, 0b00001111);

    // 配置w0的路由表 w0->s0->s2->ps1
    staticRouting_w0->AddHostRouteTo(ip_s0s8.GetAddress(1), ip_w0s0.GetAddress(1), 1);

    // 配置w1的路由表 w1->s0->s2->ps1
    staticRouting_w1->AddHostRouteTo(ip_s0s8.GetAddress(1), ip_w1s0.GetAddress(1), 1);

    // 配置w2的路由表 w2->s0->s2->ps2
    staticRouting_w2->AddHostRouteTo(ip_s0s8.GetAddress(1), ip_w2s0.GetAddress(1), 1);

    // 配置w3的路由表 w3->s0->s2->ps2
    staticRouting_w3->AddHostRouteTo(ip_s0s8.GetAddress(1), ip_w3s0.GetAddress(1), 1);

    // 配置w8的路由表 w8->s1->s8
    staticRouting_w8->AddHostRouteTo(ip_s1s8.GetAddress(1), ip_w8s1.GetAddress(1), 1);

    // 配置w9的路由表 w9->s1->s8
    staticRouting_w9->AddHostRouteTo(ip_s1s8.GetAddress(1), ip_w9s1.GetAddress(1), 1);

    // 配置w10的路由表 w10->s1->s8
    staticRouting_w10->AddHostRouteTo(ip_s1s8.GetAddress(1), ip_w10s1.GetAddress(1), 1);

    // 配置w11的路由表 w11->s1->s8
    staticRouting_w11->AddHostRouteTo(ip_s1s8.GetAddress(1), ip_w11s1.GetAddress(1), 1);

    // 配置s0的路由表 
    // s0->w0
    staticRouting_s0->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_w0s0.GetAddress(0), 1);
    // s0->w1
    staticRouting_s0->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_w1s0.GetAddress(0), 2);
    // s0->w2
    staticRouting_s0->AddHostRouteTo(ip_w2s0.GetAddress(0), ip_w2s0.GetAddress(0), 3);
    // s0->w3
    staticRouting_s0->AddHostRouteTo(ip_w3s0.GetAddress(0), ip_w3s0.GetAddress(0), 4);
    // s0->s8
    staticRouting_s0->AddHostRouteTo(ip_s0s8.GetAddress(1), ip_s0s8.GetAddress(1), 5);


    // 配置s1的路由表
    // s1->w8
    staticRouting_s1->AddHostRouteTo(ip_w8s1.GetAddress(0), ip_w8s1.GetAddress(0), 1);
    // s1->w9
    staticRouting_s1->AddHostRouteTo(ip_w9s1.GetAddress(0), ip_w9s1.GetAddress(0), 2);
    // s1->w10
    staticRouting_s1->AddHostRouteTo(ip_w10s1.GetAddress(0), ip_w10s1.GetAddress(0), 3);
    // s1->w11
    staticRouting_s1->AddHostRouteTo(ip_w11s1.GetAddress(0), ip_w11s1.GetAddress(0), 4);
    // s1->s8
    staticRouting_s1->AddHostRouteTo(ip_s1s8.GetAddress(1), ip_s1s8.GetAddress(1), 5);

    //配置s8的路由表
    // s8->s0->w0
    staticRouting_s8->AddHostRouteTo(ip_w0s0.GetAddress(0), ip_s0s8.GetAddress(0), 1);
    // s8->s0->w1
    staticRouting_s8->AddHostRouteTo(ip_w1s0.GetAddress(0), ip_s0s8.GetAddress(0), 1);
    // s8->s0->w2
    staticRouting_s8->AddHostRouteTo(ip_w2s0.GetAddress(0), ip_s0s8.GetAddress(0), 1);
    // s8->s0->w3
    staticRouting_s8->AddHostRouteTo(ip_w3s0.GetAddress(0), ip_s0s8.GetAddress(0), 1);
    // s8->s1->w8
    staticRouting_s8->AddHostRouteTo(ip_w8s1.GetAddress(0), ip_s1s8.GetAddress(0), 2);
    // s8->s1->w9
    staticRouting_s8->AddHostRouteTo(ip_w9s1.GetAddress(0), ip_s1s8.GetAddress(0), 2);
    // s8->s1->w10
    staticRouting_s8->AddHostRouteTo(ip_w10s1.GetAddress(0), ip_s1s8.GetAddress(0), 2);
    // s8->s1->w11
    staticRouting_s8->AddHostRouteTo(ip_w11s1.GetAddress(0), ip_s1s8.GetAddress(0), 2);
    

    // 添加地址映射
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w0s0.GetAddress(0), sendPort1);  // job1 - w0
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w1s0.GetAddress(0), sendPort1);  // job1 - w1
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w2s0.GetAddress(0), sendPort1);  // job1 - w2
    sinkATPSocket1->AddAddressMapping(job1Id, ip_w3s0.GetAddress(0), sendPort1);  // job1 - w3

    sinkATPSocket2->AddAddressMapping(job2Id, ip_w8s1.GetAddress(0), sendPort2);  // job2 - w8
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w9s1.GetAddress(0), sendPort2);  // job2 - w9
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w10s1.GetAddress(0), sendPort2);  // job2 - w10
    sinkATPSocket2->AddAddressMapping(job2Id, ip_w11s1.GetAddress(0), sendPort2);  // job2 - w11
    

    // 开始测量
    Simulator::Schedule(Seconds(1.0), &MeasurementTxJob1, w0job1_ATPSocket);
    Simulator::Schedule(Seconds(1.0), &MeasurementTxJob2, w8job2_ATPSocket);

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(stopTime);
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    cwndStream_job1.close();
    cwndStream_job2.close();
    sendBytesStream_job1.close();
    sendBytesStream_job2.close();

    // 累加job1所有workers的发送字节数
    uint64_t job1TotalBytes = w0job1_ATPSocket->GetTotalTxBytes() + 
                               w1job1_ATPSocket->GetTotalTxBytes() + 
                               w2job1_ATPSocket->GetTotalTxBytes() + 
                               w3job1_ATPSocket->GetTotalTxBytes();
    
    // 累加job2所有workers的发送字节数
    uint64_t job2TotalBytes = w8job2_ATPSocket->GetTotalTxBytes() + 
                               w9job2_ATPSocket->GetTotalTxBytes() + 
                               w10job2_ATPSocket->GetTotalTxBytes() + 
                               w11job2_ATPSocket->GetTotalTxBytes();
    
    std::cout << "job1 Total Bytes Sent: " << job1TotalBytes << std::endl;
    std::cout << "job2 Total Bytes Sent: " << job2TotalBytes << std::endl;
    
    // 也可以输出每个worker的发送字节数（可选）
    std::cout << "job1 - w0 bytes: " << w0job1_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job1 - w1 bytes: " << w1job1_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job1 - w2 bytes: " << w2job1_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job1 - w3 bytes: " << w3job1_ATPSocket->GetTotalTxBytes() << std::endl;
    
    std::cout << "job2 - w8 bytes: " << w8job2_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job2 - w9 bytes: " << w9job2_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job2 - w10 bytes: " << w10job2_ATPSocket->GetTotalTxBytes() << std::endl;
    std::cout << "job2 - w11 bytes: " << w11job2_ATPSocket->GetTotalTxBytes() << std::endl;

    return 0;
}
