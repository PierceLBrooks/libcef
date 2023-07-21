// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/user_tuning/user_performance_tuning_notifier.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ChromeBrowserMainExtraPartsPerformanceManager;
class PerformanceManagerMetricsProviderTest;
class PrefService;

namespace performance_manager::user_tuning {

// This singleton is responsible for managing the state of high efficiency mode
// and battery saver mode, as well as the different signals surrounding their
// toggling.
//
// It is created and owned by `ChromeBrowserMainExtraPartsPerformanceManager`
// and initialized in 2 parts:
// - Created in PostCreateThreads (so that UI can start observing it as soon as
// the first views are created) and
// - Starts to manage the modes when Start() is called in PreMainMessageLoopRun.
//
// This object lives on the main thread and should be used from it exclusively.
class UserPerformanceTuningManager {
 public:
  // The percentage of battery that is considered "low". For instance, this
  // would be `20` for 20%.
  static const uint64_t kLowBatteryThresholdPercent;

  class FrameThrottlingDelegate {
   public:
    virtual void StartThrottlingAllFrameSinks() = 0;
    virtual void StopThrottlingAllFrameSinks() = 0;

    virtual ~FrameThrottlingDelegate() = default;
  };

  class HighEfficiencyModeDelegate {
   public:
    virtual void ToggleHighEfficiencyMode(
        prefs::HighEfficiencyModeState state) = 0;
    virtual void SetTimeBeforeDiscard(base::TimeDelta time_before_discard) = 0;
    virtual ~HighEfficiencyModeDelegate() = default;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Raised when the battery saver mode interventions are activated or
    // deactivated
    virtual void OnBatterySaverModeChanged(bool is_active) {}

    // Raised when the high efficiency mode setting is changed. Get the new
    // state using `UserPerformanceTuningManager::IsHighEfficiencyModeActive()`
    virtual void OnHighEfficiencyModeChanged() {}

    // Raised when the device is plugged in or unplugged
    // Can be used by the UI to show a promo if BSM isn't configured to be
    // enabled when on battery power.
    // If the connection/disconnection from power causes battery saver to be
    // enabled/disabled, the state of battery saver will not yet be updated when
    // this is invoked. `OnBatterySaverModeChanged` will be invoked after the
    // state is updated.
    virtual void OnExternalPowerConnectedChanged(bool on_battery_power) {}

    // Raised when it becomes known that the device has a battery installed, or
    // when a device that previously had a battery is now reported as not having
    // one anymore. Overloading this function is particularly useful for code
    // that wants to know if the device has a battery during startup, because
    // `DeviceHasBattery` can wrongly return `false` for an unbounded period
    // of time until the OS provides battery data.
    virtual void OnDeviceHasBatteryChanged(bool device_has_battery) {}

    // Raised when the battery has reached the 20% threshold
    // Can be used by the UI to show a promo if BSM isn't configured to be
    // enabled when on battery power under a certain threshold.
    virtual void OnBatteryThresholdReached() {}

    // Raised when the total memory footprint reaches X%.
    // Can be used by the UI to show a promo
    virtual void OnMemoryThresholdReached() {}

    // Raised when the tab count reaches X.
    // Can be used by the UI to show a promo
    virtual void OnTabCountThresholdReached() {}

    // Raised when the count of janky intervals reaches X.
    // Can be used by the UI to show a promo
    virtual void OnJankThresholdReached() {}

    // Raised when memory metrics for a discarded page becomes available to read
    virtual void OnMemoryMetricsRefreshed() {}
  };

  class TabResourceUsage : public base::RefCounted<TabResourceUsage> {
   public:
    TabResourceUsage() = default;

    uint64_t memory_usage_in_bytes() const { return memory_usage_bytes_; }

    void set_memory_usage_in_bytes(uint64_t memory_usage_bytes) {
      memory_usage_bytes_ = memory_usage_bytes;
    }

   private:
    friend class base::RefCounted<TabResourceUsage>;
    ~TabResourceUsage() = default;

    uint64_t memory_usage_bytes_ = 0;
  };

  // Per-tab class to keep track of current memory usage for each tab.
  class ResourceUsageTabHelper
      : public content::WebContentsObserver,
        public content::WebContentsUserData<ResourceUsageTabHelper> {
   public:
    ResourceUsageTabHelper(const ResourceUsageTabHelper&) = delete;
    ResourceUsageTabHelper& operator=(const ResourceUsageTabHelper&) = delete;

    ~ResourceUsageTabHelper() override;

    // content::WebContentsObserver
    void PrimaryPageChanged(content::Page& page) override;

    uint64_t GetMemoryUsageInBytes() {
      return resource_usage_->memory_usage_in_bytes();
    }

    void SetMemoryUsageInBytes(uint64_t memory_usage_bytes) {
      resource_usage_->set_memory_usage_in_bytes(memory_usage_bytes);
    }

    scoped_refptr<const TabResourceUsage> resource_usage() const {
      return resource_usage_;
    }

   private:
    friend class content::WebContentsUserData<ResourceUsageTabHelper>;
    explicit ResourceUsageTabHelper(content::WebContents* contents);
    WEB_CONTENTS_USER_DATA_KEY_DECL();

    scoped_refptr<TabResourceUsage> resource_usage_;
  };

