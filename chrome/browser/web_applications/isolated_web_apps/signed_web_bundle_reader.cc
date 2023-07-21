// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {
namespace {
// This is blocking operation.
base::expected<uint64_t, base::File::Error> ReadLengthOfSharedFile(
    scoped_refptr<web_package::SharedFile> file) {
  int64_t length = (*file)->GetLength();
  if (length < 0) {
    return base::unexpected((*file)->GetLastFileError());
  }
  return static_cast<uint64_t>(length);
}
}  // namespace

namespace internal {

SafeWebBundleParserConnection::SafeWebBundleParserConnection(
    base::FilePath web_bundle_path,
    absl::optional<GURL> base_url)
    : web_bundle_path_(std::move(web_bundle_path)),
      base_url_(std::move(base_url)) {}

SafeWebBundleParserConnection::~SafeWebBundleParserConnection() = default;

void SafeWebBundleParserConnection::Initialize(
    InitCompleteCallback init_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kUninitialized);
  state_ = State::kInitializing;

  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>(base_url_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& file_path) -> std::unique_ptr<base::File> {
            return std::make_unique<base::File>(
                file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
          },
          web_bundle_path_),
      base::BindOnce(&SafeWebBundleParserConnection::OnFileOpened,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(init_complete_callback)));
}

void SafeWebBundleParserConnection::StartProcessingDisconnects() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kConnected);
  parser_->SetDisconnectCallback(
      base::BindOnce(&SafeWebBundleParserConnection::OnParserDisconnected,
                     // `base::Unretained` is okay to use here, since
                     // `parser_` will be deleted before `this` is deleted.
                     base::Unretained(this)));
}

void SafeWebBundleParserConnection::OnFileOpened(
    InitCompleteCallback init_complete_callback,
    std::unique_ptr<base::File> file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  if (!file->IsValid()) {
    UnusableSwbnFileError error = UnusableSwbnFileError(
        UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError,
        base::File::ErrorToString(file->error_details()));
    std::move(init_complete_callback).Run(base::unexpected(error));
    return;
  }

  file_ = base::MakeRefCounted<web_package::SharedFile>(std::move(file));
  file_->DuplicateFile(base::BindOnce(
      &SafeWebBundleParserConnection::OnFileDuplicated,
      weak_ptr_factory_.GetWeakPtr(), std::move(init_complete_callback)));
}

void SafeWebBundleParserConnection::OnFileDuplicated(
    InitCompleteCallback init_complete_callback,
    base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  base::File::Error file_error = parser_->OpenFile(std::move(file));
  if (file_error != base::File::FILE_OK) {
    UnusableSwbnFileError error = UnusableSwbnFileError(
        UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError,
        base::File::ErrorToString(file_error));
    std::move(init_complete_callback).Run(base::unexpected(error));
    return;
  }

  state_ = State::kConnected;
  std::move(init_complete_callback).Run(base::ok());
}

void SafeWebBundleParserConnection::OnParserDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kConnected);

  state_ = State::kDisconnected;
  parser_ = nullptr;
  if (!parser_disconnect_callback_for_testing_.is_null()) {
    CHECK_IS_TEST();
    parser_disconnect_callback_for_testing_.Run();
  }
}

void SafeWebBundleParserConnection::Reconnect(
    ReconnectCompleteCallback reconnect_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!parser_);
  CHECK_EQ(state_, State::kDisconnected);
  parser_ = std::make_unique<data_decoder::SafeWebBundleParser>(base_url_);
  state_ = State::kReconnecting;

  file_->DuplicateFile(base::BindOnce(
      &SafeWebBundleParserConnection::ReconnectForFile,
      weak_ptr_factory_.GetWeakPtr(), std::move(reconnect_callback)));
}

void SafeWebBundleParserConnection::ReconnectForFile(
    ReconnectCompleteCallback reconnect_callback,
    base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kReconnecting);
  base::File::Error file_error;
  if (reconnection_file_error_for_testing_.has_value()) {
    CHECK_IS_TEST();
    file_error = *reconnection_file_error_for_testing_;
  } else {
    file_error = parser_->OpenFile(std::move(file));
  }

  base::expected<void, std::string> status;
  if (file_error != base::File::FILE_OK) {
    state_ = State::kDisconnected;
    status = base::unexpected(base::File::ErrorToString(file_error));
  } else {
    state_ = State::kConnected;
    StartProcessingDisconnects();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(reconnect_callback), std::move(status)));
}

}  // namespace internal

