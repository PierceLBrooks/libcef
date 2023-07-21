// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/companion/visual_search/visual_search_eligibility.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace companion::visual_search {

using ::gfx::Rect;
using ::gfx::Size;
using ::gfx::SizeF;
using ::testing::DoubleNear;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(EligibilityModuleTest, E2eExample) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(44);
  rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_ASPECT_RATIO);
  rules->set_op(FeatureLibrary::LT);
  rules->set_threshold(3);
  rules = spec.add_classifier_score_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::SHOPPING_CLASSIFIER_SCORE);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(0.6);
  rules = spec.add_classifier_score_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::SENS_CLASSIFIER_SCORE);
  rules->set_op(FeatureLibrary::LT);
  rules->set_threshold(0.5);
  rules = spec.add_post_renormalization_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_normalizing_feature_name(FeatureLibrary::MAX_IMAGE_ONPAGE_AREA);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(0.999);

  EligibilityModule module(spec);
  SizeF viewport_size(100.0, 50.0);
  std::vector<SingleImageGeometryFeatures> images;
  images.reserve(3);
  SingleImageGeometryFeatures image1;
  image1.image_identifier = "image1";
  image1.onpage_rect = Rect(0, 0, 5, 10);
  images.push_back(std::move(image1));
  SingleImageGeometryFeatures image2;
  image2.image_identifier = "image2";
  image2.onpage_rect = Rect(0, 0, 15, 3);
  images.push_back(std::move(image2));
  // Identical to image 1, passes eligibility as well, but will have non-passing
  // shopping score.
  SingleImageGeometryFeatures image3;
  image3.image_identifier = "image3";
  image3.onpage_rect = Rect(0, 0, 5, 10);
  images.push_back(std::move(image3));
  // Identical to image 1, passes eligibility as well, but will have non-passing
  // sensitivity score.
  SingleImageGeometryFeatures image4;
  image4.image_identifier = "image4";
  image4.onpage_rect = Rect(0, 0, 5, 10);
  images.push_back(std::move(image4));
  // A large image that passes first pass, but not second pass. Its area should
  // not participate in normalization when applying third pass.
  SingleImageGeometryFeatures image5;
  image5.image_identifier = "image5";
  image5.onpage_rect = Rect(0, 0, 1000, 1000);
  images.push_back(std::move(image5));
  // Image that passes first and second pass but not third.
  SingleImageGeometryFeatures image6;
  image6.image_identifier = "image6";
  image6.onpage_rect = Rect(0, 0, 5, 9);
  images.push_back(std::move(image6));

  const std::vector<std::string> simple_pruning_image_ids =
      module.RunFirstPassEligibilityAndCacheFeatureValues(viewport_size,
                                                          images);
  ASSERT_EQ(simple_pruning_image_ids.size(), 5U);
  EXPECT_EQ(simple_pruning_image_ids.at(0), "image1");
  EXPECT_EQ(simple_pruning_image_ids.at(1), "image3");
  EXPECT_EQ(simple_pruning_image_ids.at(2), "image4");
  EXPECT_EQ(simple_pruning_image_ids.at(3), "image5");
  EXPECT_EQ(simple_pruning_image_ids.at(4), "image6");

  const base::flat_map<std::string, double> shopping_scores = {{"image1", 0.7},
                                                               {"image3", 0.5},
                                                               {"image4", 0.7},
                                                               {"image5", 0.0},
                                                               {"image6", 0.7}};
  const base::flat_map<std::string, double> sens_scores = {{"image1", 0.4},
                                                           {"image3", 0.4},
                                                           {"image4", 0.8},
                                                           {"image5", 0.0},
                                                           {"image6", 0.4}};
  const std::vector<std::string> second_pass_eligible_image_ids =
      module.RunSecondPassPostClassificationEligibility(shopping_scores,
                                                        sens_scores);

  ASSERT_EQ(second_pass_eligible_image_ids.size(), 1U);
  EXPECT_EQ(second_pass_eligible_image_ids.at(0), "image1");
  const base::flat_map<std::string, double> image1_features_after_third =
      module.GetDebugFeatureValuesForImage("image1");
  EXPECT_THAT(
      image1_features_after_third,
      UnorderedElementsAre(
          Pair("IMAGE_ONPAGE_AREA", 50), Pair("MAX_IMAGE_ONPAGE_AREA", 50),
          Pair("normalized_IMAGE_ONPAGE_AREA", DoubleNear(1, 0.01)),
          Pair("IMAGE_ONPAGE_ASPECT_RATIO", 2)));
}

