/**
 * Copyright (C) 2021 Saturneric
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
 * Saturneric<eric@bktus.com> starting on May 12, 2021.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "MainWindow.h"

#include "core/function/GlobalSettingStation.h"
#include "ui/SignalStation.h"
#include "ui/UserInterfaceUtils.h"
#include "ui/struct/SettingsObject.h"
#include "ui/thread/VersionCheckTask.h"

namespace GpgFrontend::UI {

MainWindow::MainWindow() {
  this->setMinimumSize(1200, 700);
  this->setWindowTitle(qApp->applicationName());
}

void MainWindow::Init() noexcept {
  try {
    /* get path where app was started */
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    edit_ = new TextEdit(this);
    setCentralWidget(edit_);

    /* the list of Keys available*/
    m_key_list_ = new KeyList(
        KeyMenuAbility::REFRESH | KeyMenuAbility::UNCHECK_ALL, this);

    info_board_ = new InfoBoardWidget(this);

    /* List of binary Attachments */
    attachment_dock_created_ = false;

    /* Variable containing if restart is needed */
    this->SlotSetRestartNeeded(false);

    create_actions();
    create_menus();
    create_tool_bars();
    create_status_bar();
    create_dock_windows();

    connect(edit_->tab_widget_, &QTabWidget::currentChanged, this,
            &MainWindow::slot_disable_tab_actions);
    connect(SignalStation::GetInstance(),
            &SignalStation::SignalRefreshStatusBar, this,
            [=](const QString &message, int timeout) {
              statusBar()->showMessage(message, timeout);
            });

    m_key_list_->AddMenuAction(append_selected_keys_act_);
    m_key_list_->AddMenuAction(copy_mail_address_to_clipboard_act_);
    m_key_list_->AddSeparator();
    m_key_list_->AddMenuAction(show_key_details_act_);

    restore_settings();

    edit_->CurTextPage()->setFocus();

    auto &settings = GlobalSettingStation::GetInstance().GetUISettings();

    if (!settings.exists("wizard") ||
        settings.lookup("wizard").getType() != libconfig::Setting::TypeGroup)
      settings.add("wizard", libconfig::Setting::TypeGroup);

    auto &wizard = settings["wizard"];

    // Show wizard, if the don't show wizard message box wasn't checked
    // and keylist doesn't contain a private key

    if (!wizard.exists("show_wizard"))
      wizard.add("show_wizard", libconfig::Setting::TypeBoolean) = true;

    bool show_wizard = true;
    wizard.lookupValue("show_wizard", show_wizard);

    LOG(INFO) << "wizard show_wizard" << show_wizard;

    if (show_wizard) {
      slot_start_wizard();
    }

    emit SignalLoaded();

    // if not prohibit update checking
    if (!prohibit_update_checking_) {
      auto *version_task = new VersionCheckTask();

      connect(version_task, &VersionCheckTask::SignalUpgradeVersion, this,
              &MainWindow::slot_version_upgrade);

      Thread::TaskRunnerGetter::GetInstance()
          .GetTaskRunner(Thread::TaskRunnerGetter::kTaskRunnerType_Network)
          ->PostTask(version_task);
    }

  } catch (...) {
    LOG(FATAL) << _("Critical error occur while loading GpgFrontend.");
    QMessageBox::critical(nullptr, _("Loading Failed"),
                          _("Critical error occur while loading GpgFrontend."));
    QCoreApplication::quit();
    exit(0);
  }
}

