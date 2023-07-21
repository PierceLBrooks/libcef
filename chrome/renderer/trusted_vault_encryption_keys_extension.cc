// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/trusted_vault_encryption_keys_extension.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "build/buildflag.h"
#include "chrome/common/trusted_vault_encryption_keys_extension.mojom.h"
#include "chrome/renderer/google_accounts_private_api_util.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/function_template.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace {

// This function is intended to convert a binary blob representing an encryption
// key and provided by the web via a Javascript ArrayBuffer.
std::vector<uint8_t> ArrayBufferAsBytes(
    const v8::Local<v8::ArrayBuffer>& array_buffer) {
  auto backing_store = array_buffer->GetBackingStore();
  const uint8_t* start =
      reinterpret_cast<const uint8_t*>(backing_store->Data());
  const size_t length = backing_store->ByteLength();
  return std::vector<uint8_t>(start, start + length);
}

#if !BUILDFLAG(IS_ANDROID)
// Converts a vector of raw encryption key bytes for the chromesync domain to
// TrustedVaultKey mojo structs. Because for chromesync keys passed via the
// `chrome.setSyncEncryptionKeys()` JS API, we only receive the key version of
// the *last* key in the array, only the version of the last TrustedVaultKey
// will be initialized correctly.
std::vector<chrome::mojom::TrustedVaultKeyPtr>
SyncEncryptionKeysToTrustedVaultKeys(
    const std::vector<v8::Local<v8::ArrayBuffer>>& encryption_keys,
    int32_t last_key_version) {
  std::vector<chrome::mojom::TrustedVaultKeyPtr> trusted_vault_keys;
  for (const v8::Local<v8::ArrayBuffer>& encryption_key : encryption_keys) {
    // chrome.setSyncEncryptionKeys() only passes the last key's version, so we
    // set all the other versions to -1. The remaining version numbers will be
    // ignored by the sync service.
    const bool last_key =
        encryption_keys.size() == trusted_vault_keys.size() - 1;
    trusted_vault_keys.push_back(chrome::mojom::TrustedVaultKey::New(
        /*version=*/last_key ? last_key_version : -1,
        /*bytes=*/ArrayBufferAsBytes(encryption_key)));
  }
  return trusted_vault_keys;
}

bool ParseTrustedVaultKeyArray(
    v8::Local<v8::Context> context,
    v8::Local<v8::Array> array,
    std::vector<chrome::mojom::TrustedVaultKeyPtr>* trusted_vault_keys) {
  DCHECK(trusted_vault_keys);
  for (uint32_t i = 0; i < array->Length(); ++i) {
    v8::Local<v8::Value> value;
    if (!array->Get(context, i).ToLocal(&value) || !value->IsObject()) {
      DVLOG(1) << "invalid key object";
      return false;
    }
    v8::Local<v8::Object> obj = value.As<v8::Object>();
    v8::Local<v8::Value> version_value;
    if (!obj->Get(context, gin::StringToV8(context->GetIsolate(), "version"))
             .ToLocal(&version_value) ||
        !version_value->IsInt32()) {
      DVLOG(1) << "invalid key version";
      return false;
    }
    const int32_t version = version_value.As<v8::Int32>()->Value();

    v8::Local<v8::Value> bytes_value;
    if (!obj->Get(context, gin::StringToV8(context->GetIsolate(), "bytes"))
             .ToLocal(&bytes_value) ||
        !bytes_value->IsArrayBuffer()) {
      DVLOG(1) << "invalid key bytes";
      return false;
    }
    std::vector<uint8_t> bytes =
        ArrayBufferAsBytes(bytes_value.As<v8::ArrayBuffer>());
    trusted_vault_keys->push_back(
        chrome::mojom::TrustedVaultKey::New(version, std::move(bytes)));
  }
  return true;
}

