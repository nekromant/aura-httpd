var currentNode;
var currentMethods;
var automaticRefresh = false;
var automaticRefreshPending = false;

function syntaxHighlight(json) {
    if (typeof json != 'string') {
        json = JSON.stringify(json, undefined, 2);
    }
    json = json.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return '<pre><code class="json">' + json + '</code></pre>';
}

function displayJSON(data) {
    $('.pagedata').hide();
    $('#content-json').html(syntaxHighlight(data));
    $('#content-json').show();
    $('pre code').each(function(i, block) {
        hljs.highlightBlock(block);
    });
}

function getAndDisplayJSON(uri) {
    $.ajax({
        url: uri,
        type: 'GET',

        success: function(data) {
            displayJSON(data)
        }
    });
}

function showNode(i) {
    var uri = item.mountpoint + "/status";
    $.ajax({
        url: uri,
        type: 'GET',
        success: function(data) {
            if (data.status == "online")
                getAndDisplayJSON(item.mountpoint + "/exports")
            else
                $('#content-static').html("Node offline").show();
        },
    });
}

function enableNodeMenu() {
    $(".nodebuttons").show();
}

function disableNodeMenu() {
    $(".node-button").removeClass("ui-btn-active");
    $(".nodebuttons").hide();
}

function nodeShowExports() {
    getAndDisplayJSON(currentNode.mountpoint + "/exports")
}

function nodeShowStatus() {
    getAndDisplayJSON(currentNode.mountpoint + "/status")
}

function nodeShowEvents() {
    getAndDisplayJSON(currentNode.mountpoint + "/events")
}

function nodeSubmitCall() {
    name = $("#call-method-name").val();
    args = "[ " + $("#call-method-args").val() + " ] ";
    //showError(name, args);
    auraCall(currentNode.mountpoint, name, args, function(status, data) {
        if (status != "success") {
            showError(status, data);
        } else if (data.result == "error")
            showError(data.result, data.why);
        else {
            showError("Call completed", syntaxHighlight(data));
            $('pre code').each(function(i, block) {
                hljs.highlightBlock(block);
            });
        }
    })
}

function updateArgHint() {
    var method = currentMethods[$("#call-method-name").val()];
    $("#call-method-arghint").html("");
    if (method) {
        $("#node-method-list li").addClass('ui-screen-hidden');
        method.afmt.forEach(function(item, i, arr) {
            var h = $("#call-method-arghint").html();
            if (h != "")
                $("#call-method-arghint").append(", ")
            $("#call-method-arghint").append(item)
        })

    }
    //if (currentMethods[])
}

function nodeShowWelcome() {
    disableNodeMenu();
    $('.pagedata').hide();
    $('#content-welcome').show();
}

function showError(title, text) {
    $("#error-title").html(title);
    $("#error-text").html(text);
    $('#error-message').popup();
    $('#error-message').popup('open');
    //window.location.href = "#error-message";
}

function nodeShowCallUi() {
    $('.pagedata').hide();
    $("#content-call").show();
    $("#node-method-list").html("")
    $("#node-method-list li").addClass('ui-screen-hidden');
    currentMethods = {};
    var uri = currentNode.mountpoint + "/exports";
    $.ajax({
        url: uri,
        type: 'GET',
        success: function(data) {
            data.forEach(function(item, i, arr) {
                if (item.type == "method") {
                    $("#node-method-list").append("<li>" + item.name + "</li>");
                    currentMethods[item.name] = item;
                }
            })
            $("#call-method-name").change(function() {
                updateArgHint();
            })
            $("#node-method-list li").on("click", function() {
                $("#call-method-name").val($(this).text());
                $("#node-method-list li").addClass('ui-screen-hidden');
                updateArgHint();
            });

        },
    });

}

function showNode(item) {
    $('.pagedata').hide();
    disableNodeMenu();
    if (item.type == "control")
        getAndDisplayJSON(item.mountpoint + "/fstab")
    if (item.type == "node") {
        enableNodeMenu();
        currentNode = item;
        displayJSON(item)
    }

    if (item.type == "static") {
        $("#content-static").html("<iframe width=100% height=100% border=none src='" + item.mountpoint + "/'></iframe>")
        $("#content-static").show();
    }
}


function updateNodeState(nodepath) {
    var mpnt = nodepath.replace("\/", "-");
    mpnt = mpnt.replace("\/", "-");
    $(".node-button-" + mpnt).removeClass("ui-icon-delete")
    $(".node-button-" + mpnt).removeClass("ui-icon-check")
    $(".node-button-" + mpnt).removeClass("ui-icon-forbidden")
    $.ajax({
        url: nodepath + '/status',
        type: 'GET',
        success: function(data) {
            var icon = "ui-icon-delete"
            if (data.status == "online")
                icon = "ui-icon-check"
            $(".node-button-" + mpnt).addClass(icon)
        },
        error: function() {
            $(".node-button-" + mpnt)
                .addClass("ui-icon-forbidden")
                .click(function() {
                    disableNodeMenu();
                    showError("ERROR!", "This node failed to load hence will never go online. Check server log for details");
                })
        },
    });
}

function uiReload() {
    $.ajax({
        url: "/control/fstab",
        type: 'GET',

        success: function(data) {
            $(".nodelist").html("");
            $(".mpointlist").html("");
            data["fstab"].forEach(function(item, i, arr) {
                var taget;
                var mpnt = item.mountpoint.replace("\/", "-");
                mpnt = mpnt.replace("\/", "-");
                var newli = $("<a href=\"#\">")
                    .append(item.mountpoint)
                    .addClass("ui-btn ")
                    .addClass("node-button")
                    .addClass("node-button-" + mpnt)
                    .click(function() {
                        showNode(item)
                    });


                if (item.type == "node") {
                    target = ".nodelist"
                    newli.addClass("ui-btn-icon-right")
                    updateNodeState(item.mountpoint)
                } else {
                    target = ".mpointlist"
                }
                $(target).append(newli);
            });
        }
    });

    if ($("#autorefresh").is(":checked")) {
        setTimeout(uiReload, 1000);
    }
}


$(document).on("pageinit", "#mainpage", function() {
    //$('#error-message').popup();
    $(document).on("swipeleft swiperight", "#mainpage", function(e) {
        // We check if there is no open panel on the page because otherwise
        // a swipe to close the left panel would also open the right panel (and v.v.).
        // We do this by checking the data that the framework stores on the page element (panel: open).
        if ($.mobile.activePage.jqmData("nodePanel") !== "open") {
            if (e.type === "swipeleft") {
                $("#nodePanel").panel("close");
            } else if (e.type === "swiperight") {
                $("#nodePanel").panel("open");
            }
        }
    });
});


disableNodeMenu();
uiReload();

$.ajax({
    url: '/control/version',
    //        data:{"id":id},
    type: 'GET',

    success: function(data) {
        html = "<center>" + data['aura_version'] + " (" + data['aura_versioncode'] + ")" + "</center>"
        $('.version').replaceWith(html);
    }
});
