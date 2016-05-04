function auraGetEvents(nodepath, handler, arg) {
    $.ajax({
        url: nodepath + "/events",
        type: 'GET',
        success: function(data) {
            handler(data, arg)
        }
    });
}

function doPollForResult(uri, callback, interval, arg) {
    $.ajax({
        url: uri,
        type: 'GET',
        success: function(data, status, xhr) {
            if (data.status == "pending" || status != "success") {
                setTimeout(function() {
                    doPollForResult(uri, callback, interval, arg)
                }, interval);
            } else
            if (typeof(callback) == "function")
                callback(status, data.data, arg)
        }
    });
}

function auraCallAsync(nodepath, method, args, handler, interval, arg) {
    $.ajax({
        url: nodepath + "/acall/" + method,
        type: 'PUT',
        data: JSON.stringify(args),
        error: function(xhr, status, err) {
            if (typeof(handler) == "function")
                handler(status, err, arg);
        },
        success: function(data, status, xhr) {
            if (xhr.status == 202) {
                var loc = xhr.getResponseHeader('Location');
                doPollForResult(loc, handler, interval);
            } else
            if (typeof(handler) == "function")
                handler("error", "Expected 202, got " + xhr.status, arg)
        }
    });
}

function auraCall(nodepath, method, args, handler, arg) {
    $.ajax({
        url: nodepath + "/call/" + method,
        type: 'PUT',
        data: JSON.stringify(args),
        error: function(xhr, status, err) {
            if (typeof(handler) == "function")
                handler(status, err, arg);
        },
        success: function(data, status, xhr) {
            if (typeof(handler) == "function")
                handler(status, data.data, arg);
        }
    });
}
