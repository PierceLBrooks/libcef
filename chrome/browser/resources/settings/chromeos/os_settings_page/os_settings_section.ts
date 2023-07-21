// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-section' shows a paper material themed section with a header
 * which shows its page title.
 *
 * The section can expand vertically to fill its container's padding edge.
 *
 * Example:
 *
 *    <os-settings-section
 *        page-title="[[pageTitle]]"
 *        section="[[Section.kNetwork]]">
 *      <!-- Insert your section controls here -->
 *    </os-settings-section>
 */

import '//resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {Section} from '../mojom-webui/routes.mojom-webui.js';

import {getTemplate} from './os_settings_section.html.js';

export class OsSettingsSectionElement extends PolymerElement {
  static get is() {
    return 'os-settings-section' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * If defined, the value must match one of the Section enum entries from
       * routes.mojom. MainPageBeMixin will expand this section if this section
       * name matches currentRoute.section.
       */
      section: {
        type: Number,
        reflectToAttribute: true,
      },

      /**
       * Title for the section header. Initialize so we can use the
       * getTitleHiddenStatus_ method for accessibility.
       */
      pageTitle: {
        type: String,
        value: '',
      },
    };
  }

  section: Section;
  pageTitle: string;

  override ready() {
    super.ready();

    if (this.section !== undefined) {
      assert(this.section in Section, `Invalid section: ${this.section}.`);
    }
  }

  /**
   * Get the value to which to set the aria-hidden attribute of the section
   * heading.
   * @return A return value of false will not add aria-hidden while aria-hidden
   *    requires a string of 'true' to be hidden as per aria specs. This
   *    function ensures we have the right return type.
   */
  private getTitleHiddenStatus_(): boolean|string {
    return this.pageTitle ? false : 'true';
  }

  override focus() {
    this.shadowRoot!.querySelector<HTMLElement>('.title')!.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsSectionElement.is]: OsSettingsSectionElement;
  }
}

customElements.define(OsSettingsSectionElement.is, OsSettingsSectionElement);
