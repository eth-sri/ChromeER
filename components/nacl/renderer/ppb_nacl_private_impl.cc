// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/ppb_nacl_private_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/histogram.h"
#include "components/nacl/renderer/manifest_downloader.h"
#include "components/nacl/renderer/manifest_service_channel.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "components/nacl/renderer/pnacl_translation_resource_host.h"
#include "components/nacl/renderer/sandbox_arch.h"
#include "components/nacl/renderer/trusted_plugin_channel.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "net/base/data_url.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebURLLoaderOptions.h"

namespace nacl {
namespace {

base::LazyInstance<scoped_refptr<PnaclTranslationResourceHost> >
    g_pnacl_resource_host = LAZY_INSTANCE_INITIALIZER;

bool InitializePnaclResourceHost() {
  // Must run on the main thread.
  content::RenderThread* render_thread = content::RenderThread::Get();
  if (!render_thread)
    return false;
  if (!g_pnacl_resource_host.Get()) {
    g_pnacl_resource_host.Get() = new PnaclTranslationResourceHost(
        render_thread->GetIOMessageLoopProxy());
    render_thread->AddFilter(g_pnacl_resource_host.Get());
  }
  return true;
}

struct InstanceInfo {
  InstanceInfo() : plugin_pid(base::kNullProcessId), plugin_child_id(0) {}
  GURL url;
  ppapi::PpapiPermissions permissions;
  base::ProcessId plugin_pid;
  int plugin_child_id;
  IPC::ChannelHandle channel_handle;
};

typedef std::map<PP_Instance, InstanceInfo> InstanceInfoMap;

base::LazyInstance<InstanceInfoMap> g_instance_info =
    LAZY_INSTANCE_INITIALIZER;

typedef base::ScopedPtrHashMap<PP_Instance, NexeLoadManager>
    NexeLoadManagerMap;

base::LazyInstance<NexeLoadManagerMap> g_load_manager_map =
    LAZY_INSTANCE_INITIALIZER;

NexeLoadManager* GetNexeLoadManager(PP_Instance instance) {
  NexeLoadManagerMap& map = g_load_manager_map.Get();
  NexeLoadManagerMap::iterator iter = map.find(instance);
  if (iter != map.end())
    return iter->second;
  return NULL;
}

int GetRoutingID(PP_Instance instance) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  content::RendererPpapiHost *host =
      content::RendererPpapiHost::GetForPPInstance(instance);
  if (!host)
    return 0;
  return host->GetRoutingIDForWidget(instance);
}

// Returns whether the channel_handle is valid or not.
bool IsValidChannelHandle(const IPC::ChannelHandle& channel_handle) {
  if (channel_handle.name.empty()) {
    return false;
  }

#if defined(OS_POSIX)
  if (channel_handle.socket.fd == -1) {
    return false;
  }
#endif

  return true;
}

// Callback invoked when an IPC channel connection is established.
// As we will establish multiple IPC channels, this takes the number
// of expected invocations and a callback. When all channels are established,
// the given callback will be invoked on the main thread. Its argument will be
// PP_OK if all the connections are successfully established. Otherwise,
// the first error code will be passed, and remaining errors will be ignored.
// Note that PP_CompletionCallback is designed to be called exactly once.
class ChannelConnectedCallback {
 public:
  ChannelConnectedCallback(int num_expect_calls,
                           PP_CompletionCallback callback)
      : num_remaining_calls_(num_expect_calls),
        callback_(callback),
        result_(PP_OK) {
  }

  ~ChannelConnectedCallback() {
  }

  void Run(int32_t result) {
    if (result_ == PP_OK && result != PP_OK) {
      // This is the first error, so remember it.
      result_ = result;
    }

    --num_remaining_calls_;
    if (num_remaining_calls_ > 0) {
      // There still are some pending or on-going tasks. Wait for the results.
      return;
    }

    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback_.func, callback_.user_data, result_));
  }

 private:
  int num_remaining_calls_;
  PP_CompletionCallback callback_;
  int32_t result_;

  DISALLOW_COPY_AND_ASSIGN(ChannelConnectedCallback);
};

