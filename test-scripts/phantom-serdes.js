#!/opt/phantomjs-2.1.1-linux-x86_64/bin/phantomjs --debug=true
var webPage = require('webpage');
var page = webPage.create();

page.onConsoleMessage = function(msg, lineNum, sourceId) {
  console.log('CONSOLE: ' + msg + ' (from line #' + lineNum + ' in "' + sourceId + '")');
  o = eval(msg);
  if (o[0] == "total_failed") {
    console.log("A total of " + o[1] + " tests failed")
    phantom.exit(o[1]);
    }
};

page.open('http://127.0.0.1:8088/static/test-serdes.html', function(status) {
  console.log("Status: " + status);
});

function timeout_test()
{
  console.log("TEST TIMEOUT")
  phantom.exit(1);
}

setTimeout(timeout_test, 5000);
