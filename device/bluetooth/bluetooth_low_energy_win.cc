// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_win.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/windows_version.h"

namespace {

const char kPlatformNotSupported[] =
    "Bluetooth Low energy is only supported on Windows 8 and later.";
const char kDeviceEnumError[] = "Error enumerating Bluetooth LE devices.";
const char kDeviceInfoError[] =
    "Error retrieving Bluetooth LE device information.";
const char kDeviceAddressError[] =
    "Device instance ID value does not seem to contain a Bluetooth Adapter "
    "address.";
const char kInvalidBluetoothAddress[] = "Bluetooth address format is invalid.";

// Like ScopedHandle but for HDEVINFO.  Only use this on HDEVINFO returned from
// SetupDiGetClassDevs.
class DeviceInfoSetTraits {
 public:
  typedef HDEVINFO Handle;

  static bool CloseHandle(HDEVINFO handle) {
    return ::SetupDiDestroyDeviceInfoList(handle) != FALSE;
  }

  static bool IsHandleValid(HDEVINFO handle) {
    return handle != INVALID_HANDLE_VALUE;
  }

  static HDEVINFO NullHandle() { return INVALID_HANDLE_VALUE; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DeviceInfoSetTraits);
};

typedef base::win::GenericScopedHandle<DeviceInfoSetTraits,
                                       base::win::VerifierTraits>
    ScopedDeviceInfoSetHandle;

bool StringToBluetoothAddress(const std::string& value,
                              BLUETOOTH_ADDRESS* btha,
                              std::string* error) {
  if (value.length() != 6 * 2) {
    *error = kInvalidBluetoothAddress;
    return false;
  }

  int buffer[6];
  int result = sscanf_s(value.c_str(),
                        "%02X%02X%02X%02X%02X%02X",
                        &buffer[5],
                        &buffer[4],
                        &buffer[3],
                        &buffer[2],
                        &buffer[1],
                        &buffer[0]);
  if (result != 6) {
    *error = kInvalidBluetoothAddress;
    return false;
  }

  ZeroMemory(btha, sizeof(*btha));
  btha->rgBytes[0] = buffer[0];
  btha->rgBytes[1] = buffer[1];
  btha->rgBytes[2] = buffer[2];
  btha->rgBytes[3] = buffer[3];
  btha->rgBytes[4] = buffer[4];
  btha->rgBytes[5] = buffer[5];
  return true;
}

std::string FormatBluetoothError(const char* message, HRESULT hr) {
  std::ostringstream string_stream;
  string_stream << message;
  if (FAILED(hr))
    string_stream << logging::SystemErrorCodeToString(hr);
  return string_stream.str();
}

bool CheckInsufficientBuffer(bool success,
                             const char* message,
                             std::string* error) {
  if (success) {
    *error = FormatBluetoothError(message, S_OK);
    return false;
  }

  HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
  if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
    *error = FormatBluetoothError(message, hr);
    return false;
  }

  return true;
}

bool CheckSuccess(HRESULT hr, const char* message, std::string* error) {
  if (FAILED(hr)) {
    *error = FormatBluetoothError(message, hr);
    return false;
  }

  return true;
}

bool CheckExpectedLength(size_t actual_length,
                         size_t expected_length,
                         const char* message,
                         std::string* error) {
  if (actual_length != expected_length) {
    *error = FormatBluetoothError(message, E_FAIL);
    return false;
  }

  return true;
}

// Represents a device registry property value
class DeviceRegistryPropertyValue {
 public:
  static scoped_ptr<DeviceRegistryPropertyValue>
  Create(DWORD property_type, scoped_ptr<UINT8[]> value, size_t value_size) {
    if (property_type == REG_SZ) {
      // Ensure string is zero terminated.
      CHECK_GE(value_size, 1u);
      WCHAR* value_string = reinterpret_cast<WCHAR*>(value.get());
      value_string[value_size - 1] = 0;
    }
    return scoped_ptr<DeviceRegistryPropertyValue>(
        new DeviceRegistryPropertyValue(
            property_type, value.Pass(), value_size));
  }

