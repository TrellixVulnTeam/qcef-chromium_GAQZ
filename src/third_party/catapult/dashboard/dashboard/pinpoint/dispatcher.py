# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dispatches requests to request handler classes."""

import webapp2

from dashboard.pinpoint import handlers


_URL_MAPPING = [
    # Public API.
    webapp2.Route(r'/api/isolated', handlers.Isolated),
    webapp2.Route(r'/api/isolated/<builder_name>/<git_hash>/<target>',
                  handlers.Isolated),
    webapp2.Route(r'/api/job', handlers.Job),
    webapp2.Route(r'/api/jobs', handlers.Jobs),
    webapp2.Route(r'/api/new', handlers.New),

    # Used internally by Pinpoint. Not accessible from the public API.
    webapp2.Route(r'/api/run/<job_id>', handlers.Run),
]

APP = webapp2.WSGIApplication(_URL_MAPPING, debug=False)