// Thin adapter from PP_ManifestService to ManifestServiceChannel::Delegate.
// Note that user_data is managed by the caller of LaunchSelLdr. Please see
// also PP_ManifestService's comment for more details about resource
// management.
class ManifestServiceProxy : public ManifestServiceChannel::Delegate {
 public:
  ManifestServiceProxy(const PP_ManifestService* manifest_service,
                       void* user_data)
      : manifest_service_(*manifest_service),
        user_data_(user_data) {
  }

  virtual ~ManifestServiceProxy() {
    Quit();
  }

  virtual void StartupInitializationComplete() OVERRIDE {
    if (!user_data_)
      return;

    if (!PP_ToBool(
            manifest_service_.StartupInitializationComplete(user_data_))) {
      user_data_ = NULL;
    }
  }

 private:
  void Quit() {
    if (!user_data_)
      return;

    bool result = PP_ToBool(manifest_service_.Quit(user_data_));
    DCHECK(!result);
    user_data_ = NULL;
  }

  PP_ManifestService manifest_service_;
  void* user_data_;
  DISALLOW_COPY_AND_ASSIGN(ManifestServiceProxy);
};

// Launch NaCl's sel_ldr process.
void LaunchSelLdr(PP_Instance instance,
                  const char* alleged_url,
                  PP_Bool uses_irt,
                  PP_Bool uses_ppapi,
                  PP_Bool uses_nonsfi_mode,
                  PP_Bool enable_ppapi_dev,
                  PP_Bool enable_dyncode_syscalls,
                  PP_Bool enable_exception_handling,
                  PP_Bool enable_crash_throttling,
                  const PP_ManifestService* manifest_service_interface,
                  void* manifest_service_user_data,
                  void* imc_handle,
                  struct PP_Var* error_message,
                  PP_CompletionCallback callback) {
  CHECK(ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->
            BelongsToCurrentThread());

  // Create the manifest service proxy here, so on error case, it will be
  // destructed (without passing it to ManifestServiceChannel), and QUIT
  // will be called in its destructor so that the caller of this function
  // can free manifest_service_user_data properly.
  scoped_ptr<ManifestServiceChannel::Delegate> manifest_service_proxy(
      new ManifestServiceProxy(manifest_service_interface,
                               manifest_service_user_data));

  FileDescriptor result_socket;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  *error_message = PP_MakeUndefined();
  int routing_id = 0;
  // If the nexe uses ppapi APIs, we need a routing ID.
  // To get the routing ID, we must be on the main thread.
  // Some nexes do not use ppapi and launch from the background thread,
  // so those nexes can skip finding a routing_id.
  if (uses_ppapi) {
    routing_id = GetRoutingID(instance);
    if (!routing_id) {
      ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
          FROM_HERE,
          base::Bind(callback.func, callback.user_data,
                     static_cast<int32_t>(PP_ERROR_FAILED)));
      return;
    }
  }

  InstanceInfo instance_info;
  instance_info.url = GURL(alleged_url);

  uint32_t perm_bits = ppapi::PERMISSION_NONE;
  // Conditionally block 'Dev' interfaces. We do this for the NaCl process, so
  // it's clearer to developers when they are using 'Dev' inappropriately. We
  // must also check on the trusted side of the proxy.
  if (enable_ppapi_dev)
    perm_bits |= ppapi::PERMISSION_DEV;
  instance_info.permissions =
      ppapi::PpapiPermissions::GetForCommandLine(perm_bits);
  std::string error_message_string;
  NaClLaunchResult launch_result;

  if (!sender->Send(new NaClHostMsg_LaunchNaCl(
          NaClLaunchParams(instance_info.url.spec(),
                           routing_id,
                           perm_bits,
                           PP_ToBool(uses_irt),
                           PP_ToBool(uses_nonsfi_mode),
                           PP_ToBool(enable_dyncode_syscalls),
                           PP_ToBool(enable_exception_handling),
                           PP_ToBool(enable_crash_throttling)),
          &launch_result,
          &error_message_string))) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }
  if (!error_message_string.empty()) {
    *error_message = ppapi::StringVar::StringToPPVar(error_message_string);
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }
  result_socket = launch_result.imc_channel_handle;
  instance_info.channel_handle = launch_result.ppapi_ipc_channel_handle;
  instance_info.plugin_pid = launch_result.plugin_pid;
  instance_info.plugin_child_id = launch_result.plugin_child_id;

  // Don't save instance_info if channel handle is invalid.
  if (IsValidChannelHandle(instance_info.channel_handle))
    g_instance_info.Get()[instance] = instance_info;

  *(static_cast<NaClHandle*>(imc_handle)) = ToNativeHandle(result_socket);

  // Here after, we starts to establish connections for TrustedPluginChannel
  // and ManifestServiceChannel in parallel. The invocation of the callback
  // is delegated to their connection completion callback.
  base::Callback<void(int32_t)> connected_callback = base::Bind(
      &ChannelConnectedCallback::Run,
      base::Owned(new ChannelConnectedCallback(
          2, // For TrustedPluginChannel and ManifestServiceChannel.
          callback)));

  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);

  // Stash the trusted handle as well.
  if (load_manager &&
      IsValidChannelHandle(launch_result.trusted_ipc_channel_handle)) {
    scoped_ptr<TrustedPluginChannel> trusted_plugin_channel(
        new TrustedPluginChannel(
            launch_result.trusted_ipc_channel_handle,
            connected_callback,
            content::RenderThread::Get()->GetShutdownEvent()));
    load_manager->set_trusted_plugin_channel(trusted_plugin_channel.Pass());
  } else {
    connected_callback.Run(PP_ERROR_FAILED);
  }

  // Stash the manifest service handle as well.
  if (load_manager &&
      IsValidChannelHandle(
          launch_result.manifest_service_ipc_channel_handle)) {
    scoped_ptr<ManifestServiceChannel> manifest_service_channel(
        new ManifestServiceChannel(
            launch_result.manifest_service_ipc_channel_handle,
            connected_callback,
            manifest_service_proxy.Pass(),
            content::RenderThread::Get()->GetShutdownEvent()));
    load_manager->set_manifest_service_channel(
        manifest_service_channel.Pass());
  } else {
    // Currently, manifest service works only on linux/non-SFI mode.
    // On other platforms, the socket will not be created, and thus this
    // condition needs to be handled as success.
    connected_callback.Run(PP_OK);
  }
}

