/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env node */

module.exports = function(config) {
  "use strict";

  var baseConfig = require("./karma.conf.base.js")(config);

  // List of files / patterns to load in the browser.
  baseConfig.files = baseConfig.files.concat([
    "content/libs/l10n.js",
    "content/shared/libs/react-0.13.3.js",
    "content/shared/libs/classnames-2.2.0.js",
    "content/shared/libs/lodash-3.9.3.js",
    "content/shared/libs/backbone-1.2.1.js",
    "test/shared/vendor/*.js",
    "test/shared/loop_mocha_utils.js",
    "test/karma/head.js", // Stub out DOM event listener due to races.
    "content/shared/js/loopapi-client.js",
    "content/shared/js/utils.js",
    "content/shared/js/models.js",
    "content/shared/js/mixins.js",
    "content/shared/js/actions.js",
    "content/shared/js/otSdkDriver.js",
    "content/shared/js/validate.js",
    "content/shared/js/dispatcher.js",
    "content/shared/js/store.js",
    "content/shared/js/activeRoomStore.js",
    "content/shared/js/views.js",
    "content/shared/js/textChatStore.js",
    "content/shared/js/textChatView.js",
    "content/js/feedbackViews.js",
    "content/js/conversationAppStore.js",
    "content/js/roomStore.js",
    "content/js/roomViews.js",
    "content/js/conversation.js",
    "test/desktop-local/*.js"
  ]);

  // List of files to exclude.
  baseConfig.exclude = baseConfig.exclude.concat([
    "test/desktop-local/panel_test.js"
  ]);

  // Preprocess matching files before serving them to the browser.
  // Available preprocessors: https://npmjs.org/browse/keyword/karma-preprocessor .
  baseConfig.preprocessors = {
    "content/js/*.js": ["coverage"]
  };

  baseConfig.coverageReporter.dir = "test/coverage/desktop";

  config.set(baseConfig);
};
