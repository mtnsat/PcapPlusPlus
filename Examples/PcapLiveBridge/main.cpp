#include "Packet.h"
#include "PcapLiveDevice.h"
#include "PcapLiveDeviceList.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>

using namespace std;

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

// Global flag to stop the application
bool shouldStop = false;

// Signal handler for Ctrl+C
void onApplicationInterrupted(int signum)
{
	shouldStop = true;
}

// Callback for packet capture and forwarding
void packetArriveCallback(pcpp::RawPacket* packet, pcpp::PcapLiveDevice* dev, void* userCookie)
{
	// Retrieve the destination device from the userCookie
	pcpp::PcapLiveDevice* dstDev = (pcpp::PcapLiveDevice*)userCookie;

	// Parse the raw packet into a parsed packet
	pcpp::Packet parsedPacket(packet);

	// Forward the packet to the destination device
	if (!dstDev->sendPacket(&parsedPacket))
	{
		std::cerr << "Failed to forward packet" << std::endl;
	}
}

// Function to display usage information
void printUsage(const char* programName)
{
	std::cout << "Usage: " << programName << " <interface1> <interface2>" << std::endl;
	std::cout << "Example: " << programName << " enp0s9 enp0s10" << std::endl;
}

int main(int argc, char* argv[])
{
	// Check for correct number of arguments
	if (argc != 3)
	{
		std::cerr << "Error: Incorrect number of arguments" << std::endl;
		printUsage(argv[0]);
		return 1;
	}

	const char* iface1 = argv[1];
	const char* iface2 = argv[2];

	// Get the two devices for bridging
	pcpp::PcapLiveDevice* dev1 = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByName(iface1);
	pcpp::PcapLiveDevice* dev2 = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByName(iface2);

	if (dev1 == nullptr || dev2 == nullptr)
	{
		std::cerr << "Error: Failed to find one or both devices" << std::endl;
		printUsage(argv[0]);
		return 1;
	}

	// Open both devices
	pcpp::PcapLiveDevice::DeviceConfiguration config;
	config.direction = pcpp::PcapLiveDevice::PcapDirection::PCPP_IN;
	if (!dev1->open(config) || !dev2->open(config))
	{
		std::cerr << "Error: Failed to open one or both devices" << std::endl;
		return 1;
	}

	// Register signal handler for Ctrl+C
	signal(SIGINT, onApplicationInterrupted);

	// Start capturing packets on both devices
	dev1->startCapture(packetArriveCallback, dev2);
	dev2->startCapture(packetArriveCallback, dev1);

	std::cout << "Bridge is running... Press Ctrl+C to stop" << std::endl;

	// Main loop to keep the application running
	while (!shouldStop)
	{
		sleep(1);
	}

	// Stop capturing and close the devices
	dev1->stopCapture();
	dev2->stopCapture();
	dev1->close();
	dev2->close();

	std::cout << "Bridge stopped" << std::endl;
	return 0;
}