SignedWebBundleReader::SignedWebBundleReader(
    const base::FilePath& web_bundle_path,
    const absl::optional<GURL>& base_url,
    std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
        signature_verifier)
    : signature_verifier_(std::move(signature_verifier)),
      connection_(std::make_unique<internal::SafeWebBundleParserConnection>(
          web_bundle_path,
          base_url)) {}

SignedWebBundleReader::~SignedWebBundleReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<SignedWebBundleReader> SignedWebBundleReader::Create(
    const base::FilePath& web_bundle_path,
    const absl::optional<GURL>& base_url,
    std::unique_ptr<web_package::SignedWebBundleSignatureVerifier>
        signature_verifier) {
  return base::WrapUnique(new SignedWebBundleReader(
      web_bundle_path, base_url, std::move(signature_verifier)));
}

void SignedWebBundleReader::StartReading(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kUninitialized);

  state_ = State::kInitializing;
  connection_->Initialize(
      base::BindOnce(&SignedWebBundleReader::OnConnectionInitialized,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(integrity_block_result_callback),
                     std::move(read_error_callback)));
}

void SignedWebBundleReader::OnConnectionInitialized(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback,
    base::expected<void, UnusableSwbnFileError> init_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  if (!init_status.has_value()) {
    FulfillWithError(std::move(read_error_callback),
                     std::move(init_status.error()));
    return;
  }

  connection_->parser_->ParseIntegrityBlock(
      base::BindOnce(&SignedWebBundleReader::OnIntegrityBlockParsed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(integrity_block_result_callback),
                     std::move(read_error_callback)));
}

void SignedWebBundleReader::OnIntegrityBlockParsed(
    IntegrityBlockReadResultCallback integrity_block_result_callback,
    ReadErrorCallback read_error_callback,
    web_package::mojom::BundleIntegrityBlockPtr raw_integrity_block,
    web_package::mojom::BundleIntegrityBlockParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  auto integrity_block =
      [&]() -> base::expected<web_package::SignedWebBundleIntegrityBlock,
                              UnusableSwbnFileError> {
    if (error) {
      return base::unexpected(UnusableSwbnFileError(std::move(error)));
    }
    return web_package::SignedWebBundleIntegrityBlock::Create(
               std::move(raw_integrity_block))
        .transform_error([&](std::string error) {
          return UnusableSwbnFileError(
              UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError,
              "Error while parsing the Signed Web Bundle's integrity block: " +
                  std::move(error));
        });
  }();
  if (!integrity_block.has_value()) {
    FulfillWithError(std::move(read_error_callback),
                     std::move(integrity_block.error()));
    return;
  }

  integrity_block_size_in_bytes_ = integrity_block->size_in_bytes();

  std::move(integrity_block_result_callback)
      .Run(*integrity_block,
           base::BindOnce(&SignedWebBundleReader::
                              OnShouldContinueParsingAfterIntegrityBlock,
                          weak_ptr_factory_.GetWeakPtr(), *integrity_block,
                          std::move(read_error_callback)));
}

void SignedWebBundleReader::OnShouldContinueParsingAfterIntegrityBlock(
    web_package::SignedWebBundleIntegrityBlock integrity_block,
    ReadErrorCallback callback,
    SignatureVerificationAction action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  switch (action.type()) {
    case SignatureVerificationAction::Type::kAbort:
      FulfillWithError(
          std::move(callback),
          UnusableSwbnFileError(
              UnusableSwbnFileError::Error::kIntegrityBlockValidationError,
              action.abort_message()));
      return;
    case SignatureVerificationAction::Type::kContinueAndVerifySignatures:
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(ReadLengthOfSharedFile, connection_->file_),
          base::BindOnce(&SignedWebBundleReader::OnFileLengthRead,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(integrity_block), std::move(callback)));
      return;
    case SignatureVerificationAction::Type::
        kContinueAndSkipSignatureVerification:
      ReadMetadata(std::move(callback));
      return;
  }
}

