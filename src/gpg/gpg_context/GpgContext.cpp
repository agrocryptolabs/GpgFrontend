/**
 * This file is part of GPGFrontend.
 *
 * GPGFrontend is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar.  If not, see <https://www.gnu.org/licenses/>.
 *
 * The initial version of the source code is inherited from gpg4usb-team.
 * Their source code version also complies with GNU General Public License.
 *
 * The source code version of this software was modified and released
 * by Saturneric<eric@bktus.com> starting on May 12, 2021.
 *
 */

#include "gpg/GpgContext.h"

#include <functional>
#include <unistd.h> /* contains read/write */

#ifdef _WIN32

#include <windows.h>

#endif

#define INT2VOIDP(i) (void *)(uintptr_t)(i)

namespace GpgFrontend {

/**
 * Constructor
 *  Set up gpgme-context, set paths to app-run path
 */
GpgContext::GpgContext() {

  gpgme_ctx_t _p_ctx;
  auto err = gpgme_new(&_p_ctx);
  check_gpg_error(err);
  _ctx_ref =
      CtxRefHandler(_p_ctx, [&](gpgme_ctx_t ctx) { gpgme_release(ctx); });

  gpgme_engine_info_t engineInfo;
  engineInfo = gpgme_ctx_get_engine_info(*this);

  // Check ENV before running
  bool check_pass = false, find_openpgp = false, find_gpgconf = false,
       find_assuan = false, find_cms = false;
  while (engineInfo != nullptr) {
    qDebug() << gpgme_get_protocol_name(engineInfo->protocol)
             << engineInfo->file_name << engineInfo->protocol
             << engineInfo->home_dir << engineInfo->version;
    if (engineInfo->protocol == GPGME_PROTOCOL_GPGCONF &&
        strcmp(engineInfo->version, "1.0.0") != 0)
      find_gpgconf = true;
    if (engineInfo->protocol == GPGME_PROTOCOL_OpenPGP &&
        strcmp(engineInfo->version, "1.0.0") != 0)
      find_openpgp = true, info.appPath = engineInfo->file_name;
    if (engineInfo->protocol == GPGME_PROTOCOL_CMS &&
        strcmp(engineInfo->version, "1.0.0") != 0)
      find_cms = true;
    if (engineInfo->protocol == GPGME_PROTOCOL_ASSUAN)
      find_assuan = true;
    engineInfo = engineInfo->next;
  }

  if (find_gpgconf && find_openpgp && find_cms && find_assuan)
    check_pass = true;

  if (!check_pass) {
    good_ = false;
    return;
  } else
    good_ = true;

  /** Setting the output type must be done at the beginning */
  /** think this means ascii-armor --> ? */
  gpgme_set_armor(*this, 1);
  // Speed up loading process
  gpgme_set_offline(*this, 1);

  gpgme_set_keylist_mode(
      *this, GPGME_KEYLIST_MODE_LOCAL | GPGME_KEYLIST_MODE_WITH_SECRET |
                 GPGME_KEYLIST_MODE_SIGS | GPGME_KEYLIST_MODE_SIG_NOTATIONS |
                 GPGME_KEYLIST_MODE_WITH_TOFU);
}

bool GpgContext::good() const { return good_; }

/** also from kgpgme.cpp, seems to clear password from mem */
void GpgContext::clearPasswordCache() {
  if (mPasswordCache.size() > 0) {
    mPasswordCache.fill('\0');
    mPasswordCache.truncate(0);
  }
}

/** return type should be gpgme_error_t*/

/*
 * if there is no '\n' before the PGP-Begin-Block, but for example a whitespace,
 * GPGME doesn't recognise the Message as encrypted. This function adds '\n'
 * before the PGP-Begin-Block, if missing.
 */
void GpgContext::preventNoDataErr(QByteArray *in) {
  int block_start = in->indexOf(GpgConstants::PGP_CRYPT_BEGIN);
  if (block_start > 0 && in->at(block_start - 1) != '\n') {
    in->insert(block_start, '\n');
  }
  block_start = in->indexOf(GpgConstants::PGP_SIGNED_BEGIN);
  if (block_start > 0 && in->at(block_start - 1) != '\n') {
    in->insert(block_start, '\n');
  }
}

std::string GpgContext::getGpgmeVersion() {
  return {gpgme_check_version(nullptr)};
}

} // namespace GpgFrontend
