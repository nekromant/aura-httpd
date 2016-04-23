function syntaxHighlight(json) {
    if (typeof json != 'string') {
        json = JSON.stringify(json, undefined, 2);
    }
    json = json.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return '<pre><code class="json">' + json + '</code></pre>';
}


function getAndDisplayJSON(uri) {
    $.ajax({
        url: uri,
        type: 'GET',

        success: function(data) {
            $('.ui-content').html(syntaxHighlight(data));
            $('pre code').each(function(i, block) {
                hljs.highlightBlock(block);
            });
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
                $('.ui-content').html("Node offline");
        },
    });
}

function showNode(item) {
    if (item.type == "control")
        getAndDisplayJSON(item.mountpoint + "/fstab")
    if (item.type == "node")
        getAndDisplayJSON(item.mountpoint + "/exports")
    if (item.type == "static")
        $(".ui-content").html("<iframe width=100% height=100% border=none src='" + item.mountpoint + "/'></iframe>")

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
            alert(icon)
            $(".node-button-" + mpnt).addClass(icon)
        },
        error: function() {
            $(".node-button-" + mpnt)
            .addClass("ui-icon-forbidden")
            .click(function() {
                alert('This node failed to load. Check server log for details');
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

    $.ajax({
        url: '/control/version',
        //        data:{"id":id},
        type: 'GET',

        success: function(data) {
            html = "<center>" + data['aura_version'] + " (" + data['aura_versioncode'] + ")" + "</center>"
            $('.version').replaceWith(html);
        }
    });

}

$(document).on("pageinit", "#mainpage", function() {
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
    uiReload();