void SignedWebBundleReader::OnFileLengthRead(
    web_package::SignedWebBundleIntegrityBlock integrity_block,
    ReadErrorCallback callback,
    base::expected<uint64_t, base::File::Error> file_length) {
  if (!file_length.has_value()) {
    UnusableSwbnFileError error = UnusableSwbnFileError(
        UnusableSwbnFileError::Error::kIntegrityBlockParserInternalError,
        base::File::ErrorToString(file_length.error()));
    FulfillWithError(std::move(callback), std::move(error));
    return;
  }

  signature_verifier_->VerifySignatures(
      connection_->file_, std::move(integrity_block),
      base::BindOnce(&SignedWebBundleReader::OnSignaturesVerified,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     *file_length, std::move(callback)));
}

void SignedWebBundleReader::OnSignaturesVerified(
    const base::TimeTicks& verification_start_time,
    uint64_t file_length,
    ReadErrorCallback callback,
    absl::optional<web_package::SignedWebBundleSignatureVerifier::Error>
        verification_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  base::UmaHistogramMediumTimes(
      "WebApp.Isolated.SignatureVerificationDuration",
      base::TimeTicks::Now() - verification_start_time);
  // Measure file length in MiB up to ~10GiB.
  base::UmaHistogramCounts10000(
      "WebApp.Isolated.SignatureVerificationFileLength",
      base::saturated_cast<int>(std::round(file_length / (1024.0 * 1024.0))));

  if (verification_error.has_value()) {
    FulfillWithError(std::move(callback),
                     UnusableSwbnFileError(*verification_error));
    return;
  }

  // Signatures are valid; continue with parsing of metadata.
  ReadMetadata(std::move(callback));
}

void SignedWebBundleReader::ReadMetadata(ReadErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  CHECK(integrity_block_size_in_bytes_.has_value())
      << "The integrity block must have been read before reading metadata.";
  uint64_t metadata_offset = integrity_block_size_in_bytes_.value();

  connection_->parser_->ParseMetadata(
      metadata_offset,
      base::BindOnce(&SignedWebBundleReader::OnMetadataParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignedWebBundleReader::OnMetadataParsed(
    ReadErrorCallback callback,
    web_package::mojom::BundleMetadataPtr metadata,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitializing);

  if (error) {
    FulfillWithError(std::move(callback), UnusableSwbnFileError(error));
    return;
  }

  primary_url_ = metadata->primary_url;
  entries_ = std::move(metadata->requests);

  state_ = State::kInitialized;

  connection_->StartProcessingDisconnects();

  std::move(callback).Run(base::ok());
}

void SignedWebBundleReader::FulfillWithError(ReadErrorCallback callback,
                                             UnusableSwbnFileError error) {
  state_ = State::kError;

  // This is an irrecoverable error state, thus we can safely delete
  // `connection_` here to free up resources.
  connection_.reset();

  std::move(callback).Run(base::unexpected(std::move(error)));
}

const absl::optional<GURL>& SignedWebBundleReader::GetPrimaryURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  return primary_url_;
}

std::vector<GURL> SignedWebBundleReader::GetEntries() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  std::vector<GURL> entries;
  entries.reserve(entries_.size());
  base::ranges::transform(entries_, std::back_inserter(entries),
                          [](const auto& entry) { return entry.first; });
  return entries;
}

void SignedWebBundleReader::ReadResponse(
    const network::ResourceRequest& resource_request,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  const GURL& url = net::SimplifyUrlForRequest(resource_request.url);
  auto entry_it = entries_.find(url);
  if (entry_it == entries_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(ReadResponseError::ForResponseNotFound(
                           "The Web Bundle does not contain a response for the "
                           "provided URL: " +
                           url.spec()))));
    return;
  }

  auto response_location = entry_it->second->Clone();
  if (connection_->is_disconnected()) {
    // Try reconnecting the parser if it hasn't been attempted yet.
    if (pending_read_responses_.empty()) {
      connection_->Reconnect(base::BindOnce(&SignedWebBundleReader::OnReconnect,
                                            base::Unretained(this)));
    }
    pending_read_responses_.emplace_back(std::move(response_location),
                                         std::move(callback));
    return;
  }

  ReadResponseInternal(std::move(response_location), std::move(callback));
}

