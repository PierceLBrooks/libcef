// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/close_file.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/operations/test_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/file_system/async_file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace file_system_provider {
namespace operations {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "testing-file-system";
const int kRequestId = 2;
const int kOpenRequestId = 3;

}  // namespace

class FileSystemProviderOperationsCloseFileTest : public testing::Test {
 protected:
  FileSystemProviderOperationsCloseFileTest() {}
  ~FileSystemProviderOperationsCloseFileTest() override {}

  void SetUp() override {
    file_system_info_ = ProvidedFileSystemInfo(
        kExtensionId, MountOptions(kFileSystemId, "" /* display_name */),
        base::FilePath(), false /* configurable */, true /* watchable */,
        extensions::SOURCE_FILE, IconSet());
  }

  ProvidedFileSystemInfo file_system_info_;
};

TEST_F(FileSystemProviderOperationsCloseFileTest, Execute) {
  using extensions::api::file_system_provider::CloseFileRequestedOptions;

  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  CloseFile close_file(&dispatcher, file_system_info_, kOpenRequestId,
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(close_file.Execute(kRequestId));

  ASSERT_EQ(1u, dispatcher.events().size());
  extensions::Event* event = dispatcher.events()[0].get();
  EXPECT_EQ(
      extensions::api::file_system_provider::OnCloseFileRequested::kEventName,
      event->event_name);
  const base::Value::List& event_args = event->event_args;
  ASSERT_EQ(1u, event_args.size());

  const base::Value* options_as_value = &event_args[0];
  ASSERT_TRUE(options_as_value->is_dict());

  CloseFileRequestedOptions options;
  ASSERT_TRUE(CloseFileRequestedOptions::Populate(options_as_value->GetDict(),
                                                  options));
  EXPECT_EQ(kFileSystemId, options.file_system_id);
  EXPECT_EQ(kRequestId, options.request_id);
  EXPECT_EQ(kOpenRequestId, options.open_request_id);
}

TEST_F(FileSystemProviderOperationsCloseFileTest, Execute_NoListener) {
  util::LoggingDispatchEventImpl dispatcher(false /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  CloseFile close_file(&dispatcher, file_system_info_, kOpenRequestId,
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_FALSE(close_file.Execute(kRequestId));
}

TEST_F(FileSystemProviderOperationsCloseFileTest, OnSuccess) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  CloseFile close_file(&dispatcher, file_system_info_, kOpenRequestId,
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(close_file.Execute(kRequestId));

  close_file.OnSuccess(kRequestId, RequestValue(), false /* has_more */);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_OK, callback_log[0]);
}

TEST_F(FileSystemProviderOperationsCloseFileTest, OnError) {
  util::LoggingDispatchEventImpl dispatcher(true /* dispatch_reply */);
  util::StatusCallbackLog callback_log;

  CloseFile close_file(&dispatcher, file_system_info_, kOpenRequestId,
                       base::BindOnce(&util::LogStatusCallback, &callback_log));

  EXPECT_TRUE(close_file.Execute(kRequestId));

  close_file.OnError(kRequestId, RequestValue(),
                     base::File::FILE_ERROR_TOO_MANY_OPENED);
  ASSERT_EQ(1u, callback_log.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED, callback_log[0]);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace ash
