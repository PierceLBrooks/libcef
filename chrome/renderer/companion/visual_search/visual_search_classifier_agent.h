// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_AGENT_H_
#define CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_AGENT_H_

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace companion::visual_search {

class VisualSearchClassifierAgent : public content::RenderFrameObserver {
 public:
  using ClassifierResultCallback =
      base::OnceCallback<void(std::vector<SkBitmap>)>;

  static VisualSearchClassifierAgent* Create(
      content::RenderFrame* render_frame);

  VisualSearchClassifierAgent(const VisualSearchClassifierAgent&) = delete;
  VisualSearchClassifierAgent& operator=(const VisualSearchClassifierAgent&) =
      delete;

  ~VisualSearchClassifierAgent() override;

  // RenderFrameObserver implementation:
  void OnDestruct() override;

  // This method is the main entrypoint which triggers visual classification.
  // This is ultimately going to be called via Mojom IPC from the browser
  // process.
  void StartVisualClassification(base::File visual_model,
                                 const std::string config_proto,
                                 ClassifierResultCallback callback);

 private:
  explicit VisualSearchClassifierAgent(content::RenderFrame* render_frame);

  // Private method used to post result from long-running visual classification
  // tasks that runs in the background thread. This method should run in the
  // same thread that triggered the classification task (i.e. main thread).
  void OnClassificationDone(const std::vector<SkBitmap> results);

  // Used to track whether there is an ongoing classification task, if so, we
  // drop the incoming request.
  bool is_classifying_ = false;

  // Pointer to RenderFrame used for DOM traversal and extract image bytes.
  content::RenderFrame* render_frame_ = nullptr;

  // Using a memory-mapped file to reduce memory consumption of model bytes.
  base::MemoryMappedFile visual_model_;

  // The result callback is used to give us a path back to results. It
  // typically will lead to a Mojom IPC call back to the browser process.
  ClassifierResultCallback result_callback_;

  // Pointer factory necessary for scheduling tasks on different threads.
  base::WeakPtrFactory<VisualSearchClassifierAgent> weak_ptr_factory_{this};
};

}  // namespace companion::visual_search

#endif  // CHROME_RENDERER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_CLASSIFIER_AGENT_H_