  bool AsString(std::string* value, std::string* error) {
    if (property_type_ != REG_SZ) {
      *error = "Property is not a string";
      return false;
    }

    WCHAR* value_string = reinterpret_cast<WCHAR*>(value_.get());
    *value = base::SysWideToUTF8(value_string);
    return true;
  }

 private:
  DeviceRegistryPropertyValue(DWORD property_type,
                              scoped_ptr<UINT8[]> value,
                              size_t value_size)
      : property_type_(property_type),
        value_(value.Pass()),
        value_size_(value_size) {}

  DWORD property_type_;
  scoped_ptr<UINT8[]> value_;
  size_t value_size_;

  DISALLOW_COPY_AND_ASSIGN(DeviceRegistryPropertyValue);
};

bool CollectBluetoothLowEnergyDeviceRegistryProperty(
    const ScopedDeviceInfoSetHandle& device_info_handle,
    PSP_DEVINFO_DATA device_info_data,
    DWORD property_id,
    scoped_ptr<DeviceRegistryPropertyValue>* value,
    std::string* error) {
  ULONG required_length = 0;
  BOOL success = SetupDiGetDeviceRegistryProperty(device_info_handle,
                                                  device_info_data,
                                                  property_id,
                                                  NULL,
                                                  NULL,
                                                  0,
                                                  &required_length);
  if (!CheckInsufficientBuffer(!!success, kDeviceInfoError, error))
    return false;

  scoped_ptr<UINT8[]> property_value(new UINT8[required_length]);
  ULONG actual_length = required_length;
  DWORD property_type;
  success = SetupDiGetDeviceRegistryProperty(device_info_handle,
                                             device_info_data,
                                             property_id,
                                             &property_type,
                                             property_value.get(),
                                             actual_length,
                                             &required_length);
  if (!CheckSuccess(!!success, kDeviceInfoError, error))
    return false;
  if (!CheckExpectedLength(
          actual_length, required_length, kDeviceInfoError, error))
    return false;

  (*value) = DeviceRegistryPropertyValue::Create(
                 property_type, property_value.Pass(), actual_length).Pass();
  return true;
}

bool CollectBluetoothLowEnergyDeviceInstanceId(
    const ScopedDeviceInfoSetHandle& device_info_handle,
    PSP_DEVINFO_DATA device_info_data,
    scoped_ptr<device::win::BluetoothLowEnergyDeviceInfo>& device_info,
    std::string* error) {
  ULONG required_length = 0;
  BOOL success = SetupDiGetDeviceInstanceId(
      device_info_handle, device_info_data, NULL, 0, &required_length);
  if (!CheckInsufficientBuffer(!!success, kDeviceInfoError, error))
    return false;

  scoped_ptr<WCHAR[]> instance_id(new WCHAR[required_length]);
  ULONG actual_length = required_length;
  success = SetupDiGetDeviceInstanceId(device_info_handle,
                                       device_info_data,
                                       instance_id.get(),
                                       actual_length,
                                       &required_length);
  if (!CheckSuccess(!!success, kDeviceInfoError, error))
    return false;
  if (!CheckExpectedLength(
          actual_length, required_length, kDeviceInfoError, error)) {
    return false;
  }

  if (actual_length >= 1) {
    // Ensure string is zero terminated.
    instance_id.get()[actual_length - 1] = 0;
    device_info->id = base::SysWideToUTF8(instance_id.get());
  }
  return true;
}

