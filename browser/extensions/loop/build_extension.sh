#!/bin/sh

if [ -f loop\@test.mozilla.org.xpi ]; then
  rm loop\@test.mozilla.org.xpi
fi

zip -x build_extension.sh -x standalone/\* -x build-jsx -x ui/ui-showcase.jsx run-all-loop-tests.sh -x test\* -r loop\@test.mozilla.org.xpi .