// Forward declaration.
void ReportLoadError(PP_Instance instance,
                     PP_NaClError error,
                     const char* error_message,
                     const char* console_message);

PP_Bool StartPpapiProxy(PP_Instance instance) {
  InstanceInfoMap& map = g_instance_info.Get();
  InstanceInfoMap::iterator it = map.find(instance);
  if (it == map.end()) {
    DLOG(ERROR) << "Could not find instance ID";
    return PP_FALSE;
  }
  InstanceInfo instance_info = it->second;
  map.erase(it);

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    DLOG(ERROR) << "GetInstance() failed";
    return PP_FALSE;
  }

  PP_ExternalPluginResult result = plugin_instance->SwitchToOutOfProcessProxy(
      base::FilePath().AppendASCII(instance_info.url.spec()),
      instance_info.permissions,
      instance_info.channel_handle,
      instance_info.plugin_pid,
      instance_info.plugin_child_id);

  if (result == PP_EXTERNAL_PLUGIN_OK) {
    // Log the amound of time that has passed between the trusted plugin being
    // initialized and the untrusted plugin being initialized.  This is
    // (roughly) the cost of using NaCl, in terms of startup time.
    NexeLoadManager* load_manager = GetNexeLoadManager(instance);
    if (load_manager)
      load_manager->ReportStartupOverhead();
    return PP_TRUE;
  } else if (result == PP_EXTERNAL_PLUGIN_ERROR_MODULE) {
    ReportLoadError(instance,
                    PP_NACL_ERROR_START_PROXY_MODULE,
                    "could not initialize module.",
                    "could not initialize module.");
  } else if (result == PP_EXTERNAL_PLUGIN_ERROR_INSTANCE) {
    ReportLoadError(instance,
                    PP_NACL_ERROR_START_PROXY_MODULE,
                    "could not create instance.",
                    "could not create instance.");
  }
  return PP_FALSE;
}

int UrandomFD(void) {
#if defined(OS_POSIX)
  return base::GetUrandomFD();
#else
  return -1;
#endif
}

PP_Bool Are3DInterfacesDisabled() {
  return PP_FromBool(CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kDisable3DAPIs));
}

