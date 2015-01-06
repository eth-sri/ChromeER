// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/location_bar_controller.h"

#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/feature_switch.h"

namespace extensions {

LocationBarController::LocationBarController(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      action_manager_(ExtensionActionManager::Get(browser_context_)),
      should_show_page_actions_(
          !FeatureSwitch::extension_action_redesign()->IsEnabled()),
      active_script_controller_(new ActiveScriptController(web_contents_)),
      extension_registry_observer_(this) {
  if (should_show_page_actions_)
    extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

LocationBarController::~LocationBarController() {
}

std::vector<ExtensionAction*> LocationBarController::GetCurrentActions() {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  std::vector<ExtensionAction*> current_actions;
  if (!should_show_page_actions_)
    return current_actions;

  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    // Right now, we can consolidate these actions because we only want to show
    // one action per extension. If clicking on an active script action ever
    // has a response, then we will need to split the actions.
    ExtensionAction* action = action_manager_->GetPageAction(**iter);
    if (!action)
      action = active_script_controller_->GetActionForExtension(iter->get());
    if (action)
      current_actions.push_back(action);
  }

  return current_actions;
}

void LocationBarController::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (action_manager_->GetPageAction(*extension) ||
      active_script_controller_->GetActionForExtension(extension)) {
    ExtensionActionAPI::Get(browser_context)->
        NotifyPageActionsChanged(web_contents_);
  }
}

void LocationBarController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (action_manager_->GetPageAction(*extension)) {
    ExtensionActionAPI::Get(browser_context)->
        NotifyPageActionsChanged(web_contents_);
  }
}

}  // namespace extensions
