// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/policy_ui_handler.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/policy_ui_utils.h"
#include "chrome/browser/policy/policy_value_and_status_aggregator.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/browser/policy/value_provider/chrome_policies_value_provider.h"
#include "chrome/browser/policy/value_provider/value_provider_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/chromium_strings.h"
#include "components/crx_file/id_util.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/webui/json_generation.h"
#include "components/policy/core/browser/webui/policy_webui_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/policy_scheduler.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/policy/core/common/policy_logger.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/off_hours/device_off_hours_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/user_manager/user_manager.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif

namespace {

// Key under which extension policies are grouped in JSON policy exports.
const char kExtensionsKey[] = "extensions";

}  // namespace

PolicyUIHandler::PolicyUIHandler() = default;

PolicyUIHandler::~PolicyUIHandler() {
  if (export_policies_select_file_dialog_) {
    export_policies_select_file_dialog_->ListenerDestroyed();
  }
}

void PolicyUIHandler::AddCommonLocalizedStringsToSource(
    content::WebUIDataSource* source) {
  source->AddLocalizedStrings(policy::kPolicySources);

  static constexpr webui::LocalizedString kStrings[] = {
    {"conflict", IDS_POLICY_LABEL_CONFLICT},
    {"superseding", IDS_POLICY_LABEL_SUPERSEDING},
    {"conflictValue", IDS_POLICY_LABEL_CONFLICT_VALUE},
    {"supersededValue", IDS_POLICY_LABEL_SUPERSEDED_VALUE},
    {"headerLevel", IDS_POLICY_HEADER_LEVEL},
    {"headerName", IDS_POLICY_HEADER_NAME},
    {"headerScope", IDS_POLICY_HEADER_SCOPE},
    {"headerSource", IDS_POLICY_HEADER_SOURCE},
    {"headerStatus", IDS_POLICY_HEADER_STATUS},
    {"headerValue", IDS_POLICY_HEADER_VALUE},
    {"warning", IDS_POLICY_HEADER_WARNING},
    {"levelMandatory", IDS_POLICY_LEVEL_MANDATORY},
    {"levelRecommended", IDS_POLICY_LEVEL_RECOMMENDED},
    {"error", IDS_POLICY_LABEL_ERROR},
    {"deprecated", IDS_POLICY_LABEL_DEPRECATED},
    {"future", IDS_POLICY_LABEL_FUTURE},
    {"info", IDS_POLICY_LABEL_INFO},
    {"ignored", IDS_POLICY_LABEL_IGNORED},
    {"notSpecified", IDS_POLICY_NOT_SPECIFIED},
    {"ok", IDS_POLICY_OK},
    {"scopeDevice", IDS_POLICY_SCOPE_DEVICE},
    {"scopeUser", IDS_POLICY_SCOPE_USER},
    {"scopeAllUsers", IDS_POLICY_SCOPE_ALL_USERS},
    {"title", IDS_POLICY_TITLE},
    {"unknown", IDS_POLICY_UNKNOWN},
    {"unset", IDS_POLICY_UNSET},
    {"value", IDS_POLICY_LABEL_VALUE},
    {"sourceDefault", IDS_POLICY_SOURCE_DEFAULT},
    {"loadPoliciesDone", IDS_POLICY_LOAD_POLICIES_DONE},
    {"loadingPolicies", IDS_POLICY_LOADING_POLICIES},
#if !BUILDFLAG(IS_CHROMEOS)
    {"reportUploading", IDS_REPORT_UPLOADING},
    {"reportUploaded", IDS_REPORT_UPLOADED},
#endif  // !BUILDFLAG(IS_CHROMEOS)
  };
  source->AddLocalizedStrings(kStrings);

  source->UseStringsJs();
}