int32_t BrokerDuplicateHandle(PP_FileHandle source_handle,
                              uint32_t process_id,
                              PP_FileHandle* target_handle,
                              uint32_t desired_access,
                              uint32_t options) {
#if defined(OS_WIN)
  return content::BrokerDuplicateHandle(source_handle, process_id,
                                        target_handle, desired_access,
                                        options);
#else
  return 0;
#endif
}

PP_FileHandle GetReadonlyPnaclFD(const char* filename) {
  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_GetReadonlyPnaclFD(
          std::string(filename),
          &out_fd))) {
    return base::kInvalidPlatformFileValue;
  }
  if (out_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }
  base::PlatformFile handle =
      IPC::PlatformFileForTransitToPlatformFile(out_fd);
  return handle;
}

PP_FileHandle CreateTemporaryFile(PP_Instance instance) {
  IPC::PlatformFileForTransit transit_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_NaClCreateTemporaryFile(
          &transit_fd))) {
    return base::kInvalidPlatformFileValue;
  }

  if (transit_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }

  base::PlatformFile handle = IPC::PlatformFileForTransitToPlatformFile(
      transit_fd);
  return handle;
}

int32_t GetNumberOfProcessors() {
  int32_t num_processors;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if(!sender->Send(new NaClHostMsg_NaClGetNumProcessors(&num_processors))) {
    return 1;
  }
  return num_processors;
}

PP_Bool IsNonSFIModeEnabled() {
#if defined(OS_LINUX)
  return PP_FromBool(CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kEnableNaClNonSfiMode));
#else
  return PP_FALSE;
#endif
}

int32_t GetNexeFd(PP_Instance instance,
                  const char* pexe_url,
                  uint32_t abi_version,
                  uint32_t opt_level,
                  const char* http_headers_param,
                  const char* extra_flags,
                  PP_Bool* is_hit,
                  PP_FileHandle* handle,
                  struct PP_CompletionCallback callback) {
  ppapi::thunk::EnterInstance enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  if (!pexe_url || !is_hit || !handle)
    return enter.SetResult(PP_ERROR_BADARGUMENT);
  if (!InitializePnaclResourceHost())
    return enter.SetResult(PP_ERROR_FAILED);

  std::string http_headers(http_headers_param);
  net::HttpUtil::HeadersIterator iter(
      http_headers.begin(), http_headers.end(), "\r\n");

  std::string last_modified;
  std::string etag;
  bool has_no_store_header = false;
  while (iter.GetNext()) {
    if (StringToLowerASCII(iter.name()) == "last-modified")
      last_modified = iter.values();
    if (StringToLowerASCII(iter.name()) == "etag")
      etag = iter.values();
    if (StringToLowerASCII(iter.name()) == "cache-control") {
      net::HttpUtil::ValuesIterator values_iter(
          iter.values_begin(), iter.values_end(), ',');
      while (values_iter.GetNext()) {
        if (StringToLowerASCII(values_iter.value()) == "no-store")
          has_no_store_header = true;
      }
    }
  }

  base::Time last_modified_time;
  // If FromString fails, it doesn't touch last_modified_time and we just send
  // the default-constructed null value.
  base::Time::FromString(last_modified.c_str(), &last_modified_time);

  PnaclCacheInfo cache_info;
  cache_info.pexe_url = GURL(pexe_url);
  cache_info.abi_version = abi_version;
  cache_info.opt_level = opt_level;
  cache_info.last_modified = last_modified_time;
  cache_info.etag = etag;
  cache_info.has_no_store_header = has_no_store_header;
  cache_info.sandbox_isa = GetSandboxArch();
  cache_info.extra_flags = std::string(extra_flags);

  g_pnacl_resource_host.Get()->RequestNexeFd(
      GetRoutingID(instance),
      instance,
      cache_info,
      is_hit,
      handle,
      enter.callback());

  return enter.SetResult(PP_OK_COMPLETIONPENDING);
}

void ReportTranslationFinished(PP_Instance instance, PP_Bool success) {
  // If the resource host isn't initialized, don't try to do that here.
  // Just return because something is already very wrong.
  if (g_pnacl_resource_host.Get() == NULL)
    return;
  g_pnacl_resource_host.Get()->ReportTranslationFinished(instance, success);
}

