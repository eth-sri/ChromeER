// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/notification_service.h"

using autofill::PasswordFormMap;
using password_manager::PasswordFormManager;

namespace {

password_manager::PasswordStore* GetPasswordStore(
    content::WebContents* web_contents) {
  return PasswordStoreFactory::GetForProfile(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             Profile::EXPLICIT_ACCESS).get();
}

} // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ManagePasswordsBubbleUIController);

ManagePasswordsBubbleUIController::ManagePasswordsBubbleUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      manage_passwords_icon_to_be_shown_(false),
      password_to_be_saved_(false),
      manage_passwords_bubble_needs_showing_(false),
      autofill_blocked_(false) {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents);
  if (password_store)
    password_store->AddObserver(this);
}

ManagePasswordsBubbleUIController::~ManagePasswordsBubbleUIController() {}

void ManagePasswordsBubbleUIController::UpdateBubbleAndIconVisibility() {
  #if !defined(OS_ANDROID)
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (!browser)
      return;
    LocationBar* location_bar = browser->window()->GetLocationBar();
    DCHECK(location_bar);
    location_bar->UpdateManagePasswordsIconAndBubble();
  #endif
}

void ManagePasswordsBubbleUIController::OnPasswordSubmitted(
    PasswordFormManager* form_manager) {
  form_manager_.reset(form_manager);
  password_form_map_ = form_manager_->best_matches();
  origin_ = PendingCredentials().origin;
  manage_passwords_icon_to_be_shown_ = true;
  password_to_be_saved_ = true;
  manage_passwords_bubble_needs_showing_ = true;
  autofill_blocked_ = false;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::OnPasswordAutofilled(
    const PasswordFormMap& password_form_map) {
  password_form_map_ = password_form_map;
  origin_ = password_form_map_.begin()->second->origin;
  manage_passwords_icon_to_be_shown_ = true;
  password_to_be_saved_ = false;
  manage_passwords_bubble_needs_showing_ = false;
  autofill_blocked_ = false;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::OnBlacklistBlockedAutofill() {
  manage_passwords_icon_to_be_shown_ = true;
  password_to_be_saved_ = false;
  manage_passwords_bubble_needs_showing_ = false;
  autofill_blocked_ = true;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsBubbleUIController::WebContentsDestroyed(
    content::WebContents* web_contents) {
  password_manager::PasswordStore* password_store =
      GetPasswordStore(web_contents);
  if (password_store)
    password_store->RemoveObserver(this);
}

void ManagePasswordsBubbleUIController::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  for (password_manager::PasswordStoreChangeList::const_iterator it =
           changes.begin();
       it != changes.end();
       it++) {
    const autofill::PasswordForm& changed_form = it->form();
    if (changed_form.origin != origin_)
      continue;

    if (it->type() == password_manager::PasswordStoreChange::REMOVE) {
      password_form_map_.erase(changed_form.username_value);
    } else {
      autofill::PasswordForm* new_form =
          new autofill::PasswordForm(changed_form);
      password_form_map_[changed_form.username_value] = new_form;
    }
  }
}

void ManagePasswordsBubbleUIController::
    NavigateToPasswordManagerSettingsPage() {
// TODO(mkwst): chrome_pages.h is compiled out of Android. Need to figure out
// how this navigation should work there.
#if !defined(OS_ANDROID)
  chrome::ShowSettingsSubPage(
      chrome::FindBrowserWithWebContents(web_contents()),
      chrome::kPasswordManagerSubPage);
#endif
}

void ManagePasswordsBubbleUIController::SavePassword() {
  DCHECK(form_manager_.get());
  form_manager_->Save();
}

void ManagePasswordsBubbleUIController::NeverSavePassword() {
  DCHECK(form_manager_.get());
  form_manager_->PermanentlyBlacklist();
}

void ManagePasswordsBubbleUIController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;
  // Reset password states for next page.
  manage_passwords_icon_to_be_shown_ = false;
  password_to_be_saved_ = false;
  manage_passwords_bubble_needs_showing_ = false;
  UpdateBubbleAndIconVisibility();
}

const autofill::PasswordForm& ManagePasswordsBubbleUIController::
    PendingCredentials() const {
  DCHECK(form_manager_);
  return form_manager_->pending_credentials();
}

void ManagePasswordsBubbleUIController::UpdateIconAndBubbleState(
    ManagePasswordsIcon* icon) {
  ManagePasswordsIcon::State state = ManagePasswordsIcon::INACTIVE_STATE;

  if (autofill_blocked_)
    state = ManagePasswordsIcon::BLACKLISTED_STATE;
  else if (password_to_be_saved_)
    state = ManagePasswordsIcon::PENDING_STATE;
  else if (manage_passwords_icon_to_be_shown_)
    state = ManagePasswordsIcon::MANAGE_STATE;

  icon->SetState(state);

  if (manage_passwords_bubble_needs_showing_) {
    DCHECK(state == ManagePasswordsIcon::PENDING_STATE);
    icon->ShowBubbleWithoutUserInteraction();
    manage_passwords_bubble_needs_showing_ = false;
  }
}
