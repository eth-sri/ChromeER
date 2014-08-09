// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "components/keyed_service/core/keyed_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

class EasyUnlockService : public KeyedService {
 public:
  explicit EasyUnlockService(Profile* profile);
  virtual ~EasyUnlockService();

  // Gets EasyUnlockService instance.
  static EasyUnlockService* Get(Profile* profile);

  // Registers Easy Unlock profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Launches Easy Unlock Setup app.
  void LaunchSetup();

  // Whether easy unlock is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted either the flag is enabled or its field trial is enabled.
  bool IsAllowed();

 private:
  void Initialize();
  void LoadApp();
  void UnloadApp();
  void OnPrefsChanged();

  Profile* profile_;
  PrefChangeRegistrar registrar_;
  base::WeakPtrFactory<EasyUnlockService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockService);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_SERVICE_H_
