function auraGetEvents(nodepath, handler) {
    $.ajax({
        url: nodepath + "/events",
        type: 'GET',
        success: function(data) {
            handler(data)
        }
    });
}



function doPollForResult(uri, callback) {
    $.ajax({
        url: uri,
        type: 'GET',
        success: function(data, status, xhr) {
            if (data.status == "pending") {
                setTimeout(function() {
                    doPollForResult(uri, callback)
                }, 1000);
            } else
            callback(data)
        }
    });
}


function auraCall(nodepath, method, args, handler) {
    $.ajax({
        url: nodepath + "/call/" + method,
        type: 'GET',
        data: args,
        success: function(data, status, xhr) {
            if (!data.result) {
                var loc = xhr.getResponseHeader('Location');
                doPollForResult(loc, handler);
            } else
                handler(data)
        }
    });
}
