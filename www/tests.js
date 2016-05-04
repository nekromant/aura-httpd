$("#mainpage").html("Running serdes test suite");


num_failed = 0
started = 0

function call_and_check(method, arg) {
    started++;
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
            started--;
        });
}

function kill(log)
{
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


function check_completion()
{
    if (started==0) {
        $("#mainpage").append("<br><br>completed, killing server<br>")
        kill("['total_failed', " + num_failed + "]");
    } else
    setTimeout(check_completion, 300);
}

call_and_check("echo_u8", 8)
call_and_check("echo_u8", 176)
call_and_check("echo_i8", -78)
call_and_check("echo_i8", 79)
call_and_check("echo_u16", 8)
call_and_check("echo_i16", -8)
call_and_check("echo_u16", 45000)
call_and_check("echo_i16", -20000)
call_and_check("echo_u32", 45000)
call_and_check("echo_i32", -20000)
check_completion()