void SignedWebBundleReader::OnReconnect(
    base::expected<void, std::string> status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::pair<web_package::mojom::BundleResponseLocationPtr,
                        ResponseCallback>>
      read_tasks;
  read_tasks.swap(pending_read_responses_);

  for (auto& [response_location, response_callback] : read_tasks) {
    if (!status.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(response_callback),
              base::unexpected(ReadResponseError::ForParserInternalError(
                  "Unable to open file: " + status.error()))));
    } else {
      ReadResponseInternal(std::move(response_location),
                           std::move(response_callback));
    }
  }
}

void SignedWebBundleReader::ReadResponseInternal(
    web_package::mojom::BundleResponseLocationPtr location,
    ResponseCallback callback) {
  CHECK_EQ(state_, State::kInitialized);

  connection_->parser_->ParseResponse(
      location->offset, location->length,
      base::BindOnce(&SignedWebBundleReader::OnResponseParsed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignedWebBundleReader::OnResponseParsed(
    ResponseCallback callback,
    web_package::mojom::BundleResponsePtr response,
    web_package::mojom::BundleResponseParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  if (error) {
    std::move(callback).Run(base::unexpected(
        ReadResponseError::FromBundleParseError(std::move(error))));
  } else {
    std::move(callback).Run(std::move(response));
  }
}

void SignedWebBundleReader::ReadResponseBody(
    web_package::mojom::BundleResponsePtr response,
    mojo::ScopedDataPipeProducerHandle producer_handle,
    ResponseBodyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kInitialized);

  auto data_producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  mojo::DataPipeProducer* raw_producer = data_producer.get();
  raw_producer->Write(
      connection_->file_->CreateDataSource(response->payload_offset,
                                           response->payload_length),
      base::BindOnce(
          // `producer` is passed to this callback purely for its lifetime
          // management so that it is deleted once this callback runs.
          [](std::unique_ptr<mojo::DataPipeProducer> producer,
             MojoResult result) -> net::Error {
            return result == MOJO_RESULT_OK ? net::Error::OK
                                            : net::Error::ERR_UNEXPECTED;
          },
          std::move(data_producer))
          .Then(std::move(callback)));
}

base::WeakPtr<SignedWebBundleReader> SignedWebBundleReader::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SignedWebBundleReader::SetParserDisconnectCallbackForTesting(
    base::RepeatingClosure callback) {
  connection_->parser_disconnect_callback_for_testing_ = std::move(callback);
}

void SignedWebBundleReader::SetReconnectionFileErrorForTesting(
    base::File::Error file_error) {
  connection_->reconnection_file_error_for_testing_ = file_error;
}

// static
SignedWebBundleReader::ReadResponseError
SignedWebBundleReader::ReadResponseError::FromBundleParseError(
    web_package::mojom::BundleResponseParseErrorPtr error) {
  switch (error->type) {
    case web_package::mojom::BundleParseErrorType::kVersionError:
      // A `kVersionError` error can only be triggered while parsing
      // the integrity block or metadata, not while parsing a response.
      NOTREACHED();
      [[fallthrough]];
    case web_package::mojom::BundleParseErrorType::kParserInternalError:
      return ForParserInternalError(error->message);
    case web_package::mojom::BundleParseErrorType::kFormatError:
      return ReadResponseError(Type::kFormatError, error->message);
  }
}

// static
SignedWebBundleReader::ReadResponseError
SignedWebBundleReader::ReadResponseError::ForParserInternalError(
    const std::string& message) {
  return ReadResponseError(Type::kParserInternalError, message);
}

// static
SignedWebBundleReader::ReadResponseError
SignedWebBundleReader::ReadResponseError::ForResponseNotFound(
    const std::string& message) {
  return ReadResponseError(Type::kResponseNotFound, message);
}

// static
SignedWebBundleReader::SignatureVerificationAction
SignedWebBundleReader::SignatureVerificationAction::Abort(
    const std::string& abort_message) {
  return SignatureVerificationAction(Type::kAbort, abort_message);
}

// static
SignedWebBundleReader::SignatureVerificationAction SignedWebBundleReader::
    SignatureVerificationAction::ContinueAndVerifySignatures() {
  return SignatureVerificationAction(Type::kContinueAndVerifySignatures,
                                     absl::nullopt);
}

// static
SignedWebBundleReader::SignatureVerificationAction SignedWebBundleReader::
    SignatureVerificationAction::ContinueAndSkipSignatureVerification() {
  return SignatureVerificationAction(
      Type::kContinueAndSkipSignatureVerification, absl::nullopt);
}

SignedWebBundleReader::SignatureVerificationAction::SignatureVerificationAction(
    Type type,
    absl::optional<std::string> abort_message)
    : type_(type), abort_message_(abort_message) {}

SignedWebBundleReader::SignatureVerificationAction::SignatureVerificationAction(
    const SignatureVerificationAction&) = default;

SignedWebBundleReader::SignatureVerificationAction::
    ~SignatureVerificationAction() = default;

UnsecureReader::UnsecureReader(const base::FilePath& web_bundle_path)
    : connection_(web_bundle_path, /*base_url=*/absl::nullopt) {}

UnsecureReader::~UnsecureReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UnsecureReader::StartReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  connection_.Initialize(
      base::BindOnce(&UnsecureReader::OnConnectionInitialized, GetWeakPtr()));
}

void UnsecureReader::OnConnectionInitialized(
    base::expected<void, UnusableSwbnFileError> init_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!init_status.has_value()) {
    ReturnError(init_status.error());
    return;
  }

  DoReading();
}

