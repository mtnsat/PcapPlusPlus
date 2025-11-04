#include "PcapPlusPlusVersion.h"
#include "PfRingDevice.h"
#include "PfRingDeviceList.h"
#include "SystemUtils.h"
#include "TablePrinter.h"

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

// clang-format off
static struct option PfRingBridgeOptions[] = {
    { "pfring-devices",     required_argument, 0, 'd' },
	{ "help",               optional_argument, 0, 'h' },
	{ "list",               optional_argument, 0, 'l' },
	{ "version",            optional_argument, 0, 'v' },
	{ 0,                0,                 0, 0   }
};
// clang-format on

/**
 * Print application usage
 */
void printUsage()
{
	std::cout
	    << std::endl
	    << "Usage:" << std::endl
	    << "------" << std::endl
	    << pcpp::AppName::get() << " [-hlv] -d DEVICE_1,DEVICE_2" << std::endl
	    << std::endl
	    << "Options:" << std::endl
	    << std::endl
	    << "    -h|--help                                         : Displays this help message and exits" << std::endl
	    << "    -l|--list                                         : Print the list of pf ring devices and exits"
	    << std::endl
	    << "    -v|--version                                      : Displays the current version and exits" << std::endl
	    << "    -d|--pfring-interfaces DEVICE_1,DEVICE_2          : A comma-separated list of two pf ring devices to "
	       "be bridged."
	    << std::endl
	    << "                                                      To see all available pf ring devices use the -l switch"
	    << std::endl;
}

/**
 * Print application version
 */
void printAppVersion()
{
	std::cout << pcpp::AppName::get() << " " << pcpp::getPcapPlusPlusVersionFull() << std::endl
	          << "Built: " << pcpp::getBuildDateTime() << std::endl
	          << "Built from: " << pcpp::getGitInfo() << std::endl;
	exit(0);
}

/**
 * Print to console all available pf ring devices. Used by the -l switch
 */
void listPfRingDevices()
{
	std::cout << "pf ring device list:" << std::endl;

	// go over all available pf ring devices and print info for each one
	std::vector<pcpp::PfRingDevice*> deviceList = pcpp::PfRingDeviceList::getInstance().getPfRingDevicesList();
	for (const auto& iter : deviceList)
	{
		pcpp::PfRingDevice* dev = iter;
		std::cout << "   "
		          << " Interface #" << dev->getInterfaceIndex() << ":"
		          << " Device name='" << dev->getDeviceName() << "';"
		          << " MAC address='" << dev->getMacAddress() << "';" << std::endl;
	}
}

/**
 * The callback to be called when application is terminated by ctrl-c. Do cleanup and print summary stats
 */
void onApplicationInterrupted(void* cookie)
{
	std::cout << std::endl << std::endl << "Application stopped" << std::endl;

	// signal break of infinite loop in main function
	auto shouldStop = (bool*)cookie;
	*shouldStop = true;
}

/**
 * Extract and print traffic stats from a device
 */
void printStats(pcpp::PfRingDevice* device)
{
	pcpp::PfRingDevice::PfRingStats stats;
	device->getStatistics(stats);

	std::vector<std::string> columnNames;
	columnNames.push_back("Device");
	columnNames.push_back("Total Received");
	columnNames.push_back("Total Drops");

	std::vector<int> columnLengths;
	columnLengths.push_back(10);
	columnLengths.push_back(15);
	columnLengths.push_back(15);

	pcpp::TablePrinter printer(columnNames, columnLengths);

	std::stringstream devStats;
	devStats << device->getDeviceName() << "|" << stats.recv << "|" << stats.drop;
	printer.printRow(devStats.str(), '|');
}

/**
 * callback to handle packets arriving on pf ring device
 */
void packetArriveCallback(pcpp::RawPacket* packets, uint32_t numOfPackets, uint8_t threadId, pcpp::PfRingDevice* device,
                          void* userCookie)
{
	// retrieve destination device
	auto dstDev = (pcpp::PfRingDevice*)userCookie;

	// send packets
	dstDev->sendPackets(packets, numOfPackets);
}

