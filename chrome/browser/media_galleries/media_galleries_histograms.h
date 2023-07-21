// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_HISTOGRAMS_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_HISTOGRAMS_H_

namespace media_galleries {

enum MediaGalleriesUsages {
  DIALOG_GALLERY_ADDED,
  DIALOG_PERMISSION_ADDED,
  DIALOG_PERMISSION_REMOVED,
  GET_MEDIA_FILE_SYSTEMS,
  PROFILES_WITH_USAGE,
  SHOW_DIALOG,
  SAVE_DIALOG,
  WEBUI_ADD_GALLERY,
  WEBUI_FORGET_GALLERY,
  PREFS_INITIALIZED,
  PREFS_INITIALIZED_ERROR,
  GET_ALL_MEDIA_FILE_SYSTEM_METADATA,
  GET_METADATA,
  ADD_USER_SELECTED_FOLDER,
  DELETED_START_MEDIA_SCAN,
  DELETED_CANCEL_MEDIA_SCAN,
  DELETED_ADD_SCAN_RESULTS,
  DELETED_SCAN_FINISHED,
  DELETED_ADD_SCAN_RESULTS_CANCELLED,
  DELETED_ADD_SCAN_RESULTS_ACCEPTED,
  DELETED_ADD_SCAN_RESULTS_FORGET_GALLERY,
  DIALOG_FORGET_GALLERY,
  DROP_PERMISSION_FOR_MEDIA_FILE_SYSTEM,
  GET_ALL_GALLERY_WATCH,
  REMOVE_ALL_GALLERY_WATCH,
  DELETED_ITUNES_FILE_SYSTEM_USED,
  DELETED_PICASA_FILE_SYSTEM_USED,
  DELETED_IPHOTO_FILE_SYSTEM_USED,
  MEDIA_GALLERIES_NUM_USAGES
};

void UsageCount(MediaGalleriesUsages usage);

}  // namespace media_galleries

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_HISTOGRAMS_H_