PP_FileHandle OpenNaClExecutable(PP_Instance instance,
                                 const char* file_url,
                                 uint64_t* nonce_lo,
                                 uint64_t* nonce_hi) {
  // Fast path only works for installed file URLs.
  GURL gurl(file_url);
  if (!gurl.SchemeIs("chrome-extension"))
    return PP_kInvalidFileHandle;

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  // IMPORTANT: Make sure the document can request the given URL. If we don't
  // check, a malicious app could probe the extension system. This enforces a
  // same-origin policy which prevents the app from requesting resources from
  // another app.
  blink::WebSecurityOrigin security_origin =
      plugin_instance->GetContainer()->element().document().securityOrigin();
  if (!security_origin.canRequest(gurl))
    return PP_kInvalidFileHandle;

  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  *nonce_lo = 0;
  *nonce_hi = 0;
  base::FilePath file_path;
  if (!sender->Send(
      new NaClHostMsg_OpenNaClExecutable(GetRoutingID(instance),
                                         GURL(file_url),
                                         &out_fd,
                                         nonce_lo,
                                         nonce_hi))) {
    return base::kInvalidPlatformFileValue;
  }

  if (out_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }

  base::PlatformFile handle =
      IPC::PlatformFileForTransitToPlatformFile(out_fd);
  return handle;
}

void DispatchEventOnMainThread(PP_Instance instance,
                               PP_NaClEventType event_type,
                               const std::string& resource_url,
                               PP_Bool length_is_computable,
                               uint64_t loaded_bytes,
                               uint64_t total_bytes);

void DispatchEvent(PP_Instance instance,
                   PP_NaClEventType event_type,
                   const char *resource_url,
                   PP_Bool length_is_computable,
                   uint64_t loaded_bytes,
                   uint64_t total_bytes) {
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::Bind(&DispatchEventOnMainThread,
                 instance,
                 event_type,
                 std::string(resource_url),
                 length_is_computable,
                 loaded_bytes,
                 total_bytes));
}

void DispatchEventOnMainThread(PP_Instance instance,
                               PP_NaClEventType event_type,
                               const std::string& resource_url,
                               PP_Bool length_is_computable,
                               uint64_t loaded_bytes,
                               uint64_t total_bytes) {
  NexeLoadManager* load_manager =
      GetNexeLoadManager(instance);
  // The instance may have been destroyed after we were scheduled, so do
  // nothing if it's gone.
  if (load_manager) {
    NexeLoadManager::ProgressEvent event(event_type);
    event.resource_url = resource_url;
    event.length_is_computable = PP_ToBool(length_is_computable);
    event.loaded_bytes = loaded_bytes;
    event.total_bytes = total_bytes;
    load_manager->DispatchEvent(event);
  }
}

void NexeFileDidOpen(PP_Instance instance,
                     int32_t pp_error,
                     int32_t fd,
                     int32_t http_status,
                     int64_t nexe_bytes_read,
                     const char* url,
                     int64_t time_since_open) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager) {
    load_manager->NexeFileDidOpen(pp_error,
                                  fd,
                                  http_status,
                                  nexe_bytes_read,
                                  url,
                                  time_since_open);
  }
}

void ReportLoadSuccess(PP_Instance instance,
                       const char* url,
                       uint64_t loaded_bytes,
                       uint64_t total_bytes) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadSuccess(url, loaded_bytes, total_bytes);
}

void ReportLoadError(PP_Instance instance,
                     PP_NaClError error,
                     const char* error_message,
                     const char* console_message) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadError(error, error_message, console_message);
}

void ReportLoadAbort(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadAbort();
}

void NexeDidCrash(PP_Instance instance, const char* crash_log) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->NexeDidCrash(crash_log);
}

void InstanceCreated(PP_Instance instance) {
  scoped_ptr<NexeLoadManager> new_load_manager(new NexeLoadManager(instance));
  NexeLoadManagerMap& map = g_load_manager_map.Get();
  DLOG_IF(ERROR, map.count(instance) != 0) << "Instance count should be 0";
  map.add(instance, new_load_manager.Pass());
}