TEST(EligibilityModuleTest, TestWithFeatureNormalization) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_normalizing_feature_name(FeatureLibrary::MAX_IMAGE_ONPAGE_AREA);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(0.5);

  EligibilityModule module(spec);

  SizeF viewport_size(100.0, 50.0);
  std::vector<SingleImageGeometryFeatures> images;
  images.reserve(3);
  SingleImageGeometryFeatures image1;
  image1.image_identifier = "image1";
  image1.onpage_rect = Rect(0, 0, 10, 10);
  images.push_back(std::move(image1));
  SingleImageGeometryFeatures image2;
  image2.image_identifier = "image2";
  image2.onpage_rect = Rect(0, 0, 6, 10);
  images.push_back(std::move(image2));
  // Identical to image 1, passes eligibility as well, but will have non-passing
  // shopping score.
  SingleImageGeometryFeatures image3;
  image3.image_identifier = "image3";
  image3.onpage_rect = Rect(0, 0, 4, 10);
  images.push_back(std::move(image3));
  const std::vector<std::string> eligible_image_ids =
      module.RunFirstPassEligibilityAndCacheFeatureValues(viewport_size,
                                                          images);
  ASSERT_EQ(eligible_image_ids.size(), 2U);
  EXPECT_EQ(eligible_image_ids.at(0), "image1");
  EXPECT_EQ(eligible_image_ids.at(1), "image2");

  const base::flat_map<std::string, double> image2_features =
      module.GetDebugFeatureValuesForImage("image2");
  EXPECT_THAT(
      image2_features,
      UnorderedElementsAre(
          Pair("IMAGE_ONPAGE_AREA", 60), Pair("MAX_IMAGE_ONPAGE_AREA", 100),
          Pair("normalized_IMAGE_ONPAGE_AREA", DoubleNear(0.6, 0.01))));
}

TEST(EligibilityModuleTest, TestOringRules) {
  EligibilitySpec spec;
  auto* ored_rules = spec.add_cheap_pruning_rules();
  auto* rules = ored_rules->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_normalizing_feature_name(FeatureLibrary::MAX_IMAGE_ONPAGE_AREA);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(0.5);
  rules = ored_rules->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_op(FeatureLibrary::LT);
  rules->set_threshold(45);

  EligibilityModule module(spec);
  SizeF viewport_size(100.0, 50.0);
  std::vector<SingleImageGeometryFeatures> images;
  images.reserve(3);
  SingleImageGeometryFeatures image1;
  image1.image_identifier = "image1";
  image1.onpage_rect = Rect(0, 0, 10, 10);
  images.push_back(std::move(image1));
  SingleImageGeometryFeatures image2;
  image2.image_identifier = "image2";
  image2.onpage_rect = Rect(0, 0, 6, 10);
  images.push_back(std::move(image2));
  SingleImageGeometryFeatures image3;
  image3.image_identifier = "image3";
  image3.onpage_rect = Rect(0, 0, 4, 10);
  images.push_back(std::move(image3));
  const std::vector<std::string> eligible_image_ids =
      module.RunFirstPassEligibilityAndCacheFeatureValues(viewport_size,
                                                          images);
  ASSERT_EQ(eligible_image_ids.size(), 3U);
  EXPECT_EQ(eligible_image_ids.at(0), "image1");
  EXPECT_EQ(eligible_image_ids.at(1), "image2");
  EXPECT_EQ(eligible_image_ids.at(2), "image3");
}

