function auraGetEvents(nodepath, handler)
{
    $.ajax({
        url: nodepath + "/events",
        type: 'GET',
        success: function(data) {
            handler(data)
        }
    });
}

function auraCall(nodepath, method, args, handler)
{
    $.ajax({
        url: nodepath + "/call/" + method,
        type: 'POST',
        data: args,
        success: function(data) {
            handler(data)
        }
    });
}
