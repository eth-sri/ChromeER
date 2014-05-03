// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var firstFileSystemId;
var secondFileSystemId;

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.mount('chocolate.zip', function(id) {
    firstFileSystemId = id;
    if (firstFileSystemId && secondFileSystemId)
      callback();
  }, function() {
    chrome.test.fail();
  });

  chrome.fileSystemProvider.mount('banana.zip', function(id) {
    secondFileSystemId = id;
    if (firstFileSystemId && secondFileSystemId)
      callback();
  }, function() {
    chrome.test.fail();
  });
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Tests the fileSystemProvider.unmount(). Verifies if the unmount event
    // is emitted by VolumeManager.
    function unmount() {
      var onTestSuccess = chrome.test.callbackPass(function() {});
      var firstVolumeId =
          'provided:' + chrome.runtime.id + '-' + firstFileSystemId + '-user';

      var onMountCompleted = function(event) {
        chrome.test.assertEq('unmount', event.eventType);
        chrome.test.assertEq('success', event.status);
        chrome.test.assertEq(firstVolumeId, event.volumeMetadata.volumeId);
        chrome.fileBrowserPrivate.onMountCompleted.removeListener(
            onMountCompleted);
        onTestSuccess();
      };

      chrome.fileBrowserPrivate.onMountCompleted.addListener(
          onMountCompleted);
      chrome.fileSystemProvider.unmount(firstFileSystemId, function() {
        // Wait for the unmount event.
      }, function(error) {
        chrome.test.fail();
      });
    },

    // Tests the fileSystemProvider.unmount() with a wrong id. Verifies that
    // it fails with a correct error code.
    function unmountWrongId() {
      var onTestSuccess = chrome.test.callbackPass(function() {});
      chrome.fileSystemProvider.unmount(1337, function(fileSystemId) {
        chrome.test.fail();
      }, function(error) {
        chrome.test.assertEq('SecurityError', error.name);
        onTestSuccess();
      });
    },

    // Tests if fileBrowserPrivate.removeMount() for provided file systems emits
    // the onMountRequested() event with correct arguments.
    function requestUnmountSuccess() {
      var onTestSuccess = chrome.test.callbackPass(function() {});
      var secondVolumeId =
          'provided:' + chrome.runtime.id + '-' + secondFileSystemId + '-user';

      var onUnmountRequested = function(fileSystemId, onSuccess, onError) {
        chrome.test.assertEq(secondFileSystemId, fileSystemId);
        onSuccess();
        // Not calling fileSystemProvider.unmount(), so the onMountCompleted
        // event will not be raised.
        chrome.fileSystemProvider.onUnmountRequested.removeListener(
            onUnmountRequested);
        onTestSuccess();
      };

      chrome.fileSystemProvider.onUnmountRequested.addListener(
          onUnmountRequested);
      chrome.fileBrowserPrivate.removeMount(secondVolumeId);
    },

    // End to end test with a failure. Invokes fileSystemProvider.removeMount()
    // on a provided file system, and verifies (1) if the onMountRequested()
    // event is called with correct aguments, and (2) if calling onError(),
    // results in an unmount event fired from the VolumeManager instance.
    function requestUnmountError() {
      var onTestSuccess = chrome.test.callbackPass(function() {});
      var secondVolumeId =
          'provided:' + chrome.runtime.id + '-' + secondFileSystemId + '-user';
      var unmountRequested = false;

      var onUnmountRequested = function(fileSystemId, onSuccess, onError) {
        chrome.test.assertEq(false, unmountRequested);
        chrome.test.assertEq(secondFileSystemId, fileSystemId);
        onError('IN_USE');  // enum ProviderError.
        unmountRequested = true;
        chrome.fileSystemProvider.onUnmountRequested.removeListener(
            onUnmountRequested);
      };

      var onMountCompleted = function(event) {
        chrome.test.assertEq('unmount', event.eventType);
        chrome.test.assertEq('error_unknown', event.status);
        chrome.test.assertEq(secondVolumeId, event.volumeMetadata.volumeId);
        chrome.test.assertTrue(unmountRequested);

        // Remove the handlers and mark the test as succeeded.
        chrome.fileBrowserPrivate.removeMount(secondVolumeId);
        chrome.fileBrowserPrivate.onMountCompleted.removeListener(
            onMountCompleted);
        onTestSuccess();
      };

      chrome.fileSystemProvider.onUnmountRequested.addListener(
          onUnmountRequested);
      chrome.fileBrowserPrivate.onMountCompleted.addListener(onMountCompleted);
      chrome.fileBrowserPrivate.removeMount(secondVolumeId);
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
