# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for catapult.

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""
import re
import sys

_EXCLUDED_PATHS = (
    r'(.*[\\/])?\.git[\\/].*',
    r'.+\.png$',
    r'.+\.svg$',
    r'.+\.skp$',
    r'.+\.gypi$',
    r'.+\.gyp$',
    r'.+\.gn$',
    r'.*\.gitignore$',
    r'.*codereview.settings$',
    r'.*AUTHOR$',
    r'^CONTRIBUTORS\.md$',
    r'.*LICENSE$',
    r'.*OWNERS$',
    r'.*README\.md$',
    r'^dashboard[\\/]dashboard[\\/]api[\\/]examples[\\/].*.js',
    r'^dashboard[\\/]dashboard[\\/]templates[\\/].*',
    r'^experimental[\\/]heatmap[\\/].*',
    r'^experimental[\\/]trace_on_tap[\\/]third_party[\\/].*',
    r'^perf_insights[\\/]test_data[\\/].*',
    r'^perf_insights[\\/]third_party[\\/].*',
    r'^third_party[\\/].*',
    r'^tracing[\\/]\.allow-devtools-save$',
    r'^tracing[\\/]bower\.json$',
    r'^tracing[\\/]\.bowerrc$',
    r'^tracing[\\/]tracing_examples[\\/]string_convert\.js$',
    r'^tracing[\\/]test_data[\\/].*',
    r'^tracing[\\/]third_party[\\/].*',
    r'^telemetry[\\/]support[\\/]html_output[\\/]results-template.html',
)


_CATAPULT_BUG_ID_RE = re.compile(r'#[1-9]\d*')
_RIETVELD_BUG_ID_RE = re.compile(r'[1-9]\d*')
_RIETVELD_REPOSITORY_NAMES = frozenset({'chromium', 'v8', 'angleproject'})

def CheckChangeLogBug(input_api, output_api):
  # Show a presubmit message if there is no Bug line or an empty Bug line.
  if not input_api.change.BugsFromDescription():
    return [output_api.PresubmitNotifyResult(
        'If this change has associated Catapult and/or Rietveld bug(s), add a '
        '"Bug: <bug>(, <bug>)*" line to the patch description where <bug> can '
        'be one of the following: catapult:#NNNN, ' +
        ', '.join('%s:NNNNNN' % n for n in _RIETVELD_REPOSITORY_NAMES) + '.')]

  # Check that each bug in the BUG= line has the correct format.
  error_messages = []
  catapult_bug_provided = False

  for index, bug in enumerate(input_api.change.BugsFromDescription()):
    # Check if the bug can be split into a repository name and a bug ID (e.g.
    # 'catapult:#1234' -> 'catapult' and '#1234').
    bug_parts = bug.split(':')
    if len(bug_parts) != 2:
      error_messages.append('Invalid bug "%s". Bugs should be provided in the '
                            '"<repository-name>:<bug-id>" format.' % bug)
      continue
    repository_name, bug_id = bug_parts

    if repository_name == 'catapult':
      if not _CATAPULT_BUG_ID_RE.match(bug_id):
        error_messages.append('Invalid bug "%s". Bugs in the Catapult '
                              'repository should be provided in the '
                              '"catapult:#NNNN" format.' % bug)
      catapult_bug_provided = True
    elif repository_name in _RIETVELD_REPOSITORY_NAMES:
      if not _RIETVELD_BUG_ID_RE.match(bug_id):
        error_messages.append('Invalid bug "%s". Bugs in the Rietveld %s '
                              'repository should be provided in the '
                              '"%s:NNNNNN" format.' % (bug, repository_name,
                                                       repository_name))
    else:
      error_messages.append('Invalid bug "%s". Unknown repository "%s".' % (
          bug, repository_name))

  return map(output_api.PresubmitError, error_messages)


def CheckChange(input_api, output_api):
  results = []
  try:
    sys.path += [input_api.PresubmitLocalPath()]
    from catapult_build import js_checks
    from catapult_build import html_checks
    from catapult_build import repo_checks
    results += input_api.canned_checks.PanProjectChecks(
        input_api, output_api, excluded_paths=_EXCLUDED_PATHS)
    results += CheckChangeLogBug(input_api, output_api)
    results += js_checks.RunChecks(
        input_api, output_api, excluded_paths=_EXCLUDED_PATHS)
    results += html_checks.RunChecks(
        input_api, output_api, excluded_paths=_EXCLUDED_PATHS)
    results += repo_checks.RunChecks(input_api, output_api)
  finally:
    sys.path.remove(input_api.PresubmitLocalPath())
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
