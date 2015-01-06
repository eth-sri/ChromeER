// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/devtools/cast_dev_tools_delegate.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/shell_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace shell {

namespace {

const char kTargetTypePage[] = "page";
const char kTargetTypeServiceWorker[] = "service_worker";
const char kTargetTypeOther[] = "other";

class Target : public content::DevToolsTarget {
 public:
  explicit Target(scoped_refptr<content::DevToolsAgentHost> agent_host);

  virtual std::string GetId() const OVERRIDE { return agent_host_->GetId(); }
  virtual std::string GetParentId() const OVERRIDE { return std::string(); }
  virtual std::string GetType() const OVERRIDE {
    switch (agent_host_->GetType()) {
      case content::DevToolsAgentHost::TYPE_WEB_CONTENTS:
        return kTargetTypePage;
      case content::DevToolsAgentHost::TYPE_SERVICE_WORKER:
        return kTargetTypeServiceWorker;
      default:
        break;
    }
    return kTargetTypeOther;
  }
  virtual std::string GetTitle() const OVERRIDE {
    return agent_host_->GetTitle();
  }
  virtual std::string GetDescription() const OVERRIDE { return std::string(); }
  virtual GURL GetURL() const OVERRIDE { return url_; }
  virtual GURL GetFaviconURL() const OVERRIDE { return favicon_url_; }
  virtual base::TimeTicks GetLastActivityTime() const OVERRIDE {
    return last_activity_time_;
  }
  virtual bool IsAttached() const OVERRIDE {
    return agent_host_->IsAttached();
  }
  virtual scoped_refptr<content::DevToolsAgentHost> GetAgentHost()
      const OVERRIDE {
    return agent_host_;
  }
  virtual bool Activate() const OVERRIDE;
  virtual bool Close() const OVERRIDE;

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::string id_;
  std::string title_;
  GURL url_;
  GURL favicon_url_;
  base::TimeTicks last_activity_time_;

  DISALLOW_COPY_AND_ASSIGN(Target);
};

Target::Target(scoped_refptr<content::DevToolsAgentHost> agent_host)
    : agent_host_(agent_host) {
  if (content::WebContents* web_contents = agent_host_->GetWebContents()) {
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid())
      favicon_url_ = entry->GetFavicon().url;
    last_activity_time_ = web_contents->GetLastActiveTime();
  }
}

bool Target::Activate() const {
  content::WebContents* web_contents = agent_host_->GetWebContents();
  if (!web_contents)
    return false;
  web_contents->GetDelegate()->ActivateContents(web_contents);
  return true;
}

bool Target::Close() const {
  content::WebContents* web_contents = agent_host_->GetWebContents();
  if (!web_contents)
    return false;
  web_contents->GetRenderViewHost()->ClosePage();
  return true;
}

}  // namespace

CastDevToolsDelegate::CastDevToolsDelegate() {
}

CastDevToolsDelegate::~CastDevToolsDelegate() {
}

std::string CastDevToolsDelegate::GetDiscoveryPageHTML() {
#if defined(OS_ANDROID)
  return std::string();
#else
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CAST_SHELL_DEVTOOLS_DISCOVERY_PAGE).as_string();
#endif  // defined(OS_ANDROID)
}

bool CastDevToolsDelegate::BundlesFrontendResources() {
#if defined(OS_ANDROID)
  // Since Android remote debugging connects over a Unix domain socket, Chrome
  // will not load the same homepage.
  return false;
#else
  return true;
#endif  // defined(OS_ANDROID)
}

base::FilePath CastDevToolsDelegate::GetDebugFrontendDir() {
  return base::FilePath();
}

std::string CastDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
  return "";
}

scoped_ptr<content::DevToolsTarget> CastDevToolsDelegate::CreateNewTarget(
    const GURL& url) {
  return scoped_ptr<content::DevToolsTarget>();
}

void CastDevToolsDelegate::EnumerateTargets(TargetCallback callback) {
  TargetList targets;
  content::DevToolsAgentHost::List agents =
      content::DevToolsAgentHost::GetOrCreateAll();
  for (content::DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    targets.push_back(new Target(*it));
  }
  callback.Run(targets);
}

scoped_ptr<net::StreamListenSocket>
CastDevToolsDelegate::CreateSocketForTethering(
    net::StreamListenSocket::Delegate* delegate,
    std::string* name) {
  return scoped_ptr<net::StreamListenSocket>();
}

}  // namespace shell
}  // namespace chromecast