bool CollectDeviceFriendlyName(
    const ScopedDeviceInfoSetHandle& device_info_handle,
    PSP_DEVINFO_DATA device_info_data,
    scoped_ptr<device::win::BluetoothLowEnergyDeviceInfo>& device_info,
    std::string* error) {
  scoped_ptr<DeviceRegistryPropertyValue> property_value;
  if (!CollectBluetoothLowEnergyDeviceRegistryProperty(device_info_handle,
                                                       device_info_data,
                                                       SPDRP_FRIENDLYNAME,
                                                       &property_value,
                                                       error)) {
    return false;
  }

  if (!property_value->AsString(&device_info->friendly_name, error)) {
    return false;
  }

  return true;
}

bool ExtractBluetoothAddressFromDeviceInstanceId(const std::string& instance_id,
                                                 BLUETOOTH_ADDRESS* btha,
                                                 std::string* error) {
  size_t start = instance_id.find("_");
  if (start == std::string::npos) {
    *error = kDeviceAddressError;
    return false;
  }
  size_t end = instance_id.find("\\", start);
  if (end == std::string::npos) {
    *error = kDeviceAddressError;
    return false;
  }

  start++;
  std::string address = instance_id.substr(start, end - start);
  if (!StringToBluetoothAddress(address, btha, error))
    return false;

  return true;
}

bool CollectBluetoothLowEnergyDeviceAddress(
    const ScopedDeviceInfoSetHandle& device_info_handle,
    PSP_DEVINFO_DATA device_info_data,
    scoped_ptr<device::win::BluetoothLowEnergyDeviceInfo>& device_info,
    std::string* error) {
  // TODO(rpaquay): We exctract the bluetooth device address from the device
  // instance ID string, as we did not find a more formal API for retrieving the
  // bluetooth address of a Bluetooth Low Energy device.
  // An Bluetooth device instance ID has the following format (under Win8+):
  // BTHLE\DEV_BC6A29AB5FB0\8&31038925&0&BC6A29AB5FB0
  return ExtractBluetoothAddressFromDeviceInstanceId(
      device_info->id, &device_info->address, error);
}

bool CollectBluetoothLowEnergyDeviceInfo(
    const ScopedDeviceInfoSetHandle& device_info_handle,
    PSP_DEVICE_INTERFACE_DATA device_interface_data,
    scoped_ptr<device::win::BluetoothLowEnergyDeviceInfo>* device_info,
    std::string* error) {
  // Retrieve required # of bytes for interface details
  ULONG required_length = 0;
  BOOL success = SetupDiGetDeviceInterfaceDetail(device_info_handle,
                                                 device_interface_data,
                                                 NULL,
                                                 0,
                                                 &required_length,
                                                 NULL);
  if (!CheckInsufficientBuffer(!!success, kDeviceInfoError, error))
    return false;

  scoped_ptr<UINT8[]> interface_data(new UINT8[required_length]);
  ZeroMemory(interface_data.get(), required_length);

  PSP_DEVICE_INTERFACE_DETAIL_DATA device_interface_detail_data =
      reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(interface_data.get());
  device_interface_detail_data->cbSize =
      sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

  SP_DEVINFO_DATA device_info_data = {0};
  device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

  ULONG actual_length = required_length;
  success = SetupDiGetDeviceInterfaceDetail(device_info_handle,
                                            device_interface_data,
                                            device_interface_detail_data,
                                            actual_length,
                                            &required_length,
                                            &device_info_data);
  if (!CheckSuccess(!!success, kDeviceInfoError, error))
    return false;
  if (!CheckExpectedLength(
          actual_length, required_length, kDeviceInfoError, error))
    return false;

  scoped_ptr<device::win::BluetoothLowEnergyDeviceInfo> result(
      new device::win::BluetoothLowEnergyDeviceInfo());
  result->path =
      base::FilePath(std::wstring(device_interface_detail_data->DevicePath));
  if (!CollectBluetoothLowEnergyDeviceInstanceId(
          device_info_handle, &device_info_data, result, error)) {
    return false;
  }
  if (!CollectDeviceFriendlyName(
          device_info_handle, &device_info_data, result, error)) {
    return false;
  }
  if (!CollectBluetoothLowEnergyDeviceAddress(
          device_info_handle, &device_info_data, result, error)) {
    return false;
  }
  (*device_info) = result.Pass();
  return true;
}