  class PreDiscardResourceUsage
      : public content::WebContentsUserData<PreDiscardResourceUsage> {
   public:
    PreDiscardResourceUsage(content::WebContents* contents,
                            uint64_t memory_footprint_estimate,
                            ::mojom::LifecycleUnitDiscardReason discard_reason);
    ~PreDiscardResourceUsage() override;

    // Returns the resource usage estimate in kilobytes.
    uint64_t memory_footprint_estimate_kb() const {
      return memory_footprint_estimate_;
    }

    void SetMemoryFootprintEstimateKbForTesting(
        uint64_t memory_footprint_estimate) {
      memory_footprint_estimate_ = memory_footprint_estimate;
    }

    ::mojom::LifecycleUnitDiscardReason discard_reason() const {
      return discard_reason_;
    }

    base::LiveTicks discard_liveticks() const { return discard_liveticks_; }

   private:
    friend WebContentsUserData;
    WEB_CONTENTS_USER_DATA_KEY_DECL();

    uint64_t memory_footprint_estimate_ = 0;
    ::mojom::LifecycleUnitDiscardReason discard_reason_;
    base::LiveTicks discard_liveticks_;
  };

  // Returns whether a UserPerformanceTuningManager was created and installed.
  // Should only return false in unit tests.
  static bool HasInstance();
  static UserPerformanceTuningManager* GetInstance();

  ~UserPerformanceTuningManager();

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

  // Returns true if High Efficiency mode is currently enabled.
  bool IsHighEfficiencyModeActive();

  // Returns true if the prefs underlying High Efficiency Mode are managed by an
  // enterprise policy.
  bool IsHighEfficiencyModeManaged() const;

  // Returns true if the prefs underlying High Efficiency Mode are still in the
  // default state.
  bool IsHighEfficiencyModeDefault() const;

  // Enables high efficiency mode and sets the relevant prefs accordingly.
  void SetHighEfficiencyModeEnabled(bool enabled);

  // Discards the given WebContents with the same mechanism as one that is
  // discarded through a natural timeout
  void DiscardPageForTesting(content::WebContents* web_contents);

  // Returns true if the device is a portable device that can run on battery
  // power, false otherwise.
  // This is determined asynchronously, so it may indicate false for an
  // undetermined amount of time at startup, until the battery state is
  // sampled for the first time.
  bool DeviceHasBattery() const;

  // Returns true if Battery Saver Mode interventions are active. If any state
  // transitions cause an observer notification, this is guaranteed to reflect
  // the *new* value when the observers are notified so the UI layer can make
  // decisions based on the most up-to-date state.
  bool IsBatterySaverActive() const;

  // Returns true if the device is unplugged and using battery power.
  bool IsUsingBatteryPower() const;

  // Returns the time of the last use of battery for the device.
  base::Time GetLastBatteryUsageTimestamp() const;

  // Returns the last sampled device battery percentage. A percentage of -1
  // indicates that the battery state has not been sampled yet.
  int SampledBatteryPercentage() const;

  // If called with `disabled = true`, will disable battery saver mode until
  // the device is plugged in or the user configures the battery saver mode
  // state preference.
  void SetTemporaryBatterySaverDisabledForSession(bool disabled);
  bool IsBatterySaverModeDisabledForSession() const;

 private:
  friend class ::ChromeBrowserMainExtraPartsPerformanceManager;
  friend class ::PerformanceManagerMetricsProviderTest;
  friend class UserPerformanceTuningManagerTest;
  friend class TestUserPerformanceTuningManagerEnvironment;
  friend class ChromeOSBatterySaverProvider;
  friend class DesktopBatterySaverProvider;

  // An implementation of UserPerformanceTuningNotifier::Receiver that
  // forwards the notifications to the UserPerformanceTuningManager on the Main
  // Thread.
  class UserPerformanceTuningReceiverImpl
      : public UserPerformanceTuningNotifier::Receiver {
   public:
    ~UserPerformanceTuningReceiverImpl() override;

    void NotifyTabCountThresholdReached() override;
    void NotifyMemoryThresholdReached() override;
    void NotifyMemoryMetricsRefreshed(ProxyAndPmfKbVector) override;
  };

  explicit UserPerformanceTuningManager(
      PrefService* local_state,
      std::unique_ptr<UserPerformanceTuningNotifier> notifier = nullptr,
      std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate =
          nullptr,
      std::unique_ptr<HighEfficiencyModeDelegate>
          high_efficiency_mode_delegate = nullptr);

  class BatterySaverProvider {
   public:
    virtual ~BatterySaverProvider() = default;

    virtual bool DeviceHasBattery() const = 0;
    virtual bool IsBatterySaverActive() const = 0;
    virtual bool IsUsingBatteryPower() const = 0;
    virtual base::Time GetLastBatteryUsageTimestamp() const = 0;
    virtual int SampledBatteryPercentage() const = 0;
    virtual void SetTemporaryBatterySaverDisabledForSession(bool disabled) = 0;
    virtual bool IsBatterySaverModeDisabledForSession() const = 0;
  };

  void Start();

  void OnHighEfficiencyModePrefChanged();
  void OnHighEfficiencyModeTimeBeforeDiscardChanged();

  void NotifyTabCountThresholdReached();
  void NotifyMemoryThresholdReached();
  void NotifyMemoryMetricsRefreshed();

  // Called from the installed BatterySaverProvider to signify a change in
  // battery saver mode related state.
  void NotifyOnBatterySaverModeChanged(bool battery_saver_mode_enabled);
  void NotifyOnExternalPowerConnectedChanged(bool on_battery_power);
  void NotifyOnDeviceHasBatteryChanged(bool has_battery);
  void NotifyOnBatteryThresholdReached();

  std::unique_ptr<FrameThrottlingDelegate> frame_throttling_delegate_;
  std::unique_ptr<HighEfficiencyModeDelegate> high_efficiency_mode_delegate_;

  std::unique_ptr<BatterySaverProvider> battery_saver_provider_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;

  // Command line switch for overriding the device has battery flag.
  static const char kForceDeviceHasBatterySwitch[];
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_USER_PERFORMANCE_TUNING_MANAGER_H_
