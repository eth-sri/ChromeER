// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/serial/serial_api.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/serial.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_function.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

using content::BrowserThread;

namespace {

class SerialApiTest : public ExtensionApiTest {
 public:
  SerialApiTest() {}
};

}  // namespace

namespace extensions {

class FakeSerialGetDevicesFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunAsync() OVERRIDE {
    base::ListValue* devices = new base::ListValue();
    base::DictionaryValue* device0 = new base::DictionaryValue();
    device0->SetString("path", "/dev/fakeserial");
    base::DictionaryValue* device1 = new base::DictionaryValue();
    device1->SetString("path", "\\\\COM800\\");
    devices->Append(device0);
    devices->Append(device1);
    SetResult(devices);
    SendResponse(true);
    return true;
  }
 protected:
  virtual ~FakeSerialGetDevicesFunction() {}
};

class FakeEchoSerialIoHandler : public SerialIoHandler {
 public:
  explicit FakeEchoSerialIoHandler() : opened_(false) {}

  virtual void Open(const std::string& port,
                    const OpenCompleteCallback& callback) OVERRIDE {
    DCHECK(!opened_);
    opened_ = true;
    callback.Run(true);
  }

  virtual bool ConfigurePort(
      const api::serial::ConnectionOptions& options) OVERRIDE {
    return true;
  }

  virtual void ReadImpl() OVERRIDE {}

  virtual void CancelReadImpl() OVERRIDE {
    QueueReadCompleted(0, read_cancel_reason());
  }

  virtual void WriteImpl() OVERRIDE {
    DCHECK(pending_read_buffer());
    DCHECK_LE(pending_write_buffer_len(), pending_read_buffer_len());
    memcpy(pending_read_buffer()->data(),
           pending_write_buffer()->data(),
           pending_write_buffer_len());
    QueueReadCompleted(pending_write_buffer_len(),
                       api::serial::RECEIVE_ERROR_NONE);
    QueueWriteCompleted(pending_write_buffer_len(),
                        api::serial::SEND_ERROR_NONE);
  }

  virtual void CancelWriteImpl() OVERRIDE {
    QueueWriteCompleted(0, write_cancel_reason());
  }

  virtual bool GetControlSignals(
      api::serial::DeviceControlSignals* signals) const OVERRIDE {
    signals->dcd = true;
    signals->cts = true;
    signals->ri = true;
    signals->dsr = true;
    return true;
  }

  virtual bool GetPortInfo(api::serial::ConnectionInfo* info) const OVERRIDE {
    info->bitrate.reset(new int(9600));
    info->data_bits = api::serial::DATA_BITS_EIGHT;
    info->parity_bit = api::serial::PARITY_BIT_NO;
    info->stop_bits = api::serial::STOP_BITS_ONE;
    info->cts_flow_control.reset(new bool(false));
    return true;
  }

  virtual bool Flush() const OVERRIDE { return true; }

  MOCK_METHOD1(SetControlSignals, bool(const api::serial::HostControlSignals&));

 protected:
  virtual ~FakeEchoSerialIoHandler() {}

 private:
  bool opened_;

  DISALLOW_COPY_AND_ASSIGN(FakeEchoSerialIoHandler);
};

class FakeSerialConnectFunction : public api::SerialConnectFunction {
 protected:
  virtual SerialConnection* CreateSerialConnection(
      const std::string& port,
      const std::string& owner_extension_id) const OVERRIDE {
    scoped_refptr<FakeEchoSerialIoHandler> io_handler =
        new FakeEchoSerialIoHandler;
    EXPECT_CALL(*io_handler, SetControlSignals(_)).Times(1).WillOnce(
        Return(true));
    SerialConnection* serial_connection =
        new SerialConnection(port, owner_extension_id);
    serial_connection->SetIoHandlerForTest(io_handler);
    return serial_connection;
  }

 protected:
  virtual ~FakeSerialConnectFunction() {}
};

}  // namespace extensions

ExtensionFunction* FakeSerialGetDevicesFunctionFactory() {
  return new extensions::FakeSerialGetDevicesFunction();
}

ExtensionFunction* FakeSerialConnectFunctionFactory() {
  return new extensions::FakeSerialConnectFunction();
}

// Disable SIMULATE_SERIAL_PORTS only if all the following are true:
//
// 1. You have an Arduino or compatible board attached to your machine and
// properly appearing as the first virtual serial port ("first" is very loosely
// defined as whichever port shows up in serial.getPorts). We've tested only
// the Atmega32u4 Breakout Board and Arduino Leonardo; note that both these
// boards are based on the Atmel ATmega32u4, rather than the more common
// Arduino '328p with either FTDI or '8/16u2 USB interfaces. TODO: test more
// widely.
//
// 2. Your user has permission to read/write the port. For example, this might
// mean that your user is in the "tty" or "uucp" group on Ubuntu flavors of
// Linux, or else that the port's path (e.g., /dev/ttyACM0) has global
// read/write permissions.
//
// 3. You have uploaded a program to the board that does a byte-for-byte echo
// on the virtual serial port at 57600 bps. An example is at
// chrome/test/data/extensions/api_test/serial/api/serial_arduino_test.ino.
//
#define SIMULATE_SERIAL_PORTS (1)
IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialFakeHardware) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

#if SIMULATE_SERIAL_PORTS
  ASSERT_TRUE(extensions::ExtensionFunctionDispatcher::OverrideFunction(
      "serial.getDevices", FakeSerialGetDevicesFunctionFactory));
  ASSERT_TRUE(extensions::ExtensionFunctionDispatcher::OverrideFunction(
      "serial.connect", FakeSerialConnectFunctionFactory));
#endif

  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardware) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/real_hardware")) << message_;
}
