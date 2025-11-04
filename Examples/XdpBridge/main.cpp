#include "PcapFileDevice.h"
#include "PcapLiveDeviceList.h"
#include "PcapPlusPlusVersion.h"
#include "SystemUtils.h"
#include "TablePrinter.h"
#include "XdpDevice.h"

#include <chrono>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;

#define COLLECT_STATS_EVERY_SEC 1

#define EXIT_WITH_ERROR(reason)                                                                                        \
	do                                                                                                                 \
	{                                                                                                                  \
		cout << endl << "ERROR: " << reason << endl << endl;                                                           \
		exit(1);                                                                                                       \
	} while (0)

#define EXIT_WITH_ERROR_AND_PRINT_USAGE(reason)                                                                        \
	do                                                                                                                 \
	{                                                                                                                  \
		printUsage();                                                                                                  \
		cout << endl << "ERROR: " << reason << endl << endl;                                                           \
		exit(1);                                                                                                       \
	} while (0)

static struct option XdpBridgeOptions[] = {
	{ "xdp-devices", required_argument, 0, 'd' },
	{ "help",        optional_argument, 0, 'h' },
	{ "list",        optional_argument, 0, 'l' },
	{ "version",     optional_argument, 0, 'v' },
	{ 0,	         0,	             0, 0   }
};

void printUsage()
{
	cout << endl
	     << "Usage:" << endl
	     << "------" << endl
	     << pcpp::AppName::get() << " [-hlv] -d DEVICE_1,DEVICE_2" << endl
	     << endl
	     << "Options:" << endl
	     << endl
	     << "    -h|--help                                         : Displays this help message and exits" << endl
	     << "    -l|--list                                         : Print the list of xdp devices and exits" << endl
	     << "    -v|--version                                      : Displays the current version and exits" << endl
	     << "    -d|--xdp-interfaces DEVICE_1,DEVICE_2             : A comma-separated list of two xdp devices to "
	        "be bridged."
	     << endl
	     << "                                                      To see all available xdp devices use the -l switch"
	     << endl;
}

void printAppVersion()
{
	cout << pcpp::AppName::get() << " " << pcpp::getPcapPlusPlusVersionFull() << endl
	     << "Built: " << pcpp::getBuildDateTime() << endl
	     << "Built from: " << pcpp::getGitInfo() << endl;
	exit(0);
}

void listXdpDevices()
{
	cout << endl << "Network interfaces:" << endl;
	for (const auto& device : pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList())
	{
		if (device->getIPv4Address() != pcpp::IPv4Address::Zero)
		{
			cout << "    -> Name: '" << device->getName() << "'   IP address: " << device->getIPv4Address().toString()
			     << endl;
		}
	}
	exit(0);
}

void onApplicationInterrupted(void* cookie)
{
	cout << endl << endl << "Application stopped" << endl;

	// signal break of infinite loop in main function
	auto shouldStop = (bool*)cookie;
	*shouldStop = true;
}

void printStats(pcpp::XdpDevice* device)
{
	auto stats = device->getStatistics();

	vector<string> columnNames;
	columnNames.push_back("Device");
	columnNames.push_back("Total Received");
	columnNames.push_back("Total Sent");
	columnNames.push_back("Total Drops");

	vector<int> columnLengths;
	columnLengths.push_back(10);
	columnLengths.push_back(15);
	columnLengths.push_back(15);
	columnLengths.push_back(15);

	pcpp::TablePrinter printer(columnNames, columnLengths);

	stringstream devStats;
	devStats << "UNKNOWN" << "|" << stats.rxPackets << "|" << stats.txSentPackets << "|" << stats.rxDroppedTotalPackets;
	printer.printRow(devStats.str(), '|');
}

void packetArriveCallback(pcpp::RawPacket packets[], uint32_t packetCount, pcpp::XdpDevice* device, void* userCookie)
{
	// Retrieve destination device
	auto dstDev = (pcpp::XdpDevice*)userCookie;

	// Send packets
	dstDev->sendPackets(packets, packetCount);
}

