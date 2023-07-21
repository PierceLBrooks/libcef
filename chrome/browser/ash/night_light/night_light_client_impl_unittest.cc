// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/night_light/night_light_client_impl.h"

#include "ash/public/cpp/night_light_controller.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {

namespace {

using ScheduleType = ash::NightLightController::ScheduleType;
using SimpleGeoposition = ash::NightLightController::SimpleGeoposition;

// Constructs a TimeZone object from the given |timezone_id|.
std::unique_ptr<icu::TimeZone> CreateTimezone(const char* timezone_id) {
  return base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString(timezone_id, -1, US_INV)));
}

std::u16string GetTimezoneId(const icu::TimeZone& timezone) {
  return ash::system::TimezoneSettings::GetTimezoneID(timezone);
}

// A fake implementation of NightLightController for testing.
class FakeNightLightController : public ash::NightLightController {
 public:
  FakeNightLightController() = default;

  FakeNightLightController(const FakeNightLightController&) = delete;
  FakeNightLightController& operator=(const FakeNightLightController&) = delete;

  ~FakeNightLightController() override = default;

  const SimpleGeoposition& position() const { return position_; }

  int position_pushes_num() const { return position_pushes_num_; }

  // ash::NightLightController:
  void SetCurrentGeoposition(const SimpleGeoposition& position) override {
    position_ = position;
    ++position_pushes_num_;
  }

  bool GetEnabled() const override { return false; }

  void NotifyScheduleTypeChanged(ScheduleType type) {
    for (auto& observer : observers_) {
      observer.OnScheduleTypeChanged(type);
    }
  }

 private:
  SimpleGeoposition position_;

  // The number of times a new position is pushed to this controller.
  int position_pushes_num_ = 0;
};

class FakeDelegate : public SimpleGeolocationProvider::Delegate {
 public:
  bool IsSystemGeolocationAllowed() const override { return true; }
};

// A fake implementation of NightLightClient that doesn't perform any actual
// geoposition requests.
class FakeNightLightClient : public NightLightClientImpl {
 public:
  explicit FakeNightLightClient(
      const SimpleGeolocationProvider::Delegate* delegate)
      : NightLightClientImpl(delegate, nullptr /* url_context_getter */) {}

  FakeNightLightClient(const FakeNightLightClient&) = delete;
  FakeNightLightClient& operator=(const FakeNightLightClient&) = delete;

  ~FakeNightLightClient() override = default;

  void set_position_to_send(const ash::Geoposition& position) {
    position_to_send_ = position;
  }

  int geoposition_requests_num() const { return geoposition_requests_num_; }

 private:
  // night_light::NightLightClient:
  void RequestGeoposition() override {
    OnGeoposition(position_to_send_, false, base::TimeDelta());
    ++geoposition_requests_num_;
  }

  // The position to send to the controller the next time OnGeoposition is
  // invoked.
  ash::Geoposition position_to_send_;

  // The number of new geoposition requests that have been triggered.
  int geoposition_requests_num_ = 0;
};

// Base test fixture.
class NightLightClientImplTest : public testing::TestWithParam<ScheduleType> {
 public:
  NightLightClientImplTest() : client_(&delegate_) {}

  NightLightClientImplTest(const NightLightClientImplTest&) = delete;
  NightLightClientImplTest& operator=(const NightLightClientImplTest&) = delete;

  ~NightLightClientImplTest() override = default;

  void SetUp() override {
    // Deterministic fake time that doesn't change for the sake of testing.
    client_.SetTimerForTesting(std::make_unique<base::OneShotTimer>(
        task_environment_.GetMockTickClock()));
    client_.SetClockForTesting(task_environment_.GetMockClock());

    // Notify system geolocation permission = Enabled.
    client_.OnSystemGeolocationPermissionChanged(/*enabled=*/true);
    client_.Start();
  }

