// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_item_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"


// Helpers --------------------------------------------------------------------

namespace {

enum FieldType { USERNAME_FIELD, PASSWORD_FIELD };

// Upper limit on the size of the username and password fields.
const int kUsernameFieldSize = 30;
const int kPasswordFieldSize = 22;

// Returns the width of |type| field.
int GetFieldWidth(FieldType type) {
  return ui::ResourceBundle::GetSharedInstance()
      .GetFontList(ui::ResourceBundle::SmallFont)
      .GetExpectedTextWidth(type == USERNAME_FIELD ? kUsernameFieldSize
                                                   : kPasswordFieldSize);
}

class SavePasswordRefusalComboboxModel : public ui::ComboboxModel {
 public:
  enum { INDEX_NOPE = 0, INDEX_NEVER_FOR_THIS_SITE = 1, };

  SavePasswordRefusalComboboxModel() {
    items_.push_back(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));
    items_.push_back(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON));
  }
  virtual ~SavePasswordRefusalComboboxModel() {}

 private:
  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE { return items_.size(); }
  virtual base::string16 GetItemAt(int index) OVERRIDE { return items_[index]; }
  virtual bool IsItemSeparatorAt(int index) OVERRIDE {
    return items_[index].empty();
  }
  virtual int GetDefaultIndex() const OVERRIDE { return 0; }

  std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordRefusalComboboxModel);
};

}  // namespace


// ManagePasswordsBubbleView --------------------------------------------------

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

// static
void ManagePasswordsBubbleView::ShowBubble(content::WebContents* web_contents,
                                           DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(browser->fullscreen_controller());

  if (IsShowing())
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  views::View* anchor_view = is_fullscreen ?
      NULL : browser_view->GetLocationBarView()->manage_passwords_icon_view();
  manage_passwords_bubble_ = new ManagePasswordsBubbleView(
      web_contents, anchor_view, reason);

  if (is_fullscreen) {
    manage_passwords_bubble_->set_parent_window(
        web_contents->GetView()->GetTopLevelNativeWindow());
  }

  views::BubbleDelegateView::CreateBubble(manage_passwords_bubble_);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }

  manage_passwords_bubble_->GetWidget()->Show();
}

// static
void ManagePasswordsBubbleView::CloseBubble() {
  if (manage_passwords_bubble_)
    manage_passwords_bubble_->CloseWithoutLogging();
}

// static
bool ManagePasswordsBubbleView::IsShowing() {
  // The bubble may be in the process of closing.
  return (manage_passwords_bubble_ != NULL) &&
      manage_passwords_bubble_->GetWidget()->IsVisible();
}

ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason)
    : ManagePasswordsBubble(web_contents, reason),
      BubbleDelegateView(anchor_view,
                         anchor_view ? views::BubbleBorder::TOP_RIGHT
                                     : views::BubbleBorder::NONE) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  set_notify_enter_exit_on_child(true);
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {}

void ManagePasswordsBubbleView::BuildColumnSet(views::GridLayout* layout,
                                               ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;

    case DOUBLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case LINK_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
  }
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
}

void ManagePasswordsBubbleView::AdjustForFullscreen(
    const gfx::Rect& screen_bounds) {
  if (GetAnchorView())
    return;

  // The bubble's padding from the screen edge, used in fullscreen.
  const int kFullscreenPaddingEnd = 20;
  const size_t bubble_half_width = width() / 2;
  const int x_pos = base::i18n::IsRTL() ?
      screen_bounds.x() + bubble_half_width + kFullscreenPaddingEnd :
      screen_bounds.right() - bubble_half_width - kFullscreenPaddingEnd;
  SetAnchorRect(gfx::Rect(x_pos, screen_bounds.y(), 0, 0));
}

void ManagePasswordsBubbleView::Close() {
  GetWidget()->Close();
}

void ManagePasswordsBubbleView::CloseWithoutLogging() {
  model()->OnCloseWithoutLogging();
  GetWidget()->Close();
}

