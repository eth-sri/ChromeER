// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {DOMFileSystem}
 */
var fileSystem = null;

/**
 * Map of opened files, from a <code>openRequestId</code> to <code>filePath
 * </code>.
 * @type {Object.<number, string>}
 */
var openedFiles = {};

/**
 * @type {string}
 * @const
 */
var FILE_SYSTEM_ID = 'chocolate-id';

/**
 * @type {Object}
 * @const
 */
var TESTING_ROOT = Object.freeze({
  isDirectory: true,
  name: '',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * Metadata for a testing file.
 * @type {Object}
 * @const
 */
var TESTING_TOO_LARGE_CHUNK_FILE = Object.freeze({
  isDirectory: false,
  name: 'too-large-chunk.txt',
  size: 2 * 1024 * 1024,  // 2MB
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata for a testing file.
 * @type {Object}
 * @const
 */
var TESTING_INVALID_CALLBACK_FILE = Object.freeze({
  isDirectory: false,
  name: 'invalid-request.txt',
  size: 1 * 1024 * 1024,  // 1MB
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata for a testing file.
 * @type {Object}
 * @const
 */
var TESTING_NEGATIVE_SIZE_FILE = Object.freeze({
  isDirectory: false,
  name: 'negative-size.txt',
  size: -1 * 1024 * 1024,  // -1MB
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Metadata for a testing file.
 * @type {Object}
 * @const
 */
var TESTING_RELATIVE_NAME_FILE = Object.freeze({
  isDirectory: false,
  name: '../../../b.txt',
  size: 1 * 1024 * 1024,  // 1MB
  modificationTime: new Date(2014, 1, 25, 7, 36, 12)
});

/**
 * Gets volume information for the provided file system.
 *
 * @param {string} fileSystemId Id of the provided file system.
 * @param {function(Object)} callback Callback to be called on result, with the
 *     volume information object in case of success, or null if not found.
 */
function getVolumeInfo(fileSystemId, callback) {
  chrome.fileBrowserPrivate.getVolumeMetadataList(function(volumeList) {
    for (var i = 0; i < volumeList.length; i++) {
      if (volumeList[i].extensionId == chrome.runtime.id &&
          volumeList[i].fileSystemId == fileSystemId) {
        callback(volumeList[i]);
        return;
      }
    }
    callback(null);
  });
}

/**
 * Returns metadata for the requested entry.
 *
 * To successfully acquire a DirectoryEntry, or even a DOMFileSystem, this event
 * must be implemented and return correct values.
 *
 * @param {GetMetadataRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback with metadata passed
 *     an argument.
 * @param {function(string)} onError Error callback with an error code.
 */
function onGetMetadataRequested(options, onSuccess, onError) {
  if (options.fileSystemId != FILE_SYSTEM_ID) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  if (options.entryPath == '/') {
    onSuccess(TESTING_ROOT);
    return;
  }

  if (options.entryPath == '/' + TESTING_TOO_LARGE_CHUNK_FILE.name) {
    onSuccess(TESTING_TOO_LARGE_CHUNK_FILE);
    return;
  }

  if (options.entryPath == '/' + TESTING_INVALID_CALLBACK_FILE.name) {
    onSuccess(TESTING_INVALID_CALLBACK_FILE);
    return;
  }

  if (options.entryPath == '/' + TESTING_NEGATIVE_SIZE_FILE.name) {
    onSuccess(TESTING_NEGATIVE_SIZE_FILE);
    return;
  }

  if (options.entryPath == '/' + TESTING_RELATIVE_NAME_FILE.name) {
    onSuccess(TESTING_RELATIVE_NAME_FILE);
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
}

/**
 * Requests opening a file at <code>filePath</code>. Further file operations
 * will be associated with the <code>requestId</code>
 *
 * @param {OpenFileRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onOpenFileRequested(options, onSuccess, onError) {
  if (options.fileSystemId != FILE_SYSTEM_ID) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  if (options.mode != 'READ' || options.create) {
    onError('ACCESS_DENIED');  // enum ProviderError.
    return;
  }

  if (options.filePath != '/' + TESTING_TOO_LARGE_CHUNK_FILE.name ||
      options.filePath != '/' + TESTING_INVALID_CALLBACK_FILE.name ||
      options.filePath != '/' + TESTING_NEGATIVE_SIZE_FILE.name ||
      options.filePath != '/' + TESTING_RELATIVE_NAME_FILE.name) {
    onError('NOT_FOUND');  // enum ProviderError.
    return;
  }

  openedFiles[options.requestId] = options.filePath;
  onSuccess();
}

/**
 * Requests closing a file previously opened with <code>openRequestId</code>.
 *
 * @param {CloseFileRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
function onCloseFileRequested(options, onSuccess, onError) {
  if (options.fileSystemId != FILE_SYSTEM_ID ||
      !openedFiles[options.openRequestId]) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  delete openedFiles[options.openRequestId];
  onSuccess();
}

/**
 * Requests reading contents of a file, previously opened with <code>
 * openRequestId</code>.
 *
 * @param {ReadFileRequestedOptions} options Options.
 * @param {function(ArrayBuffer, boolean)} onSuccess Success callback with a
 *     chunk of data, and information if more data will be provided later.
 * @param {function(string)} onError Error callback.
 */
function onReadFileRequested(options, onSuccess, onError) {
  var filePath = openedFiles[options.openRequestId];
  if (options.fileSystemId != FILE_SYSTEM_ID || !filePath) {
    onError('INVALID_OPERATION');  // enum ProviderError.
    return;
  }

  if (filePath == '/' + TESTING_TOO_LARGE_CHUNK_FILE.name) {
    var buffer = '';
    while (buffer.length < 4 * TESTING_TOO_LARGE_CHUNK_FILE.size) {
      buffer += 'I-LIKE-ICE-CREAM!';
    }
    var reader = new FileReader();
    reader.onload = function(e) {
      onSuccess(e.target.result, true /* hasMore */);
      onSuccess(e.target.result, true /* hasMore */);
      onSuccess(e.target.result, true /* hasMore */);
      onSuccess(e.target.result, false /* hasMore */);
    };
    reader.readAsArrayBuffer(new Blob([buffer]));
    return;
  }

  if (filePath == '/' + TESTING_INVALID_CALLBACK_FILE.name) {
    // Calling onSuccess after onError is unexpected. After handling the error
    // the request should be removed.
    onError('NOT_FOUND');
    onSuccess(new ArrayBuffer(options.length * 4), false /* hasMore */);
    return;
  }

  if (filePath == '/' + TESTING_NEGATIVE_SIZE_FILE.name) {
    onSuccess(new ArrayBuffer(-TESTING_NEGATIVE_SIZE_FILE.size * 2),
              false /* hasMore */);
    return;
  }

  if (filePath == '/' + TESTING_RELATIVE_NAME_FILE.name) {
    onSuccess(new ArrayBuffer(options.length), false /* hasMore */);
    return;
  }

  onError('INVALID_OPERATION');  // enum ProviderError.
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.mount(
      {fileSystemId: FILE_SYSTEM_ID, displayName: 'chocolate.zip'},
      function() {
        chrome.fileSystemProvider.onGetMetadataRequested.addListener(
            onGetMetadataRequested);
        chrome.fileSystemProvider.onOpenFileRequested.addListener(
            onOpenFileRequested);
        chrome.fileSystemProvider.onReadFileRequested.addListener(
            onReadFileRequested);
        var volumeId =
            'provided:' + chrome.runtime.id + '-' + FILE_SYSTEM_ID + '-user';

        getVolumeInfo(FILE_SYSTEM_ID, function(volumeInfo) {
          chrome.test.assertTrue(!!volumeInfo);
          chrome.fileBrowserPrivate.requestFileSystem(
              volumeInfo.volumeId,
              function(inFileSystem) {
                chrome.test.assertTrue(!!inFileSystem);

                fileSystem = inFileSystem;
                callback();
              });
        });
      }, function() {
        chrome.test.fail();
      });
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Tests that returning a too big chunk (4 times larger than the file size,
    // and also much more than requested 1 KB of data).
    function returnTooLargeChunk() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_TOO_LARGE_CHUNK_FILE.name,
          {create: false},
          function(fileEntry) {
            fileEntry.file(function(file) {
              // Read 1 KB of data.
              var fileSlice = file.slice(0, 1024);
              var fileReader = new FileReader();
              fileReader.onload = function(e) {
                chrome.test.fail('Reading should fail.');
              };
              fileReader.onerror = function(e) {
                onTestSuccess();
              };
              fileReader.readAsText(fileSlice);
            },
            function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Tests that calling a success callback with a non-existing request id
    // doesn't cause any harm.
    function invalidCallback() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_INVALID_CALLBACK_FILE.name,
          {create: false},
          function(fileEntry) {
            fileEntry.file(function(file) {
              // Read 1 KB of data.
              var fileSlice = file.slice(0, 1024);
              var fileReader = new FileReader();
              fileReader.onload = function(e) {
                chrome.test.fail('Reading should fail.');
              };
              fileReader.onerror = function(e) {
                onTestSuccess();
              };
              fileReader.readAsText(fileSlice);
            },
            function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Test that reading from files with negative size is not allowed.
    function negativeSize() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_NEGATIVE_SIZE_FILE.name,
          {create: false},
          function(fileEntry) {
            fileEntry.file(function(file) {
              // Read 1 KB of data.
              var fileSlice = file.slice(0, 1024);
              var fileReader = new FileReader();
              fileReader.onload = function(e) {
                var text = fileReader.result;
                chrome.test.assertEq(0, text.length);
                onTestSuccess();
              };
              fileReader.onerror = function(e) {
                onTestSuccess();
              };
              fileReader.readAsText(fileSlice);
            },
            function(error) {
              chrome.test.fail(error.name);
            });
          },
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Tests that URLs generated from a file containing .. inside is properly
    // escaped.
    function relativeName() {
      var onTestSuccess = chrome.test.callbackPass();
      fileSystem.root.getFile(
          TESTING_RELATIVE_NAME_FILE.name,
          {create: false},
          function(fileEntry) {
            chrome.test.fail('Opening a file should fail.');
          },
          function(error) {
            onTestSuccess();
          });
    },  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