  ash::Geoposition CreateValidGeoposition() {
    ash::Geoposition position;
    position.latitude = 32.0;
    position.longitude = 31.0;
    position.status = ash::Geoposition::STATUS_OK;
    position.accuracy = 10;
    position.timestamp = base::Time::Now();

    return position;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // NOTE: Don't reorder. Global controller instance has to be created first,
  // client stores its reference on initialization.
  FakeNightLightController controller_;
  FakeNightLightClient client_;

 private:
  FakeDelegate delegate_;
};

// Test that the client is retrieving geoposition periodically only when the
// schedule type is "sunset to sunrise" or "custom".
TEST_F(NightLightClientImplTest,
       TestClientRunningWhenSunsetToSunriseOnCustomSchedule) {
  EXPECT_FALSE(client_.using_geoposition());
  controller_.NotifyScheduleTypeChanged(ScheduleType::kNone);
  EXPECT_FALSE(client_.using_geoposition());
  controller_.NotifyScheduleTypeChanged(ScheduleType::kCustom);
  EXPECT_TRUE(client_.using_geoposition());
  controller_.NotifyScheduleTypeChanged(ScheduleType::kSunsetToSunrise);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(client_.using_geoposition());

  // Client should stop retrieving geopositions when schedule type changes to
  // something else.
  controller_.NotifyScheduleTypeChanged(ScheduleType::kNone);
  EXPECT_FALSE(client_.using_geoposition());
}

// Test that client only pushes valid positions.
TEST_F(NightLightClientImplTest, TestInvalidPositions) {
  EXPECT_EQ(0, controller_.position_pushes_num());
  ash::Geoposition position;
  position.latitude = 32.0;
  position.longitude = 31.0;
  position.status = ash::Geoposition::STATUS_TIMEOUT;
  position.accuracy = 10;
  position.timestamp = base::Time::Now();
  client_.set_position_to_send(position);

  controller_.NotifyScheduleTypeChanged(ScheduleType::kSunsetToSunrise);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, client_.geoposition_requests_num());
  EXPECT_EQ(0, controller_.position_pushes_num());
}

// Test that successive changes of the schedule type to sunset to sunrise do not
// trigger repeated geoposition requests.
TEST_F(NightLightClientImplTest, TestRepeatedScheduleTypeChanges) {
  // Start with a valid position, and expect it to be delivered to the
  // controller.
  EXPECT_EQ(0, controller_.position_pushes_num());
  ash::Geoposition position1;
  position1.latitude = 32.0;
  position1.longitude = 31.0;
  position1.status = ash::Geoposition::STATUS_OK;
  position1.accuracy = 10;
  position1.timestamp = base::Time::Now();
  client_.set_position_to_send(position1);

  controller_.NotifyScheduleTypeChanged(ScheduleType::kSunsetToSunrise);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, client_.geoposition_requests_num());
  EXPECT_EQ(1, controller_.position_pushes_num());
  EXPECT_EQ(task_environment_.GetMockClock()->Now(),
            client_.last_successful_geo_request_time());

  // A new different position just for the sake of comparison with position1 to
  // make sure that no new requests are triggered and the same old position will
  // be resent to the controller.
  ash::Geoposition position2;
  position2.latitude = 100.0;
  position2.longitude = 200.0;
  position2.status = ash::Geoposition::STATUS_OK;
  position2.accuracy = 10;
  position2.timestamp = base::Time::Now();
  client_.set_position_to_send(position2);
  controller_.NotifyScheduleTypeChanged(ScheduleType::kSunsetToSunrise);
  task_environment_.RunUntilIdle();
  // No new request has been triggered, however the same old valid position was
  // pushed to the controller.
  EXPECT_EQ(1, client_.geoposition_requests_num());
  EXPECT_EQ(2, controller_.position_pushes_num());
  SimpleGeoposition simple_geoposition1{position1.latitude,
                                        position1.longitude};
  EXPECT_EQ(simple_geoposition1, controller_.position());

  // The timer should be running scheduling a next request that is a
  // kNextRequestDelayAfterSuccess from the last successful request time.
  base::TimeDelta expected_delay =
      client_.last_successful_geo_request_time() +
      NightLightClientImpl::GetNextRequestDelayAfterSuccessForTesting() -
      task_environment_.GetMockClock()->Now();
  EXPECT_EQ(expected_delay, client_.timer().GetCurrentDelay());
}

// Tests that timezone changes result in new geoposition requests if the
// schedule type is sunset to sunrise or custom.
TEST_P(NightLightClientImplTest, TestTimezoneChanges) {
  EXPECT_EQ(0, controller_.position_pushes_num());
  client_.SetCurrentTimezoneIdForTesting(u"America/Los_Angeles");

  // When schedule type is none, timezone changes do not result
  // in geoposition requests.
  controller_.NotifyScheduleTypeChanged(ScheduleType::kNone);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(client_.using_geoposition());
  auto timezone = CreateTimezone("Africa/Cairo");
  client_.TimezoneChanged(*timezone);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, controller_.position_pushes_num());
  EXPECT_EQ(0, client_.geoposition_requests_num());
  EXPECT_EQ(GetTimezoneId(*timezone), client_.current_timezone_id());

  // Prepare a valid geoposition.
  ash::Geoposition position;
  position.latitude = 32.0;
  position.longitude = 31.0;
  position.status = ash::Geoposition::STATUS_OK;
  position.accuracy = 10;
  position.timestamp = base::Time::Now();
  client_.set_position_to_send(position);

  // Change the schedule type to sunset to sunrise or custom, and expect the
  // geoposition will be pushed.
  controller_.NotifyScheduleTypeChanged(GetParam());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, controller_.position_pushes_num());
  EXPECT_EQ(1, client_.geoposition_requests_num());

  // Updates with the same timezone does not result in new requests.
  timezone = CreateTimezone("Africa/Cairo");
  client_.TimezoneChanged(*timezone);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, controller_.position_pushes_num());
  EXPECT_EQ(1, client_.geoposition_requests_num());
  EXPECT_EQ(GetTimezoneId(*timezone), client_.current_timezone_id());

  // Only new timezones results in new geoposition requests.
  timezone = CreateTimezone("Asia/Tokyo");
  client_.TimezoneChanged(*timezone);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, controller_.position_pushes_num());
  EXPECT_EQ(2, client_.geoposition_requests_num());
  EXPECT_EQ(GetTimezoneId(*timezone), client_.current_timezone_id());
}