void InstanceDestroyed(PP_Instance instance) {
  NexeLoadManagerMap& map = g_load_manager_map.Get();
  DLOG_IF(ERROR, map.count(instance) == 0) << "Could not find instance ID";
  // The erase may call NexeLoadManager's destructor prior to removing it from
  // the map. In that case, it is possible for the trusted Plugin to re-enter
  // the NexeLoadManager (e.g., by calling ReportLoadError). Passing out the
  // NexeLoadManager to a local scoped_ptr just ensures that its entry is gone
  // from the map prior to the destructor being invoked.
  scoped_ptr<NexeLoadManager> temp(map.take(instance));
  map.erase(instance);
}

PP_Bool NaClDebugEnabledForURL(const char* alleged_nmf_url) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableNaClDebug))
    return PP_FALSE;
  bool should_debug;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if(!sender->Send(new NaClHostMsg_NaClDebugEnabledForURL(
         GURL(alleged_nmf_url),
         &should_debug))) {
    return PP_FALSE;
  }
  return PP_FromBool(should_debug);
}

void LogToConsole(PP_Instance instance, const char* message) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    load_manager->LogToConsole(std::string(message));
}

PP_NaClReadyState GetNaClReadyState(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->nacl_ready_state();
  return PP_NACL_READY_STATE_UNSENT;
}

PP_Bool GetIsInstalled(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return PP_FromBool(load_manager->is_installed());
  return PP_FALSE;
}

int32_t GetExitStatus(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->exit_status();
  return -1;
}

void SetExitStatus(PP_Instance instance, int32_t exit_status) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->set_exit_status(exit_status);
}

void Vlog(const char* message) {
  VLOG(1) << message;
}

void InitializePlugin(PP_Instance instance,
                      uint32_t argc,
                      const char* argn[],
                      const char* argv[]) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    load_manager->InitializePlugin(argc, argn, argv);
}

int64_t GetNexeSize(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    return load_manager->nexe_size();
  return 0;
}

PP_Bool RequestNaClManifest(PP_Instance instance,
                            const char* url,
                            PP_Bool* pp_is_data_uri) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager) {
    bool is_data_uri;
    bool result = load_manager->RequestNaClManifest(url, &is_data_uri);
    *pp_is_data_uri = PP_FromBool(is_data_uri);
    return PP_FromBool(result);
  }
  return PP_FALSE;
}

PP_Var GetManifestBaseURL(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return PP_MakeUndefined();
  const GURL& gurl = load_manager->manifest_base_url();
  if (!gurl.is_valid())
    return PP_MakeUndefined();
  return ppapi::StringVar::StringToPPVar(gurl.spec());
}

PP_Bool ResolvesRelativeToPluginBaseURL(PP_Instance instance,
                                        const char *url) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return PP_FALSE;
  const GURL& gurl = load_manager->plugin_base_url().Resolve(url);
  if (!gurl.is_valid())
    return PP_FALSE;
  return PP_TRUE;
}

PP_Var ParseDataURL(const char* data_url) {
  GURL gurl(data_url);
  std::string mime_type;
  std::string charset;
  std::string data;
  if (!net::DataURL::Parse(gurl, &mime_type, &charset, &data))
    return PP_MakeUndefined();
  return ppapi::StringVar::StringToPPVar(data);
}

void ProcessNaClManifest(PP_Instance instance, const char* program_url) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ProcessNaClManifest(program_url);
}

PP_Var GetManifestURLArgument(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager) {
    return ppapi::StringVar::StringToPPVar(
        load_manager->GetManifestURLArgument());
  }
  return PP_MakeUndefined();
}

PP_Bool IsPNaCl(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    return PP_FromBool(load_manager->IsPNaCl());
  return PP_FALSE;
}

PP_Bool DevInterfacesEnabled(PP_Instance instance) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    return PP_FromBool(load_manager->DevInterfacesEnabled());
  return PP_FALSE;
}

void DownloadManifestToBufferCompletion(PP_Instance instance,
                                        struct PP_CompletionCallback callback,
                                        struct PP_Var* out_data,
                                        base::Time start_time,
                                        PP_NaClError pp_nacl_error,
                                        const std::string& data);

