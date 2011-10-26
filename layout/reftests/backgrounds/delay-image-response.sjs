const BinaryOutputStream = Components.Constructor("@mozilla.org/binaryoutputstream;1", "nsIBinaryOutputStream", "setOutputStream");
/* This data is picked from image/test/reftest/generic/check-header.sjs */
const IMAGE_DATA =
  [
   0x89,  0x50,  0x4E,  0x47,  0x0D,  0x0A,  0x1A,  0x0A,  0x00,  0x00,  0x00,
   0x0D,  0x49,  0x48,  0x44,  0x52,  0x00,  0x00,  0x00,  0x64,  0x00,  0x00,
   0x00,  0x64,  0x08,  0x02,  0x00,  0x00,  0x00,  0xFF,  0x80,  0x02,  0x03,
   0x00,  0x00,  0x00,  0x01,  0x73,  0x52,  0x47,  0x42,  0x00,  0xAE,  0xCE,
   0x1C,  0xE9,  0x00,  0x00,  0x00,  0x9E,  0x49,  0x44,  0x41,  0x54,  0x78,
   0xDA,  0xED,  0xD0,  0x31,  0x01,  0x00,  0x00,  0x08,  0x03,  0xA0,  0x69,
   0xFF,  0xCE,  0x5A,  0xC1,  0xCF,  0x07,  0x22,  0x50,  0x99,  0x70,  0xD4,
   0x0A,  0x64,  0xC9,  0x92,  0x25,  0x4B,  0x96,  0x2C,  0x05,  0xB2,  0x64,
   0xC9,  0x92,  0x25,  0x4B,  0x96,  0x02,  0x59,  0xB2,  0x64,  0xC9,  0x92,
   0x25,  0x4B,  0x81,  0x2C,  0x59,  0xB2,  0x64,  0xC9,  0x92,  0xA5,  0x40,
   0x96,  0x2C,  0x59,  0xB2,  0x64,  0xC9,  0x52,  0x20,  0x4B,  0x96,  0x2C,
   0x59,  0xB2,  0x64,  0x29,  0x90,  0x25,  0x4B,  0x96,  0x2C,  0x59,  0xB2,
   0x14,  0xC8,  0x92,  0x25,  0x4B,  0x96,  0x2C,  0x59,  0x0A,  0x64,  0xC9,
   0x92,  0x25,  0x4B,  0x96,  0x2C,  0x05,  0xB2,  0x64,  0xC9,  0x92,  0x25,
   0x4B,  0x96,  0x02,  0x59,  0xB2,  0x64,  0xC9,  0x92,  0x25,  0x4B,  0x81,
   0x2C,  0x59,  0xB2,  0x64,  0xC9,  0x92,  0xA5,  0x40,  0x96,  0x2C,  0x59,
   0xB2,  0x64,  0xC9,  0x52,  0x20,  0x4B,  0x96,  0x2C,  0x59,  0xB2,  0x64,
   0x29,  0x90,  0x25,  0x4B,  0x96,  0x2C,  0x59,  0xB2,  0x14,  0xC8,  0x92,
   0x25,  0x4B,  0x96,  0x2C,  0x59,  0x0A,  0x64,  0xC9,  0xFA,  0xB6,  0x89,
   0x5F,  0x01,  0xC7,  0x24,  0x83,  0xB2,  0x0C,  0x00,  0x00,  0x00,  0x00,
   0x49,  0x45,  0x4E,  0x44,  0xAE,  0x42,  0x60,  0x82,
  ];

/**
 * The timer is needed when a delay is set. We need it to be out of the method
 * so it is not eaten alive by the GC.
 */
var timer;

function handleRequest(request, response) {
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png", false);

  function imageWrite() {
    var stream = new BinaryOutputStream(response.bodyOutputStream);
    stream.writeByteArray(IMAGE_DATA, IMAGE_DATA.length);
  }

  // If there is no delay, we write the image and leave.
  if (!("delay" in query)) {
    imageWrite();
    return;
  }

  // If there is a delay, we create a timer which, when it fires, will write
  // image and leave.
  response.processAsync();
  const nsITimer = Components.interfaces.nsITimer;

  timer = Components.classes["@mozilla.org/timer;1"].createInstance(nsITimer);
  timer.initWithCallback(function() {
    imageWrite();
    response.finish();
  }, query["delay"], nsITimer.TYPE_ONE_SHOT);
}