void PolicyUIHandler::RegisterMessages() {
  auto update_callback(base::BindRepeating(&PolicyUIHandler::SendStatus,
                                           base::Unretained(this)));
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(g_browser_process->local_state());
  pref_change_registrar_->Add(
      enterprise_reporting::kLastUploadSucceededTimestamp, update_callback);

  policy_value_and_status_aggregator_ = policy::PolicyValueAndStatusAggregator::
      CreateDefaultPolicyValueAndStatusAggregator(Profile::FromWebUI(web_ui()));
  policy_value_and_status_observation_.Observe(
      policy_value_and_status_aggregator_.get());

  web_ui()->RegisterMessageCallback(
      "exportPoliciesJSON",
      base::BindRepeating(&PolicyUIHandler::HandleExportPoliciesJson,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "listenPoliciesUpdates",
      base::BindRepeating(&PolicyUIHandler::HandleListenPoliciesUpdates,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reloadPolicies",
      base::BindRepeating(&PolicyUIHandler::HandleReloadPolicies,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "copyPoliciesJSON",
      base::BindRepeating(&PolicyUIHandler::HandleCopyPoliciesJson,
                          base::Unretained(this)));

#if BUILDFLAG(IS_ANDROID)
  web_ui()->RegisterMessageCallback(
      "getPolicyLogs",
      base::BindRepeating(&PolicyUIHandler::HandleGetPolicyLogs,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "uploadReport", base::BindRepeating(&PolicyUIHandler::HandleUploadReport,
                                          base::Unretained(this)));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

void PolicyUIHandler::OnPolicyValueAndStatusChanged() {
  SendPolicies();
  // Send also the status to UI because when policy value is updated, policy
  // status also might be updated and PolicyStatusProviders may not be listening
  // this change.
  SendStatus();
}

void PolicyUIHandler::FileSelected(const base::FilePath& path,
                                   int index,
                                   void* params) {
  DCHECK(export_policies_select_file_dialog_);

  WritePoliciesToJSONFile(path);

  export_policies_select_file_dialog_ = nullptr;
}

void PolicyUIHandler::FileSelectionCanceled(void* params) {
  DCHECK(export_policies_select_file_dialog_);
  export_policies_select_file_dialog_ = nullptr;
}

void PolicyUIHandler::HandleExportPoliciesJson(const base::Value::List& args) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/1228691): Unify download logic between all platforms to
  // use the WebUI download solution (and remove the Android check).
  if (!IsJavascriptAllowed()) {
    DVLOG(1) << "Tried to export policies as JSON but executing JavaScript is "
                "not allowed.";
    return;
  }

  // Since file selection doesn't work as well on Android as on other platforms,
  // simply download the JSON as a file via JavaScript.
  FireWebUIListener("download-json", base::Value(GetPoliciesAsJson()));
#else
  // If the "select file" dialog window is already opened, we don't want to open
  // it again.
  if (export_policies_select_file_dialog_)
    return;

  content::WebContents* webcontents = web_ui()->GetWebContents();

  // Building initial path based on download preferences.
  base::FilePath initial_dir =
      DownloadPrefs::FromBrowserContext(webcontents->GetBrowserContext())
          ->DownloadPath();
  base::FilePath initial_path =
      initial_dir.Append(FILE_PATH_LITERAL("policies.json"));

  export_policies_select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{FILE_PATH_LITERAL("json")}};
  gfx::NativeWindow owning_window = webcontents->GetTopLevelNativeWindow();
  export_policies_select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(), initial_path,
      &file_type_info, 0, base::FilePath::StringType(), owning_window, nullptr);
#endif
}

void PolicyUIHandler::HandleListenPoliciesUpdates(
    const base::Value::List& args) {
  // Send initial policy values and status to UI page.
  AllowJavascript();
  SendPolicies();
  SendStatus();
}

void PolicyUIHandler::HandleReloadPolicies(const base::Value::List& args) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Allow user to manually fetch remote commands. Useful for testing or when
  // the invalidation service is not working properly.
  policy::CloudPolicyManager* const device_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceCloudPolicyManager();
  Profile* const profile = Profile::FromWebUI(web_ui());
  policy::CloudPolicyManager* const user_manager =
      profile->GetUserCloudPolicyManagerAsh();

  // Fetch both device and user remote commands.
  for (policy::CloudPolicyManager* manager : {device_manager, user_manager}) {
    // Active Directory management has no CloudPolicyManager.
    if (manager) {
      policy::RemoteCommandsService* const remote_commands_service =
          manager->core()->remote_commands_service();
      if (remote_commands_service)
        remote_commands_service->FetchRemoteCommands();
    }
  }
#endif
  policy_value_and_status_aggregator_->Refresh();
}

