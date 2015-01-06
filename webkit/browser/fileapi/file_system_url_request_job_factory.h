// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_

#include <string>

#include "net/url_request/url_request_job_factory.h"

#include "webkit/browser/storage_browser_export.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace storage {

class FileSystemContext;

// |context|'s lifetime should exceed the lifetime of the ProtocolHandler.
// Currently, this is only used by ProfileIOData which owns |context| and the
// ProtocolHandler.
STORAGE_EXPORT net::URLRequestJobFactory::ProtocolHandler*
    CreateFileSystemProtocolHandler(const std::string& storage_domain,
                                    FileSystemContext* context);

}  // namespace storage

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_URL_REQUEST_JOB_FACTORY_H_
