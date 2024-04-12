/**
 * Copyright (C) 2021 Saturneric <eric@bktus.com>
 *
 * This file is part of GpgFrontend.
 *
 * GpgFrontend is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GpgFrontend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GpgFrontend. If not, see <https://www.gnu.org/licenses/>.
 *
 * The initial version of the source code is inherited from
 * the gpg4usb project, which is under GPL-3.0-or-later.
 *
 * All the source code of GpgFrontend was modified and released by
 * Saturneric <eric@bktus.com> starting on May 12, 2021.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "GeneralDialog.h"

#include "core/model/SettingsObject.h"
#include "ui/struct/settings_object/WindowStateSO.h"

namespace GpgFrontend {

GpgFrontend::UI::GeneralDialog::GeneralDialog(QString name, QWidget *parent)
    : QDialog(parent), name_(std::move(name)) {
  slot_restore_settings();
  connect(this, &QDialog::finished, this, &GeneralDialog::slot_save_settings);
}

GpgFrontend::UI::GeneralDialog::~GeneralDialog() = default;

void GpgFrontend::UI::GeneralDialog::slot_restore_settings() noexcept {
  try {
    update_rect_cache();

    SettingsObject general_windows_state(name_ + "_dialog_state");
    auto window_state = WindowStateSO(general_windows_state);

    // Restore window size & location
    if (window_state.window_save) {
      int x = window_state.x;
      int y = window_state.y;
      GF_UI_LOG_DEBUG("stored dialog pos, x: {}, y: {}", x, y);

      QPoint relative_pos = {x, y};
      QPoint pos = parent_rect_.topLeft() + relative_pos;
      GF_UI_LOG_DEBUG("relative dialog pos, x: {}, y: {}", relative_pos.x(),
                      relative_pos.y());

      int width = window_state.width;
      int height = window_state.height;
      GF_UI_LOG_DEBUG("stored dialog size, width: {}, height: {}", width,
                      height);

      QRect target_rect = {pos.x(), pos.y(), width, height};
      GF_UI_LOG_DEBUG("dialog stored target rect, width: {}, height: {}", width,
                      height);

      // check for valid
      if (width > 0 && height > 0 && screen_rect_.contains(target_rect)) {
        this->setGeometry(target_rect);
        this->rect_restored_ = true;
      }
    }

  } catch (...) {
    GF_UI_LOG_ERROR("error at restoring settings");
  }
}

void GpgFrontend::UI::GeneralDialog::slot_save_settings() noexcept {
  try {
    SettingsObject general_windows_state(name_ + "_dialog_state");

    update_rect_cache();

    GF_UI_LOG_DEBUG("dialog pos, x: {}, y: {}", rect_.x(), rect_.y());
    GF_UI_LOG_DEBUG("dialog size, width: {}, height: {}", rect_.width(),
                    rect_.height());

    // window position relative to parent
    auto relative_pos = rect_.topLeft() - parent_rect_.topLeft();
    GF_UI_LOG_DEBUG("store dialog pos, x: {}, y: {}", relative_pos.x(),
                    relative_pos.y());

    WindowStateSO window_state;
    window_state.x = relative_pos.x();
    window_state.y = relative_pos.y();
    window_state.width = rect_.width();
    window_state.height = rect_.height();
    window_state.window_save = true;

    general_windows_state.Store(window_state.Json());

  } catch (...) {
    GF_UI_LOG_ERROR("general dialog: {}, caught exception", name_);
  }
}

void GpgFrontend::UI::GeneralDialog::setPosCenterOfScreen() {
  update_rect_cache();

  int screen_width = screen_rect_.width();
  int screen_height = screen_rect_.height();
  GF_UI_LOG_DEBUG("dialog current screen available geometry", screen_width,
                  screen_height);

  // update rect of current dialog
  rect_ = this->geometry();
  this->move(screen_rect_.center() -
             QPoint(rect_.width() / 2, rect_.height() / 2));
}

/**
 * @brief
 *
 */
void GpgFrontend::UI::GeneralDialog::movePosition2CenterOfParent() {
  // update cache
  update_rect_cache();

  // log for debug
  GF_UI_LOG_DEBUG("parent pos x: {} y: {}", parent_rect_.x(), parent_rect_.y());
  GF_UI_LOG_DEBUG("parent size width: {}, height: {}", parent_rect_.width(),
                  parent_rect_.height());
  GF_UI_LOG_DEBUG("parent center pos x: {}, y: {}", parent_rect_.center().x(),
                  parent_rect_.center().y());
  GF_UI_LOG_DEBUG("dialog pos x: {} y: {}", rect_.x(), rect_.y());
  GF_UI_LOG_DEBUG("dialog size width: {} height: {}", rect_.width(),
                  rect_.height());

  if (parent_rect_.topLeft() != QPoint{0, 0} &&
      parent_rect_.size() != QSize{0, 0}) {
    if (rect_.width() <= 0) rect_.setWidth(100);
    if (rect_.height() <= 0) rect_.setHeight(100);

    QPoint target_position =
        parent_rect_.center() - QPoint(rect_.width() / 2, rect_.height() / 2);

    GF_UI_LOG_DEBUG(
        "update position to parent's center, target pos, x:{}, y: {}",
        target_position.x(), target_position.y());

    this->move(target_position);
  } else {
    setPosCenterOfScreen();
  }
}

/**
 *
 */
void GpgFrontend::UI::GeneralDialog::update_rect_cache() {
  // update size of current dialog
  rect_ = this->geometry();

  auto *screen = this->window()->screen();
  screen_rect_ = screen->availableGeometry();

  // read pos and size from parent
  if (this->parent() != nullptr) {
    QRect parent_rect;

    auto *parent_widget = qobject_cast<QWidget *>(this->parent());
    if (parent_widget != nullptr) {
      parent_rect = parent_widget->geometry();
    } else {
      auto *parent_dialog = qobject_cast<QDialog *>(this->parent());
      if (parent_dialog != nullptr) {
        parent_rect = parent_dialog->geometry();
      } else {
        auto *parent_window = qobject_cast<QMainWindow *>(this->parent());
        if (parent_window != nullptr) {
          parent_rect = parent_window->geometry();
        }
      }
    }
    parent_rect_ = parent_rect;
  } else {
    // reset parent's pos and size
    this->parent_rect_ = QRect{0, 0, 0, 0};
  }
}

/**
 * @brief
 *
 */
auto GpgFrontend::UI::GeneralDialog::isRectRestored() -> bool {
  return rect_restored_;
}

/**
 * @brief
 *
 */
void GpgFrontend::UI::GeneralDialog::showEvent(QShowEvent *event) {
  GF_UI_LOG_DEBUG("General Dialog named {} is about to show, caught show event",
                  name_);

  // default position strategy
  if (!isRectRestored()) movePosition2CenterOfParent();

  QDialog::showEvent(event);
}

}  // namespace GpgFrontend