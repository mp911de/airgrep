#include "usb.hpp"

#include <libusb.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <map>
#include <mutex>

#include "usb/usb_constants.h"

namespace device::usb_bus {
namespace {
constexpr uint8_t kVendorClass = 0xFF;
constexpr uint8_t kResetRequestType =
	static_cast<uint8_t>(static_cast<unsigned>(LIBUSB_ENDPOINT_OUT) |
						 static_cast<unsigned>(LIBUSB_REQUEST_TYPE_VENDOR) |
						 static_cast<unsigned>(LIBUSB_RECIPIENT_INTERFACE));
constexpr unsigned kControlTimeoutMs = 3000;

[[noreturn]] void fail(const char* what, int code) {
	throw Error(std::format("{}: {}", what, libusb_strerror(code)));
}

libusb_context* context() {
	static libusb_context* instance = [] {
		libusb_context* created = nullptr;
		if (const int code = libusb_init(&created); code < 0) {
			fail("libusb does not start", code);
		}
		return created;
	}();
	return instance;
}

// The devices that are present. None and a negative count where libusb fails.
struct DeviceList {
	DeviceList() {
		libusb_device** list = nullptr;
		count = libusb_get_device_list(context(), &list);
		devices = {list, static_cast<size_t>(std::max<ssize_t>(count, 0))};
	}
	DeviceList(const DeviceList&) = delete;
	DeviceList& operator=(const DeviceList&) = delete;
	DeviceList(DeviceList&&) = delete;
	DeviceList& operator=(DeviceList&&) = delete;
	~DeviceList() {
		if (devices.data()) {
			libusb_free_device_list(devices.data(), 1);
		}
	}
	std::span<libusb_device*> devices;
	ssize_t count;
};

bool ours(libusb_device* device) {
	libusb_device_descriptor descriptor{};
	return libusb_get_device_descriptor(device, &descriptor) == 0 &&
		   descriptor.idVendor == AIRGREP_USB_VENDOR &&
		   descriptor.idProduct == AIRGREP_USB_PRODUCT;
}

struct DeviceReference {
	explicit DeviceReference(libusb_device* device)
		: device(libusb_ref_device(device)) {}
	~DeviceReference() { libusb_unref_device(device); }
	DeviceReference(const DeviceReference&) = delete;
	DeviceReference& operator=(const DeviceReference&) = delete;
	DeviceReference(DeviceReference&&) = delete;
	DeviceReference& operator=(DeviceReference&&) = delete;
	libusb_device* device;
	std::string serial;
};
std::mutex serials_mutex;
std::map<libusb_device*, DeviceReference> serials;

std::string serial_of(libusb_device* device) {
	const std::scoped_lock lock(serials_mutex);
	const auto known = serials.find(device);
	if (known != serials.end()) {
		return known->second.serial;
	}
	libusb_device_descriptor descriptor{};
	libusb_device_handle* handle = nullptr;
	if (libusb_get_device_descriptor(device, &descriptor) != 0 ||
		libusb_open(device, &handle) != 0) {
		return "";
	}
	unsigned char text[64] = {};
	const int length = libusb_get_string_descriptor_ascii(
		handle, descriptor.iSerialNumber, text, sizeof text);
	libusb_close(handle);
	std::string serial(reinterpret_cast<char*>(text), std::max(length, 0));
	const bool ours =
		serial.size() == 12 && std::ranges::all_of(serial, [](unsigned char c) {
			return std::isdigit(c) || (c >= 'a' && c <= 'f');
		});
	if (!ours) {
		return "";
	}
	serials.try_emplace(device, device).first->second.serial = serial;
	return serial;
}

struct Endpoints {
	int interface = -1;
	uint8_t in = 0, out = 0;
	uint16_t packet = 64;
};

Endpoints endpoints_of(libusb_device* device) {
	Endpoints found;
	libusb_config_descriptor* configuration = nullptr;
	if (libusb_get_active_config_descriptor(device, &configuration) != 0) {
		return found;
	}
	for (const libusb_interface& alternatives :
		 std::span(configuration->interface, configuration->bNumInterfaces)) {
		if (alternatives.num_altsetting < 1 ||
			alternatives.altsetting[0].bInterfaceClass != kVendorClass) {
			continue;
		}
		const libusb_interface_descriptor& interface =
			alternatives.altsetting[0];
		Endpoints candidate{.interface = interface.bInterfaceNumber};
		for (const libusb_endpoint_descriptor& endpoint :
			 std::span(interface.endpoint, interface.bNumEndpoints)) {
			if ((endpoint.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
				LIBUSB_TRANSFER_TYPE_BULK) {
				continue;
			}
			if (endpoint.bEndpointAddress & LIBUSB_ENDPOINT_IN) {
				candidate.in = endpoint.bEndpointAddress;
				candidate.packet = endpoint.wMaxPacketSize;
			} else {
				candidate.out = endpoint.bEndpointAddress;
			}
		}
		if (candidate.in && candidate.out && candidate.packet) {
			found = candidate;
		}
	}
	libusb_free_config_descriptor(configuration);
	return found;
}

class UsbTransport : public Transport {
   public:
	UsbTransport(libusb_device_handle* handle, Endpoints endpoints)
		: handle_(handle), endpoints_(endpoints) {}

	~UsbTransport() override {
		libusb_release_interface(handle_, endpoints_.interface);
		libusb_close(handle_);
	}
	UsbTransport(const UsbTransport&) = delete;
	UsbTransport& operator=(const UsbTransport&) = delete;
	UsbTransport(UsbTransport&&) = delete;
	UsbTransport& operator=(UsbTransport&&) = delete;

	void reset_session() override {
		const int code = libusb_control_transfer(
			handle_, kResetRequestType, AIRGREP_USB_RESET_SESSION, 0,
			static_cast<uint16_t>(endpoints_.interface), nullptr, 0,
			kControlTimeoutMs);
		if (code < 0) {
			fail("Session reset failed", code);
		}
	}

	void write(std::span<const uint8_t> frame) override {
		int sent = 0;
		const int code = libusb_bulk_transfer(
			handle_, endpoints_.out, const_cast<uint8_t*>(frame.data()),
			static_cast<int>(frame.size()), &sent, kControlTimeoutMs);
		if (code < 0) {
			fail("Request not sent", code);
		}
		if (static_cast<size_t>(sent) != frame.size()) {
			throw Error("Incomplete request write, command outcome unknown");
		}
	}

	// A host read cannot split a USB packet, so it asks for whole packets. It
	// ends with the last of them or with a shorter one.
	bool read(std::vector<uint8_t>& bytes, size_t wanted,
			  std::chrono::milliseconds timeout) override {
		const size_t before = bytes.size();
		const size_t packets = std::max<size_t>(
			(wanted + endpoints_.packet - 1) / endpoints_.packet, 1);
		const size_t room = packets * endpoints_.packet;
		bytes.resize(before + room);
		int received = 0;
		const int code = libusb_bulk_transfer(
			handle_, endpoints_.in, bytes.data() + before,
			static_cast<int>(room), &received,
			static_cast<unsigned>(std::max<int64_t>(timeout.count(), 1)));
		bytes.resize(before + std::max(received, 0));
		if (code < 0 && code != LIBUSB_ERROR_TIMEOUT) {
			fail("Device does not answer", code);
		}
		return received > 0;
	}

   private:
	libusb_device_handle* handle_;
	Endpoints endpoints_;
};
}

std::vector<Found> list() {
	std::vector<Found> found;
	const DeviceList all;
	{
		if (all.count < 0) {
			fail("Cannot list USB devices", static_cast<int>(all.count));
		}
		const std::scoped_lock lock(serials_mutex);
		std::erase_if(serials, [&](const auto& known) {
			return std::ranges::find(all.devices, known.first) ==
				   all.devices.end();
		});
	}
	for (libusb_device* device : all.devices) {
		if (!ours(device)) {
			continue;
		}
		const std::string serial = serial_of(device);
		if (serial.empty()) {
			continue;
		}
		found.push_back({serial, endpoints_of(device).interface >= 0});
	}
	return found;
}

std::unique_ptr<Transport> open(const std::string& serial) {
	const DeviceList all;
	for (libusb_device* device : all.devices) {
		if (!ours(device) || serial_of(device) != serial) {
			continue;
		}
		const Endpoints endpoints = endpoints_of(device);
		if (endpoints.interface < 0) {
			throw Error("Device has no data interface. Upgrade the firmware");
		}
		libusb_device_handle* handle = nullptr;
		if (const int code = libusb_open(device, &handle); code < 0) {
			fail("Device does not open", code);
		}
		if (const int code =
				libusb_claim_interface(handle, endpoints.interface);
			code < 0) {
			libusb_close(handle);
			fail("Data interface is in use", code);
		}
		return std::make_unique<UsbTransport>(handle, endpoints);
	}
	throw Error("Device is not connected");
}
}
