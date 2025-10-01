== Explanation of the Simulation Code in NS3 3.45 ==

This simulation is designed to compare the performance of two application components in NS-3: OnOffHelper and OnOffApplication. Both are used to generate network traffic, but they differ in the way they are created and installed on the nodes.

The steps in the experiment are:

1. A folder /scratch/pcap_file is created in the root of NS-3.45 to store the output packet capture files.
2. The simulation code (wireless-animation-2AP-pcap-flowmon.cc) is executed using:
   ./ns3 run scratch/wireless-animation-2AP-pcap-flowmon
4. The code produces four output files in /pcap_file, containing packet traces and FlowMonitor statistics.
5. Put A Python script (flow_timeseries.py) on /scratch/pcap_file/ folder and then run to process the FlowMonitor output and generate time-      series plots (in PNG format).

The comparison between OnOffHelper and OnOffApplication is carried out using four key performance metrics:
- Throughput (Mbps) → measures the rate of successful data delivery.
- Packet Delivery Ratio (PDR, %) → indicates the percentage of successfully received packets.
- Packet Loss (%) → shows the ratio of lost packets.
- End-to-End Delay (ms) → measures the average time taken for packets to travel from sender to receiver.

The experiment’s objective is to observe how each application model performs under the same traffic configuration (DataRate = 2 Mbps, PacketSize = 512 bytes) and to analyze their efficiency in terms of reliability, stability, and responsiveness.