void MainWindow::restore_settings() {
  LOG(INFO) << _("Called");

  try {
    LOG(INFO) << "restore settings main_windows_state";

    SettingsObject main_windows_state("main_windows_state");

    std::string window_state = main_windows_state.Check(
        "window_state", saveState().toBase64().toStdString());
    // state sets pos & size of dock-widgets
    this->restoreState(
        QByteArray::fromBase64(QByteArray::fromStdString(window_state)));

    bool window_save = main_windows_state.Check("window_save", true);

    // Restore window size & location
    if (window_save) {
      int x = main_windows_state.Check("window_pos").Check("x", 100),
          y = main_windows_state.Check("window_pos").Check("y", 100);

      auto pos = QPoint(x, y);

      int width = main_windows_state.Check("window_size").Check("width", 800),
          height = main_windows_state.Check("window_size").Check("height", 450);

      auto size = QSize(width, height);
      this->resize(size);
      this->move(pos);
    } else {
      this->resize(QSize(800, 450));
      this->move(QPoint(100, 100));
    }

    int width = main_windows_state.Check("icon_size").Check("width", 24),
        height = main_windows_state.Check("icon_size").Check("height", 24);
    LOG(INFO) << "icon_size" << width << height;

    main_windows_state.Check("info_font_size", 10);

    // icon_style
    int s_icon_style =
        main_windows_state.Check("icon_style", Qt::ToolButtonTextUnderIcon);
    auto icon_style = static_cast<Qt::ToolButtonStyle>(s_icon_style);
    this->setToolButtonStyle(icon_style);
    import_button_->setToolButtonStyle(icon_style);

    // icons ize
    this->setIconSize(QSize(width, height));
    import_button_->setIconSize(QSize(width, height));

    LOG(INFO) << "restore settings key_server";

    SettingsObject key_server_json("key_server");

    if (!key_server_json.contains("server_list")) {
      key_server_json["server_list"] = {"https://keyserver.ubuntu.com",
                                        "http://keys.gnupg.net",
                                        "http://pool.sks-keyservers.net"};
    }

    if (!key_server_json.contains("default_server")) {
      key_server_json["default_server"] = 0;
    }

    auto &settings = GlobalSettingStation::GetInstance().GetUISettings();

    if (!settings.exists("general") ||
        settings.lookup("general").getType() != libconfig::Setting::TypeGroup)
      settings.add("general", libconfig::Setting::TypeGroup);

    auto &general = settings["general"];

    if (!general.exists("save_key_checked")) {
      general.add("save_key_checked", libconfig::Setting::TypeBoolean) = true;
    }

    if (!general.exists("non_ascii_when_export")) {
      general.add("non_ascii_when_export", libconfig::Setting::TypeBoolean) =
          true;
    }

    bool save_key_checked = true;
    general.lookupValue("save_key_checked", save_key_checked);

    try {
      LOG(INFO) << "restore settings default_key_checked";

      // Checked Keys
      SettingsObject default_key_checked("default_key_checked");
      if (save_key_checked) {
        auto key_ids_ptr = std::make_unique<KeyIdArgsList>();
        for (auto &it : default_key_checked) {
          std::string key_id = it;
          LOG(INFO) << "get checked key id" << key_id;
          key_ids_ptr->push_back(key_id);
        }
        m_key_list_->SetChecked(std::move(key_ids_ptr));
      }
    } catch (...) {
      LOG(ERROR) << "restore default_key_checked failed";
    }

    prohibit_update_checking_ = false;
    try {
      prohibit_update_checking_ =
          settings.lookup("network.prohibit_update_checking");
    } catch (...) {
      LOG(ERROR) << _("Setting Operation Error")
                 << _("prohibit_update_checking");
    }

  } catch (...) {
    LOG(ERROR) << "cannot resolve settings";
  }

  GlobalSettingStation::GetInstance().SyncSettings();
  LOG(INFO) << _("settings restored");
}

void MainWindow::save_settings() {
  auto &settings = GlobalSettingStation::GetInstance().GetUISettings();

  try {
    SettingsObject main_windows_state("main_windows_state");

    // window position and size
    main_windows_state["window_state"] = saveState().toBase64().toStdString();
    main_windows_state["window_pos"]["x"] = pos().x();
    main_windows_state["window_pos"]["y"] = pos().y();

    main_windows_state["window_size"]["width"] = size().width();
    main_windows_state["window_size"]["height"] = size().height();

    bool save_key_checked = settings.lookup("general.save_key_checked");

    // keyid-list of private checked keys
    if (save_key_checked) {
      auto key_ids_need_to_store = m_key_list_->GetChecked();

      SettingsObject default_key_checked("default_key_checked");
      default_key_checked.clear();

      for (const auto &key_id : *key_ids_need_to_store)
        default_key_checked.push_back(key_id);
    } else {
      settings["general"].remove("save_key_checked");
    }
  } catch (...) {
    LOG(ERROR) << "cannot save settings";
  };

  GlobalSettingStation::GetInstance().SyncSettings();
}

void MainWindow::close_attachment_dock() {
  if (!attachment_dock_created_) {
    return;
  }
  attachment_dock_->close();
  attachment_dock_->deleteLater();
  attachment_dock_created_ = false;
}

void MainWindow::closeEvent(QCloseEvent *event) {
  /*
   * ask to save changes, if there are
   * modified documents in any tab
   */
  if (edit_->MaybeSaveAnyTab()) {
    save_settings();
    event->accept();
  } else {
    event->ignore();
  }

  // clear password from memory
  //  GpgContext::GetInstance().clearPasswordCache();
}

}  // namespace GpgFrontend::UI
