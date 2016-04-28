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
            if (data.status == "pending" || status != "success") {
                setTimeout(function() {
                    doPollForResult(uri, callback)
                }, 1000);
            } else
            callback(status, data)
        }
    });
}


function auraCall(nodepath, method, args, handler) {
    $.ajax({
        url: nodepath + "/call/" + method,
        type: 'GET',
        data: args,
        error: function(xhr, status, err){
            handler(status, err);
        },
        success: function(data, status, xhr) {
            handler(status, data);
        }
    });
}
