#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from ast import literal_eval
import os


_HERE = os.path.dirname(__file__)
_SRC_ROOT = os.path.join(_HERE, '..', '..', '..')
_FROM_SRC = lambda p: os.path.abspath(os.path.join(_SRC_ROOT, p))


# High priority code to compile.
_NEED_TO_COMPILE = map(_FROM_SRC, [
  'chrome/browser/resources',
  'chrome/browser/ui/webui',
  'ui/webui/resources/js',
])


# Code that we'd eventually like to compile.
_WANT_TO_COMPILE = map(_FROM_SRC, [
  'chrome/renderer/resources',
  'chrome/test/data',
  'content/renderer/resources',
  'content/test/data',
  'extensions/renderer',
  'extensions/test/data',
  'remoting',
  'ui/file_manager',
  'ui/keyboard',
])


_GIT_IGNORE = open(_FROM_SRC('.gitignore')).read().splitlines()
_IGNORE_DIRS = tuple(map(_FROM_SRC, map(lambda p: p[1:], _GIT_IGNORE)))
_IGNORE_DIRS = filter(os.path.isdir, _IGNORE_DIRS)
_RELEVANT_JS = lambda f: f.endswith('.js') and not f.startswith(_IGNORE_DIRS)


def main():
  line_cache = {}

  def js_files_in_dir(js_dir):
    found_files = set()
    for root, dirs, files in os.walk(js_dir):
      abs_files = [os.path.abspath(os.path.join(root, f)) for f in files]
      found_files.update(filter(_RELEVANT_JS, abs_files))
    return found_files

  def num_lines(f):
    f = os.path.abspath(f)
    if f not in line_cache:
      line_cache[f] = len(open(f, 'r').read().splitlines())
    return line_cache[f]

  # All the files that are already compiled.
  compiled = set()

  closure_dir = os.path.join(_HERE, '..')
  root_gyp = os.path.join(closure_dir, 'compiled_resources.gyp')
  root_contents = open(root_gyp, 'r').read()
  gyp_files = literal_eval(root_contents)['targets'][0]['dependencies']

  for g in gyp_files:
    gyp_file = os.path.join(closure_dir, g.replace(':*', ''))
    targets = literal_eval(open(gyp_file, 'r').read())['targets']

    for target in targets:
      gyp_dir = os.path.dirname(gyp_file)
      target_file = os.path.join(gyp_dir, target['target_name'] + '.js')
      compiled.add(os.path.abspath(target_file))

      if 'variables' in target and 'depends' in target['variables']:
        depends = target['variables']['depends']
        rel_depends = [os.path.join(gyp_dir, d) for d in depends]
        compiled.update([os.path.abspath(d) for d in rel_depends])

  compiled_lines = sum(map(num_lines, compiled))
  print 'compiled: %d files, %d lines' % (len(compiled), compiled_lines)

  # Find and calculate the line count of all .js files in the wanted or needed
  # resource directories.
  files = set()

  for n in _NEED_TO_COMPILE:
    files.update(js_files_in_dir(n))

  need_lines = sum(map(num_lines, files))
  print 'need: %d files, %d lines' % (len(files), need_lines)

  need_done = float(compiled_lines) / need_lines * 100
  print '%.2f%% done with the code we need to compile' % need_done

  for w in _WANT_TO_COMPILE:
    files.update(js_files_in_dir(w))

  want_lines = sum(map(num_lines, files))
  print 'want: %d files, %d lines' % (len(files), want_lines)

  want_done = float(compiled_lines) / want_lines * 100
  print '%.2f%% done with the code we want to compile' % want_done


if __name__ == '__main__':
  main()