void ManagePasswordsBubbleView::Init() {
  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  SetFocusable(true);
  SetLayoutManager(layout);
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  BuildColumnSet(layout, LINK_BUTTON_COLUMN_SET);

  // This calculates the necessary widths for credential columns in the bubble.
  const int first_field_width = std::max(
      GetFieldWidth(USERNAME_FIELD),
      views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED))
          .GetPreferredSize()
          .width());

  const int second_field_width = std::max(
      GetFieldWidth(PASSWORD_FIELD),
      views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO))
          .GetPreferredSize()
          .width());

  // Build and populate the header.
  views::Label* title_label = new views::Label(model()->title());
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));

  layout->StartRowWithPadding(
      0, SINGLE_VIEW_COLUMN_SET, 0, views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  if (model()->WaitingToSavePassword()) {
    // If we've got a password that we're deciding whether or not to save,
    // then we need to display a single-view columnset containing the
    // ManagePasswordItemView, followed by double-view columnset containing
    // a "Save" and "Reject" button.
    ManagePasswordItemView* item =
        new ManagePasswordItemView(model(),
                                   model()->pending_credentials(),
                                   first_field_width,
                                   second_field_width,
                                   ManagePasswordItemView::FIRST_ITEM);
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(item);

    refuse_combobox_ =
        new views::Combobox(new SavePasswordRefusalComboboxModel());
    refuse_combobox_->set_listener(this);
    refuse_combobox_->SetStyle(views::Combobox::STYLE_ACTION);

    save_button_ = new views::BlueButton(
        this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));

    layout->StartRowWithPadding(
        0, DOUBLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
    layout->AddView(save_button_);
    layout->AddView(refuse_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    // If we have a list of passwords to store for the current site, display
    // them to the user for management. Otherwise, render a "No passwords for
    // this site" message.
    //
    // TODO(mkwst): Do we really want the "No passwords" case? It would probably
    // be better to only clear the pending password upon navigation, rather than
    // as soon as the bubble closes.
    if (!model()->best_matches().empty()) {
      for (autofill::PasswordFormMap::const_iterator i(
               model()->best_matches().begin());
           i != model()->best_matches().end();
           ++i) {
        ManagePasswordItemView* item = new ManagePasswordItemView(
            model(),
            *i->second,
            first_field_width,
            second_field_width,
            i == model()->best_matches().begin()
                ? ManagePasswordItemView::FIRST_ITEM
                : ManagePasswordItemView::SUBSEQUENT_ITEM);

        layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
        layout->AddView(item);
      }
    } else {
        views::Label* empty_label = new views::Label(
            l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS));
        empty_label->SetMultiLine(true);

        layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
        layout->AddView(empty_label);
    }

    // Build a "manage" link and "done" button, and throw them both into a new
    // row
    // containing a double-view columnset.
    manage_link_ = new views::Link(model()->manage_link());
    manage_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    manage_link_->SetUnderline(false);
    manage_link_->set_listener(this);

    done_button_ =
        new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
    done_button_->SetStyle(views::Button::STYLE_BUTTON);

    layout->StartRowWithPadding(
        0, LINK_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
    layout->AddView(manage_link_);
    layout->AddView(done_button_);
  }
}

void ManagePasswordsBubbleView::WindowClosing() {
  // Close() closes the window asynchronously, so by the time we reach here,
  // |manage_passwords_bubble_| may have already been reset.
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = NULL;
}

void ManagePasswordsBubbleView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  DCHECK(sender == save_button_ || sender == done_button_);

  if (sender == save_button_)
    model()->OnSaveClicked();
  else
    model()->OnDoneClicked();
  Close();
}

void ManagePasswordsBubbleView::LinkClicked(views::Link* source,
                                            int event_flags) {
  DCHECK_EQ(source, manage_link_);
  model()->OnManageLinkClicked();
  Close();
}

void ManagePasswordsBubbleView::OnPerformAction(views::Combobox* source) {
  DCHECK_EQ(source, refuse_combobox_);
  switch (refuse_combobox_->selected_index()) {
    case SavePasswordRefusalComboboxModel::INDEX_NOPE:
      model()->OnNopeClicked();
      break;
    case SavePasswordRefusalComboboxModel::INDEX_NEVER_FOR_THIS_SITE:
      model()->OnNeverForThisSiteClicked();
      break;
    default:
      NOTREACHED();
      break;
  }
  Close();
}