int main(int argc, char* argv[])
{
	pcpp::AppName::init(argc, argv);

	vector<string> deviceNames;
	auto optionIndex = 0;
	auto opt = 0;

	while ((opt = getopt_long(argc, argv, "d:hlv", XdpBridgeOptions, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 0:
		{
			break;
		}
		case 'd':
		{
			string interfaceListAsString = string(optarg);
			stringstream stream(interfaceListAsString);
			string interfaceAsString;
			string interface;
			// break comma-separated string into string list
			while (getline(stream, interfaceAsString, ','))
			{
				char c;
				stringstream stream2(interfaceAsString);
				stream2 >> interface;
				if (stream2.fail() || stream2.get(c))
				{
					// not an integer
					EXIT_WITH_ERROR_AND_PRINT_USAGE("Xdp device list is invalid");
				}
				deviceNames.push_back(interface);
			}
			// verify list contains two ports
			if (deviceNames.size() != 2)
			{
				EXIT_WITH_ERROR_AND_PRINT_USAGE("Xdp device list must contain two values");
			}
			break;
		}
		case 'h':
		{
			printUsage();
			exit(0);
		}
		case 'l':
		{
			listXdpDevices();
			exit(0);
		}
		case 'v':
		{
			printAppVersion();
			break;
		}
		default:
		{
			printUsage();
			exit(0);
		}
		}
	}

	// verify list is not empty
	if (deviceNames.empty())
	{
		EXIT_WITH_ERROR_AND_PRINT_USAGE("Xdp device list is empty. Please use the -i switch");
	}

	// Collect the list of Xdp devices
	vector<pcpp::XdpDevice> xdpDevicesToUse;
	for (const auto& deviceName : deviceNames)
		xdpDevicesToUse.push_back(pcpp::XdpDevice(deviceName));

	// Go over all devices and open them
	for (auto& dev : xdpDevicesToUse)
	{
		if (!dev.open())
		{
			EXIT_WITH_ERROR("Error opening the device");
		}

		// Print device configurations
		cout << "----------------------------------------" << endl;
		cout << "Attach mode : " << dev.getConfig()->attachMode << endl;
		cout << "UMEM num frames : " << dev.getConfig()->umemNumFrames << endl;
		cout << "UMEM frame size : " << dev.getConfig()->umemFrameSize << endl;
		cout << "Fill ring size : " << dev.getConfig()->fillRingSize << endl;
		cout << "Completion ring size : " << dev.getConfig()->completionRingSize << endl;
		cout << "RX size : " << dev.getConfig()->rxSize << endl;
		cout << "TX size : " << dev.getConfig()->txSize << endl;
		cout << "RX/TX batch size : " << dev.getConfig()->rxTxBatchSize << endl;
		cout << "----------------------------------------" << endl;
	}

	// Start receiving packets on both devices in separate threads
	thread([&]() { xdpDevicesToUse[0].receivePackets(packetArriveCallback, &xdpDevicesToUse[1], -1); }).detach();
	thread([&]() { xdpDevicesToUse[1].receivePackets(packetArriveCallback, &xdpDevicesToUse[0], -1); }).detach();

	// signal for breaking loop
	auto shouldStop = false;

	pcpp::ApplicationEventHandler::getInstance().onApplicationInterrupted(onApplicationInterrupted, &shouldStop);

	// infinite loop (until program is terminated)
	uint64_t counter = 0;
	int statsCounter = 1;

	// Keep running while flag is on
	while (!shouldStop)
	{
		// Sleep for 1 second
		sleep(1);

		// collect and print stats every COLLECT_STATS_EVERY_SEC seconds
		if (counter % COLLECT_STATS_EVERY_SEC == 0)
		{
			// Clear screen and move to top left
			cout << "\033[2J\033[1;1H";

			// Print devices traffic stats
			cout << "Stats #" << statsCounter++ << endl << "==========" << endl;
			printStats(&xdpDevicesToUse[0]);
			printStats(&xdpDevicesToUse[1]);
		}
		counter++;
	}

	// stop capture
	xdpDevicesToUse[0].stopReceivePackets();
	xdpDevicesToUse[1].stopReceivePackets();

	// close devices
	xdpDevicesToUse[0].close();
	xdpDevicesToUse[1].close();

	return 0;
}