bool ParseObjectToTrustedVaultKeysMap(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    base::flat_map<std::string, std::vector<chrome::mojom::TrustedVaultKeyPtr>>*
        trusted_vault_keys) {
  std::vector<
      std::pair<std::string, std::vector<chrome::mojom::TrustedVaultKeyPtr>>>
      result;
  v8::Local<v8::Array> own_property_names =
      object->GetOwnPropertyNames(context).ToLocalChecked();
  for (uint32_t i = 0; i < own_property_names->Length(); ++i) {
    v8::Local<v8::Value> key;
    if (!own_property_names->Get(context, i).ToLocal(&key) ||
        !key->IsString()) {
      DVLOG(1) << "invalid map key";
      return false;
    }
    const std::string security_domain_name(
        *v8::String::Utf8Value(context->GetIsolate(), key));

    v8::Local<v8::Value> prop_value;
    if (!object->Get(context, key).ToLocal(&prop_value) ||
        !prop_value->IsArray()) {
      DVLOG(1) << "invalid map value";
      return false;
    }
    std::vector<chrome::mojom::TrustedVaultKeyPtr> domain_keys;
    if (!ParseTrustedVaultKeyArray(context, prop_value.As<v8::Array>(),
                                   &domain_keys)) {
      DVLOG(1) << "parsing vault keys failed";
      return false;
    }
    result.emplace_back(std::move(security_domain_name),
                        std::move(domain_keys));
  }
  *trusted_vault_keys =
      base::flat_map<std::string,
                     std::vector<chrome::mojom::TrustedVaultKeyPtr>>(
          std::move(result));
  return true;
}

void RecordCallToSetSyncEncryptionKeysToUma(bool valid_args) {
  base::UmaHistogramBoolean(
      "Sync.TrustedVaultJavascriptSetEncryptionKeysValidArgs", valid_args);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(bool valid_args) {
  base::UmaHistogramBoolean(
      "Sync.TrustedVaultJavascriptAddRecoveryMethodValidArgs", valid_args);
}

}  // namespace

// static
void TrustedVaultEncryptionKeysExtension::Create(content::RenderFrame* frame) {
  new TrustedVaultEncryptionKeysExtension(frame);
}

TrustedVaultEncryptionKeysExtension::TrustedVaultEncryptionKeysExtension(
    content::RenderFrame* frame)
    : content::RenderFrameObserver(frame) {}

TrustedVaultEncryptionKeysExtension::~TrustedVaultEncryptionKeysExtension() {}

void TrustedVaultEncryptionKeysExtension::OnDestruct() {
  delete this;
}

void TrustedVaultEncryptionKeysExtension::DidCreateScriptContext(
    v8::Local<v8::Context> v8_context,
    int32_t world_id) {
  if (!render_frame() || world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }

  if (ShouldExposeGoogleAccountsJavascriptApi(render_frame())) {
    Install();
  }
}

void TrustedVaultEncryptionKeysExtension::Install() {
  DCHECK(render_frame());

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);

  // On Android, there is no existing plumbing for setSyncEncryptionKeys() and
  // setClientEncryptionKeys(), so let's not expose the Javascript function as
  // available. Namely, TrustedVaultClientAndroid::StoreKeys() isn't implemented
  // because there is no underlying Android API to invoke, given that sign in
  // and reauth flows are handled outside the browser.
