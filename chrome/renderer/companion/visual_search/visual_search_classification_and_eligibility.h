// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFICATION_AND_ELIGIBILITY_H_
#define CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFICATION_AND_ELIGIBILITY_H_

#include "chrome/common/companion/eligibility_spec.pb.h"
#include "chrome/renderer/companion/visual_search/visual_search_eligibility.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "ui/gfx/geometry/size_f.h"

namespace companion::visual_search {

using ImageId = std::string;

struct SingleImageFeaturesAndBytes {
  SingleImageGeometryFeatures features;
  SkBitmap image_contents;
  ~SingleImageFeaturesAndBytes() = default;
};

class VisualClassificationAndEligibility {
 public:
  // Extract the SingleImageGeometryFeatures needed by the eligibility
  // module.
  // TODO: move this function outside of this class.
  static SingleImageGeometryFeatures ExtractFeaturesForEligibility(
      const std::string& image_identifier,
      blink::WebElement& element);

  // Create a VisualClassificationAndEligibility Object that can then be
  // used to run classification and eligibility. Returns a nullptr if
  // there was any error.
  static std::unique_ptr<VisualClassificationAndEligibility> Create(
      const std::string& model_bytes,
      const EligibilitySpec& eligibility_spec);

  // Run through classification and eligibility.
  std::vector<ImageId> RunClassificationAndEligibility(
      base::flat_map<ImageId, SingleImageFeaturesAndBytes>& images,
      const gfx::SizeF& viewport_size);

  VisualClassificationAndEligibility(
      const VisualClassificationAndEligibility&) = delete;
  VisualClassificationAndEligibility& operator=(
      const VisualClassificationAndEligibility&) = delete;
  ~VisualClassificationAndEligibility();

 private:
  VisualClassificationAndEligibility();
  std::pair<double, double> ClassifyImage(const SkBitmap& bitmap);

  std::unique_ptr<tflite::task::vision::ImageClassifier> classifier_;
  std::unique_ptr<EligibilityModule> eligibility_module_;
};
}  // namespace companion::visual_search
#endif