void PolicyUIHandler::HandleCopyPoliciesJson(const base::Value::List& args) {
  std::string policies_json = GetPoliciesAsJson();
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::UTF8ToUTF16(policies_json));
}

#if BUILDFLAG(IS_ANDROID)
void PolicyUIHandler::HandleGetPolicyLogs(const base::Value::List& args) {
  DCHECK(policy::PolicyLogger::GetInstance()->IsPolicyLoggingEnabled());
  AllowJavascript();
  ResolveJavascriptCallback(args[0],
                            policy::PolicyLogger::GetInstance()->GetAsList());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS)
void PolicyUIHandler::HandleUploadReport(const base::Value::List& args) {
  DCHECK_EQ(1u, args.size());
  std::string callback_id = args[0].GetString();
  auto* report_scheduler = g_browser_process->browser_policy_connector()
                               ->chrome_browser_cloud_management_controller()
                               ->report_scheduler();
  if (report_scheduler) {
    report_scheduler->UploadFullReport(
        base::BindOnce(&PolicyUIHandler::OnReportUploaded,
                       weak_factory_.GetWeakPtr(), callback_id));
  } else {
    OnReportUploaded(callback_id);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

void PolicyUIHandler::SendPolicies() {
  if (!IsJavascriptAllowed())
    return;
  FireWebUIListener(
      "policies-updated",
      base::Value(
          policy_value_and_status_aggregator_->GetAggregatedPolicyNames()),
      base::Value(
          policy_value_and_status_aggregator_->GetAggregatedPolicyValues()));
}

void PolicyUIHandler::SendStatus() {
  if (!IsJavascriptAllowed())
    return;

  FireWebUIListener(
      "status-updated",
      policy_value_and_status_aggregator_->GetAggregatedPolicyStatus());
}

#if !BUILDFLAG(IS_CHROMEOS)
void PolicyUIHandler::OnReportUploaded(const std::string& callback_id) {
  if (!IsJavascriptAllowed())
    return;
  ResolveJavascriptCallback(base::Value(callback_id),
                            /*response=*/base::Value());
  SendStatus();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

std::string PolicyUIHandler::GetPoliciesAsJson() {
  base::Value::Dict policy_values =
      policy_value_and_status_aggregator_->GetAggregatedPolicyValues();
  policy_values.Remove(policy::kPolicyIdsKey);
  base::Value::Dict* extensions_dict =
      policy_values.FindDict(policy::kPolicyValuesKey)
          ->EnsureDict(kExtensionsKey);

  // Iterate through all policy headings to identify extension policies.
  for (auto entry : *policy_values.FindDict(policy::kPolicyValuesKey)) {
    if (crx_file::id_util::IdIsValid(entry.first)) {
      extensions_dict->Set(entry.first, base::Value::Dict());
    }
  }

  // Extract identified extension policies into their own category.
  for (auto entry : *extensions_dict) {
    extensions_dict->Set(entry.first,
                         policy_values.FindDict(policy::kPolicyValuesKey)
                             ->Extract(entry.first)
                             .value_or(base::Value()));
  }

  return policy::GenerateJson(
      std::move(policy_values),
      policy_value_and_status_aggregator_->GetAggregatedPolicyStatus(),
      /*params=*/
      policy::GetChromeMetadataParams(
          /*application_name=*/l10n_util::GetStringUTF8(IDS_PRODUCT_NAME)));
}

void PolicyUIHandler::WritePoliciesToJSONFile(const base::FilePath& path) {
  std::string json_policies = GetPoliciesAsJson();
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(
          [](const base::FilePath& path, base::StringPiece content) {
            base::WriteFile(path, content);
          },
          path, json_policies));
}
