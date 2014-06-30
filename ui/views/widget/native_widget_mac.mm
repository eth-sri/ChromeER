// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/font_list.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/native_theme/native_theme.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"

namespace views {
namespace {

NSInteger StyleMaskForParams(const Widget::InitParams& params) {
  // TODO(tapted): Determine better masks when there are use cases for it.
  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    return NSTitledWindowMask | NSClosableWindowMask |
           NSMiniaturizableWindowMask | NSResizableWindowMask;
  }
  return NSBorderlessWindowMask;
}

NSRect ValidateContentRect(NSRect content_rect) {
  // A contentRect with zero width or height is a banned practice in Chrome, due
  // to unpredictable OSX treatment. For now, silently give a minimum dimension.
  // TODO(tapted): Add a DCHECK, or add emulation logic (e.g. to auto-hide).
  if (NSWidth(content_rect) == 0)
    content_rect.size.width = 1;

  if (NSHeight(content_rect) == 0)
    content_rect.size.height = 1;

  return content_rect;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, public:

NativeWidgetMac::NativeWidgetMac(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      bridge_(new BridgedNativeWidget(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
}

NativeWidgetMac::~NativeWidgetMac() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  else
    CloseNow();
}

void NativeWidgetMac::OnWindowWillClose() {
  delegate_->OnNativeWidgetDestroying();
  // Note: If closed via CloseNow(), |bridge_| will already be reset. If closed
  // by the user, or via Close() and a RunLoop, this will reset it.
  bridge_.reset();
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, internal::NativeWidgetPrivate implementation:

void NativeWidgetMac::InitNativeWidget(const Widget::InitParams& params) {
  ownership_ = params.ownership;

  NSInteger style_mask = StyleMaskForParams(params);
  NSRect content_rect = ValidateContentRect(
      [NSWindow contentRectForFrameRect:gfx::ScreenRectToNSRect(params.bounds)
                              styleMask:style_mask]);

  base::scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:content_rect
                                  styleMask:style_mask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [window setReleasedWhenClosed:NO];  // Owned by scoped_nsobject.
  bridge_->Init(window, params);

  delegate_->OnNativeWidgetCreated(true);

  bridge_->SetFocusManager(GetWidget()->GetFocusManager());
}

NonClientFrameView* NativeWidgetMac::CreateNonClientFrameView() {
  return NULL;
}

bool NativeWidgetMac::ShouldUseNativeFrame() const {
  return false;
}

bool NativeWidgetMac::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::FrameTypeChanged() {
  NOTIMPLEMENTED();
}

Widget* NativeWidgetMac::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetMac::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetMac::GetNativeView() const {
  // Returns a BridgedContentView, unless there is no views::RootView set.
  return [GetNativeWindow() contentView];
}

gfx::NativeWindow NativeWidgetMac::GetNativeWindow() const {
  return bridge_ ? bridge_->ns_window() : nil;
}

Widget* NativeWidgetMac::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

const ui::Compositor* NativeWidgetMac::GetCompositor() const {
  NOTIMPLEMENTED();
  return NULL;
}

ui::Compositor* NativeWidgetMac::GetCompositor() {
  NOTIMPLEMENTED();
  return NULL;
}

ui::Layer* NativeWidgetMac::GetLayer() {
  NOTIMPLEMENTED();
  return NULL;
}

void NativeWidgetMac::ReorderNativeViews() {
  if (bridge_)
    bridge_->SetRootView(GetWidget()->GetRootView());
}

void NativeWidgetMac::ViewRemoved(View* view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetNativeWindowProperty(const char* name, void* value) {
  NOTIMPLEMENTED();
}

void* NativeWidgetMac::GetNativeWindowProperty(const char* name) const {
  NOTIMPLEMENTED();
  return NULL;
}

TooltipManager* NativeWidgetMac::GetTooltipManager() const {
  NOTIMPLEMENTED();
  return NULL;
}

void NativeWidgetMac::SetCapture() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::HasCapture() const {
  NOTIMPLEMENTED();
  return false;
}

InputMethod* NativeWidgetMac::CreateInputMethod() {
  return bridge_ ? bridge_->CreateInputMethod() : NULL;
}

internal::InputMethodDelegate* NativeWidgetMac::GetInputMethodDelegate() {
  return bridge_.get();
}

ui::InputMethod* NativeWidgetMac::GetHostInputMethod() {
  return bridge_ ? bridge_->GetHostInputMethod() : NULL;
}

void NativeWidgetMac::CenterWindow(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::GetWindowPlacement(gfx::Rect* bounds,
                                         ui::WindowShowState* maximized) const {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::SetWindowTitle(const base::string16& title) {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::InitModalType(ui::ModalType modal_type) {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetMac::GetWindowBoundsInScreen() const {
  return gfx::ScreenRectFromNSRect([GetNativeWindow() frame]);
}

gfx::Rect NativeWidgetMac::GetClientAreaBoundsInScreen() const {
  NSWindow* window = GetNativeWindow();
  return gfx::ScreenRectFromNSRect(
      [window contentRectForFrameRect:[window frame]]);
}

gfx::Rect NativeWidgetMac::GetRestoredBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void NativeWidgetMac::SetBounds(const gfx::Rect& bounds) {
  [GetNativeWindow() setFrame:gfx::ScreenRectToNSRect(bounds)
                      display:YES
                      animate:NO];
}

void NativeWidgetMac::SetSize(const gfx::Size& size) {
  // Ensure the top-left corner stays in-place (rather than the bottom-left,
  // which -[NSWindow setContentSize:] would do).
  SetBounds(gfx::Rect(GetWindowBoundsInScreen().origin(), size));
}

void NativeWidgetMac::StackAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::StackAtTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::StackBelow(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetShape(gfx::NativeRegion shape) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Close() {
  NSWindow* window = GetNativeWindow();
  // Calling performClose: will momentarily highlight the close button, but
  // AppKit will reject it if there is no close button.
  SEL close_selector = ([window styleMask] & NSClosableWindowMask)
                           ? @selector(performClose:)
                           : @selector(close);
  [window performSelector:close_selector withObject:nil afterDelay:0];
}

void NativeWidgetMac::CloseNow() {
  // Reset |bridge_| to NULL before destroying it.
  scoped_ptr<BridgedNativeWidget> bridge(bridge_.Pass());
}

void NativeWidgetMac::Show() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Hide() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::ShowWithWindowState(ui::WindowShowState state) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsVisible() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMac::Activate() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsActive() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMac::SetAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsAlwaysOnTop() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::SetVisibleOnAllWorkspaces(bool always_visible) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Maximize() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Minimize() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetMac::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::Restore() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   const gfx::Point& location,
                                   int operation,
                                   ui::DragDropTypes::DragEventSource source) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SchedulePaintInRect(const gfx::Rect& rect) {
  // TODO(tapted): This should use setNeedsDisplayInRect:, once the coordinate
  // system of |rect| has been converted.
  [GetNativeView() setNeedsDisplay:YES];
}

void NativeWidgetMac::SetCursor(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsMouseEventsEnabled() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMac::ClearNativeFocus() {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetMac::GetWorkAreaBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

Widget::MoveLoopResult NativeWidgetMac::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  NOTIMPLEMENTED();
  return Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetMac::EndMoveLoop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetVisibilityChangedAnimationsEnabled(bool value) {
  NOTIMPLEMENTED();
}

ui::NativeTheme* NativeWidgetMac::GetNativeTheme() const {
  return ui::NativeTheme::instance();
}

void NativeWidgetMac::OnRootViewLayout() const {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::RepostNativeEvent(gfx::NativeEvent native_event) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

bool Widget::ConvertRect(const Widget* source,
                         const Widget* target,
                         gfx::Rect* rect) {
  return false;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetMac(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return GetNativeWidgetForNativeWindow([native_view window]);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  id<NSWindowDelegate> window_delegate = [native_window delegate];
  if ([window_delegate respondsToSelector:@selector(nativeWidgetMac)]) {
    ViewsNSWindowDelegate* delegate =
        base::mac::ObjCCastStrict<ViewsNSWindowDelegate>(window_delegate);
    return [delegate nativeWidgetMac];
  }
  return NULL;  // Not created by NativeWidgetMac.
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  NativeWidgetPrivate* native_widget =
      GetNativeWidgetForNativeView(native_view);
  if (!native_widget)
    return NULL;

  for (NativeWidgetPrivate* parent;
       (parent = GetNativeWidgetForNativeWindow(
            [native_widget->GetNativeWindow() parentWindow]));
       native_widget = parent) {
  }
  return native_widget;
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  NativeWidgetPrivate* native_widget =
      GetNativeWidgetForNativeView(native_view);
  if (!native_widget)
    return;

  // Code expects widget for |native_view| to be added to |children|.
  if (native_widget->GetWidget())
    children->insert(native_widget->GetWidget());

  for (NSWindow* child_window : [native_widget->GetNativeWindow() childWindows])
    GetAllChildWidgets([child_window contentView], children);
}

// static
void NativeWidgetPrivate::GetAllOwnedWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* owned) {
  NOTIMPLEMENTED();
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  NOTIMPLEMENTED();
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool NativeWidgetPrivate::IsTouchDown() {
  NOTIMPLEMENTED();
  return false;
}

// static
gfx::FontList NativeWidgetPrivate::GetWindowTitleFontList() {
  NOTIMPLEMENTED();
  return gfx::FontList();
}

}  // namespace internal
}  // namespace views
