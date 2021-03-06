#!/bin/sh
# Run from topsrcdir, no args

if [ "$1" == "--help" ]; then
  echo "Usage: ./run-all-loop-tests.sh [options]"
  echo "    --skip-e10s  Skips the e10s tests"
  exit 0;
fi

# Causes script to abort immediately if error code is not checked.
set -e

# Main tests

LOOPDIR=browser/components/loop
ESLINT=standalone/node_modules/.bin/eslint
if [ -x "${LOOPDIR}/${ESLINT}" ]; then
  echo 'running eslint; see http://eslint.org/docs/rules/ for error info'
  (cd ${LOOPDIR} && ./${ESLINT} --ext .js --ext .jsm --ext .jsx .)
  if [ $? != 0 ]; then
    exit 1;
  fi
  echo 'eslint run finished.'
fi

# Build tests coverage.
MISSINGDEPSMSG="\nMake sure all dependencies are up to date by running
'npm install' inside the 'browser/components/loop/test/' directory.\n"
(
cd ${LOOPDIR}/test
if ! npm run-script build-coverage ; then
  echo $MISSINGDEPSMSG && exit 1
fi
)

./mach xpcshell-test ${LOOPDIR}/
./mach marionette-test ${LOOPDIR}/manifest.ini

# The browser_parsable_css.js can fail if we add some css that isn't parsable.
#
# The check to make sure that the media devices can be used in Loop without
# prompting is in browser_devices_get_user_media_about_urls.js. It's possible
# to mess this up with CSP handling, and probably other changes, too.

TESTS="
  ${LOOPDIR}/test/mochitest
  browser/base/content/test/general/browser_devices_get_user_media_about_urls.js
  browser/base/content/test/general/browser_parsable_css.js
"

# Due to bug 1209463, we need to split these up and run them individually to
# ensure we stop and report that there's an error.
for test in $TESTS
do
  ./mach mochitest $test
  # UITour & get user media aren't compatible with e10s currenly.
  if [ "$1" != "--skip-e10s" ] && \
     [ "$test" != "browser/base/content/test/general/browser_devices_get_user_media_about_urls.js" ];
  then
    ./mach mochitest --e10s $test
  fi
done
