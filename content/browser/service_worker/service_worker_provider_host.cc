// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include "base/stl_util.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/resource_request_body.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

static const int kDocumentMainThreadId = 0;

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    int process_id, int provider_id,
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerDispatcherHost* dispatcher_host)
    : process_id_(process_id),
      provider_id_(provider_id),
      context_(context),
      dispatcher_host_(dispatcher_host) {
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  // Clear docurl so the deferred activation of a waiting worker
  // won't associate the new version with a provider being destroyed.
  document_url_ = GURL();
  if (controlling_version_.get())
    controlling_version_->RemoveControllee(this);
  if (active_version_.get())
    active_version_->RemovePotentialControllee(this);
  if (waiting_version_.get())
    waiting_version_->RemovePotentialControllee(this);
  if (installing_version_.get())
    installing_version_->RemovePotentialControllee(this);
  if (associated_registration_.get())
    associated_registration_->RemoveListener(this);
}

void ServiceWorkerProviderHost::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    ChangedVersionAttributesMask changed_mask,
    const ServiceWorkerRegistrationInfo& info) {
  DCHECK_EQ(associated_registration_.get(), registration);
  UpdatePotentialControllees(registration->installing_version(),
                             registration->waiting_version(),
                             registration->active_version());
}

void ServiceWorkerProviderHost::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  DCHECK_EQ(associated_registration_.get(), registration);
  UnassociateRegistration();
}

void ServiceWorkerProviderHost::SetDocumentUrl(const GURL& url) {
  DCHECK(!url.has_ref());
  document_url_ = url;
}

void ServiceWorkerProviderHost::UpdatePotentialControllees(
    ServiceWorkerVersion* installing_version,
    ServiceWorkerVersion* waiting_version,
    ServiceWorkerVersion* active_version) {
   if (installing_version != installing_version_.get()) {
     scoped_refptr<ServiceWorkerVersion> previous_version = installing_version_;
     if (previous_version.get())
       previous_version->RemovePotentialControllee(this);
     if (installing_version)
       installing_version->AddPotentialControllee(this);
     installing_version_ = installing_version;
   }

   if (waiting_version != waiting_version_.get()) {
     scoped_refptr<ServiceWorkerVersion> previous_version = waiting_version_;
     if (previous_version.get())
       previous_version->RemovePotentialControllee(this);
     if (waiting_version)
       waiting_version->AddPotentialControllee(this);
     waiting_version_ = waiting_version;
   }

   if (active_version != active_version_.get()) {
     scoped_refptr<ServiceWorkerVersion> previous_version = active_version_;
     if (previous_version.get())
       previous_version->RemovePotentialControllee(this);
     if (active_version)
       active_version->AddPotentialControllee(this);
     active_version_ = active_version;
   }
}

void ServiceWorkerProviderHost::SetControllerVersionAttribute(
    ServiceWorkerVersion* version) {
  if (version == controlling_version_.get())
    return;

  scoped_refptr<ServiceWorkerVersion> previous_version = controlling_version_;
  controlling_version_ = version;
  if (version)
    version->AddControllee(this);
  if (previous_version.get())
    previous_version->RemoveControllee(this);

  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  dispatcher_host_->Send(new ServiceWorkerMsg_SetControllerServiceWorker(
      kDocumentMainThreadId, provider_id(), CreateHandleAndPass(version)));
}

bool ServiceWorkerProviderHost::SetHostedVersionId(int64 version_id) {
  if (!context_)
    return true;  // System is shutting down.
  if (active_version_.get())
    return false;  // Unexpected bad message.

  ServiceWorkerVersion* live_version = context_->GetLiveVersion(version_id);
  if (!live_version)
    return true;  // Was deleted before it got started.

  ServiceWorkerVersionInfo info = live_version->GetInfo();
  if (info.running_status != ServiceWorkerVersion::STARTING ||
      info.process_id != process_id_) {
    // If we aren't trying to start this version in our process
    // something is amiss.
    return false;
  }

  running_hosted_version_ = live_version;
  return true;
}

void ServiceWorkerProviderHost::AssociateRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(CanAssociateRegistration(registration));
  associated_registration_ = registration;
  registration->AddListener(this);
  UpdatePotentialControllees(registration->installing_version(),
                             registration->waiting_version(),
                             registration->active_version());
  SetControllerVersionAttribute(registration->active_version());
}

void ServiceWorkerProviderHost::UnassociateRegistration() {
  if (!associated_registration_.get())
    return;
  associated_registration_->RemoveListener(this);
  associated_registration_ = NULL;
  UpdatePotentialControllees(NULL, NULL, NULL);
  SetControllerVersionAttribute(NULL);
}

scoped_ptr<ServiceWorkerRequestHandler>
ServiceWorkerProviderHost::CreateRequestHandler(
    ResourceType resource_type,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    scoped_refptr<ResourceRequestBody> body) {
  if (IsHostToRunningServiceWorker()) {
    return scoped_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerContextRequestHandler(
            context_, AsWeakPtr(), blob_storage_context, resource_type));
  }
  if (ServiceWorkerUtils::IsMainResourceType(resource_type) ||
      active_version()) {
    return scoped_ptr<ServiceWorkerRequestHandler>(
        new ServiceWorkerControlleeRequestHandler(
            context_, AsWeakPtr(), blob_storage_context, resource_type, body));
  }
  return scoped_ptr<ServiceWorkerRequestHandler>();
}

bool ServiceWorkerProviderHost::CanAssociateRegistration(
    ServiceWorkerRegistration* registration) {
  if (!context_)
    return false;
  if (running_hosted_version_.get())
    return false;
  if (!registration || associated_registration_.get())
    return false;
  return true;
}

void ServiceWorkerProviderHost::PostMessage(
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  if (!dispatcher_host_)
    return;  // Could be NULL in some tests.

  std::vector<int> new_routing_ids;
  dispatcher_host_->message_port_message_filter()->
      UpdateMessagePortsWithNewRoutes(sent_message_port_ids,
                                      &new_routing_ids);

  dispatcher_host_->Send(
      new ServiceWorkerMsg_MessageToDocument(
          kDocumentMainThreadId, provider_id(),
          message,
          sent_message_port_ids,
          new_routing_ids));
}

ServiceWorkerObjectInfo ServiceWorkerProviderHost::CreateHandleAndPass(
    ServiceWorkerVersion* version) {
  ServiceWorkerObjectInfo info;
  if (context_ && version) {
    scoped_ptr<ServiceWorkerHandle> handle =
        ServiceWorkerHandle::Create(context_,
                                    dispatcher_host_,
                                    kDocumentMainThreadId,
                                    provider_id_,
                                    version);
    info = handle->GetObjectInfo();
    dispatcher_host_->RegisterServiceWorkerHandle(handle.Pass());
  }
  return info;
}

bool ServiceWorkerProviderHost::IsContextAlive() {
  return context_ != NULL;
}

}  // namespace content