#if !BUILDFLAG(IS_ANDROID)
  chrome
      ->Set(context, gin::StringToSymbol(isolate, "setSyncEncryptionKeys"),
            gin::CreateFunctionTemplate(
                isolate,
                base::BindRepeating(
                    &TrustedVaultEncryptionKeysExtension::SetSyncEncryptionKeys,
                    weak_ptr_factory_.GetWeakPtr()))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();

  if (base::FeatureList::IsEnabled(
          trusted_vault::kSetClientEncryptionKeysJsApi)) {
    chrome
        ->Set(context, gin::StringToSymbol(isolate, "setClientEncryptionKeys"),
              gin::CreateFunctionTemplate(
                  isolate,
                  base::BindRepeating(&TrustedVaultEncryptionKeysExtension::
                                          SetClientEncryptionKeys,
                                      weak_ptr_factory_.GetWeakPtr()))
                  ->GetFunction(context)
                  .ToLocalChecked())
        .Check();
  }
#endif

  chrome
      ->Set(context,
            gin::StringToSymbol(isolate,
                                "addTrustedSyncEncryptionRecoveryMethod"),
            gin::CreateFunctionTemplate(
                isolate,
                base::BindRepeating(&TrustedVaultEncryptionKeysExtension::
                                        AddTrustedSyncEncryptionRecoveryMethod,
                                    weak_ptr_factory_.GetWeakPtr()))
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
}

#if !BUILDFLAG(IS_ANDROID)
void TrustedVaultEncryptionKeysExtension::SetSyncEncryptionKeys(
    gin::Arguments* args) {
  DCHECK(render_frame());

  // This function as exposed to the web has the following signature:
  //   setSyncEncryptionKeys(callback, gaia_id, encryption_keys,
  //                         last_key_version)
  //
  // Where:
  //   callback: Allows caller to get notified upon completion.
  //   gaia_id: String representing the user's server-provided ID.
  //   encryption_keys: Array where each element is an ArrayBuffer representing
  //                    an encryption key (binary blob).
  //   last_key_version: Key version corresponding to the last key in
  //                     |encryption_keys|.

  v8::HandleScope handle_scope(args->isolate());

  v8::Local<v8::Function> callback;
  if (!args->GetNext(&callback)) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No callback";
    args->ThrowError();
    return;
  }

  std::string gaia_id;
  if (!args->GetNext(&gaia_id)) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No account ID";
    args->ThrowError();
    return;
  }

  std::vector<v8::Local<v8::ArrayBuffer>> encryption_keys;
  if (!args->GetNext(&encryption_keys)) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "Not array of strings";
    args->ThrowError();
    return;
  }

  if (encryption_keys.empty()) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "Array of strings empty";
    args->ThrowError();
    return;
  }

  int last_key_version = 0;
  if (!args->GetNext(&last_key_version)) {
    RecordCallToSetSyncEncryptionKeysToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No version provided";
    args->ThrowError();
    return;
  }

  auto global_callback =
      std::make_unique<v8::Global<v8::Function>>(args->isolate(), callback);

  if (!remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }

  RecordCallToSetSyncEncryptionKeysToUma(/*valid_args=*/true);

  std::vector<
      std::pair<std::string, std::vector<chrome::mojom::TrustedVaultKeyPtr>>>
      trusted_vault_keys;
  trusted_vault_keys.emplace_back(
      trusted_vault::kSyncSecurityDomainName,
      SyncEncryptionKeysToTrustedVaultKeys(encryption_keys, last_key_version));
  remote_->SetEncryptionKeys(
      gaia_id, std::move(trusted_vault_keys),
      base::BindOnce(
          &TrustedVaultEncryptionKeysExtension::RunCompletionCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(global_callback)));
}

