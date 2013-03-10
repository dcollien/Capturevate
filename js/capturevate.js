(function() {
    var recorders = {
        pageClicks: function(data) {
            $(document).click(function() {
                data['pageClicks'] = data['pageClicks'] || 0;
                data['pageClicks']++;
            });
        },

        secondsOnPage: function(data) {
            $(window).load(function() {
                setInterval(function() {
                    data['secondsOnPage'] = data['secondsOnPage'] || 0;
                    data['secondsOnPage']++;
                }, 1000);
            });
        },

        mouseDistance: function(data) {
            var lastX, lastY;

            $(window).load(function() {
                $(document).mousemove(function(evt) {
                    var xDist, yDist;
                    lastX = lastX || evt.offsetX;
                    lastY = lastY || evt.offsetY;

                    data['mouseXDistance'] = data['mouseXDistance'] || 0;
                    data['mouseYDistance'] = data['mouseYDistance'] || 0;
                    data['mouseDistance'] = data['mouseDistance'] || 0;

                    xDist = Math.abs(lastX - evt.offsetX);
                    yDist = Math.abs(lastY - evt.offsetY)
                    data['mouseXDistance'] += xDist;
                    data['mouseYDistance'] += yDist;
                    data['mouseDistance'] += Math.sqrt(xDist*xDist + yDist*yDist);

                    lastX = evt.offsetX;
                    lastY = evt.offsetY;
                });
            });
        },

        scrollDistance: function(data) {
            $(document).scroll(function(evt) {
                data['scrollDistance'] = data['scrollDistance'] || 0;
                data['scrollDistance'] = Math.max(data['scrollDistance'], $(document).scrollTop());
            });
        },

        windowSize: function(data) {
            $(window).load(function() {
                data['windowWidth'] = $(window).width();
                data['windowHeight'] = $(window).height();
            });

            $(window).resize(function() {
                data['windowWidth'] = $(window).width();
                data['windowHeight'] = $(window).height();
            });
        },

        userAgent: function(data) {
            data['userAgent'] = navigator.userAgent;
        }
    };

    if (typeof jQuery !== 'undefined') {
        jQuery.capturevate = function() {
            var default_options = {
                host: 'http://127.0.0.1',
                port: '8088',
                enabled: {},
                recorders: {},
                data: {}
            };
            var options;
            var data = {};
            var url;
            switch(arguments[0]) {
                // methods to respond to
                case 'version':
                    return 'jQuery Capturevate V0.1';
                
                // init
                default:
                    options = arguments[0];
                    options = $.extend({ }, default_options, options);

                    if (options.data) {
                        data = $.extend(data, options.data);
                    }

                    if (typeof data.path === 'undefined') {
                        data.path = window.location.pathname;
                    }

                    $.each(recorders, function(key, val) {
                        if (options.enabled && options.enabled[key] !== false) {
                            val(data);
                        }
                    });

                    $.each(options.recorders, function(key, val) {
                        val(data);
                    });

                    url = options.host;
                    if (options.port) {
                        url += ':' + options.port
                    }

                    data['event'] = 'load';
                    $.ajax({
                        'type': 'post',
                        'data': data,
                        'url': url,
                        'crossDomain': true,
                        'async': true
                    });

                    $(window).unload(function() {
                        data['event'] = 'unload';
                        $.ajax({
                            'type': 'post',
                            'data': data,
                            'url': url,
                            'crossDomain': true,
                            'async': false
                        });
                    });

                    return $;
            }
        };
    } else if (typeof console !== 'undefined') {
        console.log('jQuery required to use Capturevate');
    }
})();