/**
 * main method of the application. Responsible for parsing user args. At program termination worker threads are
 stopped,
 * statistics are collected from them and printed to console
 */
int main(int argc, char* argv[])
{
	pcpp::AppName::init(argc, argv);

	std::vector<std::string> deviceNames;
	auto optionIndex = 0;
	auto opt = 0;

	while ((opt = getopt_long(argc, argv, "d:hvl", PfRingBridgeOptions, &optionIndex)) != -1)
	{
		switch (opt)
		{
		case 0:
		{
			break;
		}
		case 'd':
		{
			std::string interfaceListAsString = std::string(optarg);
			std::stringstream stream(interfaceListAsString);
			std::string interfaceAsString;
			std::string interface;
			// break comma-separated string into string list
			while (getline(stream, interfaceAsString, ','))
			{
				char c;
				std::stringstream stream2(interfaceAsString);
				stream2 >> interface;
				if (stream2.fail() || stream2.get(c))
				{
					// not an integer
					EXIT_WITH_ERROR_AND_PRINT_USAGE("pf ring device list is invalid");
				}
				deviceNames.push_back(interface);
			}
			// verify list contains two ports
			if (deviceNames.size() != 2)
			{
				EXIT_WITH_ERROR_AND_PRINT_USAGE("pf ring device list must contain two values");
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
			listPfRingDevices();
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
		EXIT_WITH_ERROR_AND_PRINT_USAGE("pf ring device list is empty. Please use the -i switch");
	}

	// collect the list of pf ring devices
	std::vector<pcpp::PfRingDevice*> pfRingDevicesToUse;
	for (const auto& deviceName : deviceNames)
	{
		auto dev = pcpp::PfRingDeviceList::getInstance().getPfRingDeviceByName(deviceName);
		if (dev == NULL)
		{
			EXIT_WITH_ERROR("pf ring device with name " << deviceName << " doesn't exist");
		}
		pfRingDevicesToUse.push_back(dev);
	}

	// go over all devices and open them
	for (auto dev : pfRingDevicesToUse)
	{
		if (!dev->open())
		{
			EXIT_WITH_ERROR("Couldn't open pf ring device with name " << dev->getDeviceName());
		}

		std::cout << "Number of opened RX channels for device " << dev->getDeviceName() << " : "
		          << std::to_string(dev->getNumOfOpenedRxChannels()) << std::endl;

		if (!dev->setFilter("not outbound"))
		{
			EXIT_WITH_ERROR("Couldn't set filter for pf ring device with name " << dev->getDeviceName());
		}
	}

	// start capture threads
	if (!pfRingDevicesToUse[0]->startCaptureSingleThread(packetArriveCallback, pfRingDevicesToUse[1]))
	{
		EXIT_WITH_ERROR("Couldn't start capture on pf ring device with name "
		                << pfRingDevicesToUse[0]->getDeviceName());
	}

	if (!pfRingDevicesToUse[1]->startCaptureSingleThread(packetArriveCallback, pfRingDevicesToUse[0]))
	{
		EXIT_WITH_ERROR("Couldn't start capture on pf ring device with name "
		                << pfRingDevicesToUse[1]->getDeviceName());
	}

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

		// Print stats every COLLECT_STATS_EVERY_SEC seconds
		// cppcheck-suppress moduloofone
		if (counter % COLLECT_STATS_EVERY_SEC == 0)
		{
			// // Clear screen and move to top left
			// std::cout << "\033[2J\033[1;1H";

			// // Print devices traffic stats
			// std::cout << "Stats #" << statsCounter++ << std::endl << "==========" << std::endl;
			// printStats(pfRingDevicesToUse[0]);
			// printStats(pfRingDevicesToUse[1]);
		}
		counter++;
	}

	// stop capture
	pfRingDevicesToUse[0]->stopCapture();
	pfRingDevicesToUse[1]->stopCapture();

	// close devices
	pfRingDevicesToUse[0]->close();
	pfRingDevicesToUse[1]->close();

	return 0;
}