enum DeviceInfoResult { kOk, kError, kNoMoreDevices };

DeviceInfoResult EnumerateSingleBluetoothLowEnergyDevice(
    const ScopedDeviceInfoSetHandle& device_info_handle,
    DWORD device_index,
    scoped_ptr<device::win::BluetoothLowEnergyDeviceInfo>* device_info,
    std::string* error) {
  // Enumerate device of LE_DEVICE interface class
  GUID BluetoothInterfaceGUID = GUID_BLUETOOTHLE_DEVICE_INTERFACE;
  SP_DEVICE_INTERFACE_DATA device_interface_data = {0};
  device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
  BOOL success = ::SetupDiEnumDeviceInterfaces(device_info_handle,
                                               NULL,
                                               &BluetoothInterfaceGUID,
                                               device_index,
                                               &device_interface_data);
  if (!success) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    if (hr == HRESULT_FROM_WIN32(ERROR_NO_MORE_ITEMS)) {
      return kNoMoreDevices;
    }
    *error = FormatBluetoothError(kDeviceInfoError, hr);
    return kError;
  }

  if (!CollectBluetoothLowEnergyDeviceInfo(
          device_info_handle, &device_interface_data, device_info, error))
    return kError;

  return kOk;
}

// Opens a Device Info Set that can be used to enumerate Bluetooth LE devices
// present on the machine.
HRESULT OpenBluetoothLowEnergyDevices(ScopedDeviceInfoSetHandle* handle) {
  GUID BluetoothClassGUID = GUID_BLUETOOTHLE_DEVICE_INTERFACE;
  ScopedDeviceInfoSetHandle result(SetupDiGetClassDevs(
      &BluetoothClassGUID, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
  if (!result.IsValid()) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  (*handle) = result.Pass();
  return S_OK;
}

// Opens a Device Info Set that can be used to enumerate Bluetooth LE devices
// exposing a service GUID.
HRESULT OpenBluetoothLowEnergyService(const GUID& service_guid,
                                      ScopedDeviceInfoSetHandle* handle) {
  ScopedDeviceInfoSetHandle result(SetupDiGetClassDevs(
      &service_guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
  if (!result.IsValid()) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  (*handle) = result.Pass();
  return S_OK;
}

}  // namespace

namespace device {
namespace win {

bool IsBluetoothLowEnergySupported() {
  return base::win::GetVersion() >= base::win::VERSION_WIN8;
}

bool EnumerateKnownBluetoothLowEnergyDevices(
    ScopedVector<BluetoothLowEnergyDeviceInfo>* devices,
    std::string* error) {
  if (!IsBluetoothLowEnergySupported()) {
    *error = kPlatformNotSupported;
    return false;
  }

  ScopedDeviceInfoSetHandle info_set_handle;
  HRESULT hr = OpenBluetoothLowEnergyDevices(&info_set_handle);
  if (FAILED(hr)) {
    *error = FormatBluetoothError(kDeviceEnumError, hr);
    return false;
  }

  for (DWORD i = 0;; ++i) {
    scoped_ptr<BluetoothLowEnergyDeviceInfo> device_info;
    DeviceInfoResult result = EnumerateSingleBluetoothLowEnergyDevice(
        info_set_handle, i, &device_info, error);
    switch (result) {
      case kNoMoreDevices:
        return true;
      case kError:
        return false;
      case kOk:
        devices->push_back(device_info.release());
    }
  }
}

bool ExtractBluetoothAddressFromDeviceInstanceIdForTesting(
    const std::string& instance_id,
    BLUETOOTH_ADDRESS* btha,
    std::string* error) {
  return ExtractBluetoothAddressFromDeviceInstanceId(instance_id, btha, error);
}

}  // namespace win
}  // namespace device