void DownloadManifestToBuffer(PP_Instance instance,
                              struct PP_Var* out_data,
                              struct PP_CompletionCallback callback) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE,
        base::Bind(callback.func, callback.user_data,
                   static_cast<int32_t>(PP_ERROR_FAILED)));
  }

  const GURL& gurl = load_manager->manifest_base_url();

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  blink::WebURLLoaderOptions options;
  options.untrustedHTTP = true;

  blink::WebSecurityOrigin security_origin =
      plugin_instance->GetContainer()->element().document().securityOrigin();
  // Options settings here follow the original behavior in the trusted
  // plugin and PepperURLLoaderHost.
  if (security_origin.canRequest(gurl)) {
    options.allowCredentials = true;
  } else {
    // Allow CORS.
    options.crossOriginRequestPolicy =
        blink::WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl;
  }

  blink::WebFrame* frame =
      plugin_instance->GetContainer()->element().document().frame();
  blink::WebURLLoader* url_loader = frame->createAssociatedURLLoader(options);
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(gurl);
  request.setFirstPartyForCookies(frame->document().firstPartyForCookies());

  // ManifestDownloader deletes itself after invoking the callback.
  ManifestDownloader* client = new ManifestDownloader(
      load_manager->is_installed(),
      base::Bind(DownloadManifestToBufferCompletion,
                 instance, callback, out_data, base::Time::Now()));
  url_loader->loadAsynchronously(request, client);
}

void DownloadManifestToBufferCompletion(PP_Instance instance,
                                        struct PP_CompletionCallback callback,
                                        struct PP_Var* out_data,
                                        base::Time start_time,
                                        PP_NaClError pp_nacl_error,
                                        const std::string& data) {
  base::TimeDelta download_time = base::Time::Now() - start_time;
  HistogramTimeSmall("NaCl.Perf.StartupTime.ManifestDownload",
                     download_time.InMilliseconds());

  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (!load_manager) {
    callback.func(callback.user_data, PP_ERROR_ABORTED);
    return;
  }

  int32_t pp_error;
  switch (pp_nacl_error) {
    case PP_NACL_ERROR_LOAD_SUCCESS:
      pp_error = PP_OK;
      break;
    case PP_NACL_ERROR_MANIFEST_LOAD_URL:
      pp_error = PP_ERROR_FAILED;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_LOAD_URL,
                                    "could not load manifest url.");
      break;
    case PP_NACL_ERROR_MANIFEST_TOO_LARGE:
      pp_error = PP_ERROR_FILETOOBIG;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_TOO_LARGE,
                                    "manifest file too large.");
      break;
    case PP_NACL_ERROR_MANIFEST_NOACCESS_URL:
      pp_error = PP_ERROR_NOACCESS;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_NOACCESS_URL,
                                    "access to manifest url was denied.");
      break;
    default:
      NOTREACHED();
      pp_error = PP_ERROR_FAILED;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_LOAD_URL,
                                    "could not load manifest url.");
  }

  if (pp_error == PP_OK) {
    std::string contents;
    *out_data = ppapi::StringVar::StringToPPVar(data);
  }
  callback.func(callback.user_data, pp_error);
}

const PPB_NaCl_Private nacl_interface = {
  &LaunchSelLdr,
  &StartPpapiProxy,
  &UrandomFD,
  &Are3DInterfacesDisabled,
  &BrokerDuplicateHandle,
  &GetReadonlyPnaclFD,
  &CreateTemporaryFile,
  &GetNumberOfProcessors,
  &IsNonSFIModeEnabled,
  &GetNexeFd,
  &ReportTranslationFinished,
  &OpenNaClExecutable,
  &DispatchEvent,
  &NexeFileDidOpen,
  &ReportLoadSuccess,
  &ReportLoadError,
  &ReportLoadAbort,
  &NexeDidCrash,
  &InstanceCreated,
  &InstanceDestroyed,
  &NaClDebugEnabledForURL,
  &GetSandboxArch,
  &LogToConsole,
  &GetNaClReadyState,
  &GetIsInstalled,
  &GetExitStatus,
  &SetExitStatus,
  &Vlog,
  &InitializePlugin,
  &GetNexeSize,
  &RequestNaClManifest,
  &GetManifestBaseURL,
  &ResolvesRelativeToPluginBaseURL,
  &ParseDataURL,
  &ProcessNaClManifest,
  &GetManifestURLArgument,
  &IsPNaCl,
  &DevInterfacesEnabled,
  &DownloadManifestToBuffer
};

}  // namespace

const PPB_NaCl_Private* GetNaClPrivateInterface() {
  return &nacl_interface;
}

}  // namespace nacl