TEST(EligibilityModuleTest, TestImageVisibleArea) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_VISIBLE_AREA);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(1.1);

  EligibilityModule module(spec);
  SizeF viewport_size(3.0, 3.0);
  std::vector<SingleImageGeometryFeatures> images;
  images.reserve(2);
  SingleImageGeometryFeatures image1;
  image1.image_identifier = "image1";
  image1.onpage_rect = Rect(1, 1, 3, 3);
  images.push_back(std::move(image1));
  SingleImageGeometryFeatures image2;
  image2.image_identifier = "image2";
  image2.onpage_rect = Rect(2, 2, 2, 2);
  images.push_back(std::move(image2));

  const std::vector<std::string> eligible_image_ids =
      module.RunFirstPassEligibilityAndCacheFeatureValues(viewport_size,
                                                          images);
  ASSERT_EQ(eligible_image_ids.size(), 1U);
  EXPECT_EQ(eligible_image_ids.at(0), "image1");
}

TEST(EligibilityModuleTest, TestFeaturesForSecondPassCached) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_ASPECT_RATIO);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(4);
  rules = spec.add_classifier_score_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_normalizing_feature_name(FeatureLibrary::MAX_IMAGE_ONPAGE_AREA);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(0.5);

  EligibilityModule module(spec);
  SizeF viewport_size(100.0, 50.0);
  std::vector<SingleImageGeometryFeatures> images;
  images.reserve(1);
  SingleImageGeometryFeatures image1;
  image1.image_identifier = "image1";
  image1.onpage_rect = Rect(0, 0, 50, 10);
  images.push_back(std::move(image1));
  const std::vector<std::string> eligible_image_ids =
      module.RunFirstPassEligibilityAndCacheFeatureValues(viewport_size,
                                                          images);
  ASSERT_EQ(eligible_image_ids.size(), 1U);
  EXPECT_EQ(eligible_image_ids.at(0), "image1");
  const std::vector<std::string> second_pass_eligible_image_ids =
      module.RunSecondPassPostClassificationEligibility({}, {});
  ASSERT_EQ(second_pass_eligible_image_ids.size(), 1U);
  EXPECT_EQ(second_pass_eligible_image_ids.at(0), "image1");
}

TEST(EligibilityModuleTest, TestReuseModuleBetweenImageSets) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(100);
  rules = spec.add_classifier_score_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::SHOPPING_CLASSIFIER_SCORE);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(0.6);
  rules = spec.add_post_renormalization_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_ONPAGE_AREA);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(100);

  EligibilityModule module(spec);
  SizeF viewport_size(100.0, 50.0);
  {
    // Run 1 with the module. One image, which passes.
    std::vector<SingleImageGeometryFeatures> images;
    images.reserve(2);
    // Both image1 and image3 pass the filters here. In the second run, we'll
    // have images with the same names, but they will not pass the first and
    // the second pass respectively.
    SingleImageGeometryFeatures image1;
    image1.image_identifier = "image1";
    image1.onpage_rect = Rect(0, 0, 20, 10);
    images.push_back(std::move(image1));
    SingleImageGeometryFeatures image3;
    image3.image_identifier = "image3";
    image3.onpage_rect = Rect(0, 0, 20, 10);
    images.push_back(std::move(image3));
    const std::vector<std::string> eligible_image_ids =
        module.RunFirstPassEligibilityAndCacheFeatureValues(viewport_size,
                                                            images);
    ASSERT_EQ(eligible_image_ids.size(), 2U);
    EXPECT_THAT(eligible_image_ids, UnorderedElementsAre("image1", "image3"));
    const base::flat_map<std::string, double> shopping_scores = {
        {"image1", 0.7}, {"image3", 0.7}};
    const base::flat_map<std::string, double> sens_scores = {{"image1", 0.1},
                                                             {"image3", 0.1}};
    const std::vector<std::string> second_pass_eligible_image_ids =
        module.RunSecondPassPostClassificationEligibility(shopping_scores,
                                                          sens_scores);
    ASSERT_EQ(second_pass_eligible_image_ids.size(), 2U);
    EXPECT_THAT(second_pass_eligible_image_ids,
                UnorderedElementsAre("image1", "image3"));
  }
  {
    // Run 2 with the module.
    std::vector<SingleImageGeometryFeatures> images;
    images.reserve(3);
    SingleImageGeometryFeatures image1;
    // Gets excluded in the first pass.
    image1.image_identifier = "image1";
    image1.onpage_rect = Rect(0, 0, 2, 10);
    images.push_back(std::move(image1));
    SingleImageGeometryFeatures image2;
    image2.image_identifier = "image2";
    image2.onpage_rect = Rect(0, 0, 20, 10);
    images.push_back(std::move(image2));
    // Gets excluded in the second pass.
    SingleImageGeometryFeatures image3;
    image3.image_identifier = "image3";
    image3.onpage_rect = Rect(0, 0, 20, 10);
    images.push_back(std::move(image3));
    const std::vector<std::string> eligible_image_ids =
        module.RunFirstPassEligibilityAndCacheFeatureValues(viewport_size,
                                                            images);
    ASSERT_EQ(eligible_image_ids.size(), 2U);
    EXPECT_THAT(eligible_image_ids, UnorderedElementsAre("image2", "image3"));
    // Image3 doesn't pass the shoppy filter here.
    const base::flat_map<std::string, double> shopping_scores = {
        {"image2", 0.7}, {"image3", 0.1}};
    const base::flat_map<std::string, double> sens_scores = {{"image2", 0.1},
                                                             {"image3", 0.1}};
    const std::vector<std::string> second_pass_eligible_image_ids =
        module.RunSecondPassPostClassificationEligibility(shopping_scores,
                                                          sens_scores);
    ASSERT_EQ(second_pass_eligible_image_ids.size(), 1U);
    EXPECT_EQ(second_pass_eligible_image_ids.at(0), "image2");
  }
}

