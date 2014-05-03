// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_system_provider/file_system_provider_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::RequestManager;
using chromeos::file_system_provider::RequestValue;
using chromeos::file_system_provider::Service;

namespace extensions {
namespace {

// Error names from
// http://www.w3.org/TR/file-system-api/#errors-and-exceptions
const char kNotFoundErrorName[] = "NotFoundError";
const char kSecurityErrorName[] = "SecurityError";

// Error messages.
const char kEmptyNameErrorMessage[] = "Empty display name is not allowed.";
const char kMountFailedErrorMessage[] = "Mounting the file system failed.";
const char kUnmountFailedErrorMessage[] = "Unmounting the file system failed.";
const char kResponseFailedErrorMessage[] =
    "Sending a response for the request failed.";

// Creates a dictionary, which looks like a DOMError. The returned dictionary
// will be converted to a real DOMError object in
// file_system_provier_custom_bindings.js.
base::DictionaryValue* CreateError(const std::string& name,
                                   const std::string& message) {
  base::DictionaryValue* error = new base::DictionaryValue();
  error->SetString("name", name);
  error->SetString("message", message);
  return error;
}

// Converts ProviderError to base::File::Error. This could be redundant, if it
// was possible to create DOMError instances in Javascript easily.
base::File::Error ProviderErrorToFileError(
    api::file_system_provider::ProviderError error) {
  switch (error) {
    case api::file_system_provider::PROVIDER_ERROR_OK:
      return base::File::FILE_OK;
    case api::file_system_provider::PROVIDER_ERROR_IN_USE:
      return base::File::FILE_ERROR_IN_USE;
    case api::file_system_provider::PROVIDER_ERROR_EXISTS:
      return base::File::FILE_ERROR_EXISTS;
    case api::file_system_provider::PROVIDER_ERROR_NOT_FOUND:
      return base::File::FILE_ERROR_NOT_FOUND;
    case api::file_system_provider::PROVIDER_ERROR_ACCESS_DENIED:
      return base::File::FILE_ERROR_ACCESS_DENIED;
    case api::file_system_provider::PROVIDER_ERROR_TOO_MANY_OPENED:
      return base::File::FILE_ERROR_TOO_MANY_OPENED;
    case api::file_system_provider::PROVIDER_ERROR_NO_MEMORY:
      return base::File::FILE_ERROR_NO_MEMORY;
    case api::file_system_provider::PROVIDER_ERROR_NO_SPACE:
      return base::File::FILE_ERROR_NO_SPACE;
    case api::file_system_provider::PROVIDER_ERROR_NOT_A_DIRECTORY:
      return base::File::FILE_ERROR_NOT_A_DIRECTORY;
    case api::file_system_provider::PROVIDER_ERROR_INVALID_OPERATION:
      return base::File::FILE_ERROR_INVALID_OPERATION;
    case api::file_system_provider::PROVIDER_ERROR_SECURITY:
      return base::File::FILE_ERROR_SECURITY;
    case api::file_system_provider::PROVIDER_ERROR_ABORT:
      return base::File::FILE_ERROR_ABORT;
    case api::file_system_provider::PROVIDER_ERROR_NOT_A_FILE:
      return base::File::FILE_ERROR_NOT_A_FILE;
    case api::file_system_provider::PROVIDER_ERROR_NOT_EMPTY:
      return base::File::FILE_ERROR_NOT_EMPTY;
    case api::file_system_provider::PROVIDER_ERROR_INVALID_URL:
      return base::File::FILE_ERROR_INVALID_URL;
    case api::file_system_provider::PROVIDER_ERROR_IO:
      return base::File::FILE_ERROR_IO;
    default:
      NOTREACHED();
  }
  return base::File::FILE_ERROR_FAILED;
}

}  // namespace

bool FileSystemProviderMountFunction::RunSync() {
  using api::file_system_provider::Mount::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // It's an error if the display name is empty.
  if (params->display_name.empty()) {
    base::ListValue* result = new base::ListValue();
    result->Append(new base::StringValue(""));
    result->Append(CreateError(kSecurityErrorName,
                               kEmptyNameErrorMessage));
    SetResult(result);
    return true;
  }

  Service* service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  int file_system_id =
      service->MountFileSystem(extension_id(), params->display_name);

  // If the |file_system_id| is zero, then it means that registering the file
  // system failed.
  // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
  if (!file_system_id) {
    base::ListValue* result = new base::ListValue();
    result->Append(new base::FundamentalValue(0));
    result->Append(CreateError(kSecurityErrorName, kMountFailedErrorMessage));
    SetResult(result);
    return true;
  }

  base::ListValue* result = new base::ListValue();
  result->Append(new base::FundamentalValue(file_system_id));
  // Don't append an error on success.

  SetResult(result);
  return true;
}

bool FileSystemProviderUnmountFunction::RunSync() {
  using api::file_system_provider::Unmount::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  if (!service->UnmountFileSystem(extension_id(), params->file_system_id)) {
    // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
    base::ListValue* result = new base::ListValue();
    result->Append(CreateError(kSecurityErrorName, kUnmountFailedErrorMessage));
    SetResult(result);
    return true;
  }

  base::ListValue* result = new base::ListValue();
  SetResult(result);
  return true;
}

bool FileSystemProviderInternalUnmountRequestedSuccessFunction::RunSync() {
  using api::file_system_provider_internal::UnmountRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  ProvidedFileSystemInterface* file_system =
      service->GetProvidedFileSystem(extension_id(), params->file_system_id);
  if (!file_system) {
    base::ListValue* result = new base::ListValue();
    result->Append(
        CreateError(kNotFoundErrorName, kResponseFailedErrorMessage));
    SetResult(result);
    return true;
  }

  RequestManager* request_manager = file_system->GetRequestManager();
  DCHECK(request_manager);

  const int request_id = params->request_id;
  if (!request_manager->FulfillRequest(
          request_id,
          RequestValue::CreateForUnmountSuccess(params.Pass()),
          false /* has_more */)) {
    // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
    base::ListValue* result = new base::ListValue();
    result->Append(
        CreateError(kSecurityErrorName, kResponseFailedErrorMessage));
    SetResult(result);
    return true;
  }

  base::ListValue* result = new base::ListValue();
  SetResult(result);
  return true;
}

bool FileSystemProviderInternalUnmountRequestedErrorFunction::RunSync() {
  using api::file_system_provider_internal::UnmountRequestedError::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  ProvidedFileSystemInterface* file_system =
      service->GetProvidedFileSystem(extension_id(), params->file_system_id);
  if (!file_system) {
    base::ListValue* result = new base::ListValue();
    result->Append(
        CreateError(kNotFoundErrorName, kResponseFailedErrorMessage));
    SetResult(result);
    return false;
  }

  RequestManager* request_manager = file_system->GetRequestManager();
  DCHECK(request_manager);

  if (!request_manager->RejectRequest(
          params->request_id, ProviderErrorToFileError(params->error))) {
    // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
    base::ListValue* result = new base::ListValue();
    result->Append(
        CreateError(kSecurityErrorName, kResponseFailedErrorMessage));
    SetResult(result);
    return false;
  }

  base::ListValue* result = new base::ListValue();
  SetResult(result);
  return true;
}

bool FileSystemProviderInternalGetMetadataRequestedSuccessFunction::RunSync() {
  // TODO(mtomasz): Create a common class for these internal functions so
  // most of the code could be easily reused.
  using api::file_system_provider_internal::GetMetadataRequestedSuccess::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  ProvidedFileSystemInterface* file_system =
      service->GetProvidedFileSystem(extension_id(), params->file_system_id);
  if (!file_system) {
    base::ListValue* result = new base::ListValue();
    result->Append(
        CreateError(kNotFoundErrorName, kResponseFailedErrorMessage));
    SetResult(result);
    return false;
  }

  RequestManager* request_manager = file_system->GetRequestManager();
  DCHECK(request_manager);

  const int request_id = params->request_id;
  if (!request_manager->FulfillRequest(
          request_id,
          RequestValue::CreateForGetMetadataSuccess(params.Pass()),
          false /* has_more */)) {
    // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
    base::ListValue* result = new base::ListValue();
    result->Append(
        CreateError(kSecurityErrorName, kResponseFailedErrorMessage));
    SetResult(result);
    return false;
  }

  base::ListValue* result = new base::ListValue();
  SetResult(result);
  return true;
}

bool FileSystemProviderInternalGetMetadataRequestedErrorFunction::RunSync() {
  using api::file_system_provider_internal::UnmountRequestedError::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Service* service = Service::Get(GetProfile());
  DCHECK(service);
  if (!service)
    return false;

  ProvidedFileSystemInterface* file_system =
      service->GetProvidedFileSystem(extension_id(), params->file_system_id);
  if (!file_system) {
    base::ListValue* result = new base::ListValue();
    result->Append(
        CreateError(kNotFoundErrorName, kResponseFailedErrorMessage));
    SetResult(result);
    return true;
  }

  RequestManager* request_manager = file_system->GetRequestManager();
  DCHECK(request_manager);

  if (!request_manager->RejectRequest(
          params->request_id, ProviderErrorToFileError(params->error))) {
    // TODO(mtomasz): Pass more detailed errors, rather than just a bool.
    base::ListValue* result = new base::ListValue();
    result->Append(
        CreateError(kSecurityErrorName, kResponseFailedErrorMessage));
    SetResult(result);
    return true;
  }

  base::ListValue* result = new base::ListValue();
  SetResult(result);
  return true;
}

}  // namespace extensions
