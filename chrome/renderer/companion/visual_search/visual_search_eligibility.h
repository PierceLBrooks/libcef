// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_ELIGIBILITY_H_
#define CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_ELIGIBILITY_H_

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "chrome/common/companion/eligibility_spec.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace companion::visual_search {

using ::gfx::Rect;
using ::gfx::Size;
using ::gfx::SizeF;

// Stores the raw features of a single image.
struct SingleImageGeometryFeatures {
  std::string image_identifier;
  Size original_image_size;
  Rect onpage_rect = Rect(0, 0, 0, 0);
  ~SingleImageGeometryFeatures() = default;
};

// This class is used to determine which images are eligible to be surfaced in
// the CSC side bar according to settings set in the config proto.
class EligibilityModule {
 public:
  // Create the module using a spec.
  explicit EligibilityModule(const EligibilitySpec& spec);

  // Applies the cheap_pruning_rules from the eligibility spec. Outputs a list
  // of image identifiers that pass eligibility in no particular order. Caches
  // the values of all features that are needed across all rule sets in the
  // spec to avoid having to pass them throughout.
  std::vector<std::string> RunFirstPassEligibilityAndCacheFeatureValues(
      const SizeF& viewport_image_size,
      const std::vector<SingleImageGeometryFeatures>& images);

  // Applies the classifier_score_rules and post_renormalization_rules from the
  // eligibility spec and outputs the list of image identifiers that pass, in
  // no particular order. Should be run after RunFirstPassEligibility above
  // is run and only if the image geometry features have not changed since
  // that method was called.
  std::vector<std::string> RunSecondPassPostClassificationEligibility(
      const base::flat_map<std::string, double>& shopping_classifier_scores,
      const base::flat_map<std::string, double>& sensitivity_classifier_scores);

  // Returns a map from formatted-as-string feature name to feature value for
  // the given image_identifier.
  base::flat_map<std::string, double> GetDebugFeatureValuesForImage(
      const std::string& image_id);

  EligibilityModule(const EligibilityModule&) = delete;
  EligibilityModule& operator=(const EligibilityModule&) = delete;
  ~EligibilityModule();

 private:
  FRIEND_TEST_ALL_PREFIXES(EligibilityModuleTest, TestImageFeatureComputation);
  FRIEND_TEST_ALL_PREFIXES(EligibilityModuleTest, TestPageFeatureComputation);
  void Clear();
  void ComputeNormalizingFeatures(
      const std::vector<SingleImageGeometryFeatures>& images);
  void RenormalizeForThirdPass();
  void ComputeFeaturesForOrOfThresholdingRules(
      const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
      const SingleImageGeometryFeatures& image);
  // Eligibility evaluation methods.
  bool IsEligible(
      const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
      const std::string& image_id);
  bool EvaluateEligibilityRule(const OrOfThresholdingRules& eligibility_rule,
                               const std::string& image_id);
  bool EvaluateThresholdingRule(const ThresholdingRule& thresholding_rule,
                                const std::string& image_id);
  // Convenient methods for getting and caching feature values.
  double GetImageFeatureValue(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      const SingleImageGeometryFeatures& image);
  absl::optional<double> RetrieveImageFeatureIfPresent(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      const std::string& image_id);
  double RetrieveImageFeatureOrDie(
      FeatureLibrary::ImageLevelFeatureName feature_name,
      const std::string& image_id);
  double RetrievePageLevelFeatureOrDie(
      FeatureLibrary::PageLevelFeatureName feature_name);
  double ComputeAndGetPageLevelFeatureValue(
      FeatureLibrary::PageLevelFeatureName feature_name,
      const std::vector<SingleImageGeometryFeatures>& images,
      bool limit_to_second_pass_eligible);
  double GetMaxFeatureValue(
      FeatureLibrary::PageLevelFeatureName page_level_feature_name,
      FeatureLibrary::ImageLevelFeatureName corresponding_image_feature_name,
      const std::vector<SingleImageGeometryFeatures>& images);
  double MaxFeatureValueAfterSecondPass(
      FeatureLibrary::ImageLevelFeatureName image_feature_name);
  void GetDebugFeatureValuesForRules(
      const std::string& image_id,
      const google::protobuf::RepeatedPtrField<OrOfThresholdingRules>& rules,
      base::flat_map<std::string, double>& output_map);

  EligibilitySpec spec_;
  // Cache for features that are computed individually for each image.
  // TODO(lilymihal): Add metrics about the size of these flat_map and sets.
  base::flat_map<std::string,
                 base::flat_map<FeatureLibrary::ImageLevelFeatureName, double>>
      image_level_features_;
  // Cache for features that are computed at the level of the whole page.
  base::flat_map<FeatureLibrary::PageLevelFeatureName, double>
      page_level_features_;
  // Keep track of what images were eligible after the first and second passes.
  base::flat_set<std::string> eligible_after_first_pass_;
  base::flat_set<std::string> eligible_after_second_pass_;

  // Cache the viewport size so we don't have to pass it around. This gets set
  // in RunFirstPassEligibilityAndCacheFeatureValues.
  float viewport_width_;
  float viewport_height_;

  // Keeps track of whether the first pass has run since the last time we ran
  // the second pass.
  bool have_run_first_pass_;
};
}  // namespace companion::visual_search
#endif
