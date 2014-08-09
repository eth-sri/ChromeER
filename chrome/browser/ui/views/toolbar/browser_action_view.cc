// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_action_view.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/controls/button/label_button_border.h"

using extensions::Extension;
using views::LabelButtonBorder;

namespace {

// We have smaller insets than normal STYLE_TEXTBUTTON buttons so that we can
// fit user supplied icons in without clipping them.
const int kBorderInset = 4;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserActionView

BrowserActionView::BrowserActionView(const Extension* extension,
                                     Browser* browser,
                                     BrowserActionView::Delegate* delegate)
    : delegate_(delegate) {
  set_id(VIEW_ID_BROWSER_ACTION);
  button_.reset(new BrowserActionButton(extension, browser, delegate_));
  button_->set_drag_controller(delegate_);
  button_->set_owned_by_client();
  AddChildView(button_.get());
  button_->UpdateState();
}

BrowserActionView::~BrowserActionView() {
}

gfx::ImageSkia BrowserActionView::GetIconWithBadge() {
  return button_->GetIconWithBadge();
}

void BrowserActionView::Layout() {
  button_->SetBounds(0, 0, width(), height());
}

void BrowserActionView::GetAccessibleState(ui::AXViewState* state) {
  state->name = l10n_util::GetStringUTF16(
      IDS_ACCNAME_EXTENSIONS_BROWSER_ACTION);
  state->role = ui::AX_ROLE_GROUP;
}

gfx::Size BrowserActionView::GetPreferredSize() const {
  return gfx::Size(BrowserActionsContainer::IconWidth(false),
                   BrowserActionsContainer::IconHeight());
}

void BrowserActionView::PaintChildren(gfx::Canvas* canvas,
                                      const views::CullSet& cull_set) {
  View::PaintChildren(canvas, cull_set);
  ExtensionAction* action = button_->extension_action();
  int tab_id = button_->view_controller()->GetCurrentTabId();
  if (tab_id >= 0)
    action->PaintBadge(canvas, GetLocalBounds(), tab_id);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

BrowserActionButton::BrowserActionButton(const Extension* extension,
                                         Browser* browser,
                                         BrowserActionView::Delegate* delegate)
    : MenuButton(this, base::string16(), NULL, false),
      view_controller_(new ExtensionActionViewController(
          extension,
          browser,
          extensions::ExtensionActionManager::Get(browser->profile())->
              GetBrowserAction(*extension),
          this)),
      delegate_(delegate),
      called_registered_extension_command_(false),
      icon_observer_(NULL) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  set_context_menu_controller(view_controller_.get());

  // No UpdateState() here because View hierarchy not setup yet. Our parent
  // should call UpdateState() after creation.

  content::NotificationSource notification_source =
      content::Source<Profile>(browser->profile()->GetOriginalProfile());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
                 content::Source<ExtensionAction>(extension_action()));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED,
                 notification_source);
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_COMMAND_REMOVED,
                 notification_source);

  // We also listen for browser theme changes on linux because a switch from or
  // to GTK requires that we regrab our browser action images.
  registrar_.Add(
      this,
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(browser->profile())));
}

void BrowserActionButton::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && !called_registered_extension_command_ &&
      GetFocusManager()) {
    view_controller_->RegisterCommand();
    called_registered_extension_command_ = true;
  }

  MenuButton::ViewHierarchyChanged(details);
}

void BrowserActionButton::OnDragDone() {
  delegate_->OnBrowserActionViewDragDone();
}

void BrowserActionButton::GetAccessibleState(ui::AXViewState* state) {
  views::MenuButton::GetAccessibleState(state);
  state->role = ui::AX_ROLE_BUTTON;
}

void BrowserActionButton::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  view_controller_->ExecuteActionByUser();
}

void BrowserActionButton::UpdateState() {
  int tab_id = view_controller_->GetCurrentTabId();
  if (tab_id < 0)
    return;

  if (!IsEnabled(tab_id)) {
    SetState(views::CustomButton::STATE_DISABLED);
  } else {
    SetState(menu_visible_ ?
             views::CustomButton::STATE_PRESSED :
             views::CustomButton::STATE_NORMAL);
  }

  gfx::ImageSkia icon = *view_controller_->GetIcon(tab_id).ToImageSkia();

  if (!icon.isNull()) {
    if (!extension_action()->GetIsVisible(tab_id))
      icon = gfx::ImageSkiaOperations::CreateTransparentImage(icon, .25);

    ThemeService* theme = ThemeServiceFactory::GetForProfile(
        view_controller_->browser()->profile());

    gfx::ImageSkia bg = *theme->GetImageSkiaNamed(IDR_BROWSER_ACTION);
    SetImage(views::Button::STATE_NORMAL,
             gfx::ImageSkiaOperations::CreateSuperimposedImage(bg, icon));
  }

  // If the browser action name is empty, show the extension name instead.
  std::string title = extension_action()->GetTitle(tab_id);
  base::string16 name =
      base::UTF8ToUTF16(title.empty() ? extension()->name() : title);
  SetTooltipText(name);
  SetAccessibleName(name);

  parent()->SchedulePaint();
}

bool BrowserActionButton::IsPopup() {
  int tab_id = view_controller_->GetCurrentTabId();
  return (tab_id < 0) ? false : extension_action()->HasPopup(tab_id);
}