// static
void UnsecureSignedWebBundleIdReader::GetWebBundleId(
    const base::FilePath& web_bundle_path,
    WebBundleIdCallback result_callback) {
  std::unique_ptr<UnsecureSignedWebBundleIdReader> reader =
      base::WrapUnique(new UnsecureSignedWebBundleIdReader(web_bundle_path));
  UnsecureSignedWebBundleIdReader* const reader_raw_ptr = reader.get();

  // We pass the owning unique_ptr to the second no-op callback to keep
  // the instance of UnsecureSignedWebBundleIdReader alive.
  WebBundleIdCallback id_read_callback =
      base::BindOnce(std::move(result_callback))
          .Then(base::BindOnce(
              [](std::unique_ptr<UnsecureSignedWebBundleIdReader> owning_ptr) {
              },
              std::move(reader)));

  reader_raw_ptr->SetResultCallback(std::move(id_read_callback));
  reader_raw_ptr->StartReading();
}

UnsecureSignedWebBundleIdReader::UnsecureSignedWebBundleIdReader(
    const base::FilePath& web_bundle_path)
    : UnsecureReader(web_bundle_path) {}

void UnsecureSignedWebBundleIdReader::DoReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_.parser_->ParseIntegrityBlock(
      base::BindOnce(&UnsecureSignedWebBundleIdReader::OnIntegrityBlockParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UnsecureSignedWebBundleIdReader::ReturnError(UnusableSwbnFileError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(web_bundle_id_callback_).Run(base::unexpected(std::move(error)));
}

base::WeakPtr<UnsecureReader> UnsecureSignedWebBundleIdReader::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void UnsecureSignedWebBundleIdReader::OnIntegrityBlockParsed(
    web_package::mojom::BundleIntegrityBlockPtr raw_integrity_block,
    web_package::mojom::BundleIntegrityBlockParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    ReturnError(UnusableSwbnFileError(std::move(error)));
    return;
  }

  auto integrity_block =
      web_package::SignedWebBundleIntegrityBlock::Create(
          std::move(raw_integrity_block))
          .transform_error([](std::string error) {
            return UnusableSwbnFileError(
                UnusableSwbnFileError::Error::kIntegrityBlockParserFormatError,
                "Error while parsing the Signed Web Bundle's integrity "
                "block: " +
                    std::move(error));
          });

  if (!integrity_block.has_value()) {
    ReturnError(std::move(integrity_block.error()));
    return;
  }

  web_package::SignedWebBundleId bundle_id =
      integrity_block->signature_stack().derived_web_bundle_id();

  std::move(web_bundle_id_callback_).Run(std::move(bundle_id));
}

void UnsecureSignedWebBundleIdReader::SetResultCallback(
    WebBundleIdCallback web_bundle_id_result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web_bundle_id_callback_ = std::move(web_bundle_id_result_callback);
}

UnsecureSignedWebBundleIdReader::~UnsecureSignedWebBundleIdReader() = default;

}  // namespace web_app
