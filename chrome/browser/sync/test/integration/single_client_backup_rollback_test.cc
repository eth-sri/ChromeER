// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "sync/test/fake_server/fake_server_verifier.h"

using bookmarks_helper::AddFolder;
using bookmarks_helper::AddURL;
using bookmarks_helper::GetOtherNode;
using bookmarks_helper::ModelMatchesVerifier;
using bookmarks_helper::Move;
using bookmarks_helper::Remove;
using sync_integration_test_util::AwaitCommitActivityCompletion;

class SingleClientBackupRollbackTest : public SyncTest {
 public:
  SingleClientBackupRollbackTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SingleClientBackupRollbackTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kSyncEnableRollback);
    SyncTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientBackupRollbackTest);
};

class BackupModeChecker {
 public:
  explicit BackupModeChecker(ProfileSyncService* service,
                             base::TimeDelta timeout)
      : pss_(service),
        timeout_(timeout) {}

  bool Wait() {
    expiration_ = base::TimeTicks::Now() + timeout_;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&BackupModeChecker::PeriodicCheck, base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
    base::MessageLoop::current()->Run();
    return IsBackupComplete();
  }

 private:
  void PeriodicCheck() {
    if (IsBackupComplete() || base::TimeTicks::Now() > expiration_) {
      base::MessageLoop::current()->Quit();
    } else {
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&BackupModeChecker::PeriodicCheck, base::Unretained(this)),
          base::TimeDelta::FromSeconds(1));
    }
  }

  bool IsBackupComplete() {
    return pss_->backend_mode() == ProfileSyncService::BACKUP &&
        pss_->ShouldPushChanges();
  }

  ProfileSyncService* pss_;
  base::TimeDelta timeout_;
  base::TimeTicks expiration_;
};

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestBackupRollback TestBackupRollback
#else
#define MAYBE_TestBackupRollback DISABLED_TestBackupRollback
#endif
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestBackupRollback) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  // other_node
  //    -> top
  //      -> tier1_a
  //        -> http://mail.google.com  "tier1_a_url0"
  //      -> tier1_b
  //        -> http://www.nhl.com "tier1_b_url0"
  const BookmarkNode* top = AddFolder(0, GetOtherNode(0), 0, "top");
  const BookmarkNode* tier1_a = AddFolder(0, top, 0, "tier1_a");
  const BookmarkNode* tier1_b = AddFolder(0, top, 1, "tier1_b");
  ASSERT_TRUE(AddURL(0, tier1_a, 0, "tier1_a_url0",
                     GURL("http://mail.google.com")));
  ASSERT_TRUE(AddURL(0, tier1_b, 0, "tier1_b_url0",
                     GURL("http://www.nhl.com")));

  BackupModeChecker checker(GetSyncService(0),
                            base::TimeDelta::FromSeconds(15));
  ASSERT_TRUE(checker.Wait());

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Made bookmark changes while sync is on.
  Move(0, tier1_a->GetChild(0), tier1_b, 1);
  Remove(0, tier1_b, 0);
  ASSERT_TRUE(AddFolder(0, tier1_b, 1, "tier2_c"));
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Let server to return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  Remove(0, tier1_b, 0);

  // Wait for sync to switch to backup mode after finishing rollback.
  ASSERT_TRUE(checker.Wait());

  // Verify bookmarks are restored.
  ASSERT_EQ(1, tier1_a->child_count());
  const BookmarkNode* url1 = tier1_a->GetChild(0);
  ASSERT_EQ(GURL("http://mail.google.com"), url1->url());

  ASSERT_EQ(1, tier1_b->child_count());
  const BookmarkNode* url2 = tier1_b->GetChild(0);
  ASSERT_EQ(GURL("http://www.nhl.com"), url2->url());
}

#if defined(ENABLE_PRE_SYNC_BACKUP)
#define MAYBE_TestPrefBackupRollback TestPrefBackupRollback
#else
#define MAYBE_TestPrefBackupRollback DISABLED_TestPrefBackupRollback
#endif
// Verify local preferences are not affected by preferences in backup DB under
// backup mode.
IN_PROC_BROWSER_TEST_F(SingleClientBackupRollbackTest,
                       MAYBE_TestPrefBackupRollback) {
  const char kUrl1[] = "http://www.google.com";
  const char kUrl2[] = "http://map.google.com";
  const char kUrl3[] = "http://plus.google.com";

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  preferences_helper::ChangeStringPref(0, prefs::kHomePage, kUrl1);

  BackupModeChecker checker(GetSyncService(0),
                            base::TimeDelta::FromSeconds(15));
  ASSERT_TRUE(checker.Wait());

  // Shut down backup, then change preference.
  GetSyncService(0)->StartStopBackupForTesting();
  preferences_helper::ChangeStringPref(0, prefs::kHomePage, kUrl2);

  // Restart backup. Preference shouldn't change after backup starts.
  GetSyncService(0)->StartStopBackupForTesting();
  ASSERT_TRUE(checker.Wait());
  ASSERT_EQ(kUrl2,
            preferences_helper::GetPrefs(0)->GetString(prefs::kHomePage));

  // Start sync and change preference.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  preferences_helper::ChangeStringPref(0, prefs::kHomePage, kUrl3);
  ASSERT_TRUE(AwaitCommitActivityCompletion(GetSyncService((0))));
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Let server return rollback command on next sync request.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::USER_ROLLBACK);

  // Make another change to trigger downloading of rollback command.
  preferences_helper::ChangeStringPref(0, prefs::kHomePage, "");

  // Wait for sync to switch to backup mode after finishing rollback.
  ASSERT_TRUE(checker.Wait());

  // Verify preference is restored.
  ASSERT_EQ(kUrl2,
            preferences_helper::GetPrefs(0)->GetString(prefs::kHomePage));
}