void TrustedVaultEncryptionKeysExtension::SetClientEncryptionKeys(
    gin::Arguments* args) {
  DCHECK(render_frame());

  // This function as exposed to the web has the following signature:
  //   setClientEncryptionKeys(callback, gaia_id, encryption_keys);
  //
  // Where:
  //   callback: Allows caller to get notified upon completion.
  //   gaia_id: String representing the user's server-provided ID.
  //   encryption_keys: A map of security domain name string => array of
  //                    TrustedVaultKey, with members `version` integer and
  //                    `bytes` encryption key blob.

  v8::HandleScope handle_scope(args->isolate());

  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  v8::Local<v8::Function> callback;
  if (!args->GetNext(&callback)) {
    // TODO(https://crbug.com/1223853): Add SetClientEncryptionKeys UMA metric.
    DLOG(ERROR) << "No callback";
    args->ThrowError();
    return;
  }

  std::string gaia_id;
  if (!args->GetNext(&gaia_id)) {
    // TODO(https://crbug.com/1223853): Add SetClientEncryptionKeys UMA metric.
    DLOG(ERROR) << "No account ID";
    args->ThrowError();
    return;
  }

  v8::Local<v8::Object> encryption_keys;
  if (!args->GetNext(&encryption_keys)) {
    // TODO(https://crbug.com/1223853): Add SetClientEncryptionKeys UMA metric.
    DLOG(ERROR) << "No encryption keys object";
    args->ThrowError();
    return;
  }

  base::flat_map<std::string, std::vector<chrome::mojom::TrustedVaultKeyPtr>>
      trusted_vault_keys;
  if (!ParseObjectToTrustedVaultKeysMap(context, encryption_keys,
                                        &trusted_vault_keys)) {
    DLOG(ERROR) << "Can't parse encryption keys object";
    args->ThrowError();
    return;
  }

  if (!remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }

  // TODO(https://crbug.com/1223853): Add SetClientEncryptionKeys UMA metric.

  remote_->SetEncryptionKeys(
      gaia_id, std::move(trusted_vault_keys),
      base::BindOnce(
          &TrustedVaultEncryptionKeysExtension::RunCompletionCallback,
          weak_ptr_factory_.GetWeakPtr(),
          std::make_unique<v8::Global<v8::Function>>(args->isolate(),
                                                     callback)));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void TrustedVaultEncryptionKeysExtension::
    AddTrustedSyncEncryptionRecoveryMethod(gin::Arguments* args) {
  DCHECK(render_frame());

  // This function as exposed to the web has the following signature:
  //   addTrustedSyncEncryptionRecoveryMethod(callback, gaia_id, public_key,
  //                                          method_type_hint)
  //
  // Where:
  //   callback: Allows caller to get notified upon completion.
  //   gaia_id: String representing the user's server-provided ID.
  //   public_key: A public key representing the recovery method to be added.
  //   method_type_hint: An enum-like integer representing the added method's
  //   type. This value is opaque to the client and may only be used for
  //   future related interactions with the server.

  v8::HandleScope handle_scope(args->isolate());

  v8::Local<v8::Function> callback;
  if (!args->GetNext(&callback)) {
    RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No callback";
    args->ThrowError();
    return;
  }

  std::string gaia_id;
  if (!args->GetNext(&gaia_id)) {
    RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No account ID";
    args->ThrowError();
    return;
  }

  v8::Local<v8::ArrayBuffer> public_key;
  if (!args->GetNext(&public_key)) {
    RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No public key";
    args->ThrowError();
    return;
  }

  int method_type_hint = 0;
  if (!args->GetNext(&method_type_hint)) {
    RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(
        /*valid_args=*/false);
    DLOG(ERROR) << "No method type hint";
    args->ThrowError();
    return;
  }

  auto global_callback =
      std::make_unique<v8::Global<v8::Function>>(args->isolate(), callback);

  if (!remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(&remote_);
  }

  RecordCallToAddTrustedSyncEncryptionRecoveryMethodToUma(/*valid_args=*/true);
  remote_->AddTrustedRecoveryMethod(
      gaia_id, ArrayBufferAsBytes(public_key), method_type_hint,
      base::BindOnce(
          &TrustedVaultEncryptionKeysExtension::RunCompletionCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(global_callback)));
}

void TrustedVaultEncryptionKeysExtension::RunCompletionCallback(
    std::unique_ptr<v8::Global<v8::Function>> callback) {
  if (!render_frame()) {
    return;
  }

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Function> callback_local =
      v8::Local<v8::Function>::New(isolate, *callback);

  render_frame()->GetWebFrame()->CallFunctionEvenIfScriptDisabled(
      callback_local, v8::Undefined(isolate), /*argc=*/0, /*argv=*/{});
}
