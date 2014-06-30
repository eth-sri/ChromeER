# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'autocomplete',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'component_metrics_proto',
        'url_fixer',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'autocomplete/autocomplete_input.cc',
        'autocomplete/autocomplete_input.h',
        'autocomplete/autocomplete_scheme_classifier.h',
        'autocomplete/url_prefix.cc',
        'autocomplete/url_prefix.h',
      ],
    },
    {
      'target_name': 'autocomplete_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'autocomplete',
        'component_metrics_proto',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'autocomplete/test_scheme_classifier.cc',
        'autocomplete/test_scheme_classifier.h',
      ],
    },
  ],
}