TEST_P(NightLightClientImplTest,
       TestSystemGeolocationPermissionChangesForScheduleType) {
  EXPECT_EQ(0, controller_.position_pushes_num());

  // Prepare a valid geoposition.
  ash::Geoposition position = CreateValidGeoposition();
  client_.set_position_to_send(position);

  // Disable system geolocation permission, expect that no requests will be
  // sent, regardless of the controller setting.
  EXPECT_FALSE(client_.timer().IsRunning());
  client_.OnSystemGeolocationPermissionChanged(/*enabled=*/false);
  EXPECT_FALSE(client_.timer().IsRunning());

  // Set nightlightclient type to either SunsetToSunrise or Custom.
  controller_.NotifyScheduleTypeChanged(GetParam());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, controller_.position_pushes_num());
  EXPECT_EQ(0, client_.geoposition_requests_num());

  // Re-enable system geolocation permission and expect a new geolocation
  // request.
  client_.OnSystemGeolocationPermissionChanged(/*enabled=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, controller_.position_pushes_num());
  EXPECT_EQ(1, client_.geoposition_requests_num());

  // Fast forward to the next request.
  task_environment_.FastForwardBy(
      client_.GetNextRequestDelayAfterSuccessForTesting());
  EXPECT_EQ(2, controller_.position_pushes_num());
  EXPECT_EQ(2, client_.geoposition_requests_num());

  // Revoking the geolocation permission should stop the scheduler.
  client_.OnSystemGeolocationPermissionChanged(/*enabled=*/false);
  EXPECT_EQ(2, controller_.position_pushes_num());
  EXPECT_EQ(2, client_.geoposition_requests_num());
  EXPECT_FALSE(client_.timer().IsRunning());
}

TEST_P(NightLightClientImplTest,
       TestSystemGeolocationPermissionChangesForTimezone) {
  EXPECT_EQ(0, controller_.position_pushes_num());
  client_.SetCurrentTimezoneIdForTesting(u"America/Los_Angeles");

  // Prepare a valid geoposition.
  ash::Geoposition position = CreateValidGeoposition();
  client_.set_position_to_send(position);

  // Change the schedule type to sunset to sunrise or custom, and expect the
  // geoposition will be pushed.
  controller_.NotifyScheduleTypeChanged(GetParam());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, controller_.position_pushes_num());
  EXPECT_EQ(1, client_.geoposition_requests_num());

  // Disable geolocation permission and expect scheduler to stop.
  client_.OnSystemGeolocationPermissionChanged(/*enabled=*/false);
  EXPECT_FALSE(client_.timer().IsRunning());

  // new timezone shouldn't resume scheduling, while the geo permission is off.
  // Current timezone should update successfully.
  auto timezone = CreateTimezone("Asia/Tokyo");
  client_.TimezoneChanged(*timezone);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, controller_.position_pushes_num());
  EXPECT_EQ(1, client_.geoposition_requests_num());
  EXPECT_EQ(GetTimezoneId(*timezone), client_.current_timezone_id());
  EXPECT_FALSE(client_.timer().IsRunning());

  // Re-enable the system geolocation permission. Should result in a new
  // immediate request.
  client_.OnSystemGeolocationPermissionChanged(/*enabled=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2, controller_.position_pushes_num());
  EXPECT_EQ(2, client_.geoposition_requests_num());

  // Update the timezone again, see that the geolocation request is dispatched
  // immediately.
  timezone = CreateTimezone("Africa/Cairo");
  client_.TimezoneChanged(*timezone);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(3, controller_.position_pushes_num());
  EXPECT_EQ(3, client_.geoposition_requests_num());
  EXPECT_EQ(GetTimezoneId(*timezone), client_.current_timezone_id());
}

INSTANTIATE_TEST_SUITE_P(All,
                         NightLightClientImplTest,
                         ::testing::Values(ScheduleType::kSunsetToSunrise,
                                           ScheduleType::kCustom));
}  // namespace

}  // namespace ash
