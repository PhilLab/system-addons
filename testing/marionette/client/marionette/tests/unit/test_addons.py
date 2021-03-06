# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from marionette import MarionetteTestCase
from marionette_driver.addons import Addons, AddonInstallException

here = os.path.abspath(os.path.dirname(__file__))


class TestAddons(MarionetteTestCase):
    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.addons = Addons(self.marionette)
        self.addon_path = os.path.join(here, 'mn-restartless.xpi')


    @property
    def all_addon_ids(self):
        with self.marionette.using_context('chrome'):
            addons = self.marionette.execute_async_script("""
              Components.utils.import("resource://gre/modules/AddonManager.jsm");
              AddonManager.getAllAddons(function(addons){
                let ids = addons.map(function(x) {
                  return x.id;
                });
                marionetteScriptFinished(ids);
              });
            """)

        return addons

    def test_install_and_remove_unsigned_addon(self):
        self.addons.signature_required = False

        addon_id = self.addons.install(self.addon_path)
        self.assertIn(addon_id, self.all_addon_ids)

        self.addons.uninstall(addon_id)
        self.assertNotIn(addon_id, self.all_addon_ids)

        self.addons.signature_required = True

    def test_install_unsigned_addon_with_signature_required(self):
        self.signature_required = True

        with self.assertRaises(AddonInstallException):
            self.addons.install(self.addon_path)