TEST(EligibilityModuleTest, TestImageFractionVisible) {
  EligibilitySpec spec;
  auto* rules = spec.add_cheap_pruning_rules()->add_rules();
  rules->set_feature_name(FeatureLibrary::IMAGE_FRACTION_VISIBLE);
  rules->set_op(FeatureLibrary::GT);
  rules->set_threshold(0.26);

  EligibilityModule module(spec);

  SizeF viewport_size(3.0, 3.0);
  std::vector<SingleImageGeometryFeatures> images;
  images.reserve(2);
  SingleImageGeometryFeatures image1;
  image1.image_identifier = "image1";
  image1.onpage_rect = Rect(2, 1, 2, 2);
  images.push_back(std::move(image1));
  SingleImageGeometryFeatures image2;
  image2.image_identifier = "image2";
  image2.onpage_rect = Rect(2, 2, 2, 2);
  images.push_back(std::move(image2));

  const std::vector<std::string> eligible_image_ids =
      module.RunFirstPassEligibilityAndCacheFeatureValues(viewport_size,
                                                          images);
  ASSERT_EQ(eligible_image_ids.size(), 1U);
  EXPECT_EQ(eligible_image_ids.at(0), "image1");
}

TEST(EligibilityModuleTest, TestImageFeatureComputation) {
  // The spec doesn't matter here. Just make an empty one.
  const EligibilitySpec spec;
  EligibilityModule module(spec);
  SingleImageGeometryFeatures image;
  image.image_identifier = "image";
  image.original_image_size = Size(10, 20);
  image.onpage_rect = Rect(1, 1, 100, 400);

  EXPECT_EQ(
      module.GetImageFeatureValue(FeatureLibrary::IMAGE_ORIGINAL_AREA, image),
      200);
  EXPECT_EQ(module.GetImageFeatureValue(
                FeatureLibrary::IMAGE_ORIGINAL_ASPECT_RATIO, image),
            2);
  EXPECT_EQ(
      module.GetImageFeatureValue(FeatureLibrary::IMAGE_ONPAGE_AREA, image),
      40000);
  EXPECT_EQ(module.GetImageFeatureValue(
                FeatureLibrary::IMAGE_ONPAGE_ASPECT_RATIO, image),
            4);
  EXPECT_EQ(
      module.GetImageFeatureValue(FeatureLibrary::IMAGE_ONPAGE_HEIGHT, image),
      400);
  EXPECT_EQ(
      module.GetImageFeatureValue(FeatureLibrary::IMAGE_ONPAGE_WIDTH, image),
      100);
  EXPECT_EQ(
      module.GetImageFeatureValue(FeatureLibrary::IMAGE_ORIGINAL_HEIGHT, image),
      20);
  EXPECT_EQ(
      module.GetImageFeatureValue(FeatureLibrary::IMAGE_ORIGINAL_WIDTH, image),
      10);

  // These should now be cached.
  EXPECT_EQ(module.RetrieveImageFeatureOrDie(
                FeatureLibrary::IMAGE_ORIGINAL_AREA, "image"),
            200);
  EXPECT_EQ(module.RetrieveImageFeatureOrDie(
                FeatureLibrary::IMAGE_ORIGINAL_ASPECT_RATIO, "image"),
            2);
  EXPECT_EQ(module.RetrieveImageFeatureOrDie(FeatureLibrary::IMAGE_ONPAGE_AREA,
                                             "image"),
            40000);
  EXPECT_EQ(module.RetrieveImageFeatureOrDie(
                FeatureLibrary::IMAGE_ONPAGE_ASPECT_RATIO, "image"),
            4);
  EXPECT_EQ(module.RetrieveImageFeatureOrDie(
                FeatureLibrary::IMAGE_ONPAGE_HEIGHT, "image"),
            400);
  EXPECT_EQ(module.RetrieveImageFeatureOrDie(FeatureLibrary::IMAGE_ONPAGE_WIDTH,
                                             "image"),
            100);
  EXPECT_EQ(module.RetrieveImageFeatureOrDie(
                FeatureLibrary::IMAGE_ORIGINAL_HEIGHT, "image"),
            20);
  EXPECT_EQ(module.RetrieveImageFeatureOrDie(
                FeatureLibrary::IMAGE_ORIGINAL_WIDTH, "image"),
            10);
}