void BrowserActionButton::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED:
      UpdateState();
      // The browser action may have become visible/hidden so we need to make
      // sure the state gets updated.
      delegate_->OnBrowserActionVisibilityChanged();
      break;
    case extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED:
    case extensions::NOTIFICATION_EXTENSION_COMMAND_REMOVED: {
      std::pair<const std::string, const std::string>* payload =
          content::Details<std::pair<const std::string, const std::string> >(
              details).ptr();
      if (extension()->id() == payload->first &&
          payload->second ==
              extensions::manifest_values::kBrowserActionCommandEvent) {
        if (type == extensions::NOTIFICATION_EXTENSION_COMMAND_ADDED)
          view_controller_->RegisterCommand();
        else
          view_controller_->UnregisterCommand(true);
      }
      break;
    }
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      UpdateState();
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool BrowserActionButton::Activate() {
  if (!IsPopup())
    return true;

  view_controller_->ExecuteActionByUser();

  // TODO(erikkay): Run a nested modal loop while the mouse is down to
  // enable menu-like drag-select behavior.

  // The return value of this method is returned via OnMousePressed.
  // We need to return false here since we're handing off focus to another
  // widget/view, and true will grab it right back and try to send events
  // to us.
  return false;
}

bool BrowserActionButton::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsRightMouseButton()) {
    return IsPopup() ? MenuButton::OnMousePressed(event) :
                       LabelButton::OnMousePressed(event);
  }

  if (!views::View::ShouldShowContextMenuOnMousePress()) {
    // See comments in MenuButton::Activate() as to why this is needed.
    SetMouseHandler(NULL);

    ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);
  }
  return false;
}

void BrowserActionButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsPopup() || view_controller_->is_menu_running()) {
    // TODO(erikkay) this never actually gets called (probably because of the
    // loss of focus).
    MenuButton::OnMouseReleased(event);
  } else {
    LabelButton::OnMouseReleased(event);
  }
}

void BrowserActionButton::OnMouseExited(const ui::MouseEvent& event) {
  if (IsPopup() || view_controller_->is_menu_running())
    MenuButton::OnMouseExited(event);
  else
    LabelButton::OnMouseExited(event);
}

bool BrowserActionButton::OnKeyReleased(const ui::KeyEvent& event) {
  return IsPopup() ? MenuButton::OnKeyReleased(event) :
                     LabelButton::OnKeyReleased(event);
}

void BrowserActionButton::OnGestureEvent(ui::GestureEvent* event) {
  if (IsPopup())
    MenuButton::OnGestureEvent(event);
  else
    LabelButton::OnGestureEvent(event);
}

scoped_ptr<LabelButtonBorder> BrowserActionButton::CreateDefaultBorder() const {
  scoped_ptr<LabelButtonBorder> border = LabelButton::CreateDefaultBorder();
  border->set_insets(gfx::Insets(kBorderInset, kBorderInset,
                                 kBorderInset, kBorderInset));
  return border.Pass();
}

void BrowserActionButton::SetButtonPushed() {
  SetState(views::CustomButton::STATE_PRESSED);
  menu_visible_ = true;
}

void BrowserActionButton::SetButtonNotPushed() {
  SetState(views::CustomButton::STATE_NORMAL);
  menu_visible_ = false;
}

bool BrowserActionButton::IsEnabled(int tab_id) const {
  return view_controller_->extension_action()->GetIsVisible(tab_id);
}

gfx::ImageSkia BrowserActionButton::GetIconWithBadge() {
  int tab_id = view_controller_->GetCurrentTabId();
  gfx::Size spacing(0, ToolbarView::kVertSpacing);
  gfx::ImageSkia icon = *view_controller_->GetIcon(tab_id).ToImageSkia();
  if (!IsEnabled(tab_id))
    icon = gfx::ImageSkiaOperations::CreateTransparentImage(icon, .25);
  return extension_action()->GetIconWithBadge(icon, tab_id, spacing);
}

gfx::ImageSkia BrowserActionButton::GetIconForTest() {
  return GetImage(views::Button::STATE_NORMAL);
}

BrowserActionButton::~BrowserActionButton() {
}

void BrowserActionButton::OnIconUpdated() {
  UpdateState();
  if (icon_observer_)
    icon_observer_->OnIconUpdated(GetIconWithBadge());
}

views::View* BrowserActionButton::GetAsView() {
  return this;
}

bool BrowserActionButton::IsShownInMenu() {
  return delegate_->ShownInsideMenu();
}

views::FocusManager* BrowserActionButton::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::Widget* BrowserActionButton::GetParentForContextMenu() {
  // RunMenuAt expects a nested menu to be parented by the same widget as the
  // already visible menu, in this case the Chrome menu.
  return delegate_->ShownInsideMenu() ?
      BrowserView::GetBrowserViewForBrowser(view_controller_->browser())
          ->toolbar()->app_menu()->GetWidget() :
      GetWidget();
}

views::View* BrowserActionButton::GetReferenceViewForPopup() {
  // Browser actions in the overflow menu can still show popups, so we may need
  // a reference view other than this button's parent. If so, use the overflow
  // view.
  return parent()->visible() ? this : delegate_->GetOverflowReferenceView();
}

content::WebContents* BrowserActionButton::GetCurrentWebContents() {
  return delegate_->GetCurrentWebContents();
}

void BrowserActionButton::HideActivePopup() {
  delegate_->HideActivePopup();
}

void BrowserActionButton::OnPopupShown(bool grant_tab_permissions) {
  delegate_->SetPopupOwner(this);
  if (grant_tab_permissions)
    SetButtonPushed();
}

void BrowserActionButton::CleanupPopup() {
  // We need to do these actions synchronously (instead of closing and then
  // performing the rest of the cleanup in OnWidgetDestroyed()) because
  // OnWidgetDestroyed() can be called asynchronously from Close(), and we need
  // to keep the delegate's popup owner up-to-date.
  SetButtonNotPushed();
  delegate_->SetPopupOwner(NULL);
}

void BrowserActionButton::OnWillShowContextMenus() {
  SetButtonPushed();
}

void BrowserActionButton::OnContextMenuDone() {
  SetButtonNotPushed();
}
