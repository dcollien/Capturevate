Capturevate: Capturing Captivation
------------------------------------
The aim of Capturevate is to collect web-site interaction data to measure user engagement and captivation. Capturevate stores a user's interaction behaviour with various page elements for later analysis.

This service captures user interaction events in the browser (javascript). These events are sent on page load (asynchronously) and unload (synchronously, before the user navigates away) to a logging server to be stored for later analyis.

The logging server is designed to handle a large number of storage requests as fast as possible, as to not to have any detectable impact on the browsing experience. The logging server caches this interaction data in redis (to be fast) and a separate service drains the cache to persist the data for analysis. A service is provided which drains the redis cache into mongoDB for storage.


Built-In Measures and Data Recorded
-------------------------------------
- total clicks on the page
- seconds spent on the page
- total mouse distance travelled (X, Y and euclidean distance)
- the maximum distance from the top of a page the user has scrolled
- the size of the window (on load, and on unload after possible resizing)
- user agent data (browser and operating system information of the user)
- the location path (e.g. /index.html)
- a user (if supplied in the data option)

Using Capturevate
------------------
Capturevate comes with a jQuery plugin, which can be used as follows:

<pre>
    &lt;div id="interesting"&gt;Something Interesting&lt;/div&gt;
    &lt;script src="jquery.min.js"&gt;&lt;/script&gt;
    &lt;script src="capturevate.js"&gt;&lt;/script&gt;
    &lt;script&gt;
        $.capturevate({
            data: {
                user: 'foo'
            },
            recorders: {
                interestingClicks: function(data) {
                    $('#interesting').click(function() {
                        data['interestingClicks'] = (data['interestingClicks'] || 0) + 1;
                    });
                }
            }
        });
    &lt;/script&gt;
</pre>

Options
--------
- host: specifies the host of the logging server, defaults to 'http://127.0.0.1'
- port: specifies the port of the logging server, defaults to 8088
- data: an {object} containing extra data to send with the recorded behaviours. It is expected that the 'user' key will contain a user ID (otherwise '?' is used)
- recorders: an {object} whose values are functions which take a data object as an argument. These recorder functions will be initialised with the capturevate plugin and can be used to add to the data which is sent to the logger.
- enabled: an {object} whose values are either true or false, defining which default recorders are activated. Un-listed recorders are activated by default, e.g. the following will enable all default recorders except mouseDistance:

<pre>
    enabled: {
        pageClicks: true,
        secondsOnPage: true,
        mouseDistance: false,
        scrollDistance: true,
        windowSize: true,
        userAgent: true
    }
</pre>

Server Requirements
--------------------
The logging server (capturevate-server) depends on the libevent and hiredis libraries (and redis to be running). The mongoDB storage service (capturevate-store-mongoDB) depends on the hiredis and mongoc libraries (and redis + mongod to be running).

in ./conf/:
capturevate-server.conf and capturevate-store-mongoDB.conf specify example connection options to redis and mongoDB.

Run <code>capturevate-server</code> or <code>capturevate-store-mongoDB</code> with <code>-c &lt;config-file&gt;</code> to specify the configuration file.
otherwise, capturevate-server takes its HTTP host and port as arguments

Building and Installing Capturevate
------------------------------------
See <a href="./doc/building.md">Building Capturevate</a> for instructions