TEST(EligibilityModuleTest, TestPageFeatureComputation) {
  const EligibilitySpec spec;
  EligibilityModule module(spec);
  SingleImageGeometryFeatures image1;
  image1.image_identifier = "image1";
  image1.original_image_size = Size(10, 20);
  image1.onpage_rect = Rect(90, 90, 40, 40);
  SingleImageGeometryFeatures image2;
  image2.image_identifier = "image2";
  image2.original_image_size = Size(10, 200);
  image2.onpage_rect = Rect(80, 80, 40, 400);
  std::vector<SingleImageGeometryFeatures> images;
  images.push_back(std::move(image1));
  images.push_back(std::move(image2));

  // Artificially set viewport dimensions.
  module.viewport_width_ = 100;
  module.viewport_height_ = 100;
  EXPECT_EQ(module.ComputeAndGetPageLevelFeatureValue(
                FeatureLibrary::VIEWPORT_AREA, images, false),
            10000);
  EXPECT_EQ(module.ComputeAndGetPageLevelFeatureValue(
                FeatureLibrary::MAX_IMAGE_ORIGINAL_AREA, images, false),
            2000);
  EXPECT_EQ(module.ComputeAndGetPageLevelFeatureValue(
                FeatureLibrary::MAX_IMAGE_ORIGINAL_ASPECT_RATIO, images, false),
            20);
  EXPECT_EQ(module.ComputeAndGetPageLevelFeatureValue(
                FeatureLibrary::MAX_IMAGE_ORIGINAL_ASPECT_RATIO, images, false),
            20);
  EXPECT_EQ(module.ComputeAndGetPageLevelFeatureValue(
                FeatureLibrary::MAX_IMAGE_ONPAGE_AREA, images, false),
            16000);
  EXPECT_EQ(module.ComputeAndGetPageLevelFeatureValue(
                FeatureLibrary::MAX_IMAGE_ONPAGE_ASPECT_RATIO, images, false),
            10);
  EXPECT_EQ(module.ComputeAndGetPageLevelFeatureValue(
                FeatureLibrary::MAX_IMAGE_VISIBLE_AREA, images, false),
            400);
  EXPECT_EQ(module.ComputeAndGetPageLevelFeatureValue(
                FeatureLibrary::MAX_IMAGE_FRACTION_VISIBLE, images, false),
            0.0625);
}
}  // namespace companion::visual_search
