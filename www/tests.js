$("#mainpage").html("Running serdes test suite");


num_failed = 0
torun = [
    [ "echo_u8", 8 ],
    [ "echo_u8", 176],
    [ "echo_i8", -78 ],
    [ "echo_i8", 79 ],
    [ "echo_u16", 8 ],
    [ "echo_i16", -8 ],
    [ "echo_u16", 45000 ],
    [ "echo_i16", -20000 ],
    [ "echo_u32", 45000 ],
    [ "echo_i32", -20000],
    [ "echo_u64", 45000 ]
];

function call_next() {
    if (torun.length == 0) {
        $("#mainpage").append("<br><br>completed, killing server<br>")
        kill("['total_failed', " + num_failed + "]");
    } else {
        item = torun.pop();
        call_and_check(item[0], item[1])
    }
}


function call_and_check(method, arg) {
    auraCall("/online", method, [arg],
        function(status, data, a) {
            result = "failed";
            if (status == "success") {
                if (arg == data[0])
                    result = "passed";
                else
                    num_failed++;
                $("#mainpage").append("<b>" + result + ":</b> Call: " + method + " Expected: " + arg +
                    " Got: " + data[0] + "<br>");
            } else {
                $("#mainpage").append("<b>failed: </b>Call: " + method + " error: " + data + "<br>");
                num_failed++;
            }
            console.log("[ '" + method + "', '" + result + "'] ");
            call_next();
        });
}

function kill(log) {
    $.ajax({
        url: "/control/terminate",
        type: 'GET',
        success: function(data) {
            console.log(log)
        },
        error: function(data) {
            console.log(log)
        }
    });
}


call_next();
